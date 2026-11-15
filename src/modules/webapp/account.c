#include <re.h>
#include <baresip.h>
#include <slcacert.h>
#include <string.h>
#include "webapp.h"

static struct tmr tmr;
static struct odict *accs = NULL;
static char filename[256] = "";
static struct http_req *req = NULL;
static struct http_cli *cli = NULL;
static char version[64] = "";
static struct ua *main_ua = NULL;
static struct ua *quick_ua = NULL;
static struct ua *current_ua = NULL;


/* baresip 4.x has no "current UA" anymore, webapp tracks it itself */
struct ua *webapp_ua_current(void)
{
	struct le *le;

	for (le = list_head(uag_list()); le; le = le->next) {
		if (le->data == current_ua)
			return current_ua;
	}

	current_ua = list_ledata(list_head(uag_list()));

	return current_ua;
}


void webapp_ua_current_set(struct ua *ua)
{
	current_ua = ua;
}


struct ua* webapp_get_main_ua(void) {
	return main_ua;
}

struct ua* webapp_get_quick_ua(void) {
	return quick_ua;
}

const struct odict* webapp_accounts_get(void) {
	return (const struct odict *)accs;
}

static void update(void *arg)
{
	char json[512];
	(void)arg;

	re_snprintf(json, sizeof(json),
			"{\
			\"callback\": \"UPDATE\",\
			\"version\": \"%s\"\
			}",
			version);

	ws_send_all(WS_BARESIP, json);
}

static int sip_register(struct odict *o)
{
	struct le *le;
	char buf[512] = {0};
	char user[50] = {0};
	char password[50] = {0};
	char domain[50] = {0};
	char transport[4] = "udp";
	char opt[300] = {0};
	char opt_quick[300] = {0};
	struct ua *ua;
	bool update_found = false;
	bool quick = false;

	int err = 0;

	if (!o)
		return EINVAL;

	for (le = o->lst.head; le; le = le->next) {
		const struct odict_entry *e = le->data;

		if (odict_entry_type(e) != ODICT_STRING) {
			continue;
		}

		if (!str_cmp(odict_entry_key(e), "user")) {
			str_ncpy(user, odict_entry_str(e),
				 sizeof(user));
		}
		else if (!str_cmp(odict_entry_key(e), "password")) {
			str_ncpy(password, odict_entry_str(e),
				 sizeof(password));
		}
		else if (!str_cmp(odict_entry_key(e), "domain")) {
			str_ncpy(domain, odict_entry_str(e),
				 sizeof(domain));
		}
		else if (!str_cmp(odict_entry_key(e), "transport")) {
			str_ncpy(transport, odict_entry_str(e),
				 sizeof(transport));
		}
		else if (!str_cmp(odict_entry_key(e), "command")) {
			continue;
		}
		else if (!str_cmp(odict_entry_key(e), "status")) {
			continue;
		}
		else if (!str_cmp(odict_entry_key(e), "quick")) {
			quick = true;
			str_ncpy(opt_quick, odict_entry_str(e),
				 sizeof(opt_quick));
		}
		else if (!str_cmp(odict_entry_key(e), "update")) {
			update_found = true;
			if (!str_cmp(odict_entry_str(e), "false")) {
				continue;
			}
			str_ncpy(version, odict_entry_str(e), sizeof(version));
			tmr_start(&tmr, 8000, update, NULL);
		}
		else {
			size_t len = str_len(opt);

			re_snprintf(opt + len, sizeof(opt) - len, ";%s=%s",
				    odict_entry_key(e),
				    odict_entry_str(e));
		}
	}

	/* the update hint is only used once */
	if (update_found)
		odict_entry_del(o, "update");

	re_snprintf(buf, sizeof(buf),
		    "<sip:%s@%s;transport=%s>;auth_pass=%s;%s",
		    user, domain, transport, password, opt);
	ua_alloc(&ua, buf);
	ua_register(ua);

	main_ua = ua;

	if (quick) {
		re_snprintf(buf, sizeof(buf),
				"<sip:quick-%s@%s;transport=%s>;"
				"auth_pass=%s;%s;rtcp_mux=yes",
				user, domain, transport, password, opt_quick);
		warning("register quick %s\n", buf);
		ua_alloc(&ua, buf);
		ua_register(ua);
		quick_ua = ua;
	}

	return err;
}


void webapp_account_delete(char *user, char *domain)
{
	struct le *le;
	bool user_delete;

	if (!accs)
		return;

	for (le = accs->lst.head; le;) {
		char o_user[50];
		char o_domain[70];
		char aor[120];
		const struct odict_entry *o = le->data;
		const struct odict_entry *e;

		e = odict_lookup(odict_entry_object(o), "user");
		if (!e) {
			le = le->next;
			continue;
		}
		str_ncpy(o_user, odict_entry_str(e), sizeof(o_user));

		e = odict_lookup(odict_entry_object(o), "domain");
		if (!e) {
			le = le->next;
			continue;
		}

		str_ncpy(o_domain, odict_entry_str(e), sizeof(o_domain));

		if (!user)
			/* Delete only by domain selection */
			user_delete = true;
		else
			user_delete = !str_cmp(o_user, user);

		if (user_delete && !str_cmp(o_domain, domain)) {
			odict_entry_del(accs, odict_entry_key(o));
			re_snprintf(aor, sizeof(aor), "sip:%s@%s",
					o_user, o_domain);
			struct ua *ua = uag_find_aor(aor);
			if (ua == current_ua)
				current_ua = NULL;
			if (ua == main_ua)
				main_ua = NULL;
			if (ua == quick_ua)
				quick_ua = NULL;
			mem_deref(ua);
			webapp_write_file_json(accs, filename);
			info("webapp/account: delete user %s\n", aor);

			if (user)
				break;

			le = accs->lst.head;
			continue;
		}
		le = le->next;
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
		e = odict_lookup(odict_entry_object(eg), "user");
		if (!e)
			continue;
		str_ncpy(user, odict_entry_str(e), sizeof(user));

		e = odict_lookup(odict_entry_object(eg), "domain");
		if (!e)
			continue;
		str_ncpy(domain, odict_entry_str(e), sizeof(domain));
		re_snprintf(aor_find, sizeof(aor_find), "sip:%s@%s", user,
				domain);

		if (!str_cmp(aor_find, aor)) {
			struct odict *og = odict_entry_object(eg);

			if (odict_lookup(og, "status"))
				odict_entry_del(og, "status");
			odict_entry_add(og, "status", ODICT_BOOL, status);
		}
	}
}


