#include <re.h>
#include <baresip.h>
#include "webapp.h"


void webapp_ws_options(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	const struct odict_entry *key = NULL;
	const struct odict_entry *value = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

	key = odict_lookup(cmd, "key");
	if (!key)
		goto out;

	value = odict_lookup(cmd, "value");
	if (!value)
		goto out;

	webapp_options_set(key->u.str, value->u.str);

out:
	mem_deref(cmd);
}
