#include <re.h>
#include <baresip.h>
#include "webapp.h"

void webapp_ws_chat(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *e = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (!err) {
		e = odict_lookup(cmd, "command");
		if (!e)
			goto out;

		if (!str_cmp(e->u.str, "message")) {
			char message[462] = {0};

			e = odict_lookup(cmd, "text");
			if (!e)
				goto out;
			str_ncpy(message, e->u.str, sizeof(message));

			webapp_chat_add(NULL, message, true);
			ws_send_json(WS_CHAT, webapp_messages_get());
			webapp_chat_send(message, NULL);
		}
	}

out:
	mem_deref(cmd);
}
