#include <re.h>
#include <baresip.h>
#include "webapp.h"

void webapp_ws_contacts(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(cmd, "command");
	if (!e)
		goto out;

	if (!str_cmp(e->u.str, "addcontact")) {
		webapp_contact_add(e);
		ws_send_json(WS_CONTACT, webapp_contacts_get());
		ws_send_json(WS_CALLS, webapp_calls);
	}
	else if (!str_cmp(e->u.str, "deletecontact")) {
		char sip[100] = {0};

		e = odict_lookup(cmd, "sip");
		if (!e)
			goto out;

		str_ncpy(sip, e->u.str, sizeof(sip));
		webapp_contact_delete(sip);
		ws_send_json(WS_CONTACT, webapp_contacts_get());
	}

out:

	mem_deref(cmd);
}
