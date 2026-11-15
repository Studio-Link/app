/**
 * @file macapp.c macOS app integration
 *
 * The macOS standalone is a Swift/AppKit app bundle. AppKit owns
 * the main thread, so the Studio Link core (libre main loop, baresip) runs
 * on its own thread, started by the Swift app with sl_mac_core_main().
 *
 * Requests from the app (quit, open recordings) are passed to the re main
 * loop with a mqueue, like the tray thread on Windows (winapp.c). A quit
 * requested while the core is still starting up is kept and delivered once
 * the mqueue exists.
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <string.h>
#include <pthread.h>
#include <re.h>
#include <baresip.h>
#include "slconf.h"
#include "sllog.h"
#include "macapp.h"


static struct {
	pthread_mutex_t lock;
	struct mqueue *mq;          /* protected by lock */
	bool quit_pending;          /* protected by lock */
	sl_mac_action_h *actionh;
	sl_mac_ready_h *readyh;
	char log_path[512];
} mac = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
};


static void mqueue_handler(int id, void *data, void *arg)
{
	(void)data;
	(void)arg;

	if (mac.actionh)
		mac.actionh((enum sl_mac_action)id);
}


static void push(enum sl_mac_action action)
{
	pthread_mutex_lock(&mac.lock);

	if (mac.mq)
		(void)mqueue_push(mac.mq, action, NULL);
	else if (action == SL_MAC_QUIT)
		mac.quit_pending = true;

	pthread_mutex_unlock(&mac.lock);
}


/*
 * Swift side
 */

void sl_mac_set_ready_handler(sl_mac_ready_h *readyh)
{
	mac.readyh = readyh;
}


void sl_mac_quit(void)
{
	push(SL_MAC_QUIT);
}


void sl_mac_open_recordings(void)
{
	push(SL_MAC_OPEN_RECORDINGS);
}


const char *sl_mac_log_path(void)
{
	return mac.log_path;
}


const char *sl_mac_software(void)
{
	return sl_software();
}


const char *sl_mac_strerror(int err)
{
	return strerror(err);
}


/*
 * Core side (core thread)
 */

/**
 * Start the app integration (after libre_init)
 *
 * @param actionh  Action handler, called on the core thread
 *
 * @return 0 if success, otherwise errorcode
 */
int sl_mac_start(sl_mac_action_h *actionh)
{
	struct mqueue *mq;
	int err;

	mac.actionh = actionh;

	err = mqueue_alloc(&mq, mqueue_handler, NULL);
	if (err)
		return err;

	pthread_mutex_lock(&mac.lock);
	mac.mq = mq;
	if (mac.quit_pending)
		(void)mqueue_push(mac.mq, SL_MAC_QUIT, NULL);
	mac.quit_pending = false;
	pthread_mutex_unlock(&mac.lock);

	return 0;
}


/** Studio Link is ready, port is the web UI http port (0 = unknown) */
void sl_mac_ready(uint16_t port)
{
	char path[256];

	if (!conf_path_get(path, sizeof(path)))
		(void)re_snprintf(mac.log_path, sizeof(mac.log_path),
				  "%s/" SL_LOG_DIR, path);

	if (mac.readyh)
		mac.readyh(port);
}


/** Stop the app integration, call at the end of the shutdown */
void sl_mac_stop(void)
{
	struct mqueue *mq;

	pthread_mutex_lock(&mac.lock);
	mq = mac.mq;
	mac.mq = NULL;
	pthread_mutex_unlock(&mac.lock);

	mem_deref(mq);
	mac.actionh = NULL;
}
