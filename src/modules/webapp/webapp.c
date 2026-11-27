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
#include "assets/fonts.h"
#include "webapp.h"
#include <sllog.h>
#include <slopen.h>

static struct tmr tmr;

#ifdef WIN32
#include <winsock2.h>
#define sock_close closesocket
#else
#include <sys/socket.h>
#include <unistd.h>
#define sock_close close
#endif

static struct http_sock *httpsock = NULL;
struct odict *webapp_calls = NULL;
static char command[100] = {0};
static bool auto_answer = false;
static char provisioning_host[100] = "my.studio.link";

char *webapp_provisioning_host(void)
{
	return provisioning_host;
}

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

	http_reply(conn, scode, reason,
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"Cache-Control: no-cache, no-store, "
			"must-revalidate\r\n"
			"\r\n"
			"%b",
			ctype,
			mb->end,
			mb->buf, mb->end);
out:
	mem_deref(mb);
	return err;
}


/* Append one log file to mb, returns false if it does not exist */
static bool logs_append(struct mbuf *mb, const char *file)
{
	uint8_t buf[4096];
	FILE *f;
	size_t n;

	f = fopen(file, "rb");
	if (!f)
		return false;

	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		if (mbuf_write_mem(mb, buf, n))
			break;
	}

	(void)fclose(f);

	return true;
}


/* Download all kept log files (oldest start first) as one text file */
static void http_logs_reply(struct http_conn *conn)
{
	char path[256];
	char file[512];
	struct mbuf *mb;
	size_t pos;
	int i;

	if (conf_path_get(path, sizeof(path))) {
		http_ereply(conn, 500, "Internal Server Error");
		return;
	}

	mb = mbuf_alloc(65536);
	if (!mb) {
		http_ereply(conn, 500, "Internal Server Error");
		return;
	}

	for (i = SL_LOG_KEEP - 1; i >= 0; i--) {
		re_snprintf(file, sizeof(file), "%s/" SL_LOG_DIR "/"
			    SL_LOG_FILE_FMT, path, (unsigned)i);

		pos = mb->pos;
		mbuf_printf(mb, "===== " SL_LOG_FILE_FMT " =====\n",
			    (unsigned)i);

		if (!logs_append(mb, file)) {
			/* skip header of missing file */
			mb->pos = mb->end = pos;
			continue;
		}

		mbuf_write_str(mb, "\n");
	}

	if (!mb->end)
		mbuf_write_str(mb, "No log files found.\n");

	http_reply(conn, 200, "OK",
		   "Content-Type: text/plain; charset=utf-8\r\n"
		   "Content-Disposition: attachment; "
		   "filename=\"studio-link-logs.txt\"\r\n"
		   "Content-Length: %zu\r\n"
		   "Cache-Control: no-cache, no-store, must-revalidate\r\n"
		   "\r\n"
		   "%b",
		   mb->end, mb->buf, mb->end);

	mem_deref(mb);
}


