/**
 * @file webapp.c Webserver UI module
 *
 * Copyright (C) 2013-2020 studio-link.de Sebastian Reimers
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "assets/index_html.h"
#include "assets/js.h"
#include "assets/css.h"
#include "assets/images.h"
#include "webapp.h"

static struct tmr tmr;

static struct http_sock *httpsock = NULL;
static char webapp_call_json[150] = {0};
struct odict *webapp_calls = NULL;
static char command[100] = {0};
static bool auto_answer = false;

static int http_sreply(struct http_conn *conn, uint16_t scode,
		const char *reason, const char *ctype,
		const char *fmt, size_t size)
{
	struct mbuf *mb;
	int err = 0;
	mb = mbuf_alloc(size);
	if (!mb)
		return ENOMEM;

	err = mbuf_write_mem(mb, (const uint8_t *)fmt, size);

	if (err)
		goto out;

	http_reply(conn, 200, "OK",
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%b",
			ctype,
			mb->end,
			mb->buf, mb->end);
out:
	mem_deref(mb);
	return err;
}


static void http_req_handler(struct http_conn *conn,
			     const struct http_msg *msg, void *arg)
{
	const struct network *net = baresip_network();

	/*
	 * Dynamic Requests
	 */

	if (0 == pl_strcasecmp(&msg->path, "/ws_baresip")) {
		webapp_ws_handler(conn, WS_BARESIP, msg, webapp_ws_baresip);
		ws_send_json(WS_BARESIP, webapp_accounts_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_contacts")) {
		webapp_ws_handler(conn, WS_CONTACT, msg, webapp_ws_contacts);
		ws_send_json(WS_CONTACT, webapp_contacts_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_chat")) {
		webapp_ws_handler(conn, WS_CHAT, msg, webapp_ws_chat);
		ws_send_json(WS_CHAT, webapp_messages_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_rtaudio")) {
		webapp_ws_handler(conn, WS_RTAUDIO, msg, webapp_ws_rtaudio);
#ifndef SLPLUGIN
		ws_send_json(WS_RTAUDIO, slrtaudio_get_interfaces());
#endif
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_meter")) {
		webapp_ws_handler(conn, WS_METER, msg, webapp_ws_meter);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_calls")) {
		webapp_ws_handler(conn, WS_CALLS, msg, webapp_ws_calls);
		ws_send_json(WS_CALLS, webapp_calls);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_options")) {
		webapp_ws_handler(conn, WS_OPTIONS, msg, webapp_ws_options);
		ws_send_json(WS_OPTIONS, webapp_options_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/status.json")) {
		char ipv4[15];
		char json[255];
		sa_ntop(net_laddr_af(net, AF_INET), ipv4, sizeof(ipv4));

		re_snprintf(json, sizeof(json), "[{ \"name\": \"IPv4\", \
				\"value\": \"%s\", \"label\": \"default\"}]",
				ipv4);

		http_sreply(conn, 200, "OK",
				"application/json",
				(const char*)json,
				strlen(json));
		return;
	}

	/*
	 * Static Requests
	 */

	if (0 == pl_strcasecmp(&msg->path, "/")) {
		http_sreply(conn, 200, "OK",
				"text/html",
				(const char*)dist_index_html,
				dist_index_html_len);

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/version")) {
		http_sreply(conn, 200, "OK",
				"text/html",
				(const char*)SLVERSION,
				sizeof(SLVERSION));

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/swvariant")) {
#ifdef SLPLUGIN
		const char* value = "plugin";
#else
		const char* value = "standalone";
#endif
		http_sreply(conn, 200, "OK",
				"text/html",
				value,
				strlen(value));
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/app.js")) {
		http_sreply(conn, 200, "OK",
				"application/javascript",
				(const char*)dist_app_js,
				dist_app_js_len);

		return;
	}
#ifdef SLPLUGIN
	if (0 == pl_strcasecmp(&msg->path, "/images/logo.svg")) {
		http_sreply(conn, 200, "OK",
				"image/svg+xml",
				(const char*)dist_images_logo_plugin_svg,
				dist_images_logo_plugin_svg_len);

		return;
	}
#else
	if (0 == pl_strcasecmp(&msg->path, "/images/logo.svg")) {
		http_sreply(conn, 200, "OK",
				"image/svg+xml",
				(const char*)dist_images_logo_standalone_svg,
				dist_images_logo_standalone_svg_len);

		return;
	}
#endif
	if (0 == pl_strcasecmp(&msg->path, "/app.css")) {
		http_sreply(conn, 200, "OK",
				"text/css",
				(const char*)dist_app_css,
				dist_app_css_len);
		return;
	}

	http_ereply(conn, 404, "Not found");
}


static void ua_event_current_set(struct ua *ua)
{
	uag_current_set(ua);
	webapp_account_current();
	ws_send_json(WS_BARESIP, webapp_accounts_get());
}


struct list* sl_sessions(void);

int webapp_session_stop_stream(void)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	int err = 0;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->stream) {
			ua_hangup(uag_current(), sess->call, 0, NULL);
			sess->call = NULL;

			return err;
		}
	}

	return err;
}

