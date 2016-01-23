#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"

static struct odict *options = NULL;
static char filename[256] = "";


const struct odict* webapp_options_get(void) 
{
	return (const struct odict *)options;
}


void webapp_options_set(char *key, char *value)
{
	odict_entry_del(options, key);
	odict_entry_add(options, key, ODICT_STRING, value);
	ws_send_json(WS_OPTIONS, options);
	webapp_write_file_json(options, filename);
}


void webapp_options_getv(char *key)
{
}


int webapp_options_init(void)
{
	char path[256] = "";
	struct mbuf *mb;
	int err = 0;

	mb = mbuf_alloc(8192);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/options.json", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		err = odict_alloc(&options, DICT_BSIZE);
#ifdef SLPLUGIN
		odict_entry_add(options, "auto-mix-n-1", ODICT_STRING, "true");
#endif
	}
	else {
		err = json_decode_odict(&options, DICT_BSIZE,
				(char *)mb->buf, mb->end, MAX_LEVELS);
	}
	if (err)
		goto out;

out:
	mem_deref(mb);
	return err;
}


void webapp_options_close(void)
{
	webapp_write_file_json(options, filename);
	mem_deref(options);
}
