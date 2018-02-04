/**
 * @file applive.c Init Live Session
 *
 * Copyright (C) 2013-2018 studio-link.de Sebastian Reimers
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "webapp.h"

static struct tmr tmr_startup, tmr_call;
static char command[255] = {0};


static bool no_active_call(void)
{
	struct le *le;

	for (le = list_head(uag_list()); le; le = le->next) {

		struct ua *ua = le->data;

		if (ua_call(ua))
			return false;
	}

	return true;
}


static void startup_call(void *arg)
{
	struct call *call = NULL;
	if (no_active_call()) {
		ua_connect(uag_current(), &call, NULL,
				"stream", NULL, VIDMODE_ON);
	}
	tmr_start(&tmr_call, 6000, startup_call, NULL);
}


static void startup(void *arg)
{
	startup_call(NULL);
	system(command);
}


static int module_init(void)
{
	int err = 0;
	struct config *cfg = conf_config();

	webapp_accounts_init();

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open https://stream.studio-link.de/stream/login/%s",
			cfg->sip.uuid);
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "start https://stream.studio-link.de/stream/login/%s",
			cfg->sip.uuid);
#else
	re_snprintf(command, sizeof(command), "xdg-open https://stream.studio-link.de/stream/login/%s",
			cfg->sip.uuid);
#endif


	tmr_init(&tmr_startup);
	tmr_init(&tmr_call);
	tmr_start(&tmr_startup, 1500, startup, NULL);

	return err;
}


static int module_close(void)
{
	tmr_cancel(&tmr_startup);
	tmr_cancel(&tmr_call);
	webapp_accounts_close();
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(apponair) = {
	"apponair",
	"application",
	module_init,
	module_close,
};
