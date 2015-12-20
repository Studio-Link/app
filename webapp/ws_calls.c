#include <re.h>
#include <baresip.h>
#include "webapp.h"

#define SIP_CLOSED "{ \"callback\": \"CLOSED\"}"


static struct call* get_call(char *peer)
{
	struct list *calls = ua_calls(uag_current());
	struct call *call = NULL;
	struct le *le;

	for (le = list_head(calls); le; le = le->next) {
		call = le->data;
		const char *call_peer = call_peeruri(call);
		if (!str_cmp(peer, call_peer))
			return call;
	}

	return NULL;
}

static bool active_calls(void)
{
	struct le *le;

	for (le = list_head(uag_list()); le; le = le->next) {

		struct ua *ua = le->data;

		if (ua_call(ua))
			return true;
	}

	return false;
}


void webapp_ws_calls(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	struct call *call = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
	  	goto out;
	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;

	if (!str_cmp(e->u.str, "accept")) {
		ua_answer(uag_current(), NULL);
	}
	else if (!str_cmp(e->u.str, "hangup")) {
		e = odict_lookup(cmd, "peer");
		odict_entry_del(webapp_calls, e->u.str);
		call = get_call(e->u.str);
		ua_hangup(uag_current(), call, 0, NULL);
		webapp_call_status = WS_CALL_OFF;
		if (!active_calls())
			ws_send_all(WS_CALLS, SIP_CLOSED);
		ws_send_json(WS_CALLS, webapp_calls);
	}

out:
	mem_deref(cmd);
}
