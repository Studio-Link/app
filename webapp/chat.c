#include <re.h>
#include <baresip.h>
#include <string.h>
#include <time.h>
#include "webapp.h"

enum {
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

static struct odict *messages = NULL;


const struct odict* webapp_messages_get(void) {
	return (const struct odict *)messages;
}

int webapp_chat_add(const char *peer, const char *message, bool self)
{
	char time_s[256] = {0};
	int err = 0;
	struct odict *o;

	err = odict_alloc(&o, DICT_BSIZE);
	if(err)
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
	char message[256] = {0};

	(void)re_snprintf(message, sizeof(message), "%b",
			mbuf_buf(body), mbuf_get_left(body));

	(void)webapp_chat_add(peer->p, message, false);
}


int webapp_chat_init(void)
{
	int err = 0;
	err = message_init(message_handler, NULL);
	if(err)
		return err;

	err = odict_alloc(&messages, DICT_BSIZE);

	return err;
}


void webapp_chat_close(void)
{
	mem_deref(messages);
}
