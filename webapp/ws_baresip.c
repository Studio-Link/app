#include <re.h>
#include <baresip.h>
#include "webapp.h"

#define SIP_EMPTY "{ \"callback\": \"CLOSED\",\
	\"message\": \"SIP Number Empty\" }"
#define SIP_CLOSED "{ \"callback\": \"CLOSED\"}"

static void sip_delete(struct odict *cmd, const struct odict_entry *e)
{
	char user[50];
	char domain[50];

	e = odict_lookup(cmd, "user");
	if (e)
		str_ncpy(user, e->u.str, sizeof(user));
	e = odict_lookup(cmd, "domain");
	if (e)
		str_ncpy(domain, e->u.str, sizeof(domain));
	webapp_account_delete(user, domain);
	ws_send_json(WS_BARESIP, webapp_accounts_get());
}


static bool have_active_calls(void)
{
	struct le *le;

	for (le = list_head(uag_list()); le; le = le->next) {

		struct ua *ua = le->data;

		if (ua_call(ua))
			return true;
	}

	return false;
}


void webapp_ws_baresip(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, 32, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), 8);
	if (!err) {
		e = odict_lookup(cmd, "command");
		if (e) {
			if (!str_cmp(e->u.str, "accept")) {
				ua_answer(uag_current(), NULL);
			}
			else if (!str_cmp(e->u.str, "hangup")) {
				warning("close: %p", uag_current());
				ua_hangup(uag_current(), NULL, 0, NULL);
				if(!have_active_calls())
					ws_send_all(WS_BARESIP, SIP_CLOSED);
			}
			else if (!str_cmp(e->u.str, "call")) {
				e = odict_lookup(cmd, "dial");
				if (!str_cmp(e->u.str, "")) {
					ws_send_all(WS_BARESIP, SIP_EMPTY);
				}
				else {
					ua_connect(uag_current(), NULL, NULL,
							e->u.str, NULL, VIDMODE_ON);
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
				if(e) {
					snprintf(aorfind, sizeof(aorfind), "sip:%s", e->u.str);
					uag_current_set(uag_find_aor(aorfind));
					warning("Current Account: %s\n", ua_aor(uag_current()));
					webapp_account_current();
					ws_send_json(WS_BARESIP, webapp_accounts_get());
				}
			}
		}
	}

	mem_deref(cmd);
}
