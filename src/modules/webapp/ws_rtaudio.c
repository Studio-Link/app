/**
 * @file ws_rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018-2019 studio-link.de
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"


void webapp_ws_rtaudio_sync(void)
{
#ifndef SLPLUGIN
	ws_send_json(WS_RTAUDIO, slaudio_get_interfaces());
#endif
}


void webapp_ws_rtaudio(const struct websock_hdr *hdr,
		struct mbuf *mb, void *arg)
{

#ifndef SLPLUGIN
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
		slaudio_set_driver(e->u.integer);
		goto out;
	}

	if (!str_cmp(e->u.str, "input")) {
		e = odict_lookup(cmd, "id");
		slaudio_set_input(e->u.integer);
		goto out;
	}

	if (!str_cmp(e->u.str, "first_input_channel")) {
		e = odict_lookup(cmd, "id");
		slaudio_set_first_input_channel(e->u.integer);
		goto out;
	}

	if (!str_cmp(e->u.str, "output")) {
		e = odict_lookup(cmd, "id");
		slaudio_set_output(e->u.integer);
		goto out;
	}

	if (!str_cmp(e->u.str, "reload")) {
		slaudio_reset();
		goto out;
	}

out:
	mem_deref(cmd);
#endif
}


int webapp_ws_rtaudio_init(void)
{
	int err = 0;

	return err;
}


void webapp_ws_rtaudio_close(void)
{	
}
