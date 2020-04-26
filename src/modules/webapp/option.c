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
	int err = 0;
	info("webapp/option: %s: %s\n", key, value);
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
	if (!str_cmp(key, "monitoring")) {
		if (!str_cmp(value, "true")) {
			slaudio_monitor_set(true);
		}
		else {
			slaudio_monitor_set(false);
		}
	}
	if (!str_cmp(key, "monostream")) {
		if (!str_cmp(value, "true")) {
			slaudio_monostream_set(true);
		}
		else {
			slaudio_monostream_set(false);
			webapp_options_set("monorecord", "");
		}
	}
	if (!str_cmp(key, "monorecord")) {
		if (!str_cmp(value, "true")) {
			slaudio_monorecord_set(true);
			webapp_options_set("monostream", "true");
		}
		else {
			slaudio_monorecord_set(false);
		}
	}
	if (!str_cmp(key, "record")) {
		if (!str_cmp(value, "false")) {
			slaudio_record_set(false);
		}
		else {
			slaudio_record_set(true);
		}
	}
	if (!str_cmp(key, "mute")) {
		if (!str_cmp(value, "false")) {
			slaudio_mute_set(false);
		}
		else {
			slaudio_mute_set(true);
		}
	}
	if (!str_cmp(key, "onair")) {
		static struct call *call = NULL;
		if (!str_cmp(value, "false")) {
			webapp_session_stop_stream();
		}
		else {
			struct config *cfg = conf_config();
	#if defined (DARWIN)
			re_snprintf(command, sizeof(command), "open https://stream.studio-link.de/stream/login/%s?version=2",
					cfg->sip.uuid);
	#elif defined (WIN32)
			re_snprintf(command, sizeof(command), "start https://stream.studio-link.de/stream/login/%s?version=2",
					cfg->sip.uuid);
	#else
			re_snprintf(command, sizeof(command), "xdg-open https://stream.studio-link.de/stream/login/%s?version=2",
					cfg->sip.uuid);
	#endif
			err = system(command);

			if (err) {};

			ua_connect(uag_current(), &call, NULL,
					"stream@studio.link", VIDMODE_OFF);

			webapp_call_update(call, "Outgoing");
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


char* webapp_options_getv(char *key, char *def)
{
	const struct odict_entry *e = NULL;

	e = odict_lookup(options, key);

	if (!e)
		return def;

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
	odict_entry_del(options, "record");
	odict_entry_del(options, "auto-mix-n-1");
	odict_entry_del(options, "onair");
	odict_entry_del(options, "raisehand");
	odict_entry_del(options, "afk");
	odict_entry_del(options, "mute");

	webapp_options_set("monorecord", webapp_options_getv("monorecord", "true"));
	webapp_options_set("monostream", webapp_options_getv("monostream", "true"));
	webapp_options_set("monitoring", webapp_options_getv("monitoring", ""));

out:
	mem_deref(mb);
	return err;
}


void webapp_options_close(void)
{
	webapp_write_file_json(options, filename);
	mem_deref(options);
}
