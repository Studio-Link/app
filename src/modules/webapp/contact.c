#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"

static struct odict *contacts = NULL;

static char filename[256] = "";


const struct odict* webapp_contacts_get(void) {
	return (const struct odict *)contacts;
}


static int contact_register(const struct odict_entry *o)
{
	struct le *le;
	struct pl pl;
	char buf[512] = {0};
	char sip[100] = {0};
	char name[50] = {0};
	struct contacts *mycontacts = baresip_contacts();

	int err = 0;

	if (o->type == ODICT_OBJECT) {
		le = o->u.odict->lst.head;

	}
	else {
		le = (void *)&o->le;
	}

	for (le=le; le; le=le->next) {
		const struct odict_entry *e = le->data;

		if (e->type != ODICT_STRING) {
			continue;
		}

		if (!str_cmp(e->key, "name")) {
			str_ncpy(name, e->u.str, sizeof(name));
		}
		else if (!str_cmp(e->key, "sip")) {
			str_ncpy(sip, e->u.str, sizeof(sip));
		}
		else if (!str_cmp(e->key, "command")) {
			continue;
		}
		else if (!str_cmp(e->key, "status")) {
			continue;
		}
	}

	re_snprintf(buf, sizeof(buf), "\"%s\"<sip:%s>",
			name, sip);

	pl.p = buf;
	pl.l = strlen(buf);
	contact_add(mycontacts, NULL, &pl);

	return err;
}


void webapp_contact_add(const struct odict_entry *contact)
{
	contact_register(contact);
	webapp_odict_add(contacts, contact);
	webapp_write_file_json(contacts, filename);
}


void webapp_contact_delete(const char *sip)
{
	struct le *le;
	for (le = contacts->lst.head; le; le = le->next) {
		char o_sip[100];
		const struct odict_entry *o = le->data;
		const struct odict_entry *e;

		e = odict_lookup(o->u.odict, "sip");
		if (!e)
			continue;
		str_ncpy(o_sip, e->u.str, sizeof(o_sip));

		if (!str_cmp(o_sip, sip)) {
			odict_entry_del(contacts, o->key);
			/*
			snprintf(aor, sizeof(aor), "sip:%s@%s", user, domain);
			mem_deref(uag_find_aor(aor));
			*/
			webapp_write_file_json(contacts, filename);
			break;
		}
	}
}


int webapp_contacts_init(void)
{
	char path[256] = "";
	struct mbuf *mb;
	struct le *le;
	int err = 0;

	mb = mbuf_alloc(8192);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/contacts.json", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		err = odict_alloc(&contacts, DICT_BSIZE);
	}
	else {
		err = json_decode_odict(&contacts, DICT_BSIZE,
				(char *)mb->buf, mb->end, MAX_LEVELS);
	}
	if (err)
		goto out;

	for (le = contacts->lst.head; le; le = le->next) {
		contact_register(le->data);
	}

out:
	mem_deref(mb);
	return err;
}


void webapp_contacts_close(void)
{
	webapp_write_file_json(contacts, filename);
	mem_deref(contacts);
}
