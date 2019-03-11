#include <re.h>
#include <baresip.h>
#include <string.h>
#include <time.h>
#include "webapp.h"

static struct odict *messages = NULL;
static struct message_lsnr *message_lsnr;

const struct odict* webapp_messages_get(void) {
	return (const struct odict *)messages;
}


static int peercmp(const char *peer1, const char *peer2) {
	struct pl peer1_flt, peer2_flt;
	int err;

	if (!peer1)
		return 1;
	if (!peer2)
		return 1;

	re_regex(peer1, strlen(peer1), "sip:[-a-zA-Z0-9@.]+", &peer1_flt);
	re_regex(peer2, strlen(peer2), "sip:[-a-zA-Z0-9@.]+", &peer2_flt);
	err = pl_cmp(&peer1_flt, &peer2_flt);

	return err;
}


int webapp_chat_send(char *message, char *exclude_peer)
{
	struct list *calls = ua_calls(uag_current());
	struct call *call = NULL;
	struct le *le;
        char msg[512];
	int err = 0;


	for (le = list_head(calls); le; le = le->next) {
		call = le->data;
		if (!peercmp(call_peeruri(call), exclude_peer))
			continue;
		if (exclude_peer) {
			re_snprintf(msg, sizeof(msg), "%s;%s", message, exclude_peer);
		} else {
			re_snprintf(msg, sizeof(msg), "%s", message);
		}
		err = message_send(uag_current(), call_peeruri(call), msg, NULL, NULL);
		if (err)
			warning("message failed: %d\n", err);
	}

	return err;
}


int webapp_chat_add(const char *peer, const char *message, bool self)
{
	char time_s[20] = {0};
	int err = 0;
	struct odict *o;

	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return ENOMEM;

	odict_entry_add(o, "message", ODICT_STRING, message);
	odict_entry_add(o, "peer", ODICT_STRING, peer);
	odict_entry_add(o, "self", ODICT_BOOL, self);

	(void)re_snprintf(time_s, sizeof(time_s), "%d", time(NULL));

	odict_entry_add(messages, time_s, ODICT_OBJECT, o);

	mem_deref(o);

	return err;
}


static void message_handler(const struct pl *peer, const struct pl *ctype,
		struct mbuf *body, void *arg)
{
	(void)ctype;
	(void)arg;
	char message[512] = {0};
	char s_peer[50] = {0};
	struct contacts *contacts = baresip_contacts();

	(void)re_snprintf(message, sizeof(message), "%b",
			mbuf_buf(body), mbuf_get_left(body));

	(void)re_snprintf(s_peer, sizeof(s_peer), "%r", peer);

	warning("message from %s: %s\n", s_peer, message);
	
	webapp_chat_send(message, s_peer);

	struct contact *c = contact_find(contacts, s_peer);

	if (c) {
		const struct sip_addr *addr = contact_addr(c);
		(void)re_snprintf(s_peer, sizeof(s_peer), "%r", &addr->dname);
	}

	(void)webapp_chat_add(s_peer, message, false);
	ws_send_json(WS_CHAT, webapp_messages_get());
}


int webapp_chat_init(void)
{
	int err = 0;
	err = message_listen(&message_lsnr, baresip_message(),
			message_handler, NULL);

	if (err)
		return err;

	err = odict_alloc(&messages, DICT_BSIZE);

	return err;
}


void webapp_chat_close(void)
{
	message_lsnr = mem_deref(message_lsnr);
	mem_deref(messages);
}