int webapp_session_delete(char * const sess_id, struct call *call)
{
	char id[64] = {0};
	int err = 0;
	struct list *tsession;
	struct session *sess;
	struct le *le;

	if (!sess_id && !call)
		return EINVAL;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		re_snprintf(id, sizeof(id), "%x", sess);

		if (sess_id) {
			if (!str_cmp(id, sess_id)) {
				ua_hangup(uag_current(), sess->call, 0, NULL);
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				warning("delete session id/channel: %s/%d\n",
						id, sess->ch);
				break;
			}
		}
		if (call) {
			if (sess->call == call) {
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				warning("delete session id/channel: %s/%d\n",
						id, sess->ch);
				break;
			}
		}
	}

	return err;
}


int webapp_call_update(struct call *call, char *state)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	struct odict *o;
	char id[64] = {0};
	int err = 0;
	bool new = true; 


	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return ENOMEM;

	tsession = sl_sessions();

#ifndef SLPLUGIN
	if (!str_cmp(call_peeruri(call), "sip:stream@studio-link.de;transport=tls")) {
		for (le = tsession->head; le; le = le->next) {
			sess = le->data;

			if (sess->stream) {
				sess->call = call;
				return err;
			}
		}
	}
#endif

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		if (sess->call == call) {
			new = false;
			warning("session update: %d\n", sess->ch);
		}
	}

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		if (new && !sess->call) {
			sess->call = call;
			new = false;
		}

		if (sess->call != call)
			continue;


		re_snprintf(id, sizeof(id), "%x", sess);
		warning("session id/channel: %s/%d/%d\n", id, sess->ch, new);

		odict_entry_del(webapp_calls, id);
		odict_entry_add(o, "peer", ODICT_STRING, call_peeruri(call));
		odict_entry_add(o, "state", ODICT_STRING, state);
#ifdef SLPLUGIN
		odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch+1);
#else
		odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch);
#endif
		odict_entry_add(webapp_calls, id, ODICT_OBJECT, o);
	}

	ws_send_json(WS_CALLS, webapp_calls);
	mem_deref(o);
	return err;
}


static void ua_event_handler(struct ua *ua, enum ua_event ev,
		struct call *call, const char *prm, void *arg)
{

	switch (ev) {
		case UA_EVENT_CALL_INCOMING:
			ua_event_current_set(ua);
			if(!auto_answer) {
				re_snprintf(webapp_call_json, sizeof(webapp_call_json),
						"{ \"callback\": \"INCOMING\",\
						\"peeruri\": \"%s\",\
						\"key\": \"%x\" }",
						call_peeruri(call), call);
				webapp_call_update(call, "Incoming");
				ws_send_all(WS_CALLS, webapp_call_json);
			} else {
				debug("auto answering call\n");
				ua_answer(uag_current(), call);
			}
			break;

		case UA_EVENT_CALL_ESTABLISHED:
			ua_event_current_set(ua);
			webapp_call_update(call, "Established");
			break;

		case UA_EVENT_CALL_CLOSED:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"CLOSED\",\
					\"message\": \"%s\" }", prm);
			webapp_session_delete(NULL, call);
			ws_send_all(WS_CALLS, webapp_call_json);
			ws_send_json(WS_CALLS, webapp_calls);
			break;

