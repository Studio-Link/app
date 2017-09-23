#include <re.h>
#include <baresip.h>
#include "webapp.h"


static void send_message(char *message)
{
	struct list *calls = ua_calls(uag_current());
	struct call *call = NULL;
	struct le *le;
	int err = 0;

	for (le = list_head(calls); le; le = le->next) {
		call = le->data;
		warning("MESSAGE TO:%s, MESSAGE: %s\n", call_peeruri(call), message);
		err = message_send(uag_current(), call_peeruri(call), message, NULL, NULL);
		if (err)
			warning("message failed: %d\n", err);
	}
}


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
			char message[512] = {0};

			e = odict_lookup(cmd, "text");
			if (!e)
				goto out;
			str_ncpy(message, e->u.str, sizeof(message));

			webapp_chat_add(NULL, message, true);
			ws_send_json(WS_CHAT, webapp_messages_get());
			send_message(message);
		}
	}

out:
	mem_deref(cmd);
}
