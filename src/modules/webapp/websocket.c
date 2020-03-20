#include <re.h>
#include <baresip.h>
#include "webapp.h"

static struct list ws_srv_conns;
static struct websock *ws;
static struct tmr tmr_exit;


int webapp_ws_handler(struct http_conn *conn, enum ws_type type,
		const struct http_msg *msg, websock_recv_h *recvh)
{
	struct webapp *webapp;
	webapp = mem_zalloc(sizeof(*webapp), NULL);
	websock_accept(&webapp->wc_srv, ws, conn, msg,
			0, recvh,
			srv_websock_close_handler, webapp);
	list_append(&ws_srv_conns, &webapp->le, webapp);
	debug("websocket created: %p\n", webapp->wc_srv);
	webapp->ws_type = type;

	tmr_cancel(&tmr_exit);

	return 0;
}


void ws_send_all(enum ws_type ws_type, char *str) {
	struct le *le;
	for (le = list_head(&ws_srv_conns); le; le = le->next) {
		struct webapp *webapp = le->data;
		if (webapp->ws_type == ws_type) {
			websock_send(webapp->wc_srv, WEBSOCK_TEXT,
					"%s", str);
		}
	}
}


void ws_send_all_b(enum ws_type ws_type, struct mbuf *mb) {
	struct le *le;
	for (le = list_head(&ws_srv_conns); le; le = le->next) {
		struct webapp *webapp = le->data;
		if (webapp->ws_type == ws_type) {
			websock_send(webapp->wc_srv, WEBSOCK_TEXT,
					"%b", mb->buf, mb->end);
		}
	}
}


static int print_handler(const char *p, size_t size, void *arg)
{
	        return mbuf_write_mem(arg, (uint8_t *)p, size);
}


void ws_send_json(enum ws_type type, const struct odict *o)
{
	struct re_printf pf;
	struct mbuf *mbr = mbuf_alloc(4096);
	pf.vph = print_handler;
	pf.arg = mbr;

	json_encode_odict(&pf, o);
	ws_send_all_b(type, mbr);
	mem_deref(mbr);
}


static void exit_baresip(void *arg) {
		ua_stop_all(false);
}


void srv_websock_close_handler(int err, void *arg)
{
	(void)err;
	struct webapp *webapp_p = arg;
	debug("websocket closed: %p\n", webapp_p->wc_srv);
	mem_deref(webapp_p->wc_srv);
	list_unlink(&webapp_p->le);
	mem_deref(webapp_p);
#ifndef SLPLUGIN
	/* close studio link if browser is closed */
	if (!webapp_active_calls()) {
		tmr_start(&tmr_exit, 800, exit_baresip, NULL);
	}
#endif
}


static void websock_shutdown_handler(void *arg)
{
	info("websocket shutdown\n");
}


void webapp_ws_init(void)
{
	int err;
	tmr_init(&tmr_exit);

	list_init(&ws_srv_conns);
	err = websock_alloc(&ws, websock_shutdown_handler, NULL);
	if (err) {
		mem_deref(ws);
		return;
	}
}


void webapp_ws_close(void)
{
	info("webapp_ws_close\n");
	tmr_cancel(&tmr_exit);
	struct le *le;
	if (!list_isempty(&ws_srv_conns)) {
		for (le = list_head(&ws_srv_conns); le; le = le->next) {
			if (le->data) {
				struct webapp *webapp = le->data;
				mem_deref(webapp->wc_srv);
			}
		}
	}

	mem_deref(ws);
	list_flush(&ws_srv_conns);
}
