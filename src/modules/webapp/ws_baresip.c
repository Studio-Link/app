#include <re.h>
#include <baresip.h>
#include <slua.h>
#include "webapp.h"

#define SIP_EMPTY "{ \"callback\": \"CLOSED\",\
	\"message\": \"SIP Number Empty\" }"

#define SIP_MAX_CALLS "{ \"callback\": \"CLOSED\",\
	\"message\": \"Max Calls reached...\" }"

#define SIP_TRACK_BUSY "{ \"callback\": \"CLOSED\",\
	\"message\": \"Track is not available\" }"

static void sip_delete(struct odict *cmd, const struct odict_entry *e)
{
	char user[50];
	char domain[70];

	e = odict_lookup(cmd, "user");
	if (e)
		str_ncpy(user, odict_entry_str(e), sizeof(user));
	e = odict_lookup(cmd, "domain");
	if (e)
		str_ncpy(domain, odict_entry_str(e), sizeof(domain));
	webapp_account_delete(user, domain);
	warning("delete executed\n");
	ws_send_json(WS_BARESIP, webapp_accounts_get());
}


void webapp_ws_baresip(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	(void)hdr;
	(void)arg;
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

	if (!str_cmp(odict_entry_str(e), "call")) {
		e = odict_lookup(cmd, "dial");
		if (!str_cmp(odict_entry_str(e), "")) {
			ws_send_all(WS_CALLS, SIP_EMPTY);
		}
		else {
			uint64_t num = 0;
			int8_t track = 0;

			/* optional: call out on this track (plugin track) */
			if (odict_get_number(cmd, &num, "track") &&
			    num <= INT8_MAX)
				track = (int8_t)num;

			if (!webapp_session_available_track(track)) {
				ws_send_all(WS_CALLS, track ? SIP_TRACK_BUSY :
					    SIP_MAX_CALLS);
				goto out;
			}

			err = sl_ua_connect(webapp_ua_current(), &call,
					odict_entry_str(e));
			if (err) {
				webapp_call_error(odict_entry_str(e), err);
				goto out;
			}
			webapp_call_update_track(call, "Outgoing", track);
		}
	}
	else if (!str_cmp(odict_entry_str(e), "addsip")) {
		webapp_account_add(cmd);
		ws_send_json(WS_BARESIP, webapp_accounts_get());
	}
	else if (!str_cmp(odict_entry_str(e), "deletesip")) {
		sip_delete(cmd, e);
	}
	else if (!str_cmp(odict_entry_str(e), "uagcurrent")) {
		char aorfind[100] = {0};

		e = odict_lookup(cmd, "aor");
		if (!e)
			goto out;
		snprintf(aorfind, sizeof(aorfind), "sip:%s",
			 odict_entry_str(e));
		webapp_ua_current_set(uag_find_aor(aorfind));
		webapp_account_current();
		ws_send_json(WS_BARESIP, webapp_accounts_get());
	}

out:
	mem_deref(cmd);
}
