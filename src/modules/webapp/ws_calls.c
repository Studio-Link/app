#include <re.h>
#include <baresip.h>
#include "webapp.h"

#define SIP_CLOSED "{ \"callback\": \"CLOSED\"}"

static struct call* get_call(char *sid)
{
	struct list *calls = ua_calls(uag_current());
	struct call *call = NULL;
	struct le *le;
	char id[64] = {0};

	for (le = list_head(calls); le; le = le->next) {
		call = le->data;
		re_snprintf(id, sizeof(id), "%x", call);
		if (!str_cmp(id, sid))
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

	call = get_call(key->u.str);

	if (!str_cmp(e->u.str, "accept")) {
		ua_answer(uag_current(), call);
	}
	else if (!str_cmp(e->u.str, "hangup")) {
		webapp_call_delete(call);
		ua_hangup(uag_current(), call, 0, NULL);
		webapp_call_status = WS_CALL_OFF;
		if (!active_calls()) {
			ws_send_all(WS_CALLS, SIP_CLOSED);
		}
		ws_send_json(WS_CALLS, webapp_calls);
	}

out:
	mem_deref(cmd);
}
