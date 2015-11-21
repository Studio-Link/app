#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"

enum {
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

static struct odict *accs = NULL;

static char filename[256] = "";

static struct http_req *req = NULL;

const struct odict* webapp_accounts_get(void) {
	return (const struct odict *)accs;
}


static int sip_register(const struct odict_entry *o)
{
	struct le *le;
	char buf[512] = {0};
	char user[50] = {0};
	char password[50] = {0};
	char domain[50] = {0};
	char transport[4] = "udp";
	char opt[300] = {0};

	int err = 0;

	if (o->type == ODICT_OBJECT) {
		le = o->u.odict->lst.head;

	}
	else {
		le = (void *)&o->le;
	}

	if (!le)
		return 1;

	for (le=le->list->head; le; le=le->next) {
		const struct odict_entry *e = le->data;

		if (e->type != ODICT_STRING) {
			continue;
		}

		if (!str_cmp(e->key, "user")) {
			str_ncpy(user, e->u.str, sizeof(user));
		}
		else if (!str_cmp(e->key, "password")) {
			str_ncpy(password, e->u.str, sizeof(password));
		}
		else if (!str_cmp(e->key, "domain")) {
			str_ncpy(domain, e->u.str, sizeof(domain));
		}
		else if (!str_cmp(e->key, "transport")) {
			str_ncpy(transport, e->u.str, sizeof(transport));
		}
		else if (!str_cmp(e->key, "command")) {
			continue;
		}
		else if (!str_cmp(e->key, "status")) {
			continue;
		}
		else {
			re_snprintf(opt, sizeof(opt), "%s;%s=%s",
					opt, e->key, e->u.str);
		}
	}

	re_snprintf(buf, sizeof(buf), "<sip:%s:%s@%s;transport=%s>%s",
			user, password, domain, transport, opt);
	ua_alloc(NULL, buf);

	return err;
}


void webapp_account_delete(char *user, char *domain)
{
	struct le *le;
	for (le = accs->lst.head; le; le = le->next) {
		char o_user[50];
		char o_domain[50];
		char aor[100];
		const struct odict_entry *o = le->data;
		const struct odict_entry *e;

		e = odict_lookup(o->u.odict, "user");
		if (!e)
			continue;
		str_ncpy(o_user, e->u.str, sizeof(o_user));

		e = odict_lookup(o->u.odict, "domain");
		if (!e)
			continue;

		str_ncpy(o_domain, e->u.str, sizeof(o_domain));

		if (!str_cmp(o_user, user) && !str_cmp(o_domain, domain)) {
			odict_entry_del(accs, o->key);
			snprintf(aor, sizeof(aor), "sip:%s@%s", user, domain);
			mem_deref(uag_find_aor(aor));
			webapp_write_file(accs, filename);
			break;
		}
	}

}


void webapp_account_status(const char *aor, bool status)
{
	const struct odict_entry *e;
	const struct odict_entry *eg;
	struct le *le;
	char user[50] = {0};
	char domain[50] = {0};
	char aor_find[100] = {0};

	for (le = accs->lst.head; le; le = le->next) {
		eg = le->data;
		e = odict_lookup(eg->u.odict, "user");
		if (!e)
			continue;
		str_ncpy(user, e->u.str, sizeof(user));

		e = odict_lookup(eg->u.odict, "domain");
		if (!e)
			continue;
		str_ncpy(domain, e->u.str, sizeof(domain));
		snprintf(aor_find, sizeof(aor_find), "sip:%s@%s", user,
				domain);


		if (!str_cmp(aor_find, aor)) {
			if (odict_lookup(eg->u.odict, "status"))
				odict_entry_del(eg->u.odict, "status");
			odict_entry_add(eg->u.odict, "status",
					ODICT_BOOL, status);
		}
	}
}


void webapp_account_add(const struct odict_entry *acc)
{
	sip_register(acc);
	webapp_odict_add(acc, accs);
	webapp_write_file(accs, filename);
}


void webapp_account_current(void)
{
	const struct odict_entry *e;
	const struct odict_entry *eg;
	char o_user[50] = {0};
	char o_domain[50] = {0};
	char aor_find[100] = {0};
	char aor_current[100] = {0};
	const char *uag = NULL;

	uag = ua_aor(uag_current());
	if(!uag) 
		return;
	re_snprintf(aor_current, sizeof(aor_current), "%s", uag);

	struct le *le;
	for (le = accs->lst.head; le; le = le->next) {
		eg = le->data;
		e = odict_lookup(eg->u.odict, "user");
		if (!e)
			continue;
		str_ncpy(o_user, e->u.str, sizeof(o_user));

		e = odict_lookup(eg->u.odict, "domain");
		if (!e)
			continue;
		str_ncpy(o_domain, e->u.str, sizeof(o_domain));

		snprintf(aor_find, sizeof(aor_find), "sip:%s@%s", o_user,
				o_domain);

		if (odict_lookup(eg->u.odict, "current"))
			odict_entry_del(eg->u.odict, "current");

		if (!str_cmp(aor_find, aor_current))
			odict_entry_add(eg->u.odict, "current",
					ODICT_BOOL, true);
	}
}


static void http_resp_handler(int err, const struct http_msg *msg, void *arg)
{
	if(!msg)
		return;
	struct mbuf *mb = msg->mb;
	struct odict *o = NULL;
	struct odict_entry *e;
	char message[8192] = {0};
	char user[50] = {0};
	char domain[50] = {0};

	(void)re_snprintf(message, sizeof(message), "%b",
			mb->buf, mb->end);

	err = json_decode_odict(&o, DICT_BSIZE, strstr(message, "{"),
			strstr(message, "}")-strstr(message, "{")+1,
			MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(o, "user");
	str_ncpy(user, e->u.str, sizeof(user));

	e = odict_lookup(o, "domain");
	str_ncpy(domain, e->u.str, sizeof(domain));

	/* if user already exists, delete him */
	webapp_account_delete(user, domain);

	if (e) {
		webapp_account_add(e);
	}

out:
	mem_deref(o);
	mem_deref(req);
	return;
}


static void http_data_handler(struct mbuf *mb, void *arg)
{
}


static void provisioning(void)
{
	char url[255] = {0};
	char host[] = "vpn.studio-link.de";
	char path[] = "provisioning/index.php";
	struct http_cli *cli = NULL;
	int err = 0;

	struct config *cfg = conf_config();

	re_snprintf(url, sizeof(url), "https://%s/%s?uuid=%s",
			host, path, cfg->sip.uuid);

	http_client_alloc(&cli, net_dnsc());

	err = http_request(&req, cli, "GET", url, http_resp_handler,
			http_data_handler, NULL, NULL);

	mem_deref(cli);
}


int webapp_accounts_init(void)
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
				"%s/accounts.json", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		err = odict_alloc(&accs, DICT_BSIZE);
	}
	else {
		err = json_decode_odict(&accs, DICT_BSIZE,
				(char *)mb->buf, mb->end, MAX_LEVELS);
	}
	if (err)
		goto out;


	for (le = accs->lst.head; le; le = le->next) {
		sip_register(le->data);
	}
	provisioning();
	webapp_account_current();

out:
	mem_deref(mb);
	return err;
}


void webapp_accounts_close(void)
{
	webapp_write_file(accs, filename);
	mem_deref(accs);
}
