/**
 * @file slplugin.c Studio Link DAW plugin lifecycle
 *
 * libre and baresip run in a dedicated thread, which does the whole
 * init -> re_main() -> close sequence. The plugin wrappers only call
 * sl_plugin_init() and sl_plugin_close() (reference counted, one call per
 * plugin instance).
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <re.h>
#include <baresip.h>
#include "slconf.h"
#include "slcacert.h"
#include "slplugin.h"


enum {
	ASYNC_WORKERS = 4,
	STOP_TIMEOUT  = 5000, /* [ms] force re_main exit after ua_stop_all */
};

enum mq_id {
	MQ_STOP,
};

static struct {
	once_flag once;
	mtx_t lock;       /* serializes init/close, guards the fields below */
	cnd_t ready_cnd;
	unsigned refs;
	bool running;     /* thread is started and not joined yet */
	bool ready;       /* init sequence finished (see err) */
	int err;          /* result of the init sequence */
	thrd_t thread;
	struct mqueue *mq;
	struct tmr tmr_stop;
} sl = {
	.once = ONCE_FLAG_INIT,
};


static void once_init(void)
{
	mtx_init(&sl.lock, mtx_plain);
	cnd_init(&sl.ready_cnd);
}


static void ua_exit_handler(void *arg)
{
	(void)arg;
	debug("slplugin: ua exited -- stopping main runloop\n");

	re_cancel();
}


static void tmr_stop_handler(void *arg)
{
	(void)arg;
	warning("slplugin: ua stop timeout -- stopping main runloop\n");

	re_cancel();
}


static void mqueue_handler(int id, void *data, void *arg)
{
	(void)data;
	(void)arg;

	switch ((enum mq_id)id) {

	case MQ_STOP:
		tmr_start(&sl.tmr_stop, STOP_TIMEOUT, tmr_stop_handler, NULL);
		ua_stop_all(false);
		break;
	}
}


static void signal_ready(int err)
{
	mtx_lock(&sl.lock);
	sl.err   = err;
	sl.ready = true;
	cnd_signal(&sl.ready_cnd);
	mtx_unlock(&sl.lock);
}


static int plugin_thread(void *arg)
{
	struct mqueue *mq = NULL;
	int err;
	(void)arg;

	err = libre_init();
	if (err) {
		signal_ready(err);
		return err;
	}

	tmr_init(&sl.tmr_stop);

	err = sl_conf_configure(false);
	if (err) {
		warning("slplugin: configure failed: %m\n", err);
		goto out;
	}

	err = re_thread_async_init(ASYNC_WORKERS);
	if (err)
		goto out;

	err = baresip_init(conf_config());
	if (err) {
		warning("slplugin: baresip init failed (%m)\n", err);
		goto out;
	}

	err = ua_init(sl_software(), true, true, true);
	if (err)
		goto out;

	uag_set_exit_handler(ua_exit_handler, NULL);

	/* SIP TLS: trust the embedded CA certificates (verification is
	 * enabled with sip_verify_server) */
	if (uag_tls())
		(void)sl_cacert_add(uag_tls());

	err = conf_modules();
	if (err)
		goto out;

	err = mqueue_alloc(&mq, mqueue_handler, NULL);
	if (err)
		goto out;

	mtx_lock(&sl.lock);
	sl.mq = mq;
	mtx_unlock(&sl.lock);

	info("slplugin: studio link is ready.\n");
	signal_ready(0);

	/* no signal handler, signals belong to the host */
	err = re_main(NULL);

	mtx_lock(&sl.lock);
	sl.mq = NULL;
	mtx_unlock(&sl.lock);

 out:
	tmr_cancel(&sl.tmr_stop);
	mem_deref(mq);

	if (err)
		ua_stop_all(true);

	ua_close();
	module_app_unload();
	conf_close();
	baresip_close();
	mod_close();
	re_thread_async_close();

	tmr_debug();
	libre_close();
	mem_debug();

	/* no-op if ready was already signaled */
	mtx_lock(&sl.lock);
	if (!sl.ready) {
		sl.err   = err ? err : EINVAL;
		sl.ready = true;
		cnd_signal(&sl.ready_cnd);
	}
	mtx_unlock(&sl.lock);

	return err;
}


/* sl.lock must be held */
static void thread_stop(void)
{
	if (!sl.running)
		return;

	if (sl.mq)
		mqueue_push(sl.mq, MQ_STOP, NULL);

	/* the thread needs the lock for its shutdown bookkeeping */
	mtx_unlock(&sl.lock);
	thrd_join(sl.thread, NULL);
	mtx_lock(&sl.lock);

	sl.running = false;
	sl.ready   = false;
}


/**
 * Start Studio Link (first call) or add a reference (following calls)
 *
 * Blocks until baresip is initialized. Only a successful call takes a
 * reference that has to be released with sl_plugin_close().
 *
 * @return 0 if success, otherwise errorcode
 */
int sl_plugin_init(void)
{
	int err;

	call_once(&sl.once, once_init);

	mtx_lock(&sl.lock);

	if (sl.refs) {
		++sl.refs;
		err = 0;
		goto out;
	}

	sl.ready = false;
	sl.err   = 0;

	err = thread_create_name(&sl.thread, "studiolink", plugin_thread,
				 NULL);
	if (err)
		goto out;

	sl.running = true;

	while (!sl.ready)
		cnd_wait(&sl.ready_cnd, &sl.lock);

	err = sl.err;
	if (err) {
		/* init failed: the thread exits by itself */
		thread_stop();
		goto out;
	}

	sl.refs = 1;

 out:
	mtx_unlock(&sl.lock);

	return err;
}


/**
 * Release a reference, the last one stops Studio Link
 */
void sl_plugin_close(void)
{
	call_once(&sl.once, once_init);

	mtx_lock(&sl.lock);

	if (!sl.refs)
		goto out;

	if (--sl.refs)
		goto out;

	thread_stop();

 out:
	mtx_unlock(&sl.lock);
}
