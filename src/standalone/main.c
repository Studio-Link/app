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
#ifdef WIN32
#include "winapp.h"
#endif

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


#ifdef WIN32
/* slaudio module */
void slaudio_record_open_folder(void);


static void win_action_handler(enum sl_win_action action)
{
	switch (action) {

	case SL_WIN_QUIT:
		info("quit from tray menu\n");
		ua_stop_all(false);
		break;

	case SL_WIN_OPEN_RECORDINGS:
		slaudio_record_open_folder();
		break;
	}
}


/* The web UI port, written by the webapp module to webapp.conf */
static uint16_t webapp_port(void)
{
	char file[512];
	struct conf *conf = NULL;
	uint32_t port = 0;

	if (conf_path_get(file, sizeof(file)))
		return 0;

	if (re_snprintf(file + str_len(file), sizeof(file) - str_len(file),
			"\\webapp.conf") < 0)
		return 0;

	if (conf_alloc(&conf, file))
		return 0;

	(void)conf_get_u32(conf, "sl_port", &port);
	mem_deref(conf);

	return (uint16_t)port;
}
#endif


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
	bool conf_override = false;
	uint32_t tmo = 0;
	int dbg_level = DBG_INFO;
	int err;

#ifdef WIN32
	/* GUI app: no console, unless started from cmd/PowerShell */
	sl_win_console_attach();
#endif

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
			conf_override = true;
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

#ifdef WIN32
	/* already running: open its web UI instead (not with -f, to allow
	 * a second instance with another config for testing) */
	if (!conf_override && sl_win_activate_running()) {
		libre_close();
		return 0;
	}
#endif

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

#ifdef WIN32
	err = sl_win_tray_start(win_action_handler, sl_software());
	if (err)
		warning("main: tray icon failed (%m)\n", err);
#endif

	err = conf_modules();
	if (err)
		goto out;

#ifdef WIN32
	sl_win_tray_set_port(webapp_port());
#endif

	info("studio link is ready.\n");

	if (tmo)
		tmr_start(&tmr_quit, tmo * 1000, tmr_quit_handler, NULL);

	err = re_main(signal_handler);

 out:
#ifdef WIN32
	if (err)
		sl_win_error("Studio Link could not be started (%m).\n\n"
			     "The log files can help to find the reason, "
			     "see the \"logs\" folder in %%USERPROFILE%%"
			     "\\.studio-link", err);
#endif
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

#ifdef WIN32
	sl_win_tray_stop();
#endif

	return err;
}
