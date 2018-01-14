#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "webapp.h"

static struct odict *options = NULL;
static char filename[256] = "";
static char command[255] = {0};


const struct odict* webapp_options_get(void)
{
	return (const struct odict *)options;
}


void webapp_options_set(char *key, char *value)
{
#ifdef SLPLUGIN
	if (!str_cmp(key, "bypass")) {
		if (!str_cmp(value, "false")) {
			effect_set_bypass(false);
		}
		else {
			effect_set_bypass(true);
		}
	}
#else
	if (!str_cmp(key, "mono")) {
		if (!str_cmp(value, "false")) {
			webapp_mono_set(false);
		}
		else {
			webapp_mono_set(true);
		}
	}
	if (!str_cmp(key, "record")) {
		if (!str_cmp(value, "false")) {
			webapp_record_set(false);
		}
		else {
			webapp_record_set(true);
		}
	}
	if (!str_cmp(key, "onair")) {
		static struct call *call = NULL;
		if (!str_cmp(value, "false")) {
			ua_hangup(uag_current(), call, 0, NULL);
		}
		else {
			struct config *cfg = conf_config();
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

			ua_connect(uag_current(), &call, NULL,
					"stream", NULL, VIDMODE_ON);

		}
	}
#endif
	if (!str_cmp(key, "raisehand")) {
		if (!str_cmp(value, "true")) {
			webapp_chat_send("raisehandon", NULL);
			odict_entry_del(options, "afk");
			odict_entry_add(options, "afk", ODICT_STRING, "false");
		} else {
			webapp_chat_send("raisehandoff", NULL);
		}
	}
	if (!str_cmp(key, "afk")) {
		if (!str_cmp(value, "true")) {
			webapp_chat_send("afkon", NULL);
			odict_entry_del(options, "raisehand");
			odict_entry_add(options, "raisehand", ODICT_STRING, "false");
		} else {
			webapp_chat_send("afkoff", NULL);
		}
	}

	odict_entry_del(options, key);
	odict_entry_add(options, key, ODICT_STRING, value);
	ws_send_json(WS_OPTIONS, options);
	webapp_write_file_json(options, filename);
}


char* webapp_options_getv(char *key)
{
	const struct odict_entry *e = NULL;

	e = odict_lookup(options, key);

	if (!e)
		return NULL;

	return e->u.str;
}


int webapp_options_init(void)
{
	char path[256] = "";
	struct mbuf *mb;
	int err = 0;

	mb = mbuf_alloc(8192);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/options.json", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		err = odict_alloc(&options, DICT_BSIZE);
	}
	else {
		err = json_decode_odict(&options, DICT_BSIZE,
				(char *)mb->buf, mb->end, MAX_LEVELS);
	}
	if (err)
		goto out;
	odict_entry_del(options, "bypass");
	odict_entry_del(options, "mono");
	odict_entry_del(options, "record");
	odict_entry_del(options, "auto-mix-n-1");
	odict_entry_del(options, "onair");
	odict_entry_del(options, "raisehand");
	odict_entry_del(options, "afk");

out:
	mem_deref(mb);
	return err;
}


void webapp_options_close(void)
{
	webapp_write_file_json(options, filename);
	mem_deref(options);
}