		case UA_EVENT_REGISTER_OK:
			warning("Register OK: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), true);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_UNREGISTERING:
			warning("UNREGISTERING: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_REGISTER_FAIL:
			warning("Register Fail: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		default:
			break;
	}
}

static char * my_strtok_r (char *s, const char *delim, char **save_ptr)
{
	char *end;
	if (s == NULL)
		s = *save_ptr;
	if (*s == '\0') {
		*save_ptr = s;
		return NULL;
	}
	/* Scan leading delimiters.  */
	s += strspn (s, delim);
	if (*s == '\0') {
		*save_ptr = s;
		return NULL;
	}
	/* Find the end of the token.  */
	end = s + strcspn (s, delim);
	if (*end == '\0') {
		*save_ptr = end;
		return s;
	}
	/* Terminate the token and make *SAVE_PTR point past it.  */
	*end = '\0';
	*save_ptr = end + 1;
	return s;
}

static int http_port(void)
{
	char path[256] = "";
	char filename[256] = "";
	struct mbuf *mb;
	struct sa srv;
	struct sa listen;
	int port = 0;
	char port_string[10];
	char bind[256] = "127.0.0.1";
	char file_contents[256];
	int err = 0;

	mb = mbuf_alloc(20);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/webapp.conf", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	if (err) {
		port = 0;
	}
	else {
		char *str = (char *)mb->buf;
		char *p, *temp = {0};
		p = my_strtok_r(str, "\n", &temp);
		if(p) {
			do {
				char *tok_temp;
				char *tok = my_strtok_r(p, " ", &tok_temp);
				char *val = my_strtok_r(NULL, " ", &tok_temp);
				if(tok && val) {
					if(!str_cmp(tok, "http_listen")) {
						char *tmp = strchr(val, ':');
						if(tmp) {
							*tmp = 0;
							strcpy(bind, val);
							tmp++;
						}
						port = atoi(tmp);
					} else if(!str_cmp(tok, "auto_answer")) {
						auto_answer=atoi(val);
					}
				}
			} while ((p = my_strtok_r(NULL, "\n", &temp)) != NULL);
		}
	}

	err = sa_set_str(&srv, bind, port);

	err |= http_listen(&httpsock, &srv, http_req_handler, NULL);
	if (err)
		goto out;
	tcp_sock_local_get(http_sock_tcp(httpsock), &listen);

	re_snprintf(port_string, sizeof(port_string), "%d",
			sa_port(&listen));

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open http://127.0.0.1:%s/",
			port_string);
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "start http://127.0.0.1:%s/",
			port_string);
#else
	re_snprintf(command, sizeof(command), "xdg-open http://127.0.0.1:%s/",
			port_string);
#endif
	info("http listening on ip: %s port: %s\n", bind, port_string);

	re_snprintf(file_contents, sizeof(file_contents), "http_listen %s:%s\nauto_answer %d\n", bind, port_string, auto_answer);
	webapp_write_file(file_contents, filename);

out:
	mem_deref(mb);
	return err;
}


static void syscmd(void *arg)
{
#ifndef SLBOX
	int err = 0;
	err = system(command);
	if (err) {}
#endif
}

static int module_init(void)
{
	int err = 0;

	err = odict_alloc(&webapp_calls, DICT_BSIZE);
	if (err)
		goto out;

#ifdef SLPLUGIN
	(void)re_fprintf(stderr, "Studio Link Webapp %s - Effect Plugin"
			" Copyright (C) 2013-2019"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#else
	(void)re_fprintf(stderr, "Studio Link Webapp %s - Standalone"
			" Copyright (C) 2013-2019"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#endif

	err = http_port();
	if (err)
		goto out;

	uag_event_register(ua_event_handler, NULL);
	webapp_ws_init();
	webapp_accounts_init();
	webapp_contacts_init();
	webapp_options_init();
	//webapp_chat_init();
	webapp_ws_meter_init();

	tmr_init(&tmr);
#if defined (SLPLUGIN)
	tmr_start(&tmr, 800, syscmd, NULL);
#else
	syscmd(NULL);
	webapp_ws_rtaudio_init();
#endif

out:
	if (err) {
		mem_deref(httpsock);
	}
	return err;
}


static int module_close(void)
{
	tmr_cancel(&tmr);
	uag_event_unregister(ua_event_handler);

	webapp_ws_meter_close();
	webapp_accounts_close();
	webapp_contacts_close();
	webapp_options_close();
	//webapp_chat_close();
#ifndef SLPLUGIN
	webapp_ws_rtaudio_close();
#endif
	webapp_ws_close();
	mem_deref(httpsock);
	mem_deref(webapp_calls);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(webapp) = {
	"webapp",
	"application",
	module_init,
	module_close,
};
