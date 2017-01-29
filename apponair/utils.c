#include <re.h>
#include <baresip.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "webapp.h"

void webapp_odict_add(struct odict *og, const struct odict_entry *eg)
{
	struct le *le;
	struct odict *o;
	int err = 0;
	char index[64];
	size_t index_cnt=0;

	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return;

	le = (void *)&eg->le;
	if (!le)
		return;

	for (le=le->list->head; le; le=le->next) {
		const struct odict_entry *e = le->data;
		if (!str_cmp(e->key, "command"))
			continue;
		odict_entry_add(o, e->key, e->type, e->u.str);
	}

	/* Limited Loop
	 * No one will need more than 100 accounts for a personal computer
	 */
	for (int i=0; i<100; i++) {
		re_snprintf(index, sizeof(index), "%u", index_cnt);
		if (!odict_lookup(og, index)) {
			break;
		}
		index_cnt = index_cnt + 1;
	}

	odict_entry_add(og, index, ODICT_OBJECT, o);

	mem_deref(o);
}


int webapp_write_file(char *string, char *filename)
{
	FILE *f = NULL;
	int err = 0;

	f = fopen(filename, "w");
	if (!f)
		return errno;

	re_fprintf(f, "%s", string);

	if (f)
		(void)fclose(f);

	return err;
}


int webapp_write_file_json(struct odict *json, char *filename)
{
	FILE *f = NULL;
	int err = 0;

	f = fopen(filename, "w");
	if (!f)
		return errno;

	re_fprintf(f, "%H", json_encode_odict, json);

	if (f)
		(void)fclose(f);

	return err;
}


int webapp_load_file(struct mbuf *mb, char *filename)
{
	int err = 0, fd = open(filename, O_RDONLY);

	if (fd < 0)
		return errno;

	for (;;) {
		uint8_t buf[1024];

		const ssize_t n = read(fd, (void *)buf, sizeof(buf));
		if (n < 0) {
			err = errno;
			break;
		}
		else if (n == 0)
			break;

		err |= mbuf_write_mem(mb, buf, n);
	}

	(void)close(fd);

	return err;
}