void webapp_account_add(struct odict *acc)
{
	sip_register(acc);
	webapp_odict_add(accs, acc);
	webapp_write_file_json(accs, filename);
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

	uag = account_aor(ua_account(webapp_ua_current()));
	if (!uag)
		return;
	re_snprintf(aor_current, sizeof(aor_current), "%s", uag);

	struct le *le;
	for (le = accs->lst.head; le; le = le->next) {
		eg = le->data;
		e = odict_lookup(odict_entry_object(eg), "user");
		if (!e)
			continue;
		str_ncpy(o_user, odict_entry_str(e), sizeof(o_user));

		e = odict_lookup(odict_entry_object(eg), "domain");
		if (!e)
			continue;
		str_ncpy(o_domain, odict_entry_str(e), sizeof(o_domain));

		re_snprintf(aor_find, sizeof(aor_find), "sip:%s@%s", o_user,
				o_domain);

		if (odict_lookup(odict_entry_object(eg), "current"))
			odict_entry_del(odict_entry_object(eg), "current");

		if (!str_cmp(aor_find, aor_current))
			odict_entry_add(odict_entry_object(eg), "current",
					ODICT_BOOL, true);
	}
}


static void http_resp_handler(int err, const struct http_msg *msg, void *arg)
{
	struct mbuf *mb = NULL;
	struct odict *o = NULL;
	const struct odict_entry *e;
	char message[8192] = {0};
	char user[50] = {0};
	char domain[50] = {0};
	char user_quick[100] = {0};
	(void)arg;

	if (err || !msg) {
		warning("webapp/account: provisioning failed (%m)\n", err);
		return;
	}

	if (msg->scode != 200) {
		warning("webapp/account: provisioning failed: %u %r\n",
			msg->scode, &msg->reason);
		return;
	}

	mb = msg->mb;


	(void)re_snprintf(message, sizeof(message), "%b",
			mb->buf, mb->end);

	err = json_decode_odict(&o, DICT_BSIZE, strstr(message, "{"),
			strstr(message, "}")-strstr(message, "{")+1,
			MAX_LEVELS);
	if (err)
		goto out;

	e = odict_lookup(o, "user");
	str_ncpy(user, odict_entry_str(e), sizeof(user));

	info("webapp/account: add user: %s\n", user);

	e = odict_lookup(o, "domain");
	str_ncpy(domain, odict_entry_str(e), sizeof(domain));

	(void)re_snprintf(user_quick, sizeof(user_quick), "sip:quick-%s@%s",
			user, domain);

	{
		struct ua *ua = uag_find_aor(user_quick);
		if (ua == quick_ua)
			quick_ua = NULL;
		if (ua == current_ua)
			current_ua = NULL;
		mem_deref(ua);
	}

	webapp_account_delete(user, domain);
	webapp_account_delete(user_quick, domain);
	webapp_account_delete(NULL, "studio-link.de");
	webapp_account_delete(NULL, "studio.link");
	webapp_account_delete(NULL, "studio.link:443");

	if (e) {
		webapp_account_add(o);
	}

	webapp_ua_current_set(main_ua);
	webapp_account_current();

out:
	mem_deref(o);
}

static void provisioning(void)
{
	char url[255] = {0};
	char path[] = "api/v1/provisioning";
	struct config *cfg = conf_config();
	const struct network *net = baresip_network();

	info("webapp/account: start provisioning\n");

#ifdef SLPLUGIN
	re_snprintf(url, sizeof(url),
			"https://%s/%s/%s?version=%s&type=plugin",
			webapp_provisioning_host(), path, cfg->sip.uuid,
			SLVERSION);
#else
	re_snprintf(url, sizeof(url),
			"https://%s/%s/%s?version=%s&type=standalone",
			webapp_provisioning_host(), path, cfg->sip.uuid,
			SLVERSION);
#endif

	sl_http_client_alloc(&cli, net_dnsc(net));

	http_request(&req, cli, "GET", url, http_resp_handler,
			NULL, NULL, NULL, NULL);
}

static void startup(void *arg)
{
	(void)arg;
	struct le *le;
	for (le = accs->lst.head; le; le = le->next) {
		sip_register(odict_entry_object(le->data));
	}
	provisioning();
}

int webapp_accounts_init(void)
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
				"%s/accounts.json", path) < 0)
		return ENOMEM;

	info("webapp/account: init\n");

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


	tmr_init(&tmr);
	tmr_start(&tmr, 1000, startup, NULL);

out:
	mem_deref(mb);
	return err;
}


void webapp_accounts_close(void)
{
	tmr_cancel(&tmr);
	req = mem_deref(req);
	cli = mem_deref(cli);
	webapp_write_file_json(accs, filename);
	accs = mem_deref(accs);
	current_ua = NULL;
}
