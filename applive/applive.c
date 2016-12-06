/**
 * @file applive.c Init Live Session
 *
 * Copyright (C) 2016 studio-link.de Sebastian Reimers
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "webapp.h"

static struct tmr tmr;
static char command[255] = {0};


static void startup(void *arg)
{
	struct call *call = NULL;
	ua_connect(uag_current(), &call, NULL,
			"stream", NULL, VIDMODE_ON);
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

	system(command);

	tmr_init(&tmr);
	tmr_start(&tmr, 3000, startup, NULL);

	return err;
}


static int module_close(void)
{
	tmr_cancel(&tmr);
	webapp_accounts_close();
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(applive) = {
	"applive",
	"application",
	module_init,
	module_close,
};
