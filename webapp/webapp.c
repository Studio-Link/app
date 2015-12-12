/**
 * @file webapp.c Webserver UI module
 *
 * Copyright (C) 2015 studio-link.de Sebastian Reimers
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "assets/index_html.h"
#include "assets/js.h"
#include "assets/css.h"
#include "assets/fonts.h"
#include "webapp.h"

#define SLVERSION "15.12.0-alpha"

static struct http_sock *httpsock = NULL;
enum webapp_call_state webapp_call_status = WS_CALL_OFF;
static char webapp_call_json[150] = {0};

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

	/*
	 * Dynamic Requests
	 */

	if (0 == pl_strcasecmp(&msg->path, "/ws_baresip")) {
		webapp_ws_handler(conn, WS_BARESIP, msg, webapp_ws_baresip);
		ws_send_json(WS_BARESIP, webapp_accounts_get());
		if (webapp_call_status != WS_CALL_OFF)
			ws_send_all(WS_BARESIP, webapp_call_json);
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
	if (0 == pl_strcasecmp(&msg->path, "/ws_meter")) {
		webapp_ws_handler(conn, WS_METER, msg, webapp_ws_meter);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/status.json")) {
		char ipv4[15];
		char json[255];

		sa_ntop(net_laddr_af(AF_INET), ipv4, sizeof(ipv4));

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
				(const char*)index_min_html,
				index_min_html_len);

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/version")) {
		http_sreply(conn, 200, "OK",
				"text/html",
				(const char*)SLVERSION,
				sizeof(SLVERSION));

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/js/all.js")) {
		http_sreply(conn, 200, "OK",
				"application/javascript",
				(const char*)js_all_js,
				js_all_js_len);

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/css/all.css")) {
		http_sreply(conn, 200, "OK",
				"text/css",
				(const char*)css_all_css,
				css_all_css_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.eot")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_eot,
			    fonts_glyphicons_halflings_regular_eot_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.svg")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_svg,
			    fonts_glyphicons_halflings_regular_svg_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.ttf")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_ttf,
			    fonts_glyphicons_halflings_regular_ttf_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.woff")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_woff,
			    fonts_glyphicons_halflings_regular_woff_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.woff2")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_woff2,
			    fonts_glyphicons_halflings_regular_woff2_len);
		return;
	}


	http_ereply(conn, 404, "Not found");
}


static void ua_event_current_set(struct ua *ua) {
			uag_current_set(ua);
			webapp_account_current();
			ws_send_json(WS_BARESIP, webapp_accounts_get());
}


static void ua_event_handler(struct ua *ua, enum ua_event ev,
		struct call *call, const char *prm, void *arg)
{

	switch (ev) {
		case UA_EVENT_CALL_INCOMING:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"INCOMING\",\
					\"peeruri\": \"%s\" }",
					call_peeruri(call));
			ws_send_all(WS_BARESIP, webapp_call_json);
			webapp_call_status = WS_CALL_RINGING;
			break;

		case UA_EVENT_CALL_ESTABLISHED:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"ESTABLISHED\",\
					\"peeruri\": \"%s\" }",
					call_peeruri(call));
			ws_send_all(WS_BARESIP, webapp_call_json);
			webapp_call_status = WS_CALL_ON;
			break;

		case UA_EVENT_CALL_CLOSED:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"CLOSED\",\
					\"message\": \"%s\" }", prm);
			ws_send_all(WS_BARESIP, webapp_call_json);
			webapp_call_status = WS_CALL_OFF;
			break;

		case UA_EVENT_REGISTER_OK:
			warning("Register OK: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), true);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_UNREGISTERING:
		case UA_EVENT_REGISTER_FAIL:
			warning("Register Fail: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		default:
			break;
	}
}


static int module_init(void)
{
	int err = 0;
	struct sa srv;
	struct sa listen;
	char command[100] = {0};

#ifdef SLPLUGIN
	(void)re_fprintf(stderr, "Studio Link Webapp v%s - Effect Plugin"
			" Copyright (C) 2015"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#else
	(void)re_fprintf(stderr, "Studio Link Webapp v%s - Standalone"
			" Copyright (C) 2015"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#endif

	err |= sa_set_str(&srv, "127.0.0.1", 0);

	err = http_listen(&httpsock, &srv, http_req_handler, NULL);
	if (err)
		goto out;
	tcp_sock_local_get(http_sock_tcp(httpsock), &listen);

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open http://localhost:%d/",
			sa_port(&listen));
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "start http://localhost:%d/",
			sa_port(&listen));
#else
	re_snprintf(command, sizeof(command), "xdg-open http://localhost:%d/",
			sa_port(&listen));
#endif
	info("http listening on port: %d\n", sa_port(&listen));

	uag_event_register(ua_event_handler, NULL);

	webapp_ws_init();
	webapp_accounts_init();
	webapp_contacts_init();
	webapp_chat_init();
	webapp_ws_meter_init();

#ifndef SLBOX
	system(command);
#endif

out:
	if (err) {
		mem_deref(httpsock);
	}
	return err;
}


static int module_close(void)
{
	uag_event_unregister(ua_event_handler);

	webapp_ws_meter_close();
	webapp_accounts_close();
	webapp_contacts_close();
	webapp_chat_close();
	webapp_ws_close();

	mem_deref(httpsock);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(webapp) = {
	"webapp",
	"application",
	module_init,
	module_close,
};
