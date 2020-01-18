/**
 * @file src/main.c  Main application code
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */
#ifdef SOLARIS
#define __EXTENSIONS__ 1
#endif
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_GETOPT
#include <getopt.h>
#endif
#include <re.h>
#include <baresip.h>


#ifdef WIN32

#include <windows.h>
#include <imagehlp.h>


int addr2line(void const * const addr)
{
	char addr2line_cmd[512] = {0};

	/* have addr2line map the address to the relent line in the code */
#ifdef __APPLE__
	/* apple does things differently... */
	sprintf(addr2line_cmd,"atos -o %.256s %p",  addr); 
#else
	sprintf(addr2line_cmd,"addr2line -f -p -e baresip.exe %p", addr); 
#endif

	fprintf(stdout, "%s\n", addr2line_cmd);
}

void windows_print_stacktrace(CONTEXT* context)
{
	SymInitialize(GetCurrentProcess(), 0, true);

	STACKFRAME frame = { 0 };

	/* setup initial stack frame */
	frame.AddrPC.Offset         = context->Rip;
	frame.AddrPC.Mode           = AddrModeFlat;
	frame.AddrStack.Offset      = context->Rsp;
	frame.AddrStack.Mode        = AddrModeFlat;
	frame.AddrFrame.Offset      = context->Rbp;
	frame.AddrFrame.Mode        = AddrModeFlat;

	while (StackWalk(IMAGE_FILE_MACHINE_AMD64,
				GetCurrentProcess(),
				GetCurrentThread(),
				&frame,
				context,
				0,
				SymFunctionTableAccess,
				SymGetModuleBase,
				0 ) )
	{
		//fprintf(stdout, "stack address: %p\n", (void*)frame.AddrPC.Offset);
		 addr2line((void*)frame.AddrPC.Offset);

	}

	SymCleanup( GetCurrentProcess() );
}

LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS * ExceptionInfo)
{
	switch(ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		case EXCEPTION_ACCESS_VIOLATION:
			fputs("Error: EXCEPTION_ACCESS_VIOLATION\n", stderr);
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			fputs("Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n", stderr);
			break;
		case EXCEPTION_BREAKPOINT:
			fputs("Error: EXCEPTION_BREAKPOINT\n", stderr);
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			fputs("Error: EXCEPTION_DATATYPE_MISALIGNMENT\n", stderr);
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			fputs("Error: EXCEPTION_FLT_DENORMAL_OPERAND\n", stderr);
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			fputs("Error: EXCEPTION_FLT_DIVIDE_BY_ZERO\n", stderr);
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			fputs("Error: EXCEPTION_FLT_INEXACT_RESULT\n", stderr);
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			fputs("Error: EXCEPTION_FLT_INVALID_OPERATION\n", stderr);
			break;
		case EXCEPTION_FLT_OVERFLOW:
			fputs("Error: EXCEPTION_FLT_OVERFLOW\n", stderr);
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			fputs("Error: EXCEPTION_FLT_STACK_CHECK\n", stderr);
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			fputs("Error: EXCEPTION_FLT_UNDERFLOW\n", stderr);
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			fputs("Error: EXCEPTION_ILLEGAL_INSTRUCTION\n", stderr);
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			fputs("Error: EXCEPTION_IN_PAGE_ERROR\n", stderr);
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			fputs("Error: EXCEPTION_INT_DIVIDE_BY_ZERO\n", stderr);
			break;
		case EXCEPTION_INT_OVERFLOW:
			fputs("Error: EXCEPTION_INT_OVERFLOW\n", stderr);
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			fputs("Error: EXCEPTION_INVALID_DISPOSITION\n", stderr);
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			fputs("Error: EXCEPTION_NONCONTINUABLE_EXCEPTION\n", stderr);
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			fputs("Error: EXCEPTION_PRIV_INSTRUCTION\n", stderr);
			break;
		case EXCEPTION_SINGLE_STEP:
			fputs("Error: EXCEPTION_SINGLE_STEP\n", stderr);
			break;
		case EXCEPTION_STACK_OVERFLOW:
			fputs("Error: EXCEPTION_STACK_OVERFLOW\n", stderr);
			break;
		default:
			fputs("Error: Unrecognized Exception\n", stderr);
			break;
	}
	fflush(stderr);
	/* If this is a stack overflow then we can't walk the stack, so just show
	   where the error happened */
	if (EXCEPTION_STACK_OVERFLOW != ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		windows_print_stacktrace(ExceptionInfo->ContextRecord);
	}
	else
	{
		fprintf(stdout, "stack overflow: %p\n", (void*)ExceptionInfo->ContextRecord->Rip);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void set_signal_handler()
{
	SetUnhandledExceptionFilter(windows_exception_handler);
}

#endif


static void signal_handler(int sig)
{
	static bool term = false;

	if (term) {
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

	/* The main run-loop can be stopped now */
	re_cancel();
}


static void usage(void)
{
	(void)re_fprintf(stderr,
			 "Usage: baresip [options]\n"
			 "options:\n"
			 "\t-4               Prefer IPv4\n"
#if HAVE_INET6
			 "\t-6               Prefer IPv6\n"
#endif
			 "\t-d               Daemon\n"
			 "\t-e <commands>    Execute commands (repeat)\n"
			 "\t-f <path>        Config path\n"
			 "\t-m <module>      Pre-load modules (repeat)\n"
			 "\t-p <path>        Audio files\n"
			 "\t-h -?            Help\n"
			 "\t-r               Reset configuration\n"
			 "\t-t               Test and exit\n"
			 "\t-n <net_if>      Specify network interface\n"
			 "\t-u <parameters>  Extra UA parameters\n"
			 "\t-v               Verbose debug\n"
			 );
}


int main(int argc, char *argv[])
{
	int prefer_ipv6 = -1, run_daemon = false, test = false, reset_conf = false;
	const char *ua_eprm = NULL;
	const char *execmdv[16];
	const char *net_interface = NULL;
	const char *audio_path = NULL;
	const char *modv[16];
	size_t execmdc = 0;
	size_t modc = 0;
	size_t i;
	int err;

	set_signal_handler();

	/*
	 * turn off buffering on stdout
	 */
	setbuf(stdout, NULL);

	(void)re_fprintf(stdout, "baresip v%s"
			 " Copyright (C) 2010 - 2019"
			 " Alfred E. Heggestad et al.\n",
			 BARESIP_VERSION);

	(void)sys_coredump_set(true);

	err = libre_init();
	if (err)
		goto out;

#ifdef HAVE_GETOPT
	for (;;) {
		const int c = getopt(argc, argv, "46de:f:p:hu:n:vrtm:");
		if (0 > c)
			break;

		switch (c) {

		case '?':
		case 'h':
			usage();
			return -2;

		case '4':
			prefer_ipv6 = false;
			break;

#if HAVE_INET6
		case '6':
			prefer_ipv6 = true;
			break;
#endif

		case 'd':
			run_daemon = true;
			break;

		case 'e':
			if (execmdc >= ARRAY_SIZE(execmdv)) {
				warning("max %zu commands\n",
					ARRAY_SIZE(execmdv));
				err = EINVAL;
				goto out;
			}
			execmdv[execmdc++] = optarg;
			break;

		case 'f':
			conf_path_set(optarg);
			break;

		case 'm':
			if (modc >= ARRAY_SIZE(modv)) {
				warning("max %zu modules\n",
					ARRAY_SIZE(modv));
				err = EINVAL;
				goto out;
			}
			modv[modc++] = optarg;
			break;

		case 'p':
			audio_path = optarg;
			break;

		case 't':
			test = true;
			break;

		case 'n':
			net_interface = optarg;
			break;

		case 'u':
			ua_eprm = optarg;
			break;

		case 'v':
			log_enable_debug(true);
			break;
		case 'r':
			reset_conf = true;
			break;
		default:
			break;
		}
	}
#else
	(void)argc;
	(void)argv;
#endif

	err = conf_configure(reset_conf);
	if (err) {
		warning("main: configure failed: %m\n", err);
		goto out;
	}

	/*
	 * Set the network interface before initializing the config
	 */
	if (net_interface) {
		struct config *theconf = conf_config();

		str_ncpy(theconf->net.ifname, net_interface,
			 sizeof(theconf->net.ifname));
	}

	/*
	 * Set prefer_ipv6 preferring the one given in -6 argument (if any)
	 */
	if (prefer_ipv6 != -1)
		conf_config()->net.prefer_ipv6 = prefer_ipv6;
	else
		prefer_ipv6 = conf_config()->net.prefer_ipv6;

	/*
	 * Initialise the top-level baresip struct, must be
	 * done AFTER configuration is complete.
	*/
	err = baresip_init(conf_config());
	if (err) {
		warning("main: baresip init failed (%m)\n", err);
		goto out;
	}

	/* Set audio path preferring the one given in -p argument (if any) */
	if (audio_path)
		play_set_path(baresip_player(), audio_path);
	else if (str_isset(conf_config()->audio.audio_path)) {
		play_set_path(baresip_player(),
			      conf_config()->audio.audio_path);
	}

	/* NOTE: must be done after all arguments are processed */
	if (modc) {

		info("pre-loading modules: %zu\n", modc);

		for (i=0; i<modc; i++) {

			err = module_preload(modv[i]);
			if (err) {
				re_fprintf(stderr,
					   "could not pre-load module"
					   " '%s' (%m)\n", modv[i], err);
			}
		}
	}

	/* Initialise User Agents */
	err = ua_init("baresip v" BARESIP_VERSION " (" ARCH "/" OS ")",
		      true, true, true);
	if (err)
		goto out;

	uag_set_exit_handler(ua_exit_handler, NULL);

	if (ua_eprm) {
		err = uag_set_extra_params(ua_eprm);
		if (err)
			goto out;
	}

	if (test)
		goto out;

	/* Load modules */
	err = conf_modules();
	if (err)
		goto out;

	if (run_daemon) {
		err = sys_daemon();
		if (err)
			goto out;

		log_enable_stdout(false);
	}

	info("baresip is ready.\n");

	/* Execute any commands from input arguments */
	for (i=0; i<execmdc; i++) {
		ui_input_str(execmdv[i]);
	}

	/* Main loop */
	err = re_main(NULL);

 out:
	if (err)
		ua_stop_all(true);

	ua_close();

	/* note: must be done before mod_close() */
	module_app_unload();

	conf_close();

	baresip_close();

	/* NOTE: modules must be unloaded after all application
	 *       activity has stopped.
	 */
	debug("main: unloading modules..\n");
	mod_close();

	libre_close();

	/* Check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
