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
			"Cache-Control: no-cache, no-store, must-revalidate\r\n"
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
		sys_msleep(200);
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
		ws_send_json(WS_RTAUDIO, slaudio_get_interfaces());
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


static void ua_event_handler(struct ua *ua, enum ua_event ev,
		struct call *call, const char *prm, void *arg)
{
	int8_t key = 0;

	switch (ev) {
		case UA_EVENT_CALL_INCOMING:
			ua_event_current_set(ua);

			if(!webapp_session_available()) {
				ua_hangup(uag_current(), call, 486, "Max Calls");
				return;
			}

			key = webapp_call_update(call, "Incoming");
			if (!key)
				return;

			if(!auto_answer) {
				re_snprintf(webapp_call_json, sizeof(webapp_call_json),
						"{ \"callback\": \"INCOMING\",\
						\"peeruri\": \"%s\",\
						\"key\": \"%d\" }",
						call_peeruri(call), key);
			} else {
				debug("auto answering call\n");
				ua_answer(uag_current(), call);
			}
			ws_send_all(WS_CALLS, webapp_call_json);
			break;

		case UA_EVENT_CALL_ESTABLISHED:
			ua_event_current_set(ua);
			webapp_call_update(call, "Established");
#ifndef SLPLUGIN
#if 0
			/* Auto-Record */
			webapp_options_set("record", "true");
#endif
#endif
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
			webapp_account_status(ua_aor(ua), true);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_UNREGISTERING:
			warning("webapp: unregistering: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_REGISTER_FAIL:
			warning("webapp: Register Fail: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		default:
			break;
	}
}


static int http_port(void)
{
	char path[256] = {0};
	char filename[256] = {0};
	struct sa srv;
	struct sa listen;
	uint32_t port = 0;
	char port_string[10] = {0};
	char bind[256] = "127.0.0.1";
	char file_contents[256];
	int err = 0;
	struct conf *conf_obj = NULL;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/webapp.conf", path) < 0)
		return ENOMEM;

	err = conf_alloc(&conf_obj, filename);
	if (!err) {
		conf_get_str(conf_obj, "sl_listen", bind, sizeof(bind));
		conf_get_u32(conf_obj, "sl_port", &port);
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

	re_snprintf(file_contents, sizeof(file_contents), "sl_listen\t%s\nsl_port\t%s\nsl_auto_answer\t%d\n", bind, port_string, auto_answer);
	webapp_write_file(file_contents, filename);

out:
	if (conf_obj)
		mem_deref(conf_obj);
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
			" Copyright (C) 2013-2020"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#else
	(void)re_fprintf(stderr, "Studio Link Webapp %s - Standalone"
			" Copyright (C) 2013-2020"
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
	webapp_sessions_init();

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

	webapp_sessions_close();
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
