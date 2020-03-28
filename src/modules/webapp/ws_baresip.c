#include <re.h>
#include <baresip.h>
#include "webapp.h"

#define SIP_EMPTY "{ \"callback\": \"CLOSED\",\
	\"message\": \"SIP Number Empty\" }"

#define SIP_MAX_CALLS "{ \"callback\": \"CLOSED\",\
	\"message\": \"Max Calls reached...\" }"

static void sip_delete(struct odict *cmd, const struct odict_entry *e)
{
	char user[50];
	char domain[70];

	e = odict_lookup(cmd, "user");
	if (e)
		str_ncpy(user, e->u.str, sizeof(user));
	e = odict_lookup(cmd, "domain");
	if (e)
		str_ncpy(domain, e->u.str, sizeof(domain));
	webapp_account_delete(user, domain);
	warning("delete executed\n");
	ws_send_json(WS_BARESIP, webapp_accounts_get());
}


void webapp_ws_baresip(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	struct call *call = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;

	if (!str_cmp(e->u.str, "call")) {
		e = odict_lookup(cmd, "dial");
		if (!str_cmp(e->u.str, "")) {
			ws_send_all(WS_CALLS, SIP_EMPTY);
		}
		else {
			if (!webapp_session_available()) {
				ws_send_all(WS_CALLS, SIP_MAX_CALLS);
				goto out;
			}

			ua_connect(uag_current(), &call, NULL,
					e->u.str, VIDMODE_OFF);
			webapp_call_update(call, "Outgoing");
		}
	}
	else if (!str_cmp(e->u.str, "addsip")) {
		webapp_account_add(e);
		ws_send_json(WS_BARESIP, webapp_accounts_get());
	}
	else if (!str_cmp(e->u.str, "deletesip")) {
		sip_delete(cmd, e);
	}
	else if (!str_cmp(e->u.str, "uagcurrent")) {
		char aorfind[100] = {0};

		e = odict_lookup(cmd, "aor");
		if (!e)
			goto out;
		snprintf(aorfind, sizeof(aorfind), "sip:%s", e->u.str);
		uag_current_set(uag_find_aor(aorfind));
		webapp_account_current();
		ws_send_json(WS_BARESIP, webapp_accounts_get());
	}

out:
	mem_deref(cmd);
}
