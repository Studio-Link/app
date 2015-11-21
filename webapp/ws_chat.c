#include <re.h>
#include <baresip.h>
#include "webapp.h"

void webapp_ws_chat(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, 32, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), 8);
	if (!err) {
		e = odict_lookup(cmd, "command");
		if (!e)
			goto out;

		if (!str_cmp(e->u.str, "message")) {
			char peer[50] = {0};
			char message[100] = {0};
			
			e = odict_lookup(cmd, "text");
			if(!e)
				goto out;
			str_ncpy(message, e->u.str, sizeof(message));

			e = odict_lookup(cmd, "peer");
			if(!e)
				goto out;
			str_ncpy(peer, e->u.str, sizeof(peer));

			warning("MESSAGE TO:%s, MESSAGE: %s\n", peer, message);
			webapp_chat_add(peer, message, true);
			ws_send_json(WS_CHAT, webapp_messages_get());
			//message_send(uag_current(), peer, message)
		}
	}

out:
	mem_deref(cmd);
}