static void http_req_handler(struct http_conn *conn,
			     const struct http_msg *msg, void *arg)
{
	const struct network *net = baresip_network();
	(void)arg;

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
	if (0 == pl_strcasecmp(&msg->path, "/ws_audio")) {
		webapp_ws_handler(conn, WS_AUDIO, msg, webapp_ws_audio);
#ifndef SLPLUGIN
		ws_send_json(WS_AUDIO, slaudio_get_interfaces());
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

	if (0 == pl_strcasecmp(&msg->path, "/logs.txt")) {
		http_logs_reply(conn);
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
	if (0 == pl_strcasecmp(&msg->path, "/fonts/roboto-mono-400.woff2")) {
		http_sreply(conn, 200, "OK",
				"font/woff2",
				(const char*)dist_fonts_roboto_mono_400_woff2,
				dist_fonts_roboto_mono_400_woff2_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/fonts/roboto-mono-500.woff2")) {
		http_sreply(conn, 200, "OK",
				"font/woff2",
				(const char*)dist_fonts_roboto_mono_500_woff2,
				dist_fonts_roboto_mono_500_woff2_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/fonts/roboto-mono-700.woff2")) {
		http_sreply(conn, 200, "OK",
				"font/woff2",
				(const char*)dist_fonts_roboto_mono_700_woff2,
				dist_fonts_roboto_mono_700_woff2_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/app.css")) {
		http_sreply(conn, 200, "OK",
				"text/css",
				(const char*)dist_app_css,
				dist_app_css_len);
		return;
	}

	http_ereply(conn, 404, "Not found");
}


/**
 * Send a call event to the webui, encoded with odict (JSON escaped, no
 * fixed buffer size: long peer URIs must not be truncated)
 */
static void send_call_msg(const char *callback, const char *k1,
			  const char *v1, const char *k2, const char *v2)
{
	struct odict *o = NULL;

	if (odict_alloc(&o, 4))
		return;

	odict_entry_add(o, "callback", ODICT_STRING, callback);
	if (k1)
		odict_entry_add(o, k1, ODICT_STRING, v1 ? v1 : "");
	if (k2)
		odict_entry_add(o, k2, ODICT_STRING, v2 ? v2 : "");

	ws_send_json(WS_CALLS, o);
	mem_deref(o);
}


static void event_handler(enum bevent_ev ev, struct bevent *event,
			  void *arg)
{
	struct call *call = bevent_get_call(event);
	struct ua *ua = bevent_get_ua(event);
	const char *prm = bevent_get_text(event);
	const char *aor = account_aor(ua_account(ua));
	unsigned key = 0;
	int err;
	(void)arg;

	switch (ev) {
		case BEVENT_SIPSESS_CONN:
			/* baresip 4.x: incoming calls must be accepted by the
			 * application (previously done by the core) */
			if (!webapp_session_available()) {
				(void)sip_treply(NULL, uag_sip(),
						bevent_get_msg(event),
						486, "Max Calls");
				bevent_stop(event);
				return;
			}

			err = ua_accept(uag_find_msg(bevent_get_msg(event)),
					bevent_get_msg(event));
			if (err)
				warning("webapp: could not accept incoming "
					"call (%m)\n", err);
			bevent_stop(event);
			return;

		case BEVENT_CALL_INCOMING:
			if (!webapp_session_available()) {
				ua_hangup(call_get_ua(call), call, 486,
						"Max Calls");
				return;
			}

			key = webapp_call_update(call, "Incoming");
			if (!key)
				return;

			if (!auto_answer) {
				char skey[8];

				re_snprintf(skey, sizeof(skey), "%u", key);
				send_call_msg("INCOMING",
					      "peeruri", call_peeruri(call),
					      "key", skey);
			}
			else {
				debug("auto answering call\n");
				ua_answer(call_get_ua(call), call,
						VIDMODE_OFF);
			}
			break;

		case BEVENT_CALL_ESTABLISHED:
			webapp_call_update(call, "Established");
			break;

		case BEVENT_CALL_CLOSED:
			if (!webapp_session_local_hangup())
				send_call_msg("CLOSED", "message", prm,
					      NULL, NULL);
			webapp_session_delete(NULL, call);
			ws_send_json(WS_CALLS, webapp_calls);
			break;

		case BEVENT_REGISTER_OK:
			webapp_account_status(aor, true);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case BEVENT_UNREGISTERING:
			warning("webapp: unregistering: %s\n", aor);
			webapp_account_status(aor, false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case BEVENT_REGISTER_FAIL:
			warning("webapp: Register Fail: %s\n", aor);
			webapp_account_status(aor, false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

#ifdef SLPLUGIN
		case BEVENT_MODULE: {
			/* host sample rate from sl_plugin_samplerate(),
			 * shown as warning by the web app if not 48 kHz */
			struct pl srate;
			char value[16];

			if (re_regex(prm, str_len(prm),
				     "slplugin,samplerate,[0-9]+", &srate))
				break;

			if (pl_strcpy(&srate, value, sizeof(value)))
				break;

			webapp_options_set("samplerate", value);
			break;
		}
#endif

		default:
			break;
	}
}


/*
 * Listen exclusively on the web app port. tcp_listen() sets SO_REUSEADDR and
 * SO_REUSEPORT, so a second Studio Link process (e.g. a DAW that runs every
 * plugin instance in its own process, or AU and VST3 loaded side by side)
 * could bind the same port and the browser would talk to a random process.
 */
static int http_listen_exclusive(struct http_sock **sockp,
				 const struct sa *laddr)
{
	re_sock_t fd;
	int err;

	fd = socket(sa_af(laddr), SOCK_STREAM, IPPROTO_TCP);
	if (fd == RE_BAD_SOCK)
		return RE_ERRNO_SOCK;

#ifdef WIN32
	{
		/* Windows: SO_REUSEADDR would allow port stealing */
		BOOL on = TRUE;
		(void)setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
				 (const char *)&on, sizeof(on));
	}
#else
	{
		/* restart while old connections are in TIME_WAIT, without
		 * SO_REUSEPORT a bound port of another process fails */
		int on = 1;
		(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on,
				 sizeof(on));
	}
#endif

	if (bind(fd, &laddr->u.sa, laddr->len) < 0) {
		err = RE_ERRNO_SOCK;
		goto out;
	}

	if (listen(fd, SOMAXCONN) < 0) {
		err = RE_ERRNO_SOCK;
		goto out;
	}

	err = net_sockopt_blocking_set(fd, false);
	if (err)
		goto out;

	/* the http socket owns the fd from here */
	return http_listen_fd(sockp, fd, http_req_handler, NULL);

 out:
	(void)sock_close(fd);
	return err;
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
	char file_contents[512];
	int err = 0;
	struct conf *conf_obj = NULL;
	bool persist = true;

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
		conf_get_bool(conf_obj, "sl_auto_answer", &auto_answer);
		conf_get_str(conf_obj, "sl_provisioning_host",
			provisioning_host, sizeof(provisioning_host));
	}

	info("webapp.conf/provisioning_host: %s\n", provisioning_host);

	err = sa_set_str(&srv, bind, port);
	if (err)
		goto out;

	err = http_listen_exclusive(&httpsock, &srv);
	if (err && port) {
		/* port used by another (Studio Link) process: use a new one
		 * for this process, keep the stored port for the other */
		warning("webapp: port %u is in use (%m), maybe another "
			"Studio Link process (DAW plugin sandbox?) -- using "
			"a new port\n", port, err);
		sa_set_port(&srv, 0);
		persist = false;
		err = http_listen_exclusive(&httpsock, &srv);
	}
	if (err)
		goto out;
	tcp_sock_local_get(http_sock_tcp(httpsock), &listen);

	re_snprintf(port_string, sizeof(port_string), "%d",
			sa_port(&listen));

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open http://127.0.0.1:%s/",
			port_string);
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "http://127.0.0.1:%s/",
			port_string);
#else
	re_snprintf(command, sizeof(command), "xdg-open http://127.0.0.1:%s/",
			port_string);
#endif
	info("http listening on ip: %s port: %s\n", bind, port_string);

	re_snprintf(file_contents, sizeof(file_contents),
			"sl_listen\t%s\n"
			"sl_port\t%s\n"
			"sl_auto_answer\t%d\n"
			"sl_provisioning_host\t%s\n",
			bind, port_string, auto_answer, provisioning_host);
	if (persist)
		webapp_write_file(file_contents, filename);

out:
	if (conf_obj)
		mem_deref(conf_obj);
	return err;
}


static void syscmd(void *arg)
{
	(void)arg;
#if defined (WIN32)
	if (sl_shell_open(command))
		warning("webapp: could not open %s\n", command);
#elif !defined (SLBOX)
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
			" Copyright (C) 2013-2026"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#else
	(void)re_fprintf(stderr, "Studio Link Webapp %s - Standalone"
			" Copyright (C) 2013-2026"
			" Sebastian Reimers <studio-link.de>\n", SLVERSION);
#endif

	err = http_port();
	if (err)
		goto out;

	bevent_register(event_handler, NULL);
	webapp_ws_init();
	webapp_options_init(); /* first, verbose SIP trace for REGISTER */
	webapp_accounts_init();
	webapp_contacts_init();
	webapp_ws_meter_init();
	webapp_sessions_init();

	tmr_init(&tmr);
#if defined (SLPLUGIN)
	tmr_start(&tmr, 800, syscmd, NULL);
#else
	syscmd(NULL);
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
	bevent_unregister(event_handler);

	webapp_sessions_close();
	webapp_ws_meter_close();
	webapp_accounts_close();
	webapp_contacts_close();
	webapp_options_close();
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
