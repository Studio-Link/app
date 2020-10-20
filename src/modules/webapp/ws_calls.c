#include <re.h>
#include <baresip.h>
#include "webapp.h"

#define SIP_CLOSED "{ \"callback\": \"CLOSED\"}"


void webapp_ws_calls(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	struct call *call = NULL;
	const struct odict_entry *e = NULL;
	const struct odict_entry *key = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
	  	goto out;

	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;

	key = odict_lookup(cmd, "key");
	if (!key)
		goto out;

	call = webapp_session_get_call(key->u.str);
	if (!call)
		goto out;

	if (!str_cmp(e->u.str, "accept")) {
		ua_answer(uag_current(), call, VIDMODE_OFF);
	}
	else if (!str_cmp(e->u.str, "hangup")) {
		webapp_session_delete(key->u.str, NULL);
		if (!webapp_active_calls()) {
			ws_send_all(WS_CALLS, SIP_CLOSED);
		}
	}
	else if (!str_cmp(e->u.str, "dtmf")) {
		e = odict_lookup(cmd, "tone");
		if (!e)
			goto out;

		call_send_digit(call, e->u.str[0]);
	}

out:
	mem_deref(cmd);
}
