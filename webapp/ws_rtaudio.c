/**
 * @file ws_rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018 studio-link.de
 */
#include <re.h>
#include <baresip.h>
#include "webapp.h"
#ifndef SLPLUGIN
#include <rtaudio_c.h>
#endif

static int driver = -1;
static int input = -1;
static int output = -1;
static struct odict *interfaces = NULL;


const struct odict* webapp_ws_rtaudio_get(void)
{
	return (const struct odict *)interfaces;
}


int webapp_ws_rtaudio_get_driver(void)
{
	return driver;
}


int webapp_ws_rtaudio_get_input(void)
{
	return input;
}


int webapp_ws_rtaudio_get_output(void)
{
	return output;
}


static int ws_rtaudio_drivers(void) {
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
			if (driver == -1) {
				driver = RTAUDIO_API_LINUX_ALSA;
			}
		}
		if(apis[i] == RTAUDIO_API_LINUX_PULSE) {
			odict_entry_add(o, "display", ODICT_STRING, "Pulseaudio");
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_WASAPI) {
			odict_entry_add(o, "display", ODICT_STRING, "WASAPI");
			if (driver == -1) {
				driver = RTAUDIO_API_WINDOWS_WASAPI;
			}
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_DS) {
			odict_entry_add(o, "display", ODICT_STRING, "DirectSound");
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_ASIO) {
			odict_entry_add(o, "display", ODICT_STRING, "ASIO");
		}
		if(apis[i] == RTAUDIO_API_MACOSX_CORE) {
			odict_entry_add(o, "display", ODICT_STRING, "Coreaudio");
			if (driver == -1) {
				driver = RTAUDIO_API_MACOSX_CORE;
			}
		}

		if(apis[i] == (unsigned int)driver) {
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		} else {
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		odict_entry_add(o, "id", ODICT_INT, apis[i]);
		odict_entry_add(array, idx, ODICT_OBJECT, o);
		mem_deref(o);
	}
	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

#endif
	
	return 0;
}


static int ws_rtaudio_devices() {
	int err;
#ifndef SLPLUGIN
	rtaudio_t audio = rtaudio_create(driver);
	rtaudio_device_info_t info;
	struct odict *o_in;
	struct odict *o_out;
	struct odict *array_in;
	struct odict *array_out;
	char idx[2];
	err = odict_alloc(&array_in, DICT_BSIZE);
	if (err)
		return ENOMEM;
	err = odict_alloc(&array_out, DICT_BSIZE);
	if (err)
		return ENOMEM;

	for (int i = 0; i < rtaudio_device_count(audio); i++) {
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		info = rtaudio_get_device_info(audio, i);
		if (rtaudio_error(audio) != NULL) {
			fprintf(stderr, "error: %s\n", rtaudio_error(audio));
			err = 1;
			goto out;
		}
		warning("%c%d: %s: %d\n",
				(info.is_default_input || info.is_default_output) ? '*' : ' ', i,
				info.name, info.preferred_sample_rate);

		if (!info.probed)
			continue;

		err = odict_alloc(&o_in, DICT_BSIZE);
		if (err)
			return ENOMEM;
		err = odict_alloc(&o_out, DICT_BSIZE);
		if (err)
			return ENOMEM;

		if (output == -1 && info.is_default_output) {
			output = i;
		}

		if (input == -1 && info.is_default_input) {
			input = i;
		}

		if (info.output_channels > 0) {
			if (output == i) {
				odict_entry_add(o_out, "selected", ODICT_BOOL, true);
			} else {
				odict_entry_add(o_out, "selected", ODICT_BOOL, false);
			}
			odict_entry_add(o_out, "id", ODICT_INT, i);
			odict_entry_add(o_out, "display", ODICT_STRING, info.name);
			odict_entry_add(array_out, idx, ODICT_OBJECT, o_out);
		}
		
		if (info.input_channels > 0) {
			if (input == i) {
				odict_entry_add(o_in, "selected", ODICT_BOOL, true);
			} else {
				odict_entry_add(o_in, "selected", ODICT_BOOL, false);
			}
			odict_entry_add(o_in, "id", ODICT_INT, i);
			odict_entry_add(o_in, "display", ODICT_STRING, info.name);
			odict_entry_add(array_in, idx, ODICT_OBJECT, o_in);
		}

		mem_deref(o_out);
		mem_deref(o_in);
	}

	odict_entry_add(interfaces, "input", ODICT_ARRAY, array_in);
	odict_entry_add(interfaces, "output", ODICT_ARRAY, array_out);
	mem_deref(array_in);
	mem_deref(array_out);
	rtaudio_destroy(audio);

#endif
out:
	return err;
}


static int webapp_ws_rtaudio_reset(void)
{
	int err;

	if(interfaces)
		mem_deref(interfaces);

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	err = ws_rtaudio_drivers();
	if (err)
		return err;

	err = ws_rtaudio_devices();
	
	ws_send_json(WS_RTAUDIO, webapp_ws_rtaudio_get());

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
		driver = e->u.integer;
		goto out;
	}
	if (!str_cmp(e->u.str, "input")) {
		e = odict_lookup(cmd, "id");
		input = e->u.integer;
		goto out;
	}
	if (!str_cmp(e->u.str, "output")) {
		e = odict_lookup(cmd, "id");
		output = e->u.integer;
		goto out;
	}

out:
	webapp_ws_rtaudio_reset();
	mem_deref(cmd);
}



int webapp_ws_rtaudio_init(void)
{
	int err = 0;

	err = webapp_ws_rtaudio_reset();
	return err;
}

void webapp_ws_rtaudio_close(void)
{	
	mem_deref(interfaces);
}
