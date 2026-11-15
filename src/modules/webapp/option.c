#include <re.h>
#include <baresip.h>
#include <slua.h>
#include <string.h>
#include <stdlib.h>
#include "webapp.h"
#include <slopen.h>

#define STREAM_LOGIN_URL \
	"https://stream.studio-link.de/stream/login/%s?version=2"

static struct odict *options = NULL;
static char filename[256] = "";

/* User settings, persisted to options.json and listed in the Options tab */
static const char *persist_keys[] = {
	"monitoring",
	"monorecord",
	"monostream",
};

/* Runtime state, sent to the webui but never persisted */
static const char *state_keys[] = {
	"bypass",
	"record",
	"onair",
	"raisehand",
	"afk",
	"mute",
	"samplerate", /* set by the running host */
};


static bool key_in(const char *key, const char **keys, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (!str_cmp(key, keys[i]))
			return true;
	}

	return false;
}


/* Copy only persistent string entries from src into a new dict */
static int options_persistent_dup(struct odict **dstp, const struct odict *src)
{
	struct odict *dst;
	int err;

	err = odict_alloc(&dst, DICT_BSIZE);
	if (err)
		return err;

	for (size_t i = 0; src && i < RE_ARRAY_SIZE(persist_keys); i++) {
		const char *val = odict_string(src, persist_keys[i]);
		if (!val)
			continue;

		err = odict_entry_add(dst, persist_keys[i], ODICT_STRING, val);
		if (err) {
			mem_deref(dst);
			return err;
		}
	}

	*dstp = dst;
	return 0;
}


static void options_write(void)
{
	struct odict *o = NULL;

	if (options_persistent_dup(&o, options))
		return;

	webapp_write_file_json(o, filename);
	mem_deref(o);
}


const struct odict* webapp_options_get(void)
{
	return (const struct odict *)options;
}


void webapp_options_set(char *key, char *value)
{
	if (!key || !value)
		return;

	info("webapp/option: %s: %s\n", key, value);

	/* One-shot command, not an option */
	if (!str_cmp(key, "record-folder")) {
#ifndef SLPLUGIN
		slaudio_record_open_folder();
#endif
		return;
	}

	if (!key_in(key, persist_keys, RE_ARRAY_SIZE(persist_keys)) &&
	    !key_in(key, state_keys, RE_ARRAY_SIZE(state_keys))) {
		warning("webapp/option: ignoring unknown key %s\n", key);
		return;
	}

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
			char command[255] = {0};
			int err;

	#if defined (DARWIN)
			re_snprintf(command, sizeof(command),
					"open " STREAM_LOGIN_URL,
					cfg->sip.uuid);
	#elif defined (WIN32)
			re_snprintf(command, sizeof(command),
					STREAM_LOGIN_URL,
					cfg->sip.uuid);
	#else
			re_snprintf(command, sizeof(command),
					"xdg-open " STREAM_LOGIN_URL,
					cfg->sip.uuid);
	#endif
#if defined (WIN32)
			err = sl_shell_open(command);
#else
			err = system(command);
#endif
			if (err)
				warning("webapp/option: could not open %s\n",
					command);

			sl_ua_connect(webapp_get_main_ua(), &call,
					"stream@studio.link");

			webapp_call_update(call, "Outgoing");
		}
	}
#endif

	odict_entry_del(options, key);
	odict_entry_add(options, key, ODICT_STRING, value);
	ws_send_json(WS_OPTIONS, options);

	if (key_in(key, persist_keys, RE_ARRAY_SIZE(persist_keys)))
		options_write();
}


char* webapp_options_getv(char *key, char *def)
{
	const struct odict_entry *e = NULL;

	e = odict_lookup(options, key);

	if (!e)
		return def;

	return odict_entry_str(e);
}


int webapp_options_init(void)
{
	char path[256] = {0};
#ifndef SLPLUGIN
	char *key;
#endif
	struct odict *loaded = NULL;
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

	/* Keep only known settings, drops runtime state and stale keys
	 * left behind by older versions */
	if (!webapp_load_file(mb, filename)) {
		if (json_decode_odict(&loaded, DICT_BSIZE, (char *)mb->buf,
				      mb->end, MAX_LEVELS))
			warning("webapp/option: invalid %s\n", filename);
	}

	err = options_persistent_dup(&options, loaded);
	mem_deref(loaded);
	if (err)
		goto out;

	options_write();

#ifndef SLPLUGIN
	str_dup(&key, webapp_options_getv("monorecord", "true"));
	webapp_options_set("monorecord", key);
	mem_deref(key);

	str_dup(&key, webapp_options_getv("monostream", "true"));
	webapp_options_set("monostream", key);
	mem_deref(key);

	str_dup(&key, webapp_options_getv("monitoring", ""));
	webapp_options_set("monitoring", key);
	mem_deref(key);
#endif

out:
	mem_deref(mb);
	return err;
}


void webapp_options_close(void)
{
	options_write();
	options = mem_deref(options);
}
