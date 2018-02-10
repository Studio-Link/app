/**
 * @file ws_rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018 studio-link.de
 */
#include <re.h>
#include <baresip.h>
#ifndef SLPLUGIN
#include <rtaudio_c.h>
#endif
#include "webapp.h"

static struct odict *interfaces = NULL;
static rtaudio_api_t driver = RTAUDIO_API_UNSPECIFIED;

const struct odict* webapp_ws_rtaudio_get(void)
{
	return (const struct odict *)interfaces;
}

static int ws_rtaudio_drivers(int driver_selected) {
#ifndef SLPLUGIN
	int err;
	rtaudio_api_t const *apis = rtaudio_compiled_api();
	struct odict *o;
	struct odict *array;
	char idx[2];

	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;


	for(unsigned int i = 0; i < sizeof(apis)/sizeof(rtaudio_api_t); i++) {
		(void)re_snprintf(idx, sizeof(idx), "%d", i);
		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		if(apis[i] == RTAUDIO_API_LINUX_ALSA) {
			odict_entry_add(o, "display", ODICT_STRING, "ALSA");
			if (driver_selected == RTAUDIO_API_UNSPECIFIED) {
				driver_selected = RTAUDIO_API_LINUX_ALSA;
			}
		}
		if(apis[i] == RTAUDIO_API_LINUX_PULSE) {
			odict_entry_add(o, "display", ODICT_STRING, "Pulseaudio");
		}
		
		if(apis[i] == driver_selected) {
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		} else {
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		driver = driver_selected;

		odict_entry_add(o, "id", ODICT_INT, apis[i]);
		odict_entry_add(array, idx, ODICT_OBJECT, o);
		mem_deref(o);
	}
	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

#endif
	
	return 0;
}


static int ws_rtaudio_devices(void) {
	int err;
#ifndef SLPLUGIN
	rtaudio_t audio = rtaudio_create(driver);
	rtaudio_device_info_t info;
	struct odict *o;
	struct odict *array;
	char idx[2];
	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;

	/* Print list of device names and native sample rates */
	for (int i = 0; i < rtaudio_device_count(audio); i++) {
		(void)re_snprintf(idx, sizeof(idx), "%d", i);
		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		info = rtaudio_get_device_info(audio, i);
		if (rtaudio_error(audio) != NULL) {
			fprintf(stderr, "error: %s\n", rtaudio_error(audio));
			err = 1;
			goto out;
		}
		warning("%c%d: %s: %d\n",
				(info.is_default_input || info.is_default_output) ? '*' : ' ', i,
				info.name, info.preferred_sample_rate);
		odict_entry_add(o, "id", ODICT_INT, i);
		odict_entry_add(o, "selected", ODICT_BOOL, (info.is_default_input || info.is_default_output) ? true : false);
		odict_entry_add(o, "display", ODICT_STRING, info.name);
		odict_entry_add(array, idx, ODICT_OBJECT, o);
		mem_deref(o);
	}
	odict_entry_add(interfaces, "input", ODICT_ARRAY, array);
	odict_entry_add(interfaces, "output", ODICT_ARRAY, array);
	mem_deref(array);
#endif
out:
	return err;
}


void webapp_ws_rtaudio(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;

	if (!str_cmp(e->u.str, "driver")) {
		e = odict_lookup(cmd, "id");
		ws_rtaudio_drivers(e->u.integer);
		ws_rtaudio_devices();
		ws_send_json(WS_RTAUDIO, webapp_ws_rtaudio_get());
	}

out:
	mem_deref(cmd);
}


int webapp_ws_rtaudio_init(void)
{
	int err = 0;

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return err;

	err = ws_rtaudio_drivers(RTAUDIO_API_UNSPECIFIED);
	if (err)
		return err;

	err = ws_rtaudio_devices();

	return err;
}

void webapp_ws_rtaudio_close(void)
{
	mem_deref(interfaces);
}
