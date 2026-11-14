/**
 * @file main.c Studio Link Standalone
 *
 * Based on baresip src/main.c
 *
 * Copyright (C) 2010 - 2025 Alfred E. Heggestad et al.
 * Copyright (C) 2026 studio-link.de
 */
#include <stdlib.h>
#ifdef HAVE_GETOPT
#include <getopt.h>
#endif
#include <re.h>
#include <baresip.h>
#include "slconf.h"
#include "slcacert.h"

#define DEBUG_MODULE ""
#define DEBUG_LEVEL 0
#include <re_dbg.h>


enum { ASYNC_WORKERS = 4 };


static void signal_handler(int sig)
{
	static bool term = false;

	if (term) {
		module_app_unload();
		mod_close();
		exit(0);
	}

	term = true;

	info("terminated by signal %d\n", sig);

	ua_stop_all(false);
}


static void ua_exit_handler(void *arg)
{
	(void)arg;
	debug("ua exited -- stopping main runloop\n");

	re_cancel();
}


static void tmr_quit_handler(void *arg)
{
	(void)arg;

	ua_stop_all(false);
}


#ifdef HAVE_GETOPT
static void usage(void)
{
	(void)re_fprintf(stderr,
			 "Usage: studio-link-standalone [options]\n"
			 "options:\n"
			 "\t-f <path>        Config path\n"
			 "\t-h -?            Help\n"
			 "\t-r               Reset configuration\n"
			 "\t-s               Enable SIP trace\n"
			 "\t-t <sec>         Quit after <sec> seconds\n"
			 "\t-v               Verbose debug\n"
			 );
}
#endif


int main(int argc, char *argv[])
{
	struct tmr tmr_quit;
	bool sip_trace = false;
	bool reset_conf = false;
	uint32_t tmo = 0;
	int dbg_level = DBG_INFO;
	int err;

	setbuf(stdout, NULL);

	(void)re_fprintf(stdout, "%s\n", sl_software());

	err = libre_init();
	if (err)
		goto out;

	tmr_init(&tmr_quit);

#ifdef HAVE_GETOPT
	for (;;) {
		const int c = getopt(argc, argv, "f:hrst:v");
		if (0 > c)
			break;

		switch (c) {

		case '?':
		case 'h':
			usage();
			return -2;

		case 'f':
			sl_conf_path_set(optarg);
			break;

		case 'r':
			reset_conf = true;
			break;

		case 's':
			sip_trace = true;
			break;

		case 't':
			tmo = atoi(optarg);
			break;

		case 'v':
			log_enable_debug(true);
			dbg_level = DBG_DEBUG;
			break;

		default:
			break;
		}
	}
#else
	(void)argc;
	(void)argv;
#endif

	dbg_init(dbg_level, DBG_ANSI);

	err = sl_conf_configure(reset_conf);
	if (err) {
		warning("main: configure failed: %m\n", err);
		goto out;
	}

	re_thread_async_init(ASYNC_WORKERS);

	err = baresip_init(conf_config());
	if (err) {
		warning("main: baresip init failed (%m)\n", err);
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

	if (sip_trace)
		uag_enable_sip_trace(true);

	err = conf_modules();
	if (err)
		goto out;

	info("studio link is ready.\n");

	if (tmo)
		tmr_start(&tmr_quit, tmo * 1000, tmr_quit_handler, NULL);

	err = re_main(signal_handler);

 out:
	tmr_cancel(&tmr_quit);

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

	return err;
}
