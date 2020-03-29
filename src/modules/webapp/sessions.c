#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"

static struct tmr tmr_stream;
static struct tmr tmr_jitter_buffer;
static bool streaming = false;
struct list* sl_sessions(void);

static void stream_check(void *arg)
{
	struct session *sess = arg;

	/* reconnect stream if active */
	if (sess->stream && streaming) {
		info("webapp/stream_check: reconnect\n");
		ua_connect(uag_current(), &sess->call, NULL,
				"stream@studio.link", VIDMODE_OFF);
	}
}


bool webapp_session_available(void)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		if (sess->stream)
			continue;

		if (!sess->call)
			return true;
	}

	return false;
}


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
			streaming = false;
			ua_hangup(uag_current(), sess->call, 0, NULL);
			sess->call = NULL;

			return err;
		}
	}

	return err;
}


struct call* webapp_session_get_call(char * const sess_id)
{
	char id[64] = {0};
	struct list *tsession;
	struct session *sess;
	struct le *le;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		re_snprintf(id, sizeof(id), "%d", sess->track);

		if (!str_cmp(id, sess_id)) {
			return sess->call;
		}
	}

	return NULL;
}


int webapp_session_delete(char * const sess_id, struct call *call)
{
	char id[64] = {0};
	int err = 0;
	struct list *tsession;
	struct session *sess;
	struct le *le;
	int active_calls = 0;

	if (!sess_id && !call)
		return EINVAL;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local)
			continue;

		/* reconnect stream if active */
		if (sess->stream && streaming) {
			tmr_start(&tmr_stream, 2000, stream_check, (void *)sess);
		}

		re_snprintf(id, sizeof(id), "%d", sess->track);

		if (sess_id) {
			if (!str_cmp(id, sess_id)) {
				ua_hangup(uag_current(), sess->call, 0, NULL);
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				break;
			}
		}
		if (call) {
			if (sess->call == call) {
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				break;
			}
		}
	}

	/* Calculate active calls */
	for (le = tsession->head; le; le = le->next) {
		sess = le->data;
		if (sess->call)
			++active_calls;
	}

#ifndef SLPLUGIN
#if 0
	/* Auto-Record off if no call*/
	if (!active_calls)
		webapp_options_set("record", "false");
#endif
#endif

	return err;
}


int8_t webapp_call_update(struct call *call, char *state)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	struct odict *o;
	char id[64] = {0};
	int err = 0;
	int8_t track = 0;
	bool new = true; 


	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return 0;

	tsession = sl_sessions();

#ifndef SLPLUGIN
	if (!str_cmp(call_peeruri(call), "sip:stream@studio.link;transport=tls")) {
		for (le = tsession->head; le; le = le->next) {
			sess = le->data;

			if (sess->stream) {
				sess->call = call;
				streaming = true;
				return sess->track;
			}
		}
	}
#endif

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local || sess->stream)
			continue;

		if (sess->call == call) {
			new = false;
			debug("webapp: session update: %d\n", sess->ch);
		}
	}

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local || sess->stream)
			continue;

		if (new && !sess->call) {
			sess->call = call;
			new = false;
		}

		if (sess->call != call)
			continue;

		track = sess->track;
		re_snprintf(id, sizeof(id), "%d", sess->track);

		odict_entry_del(webapp_calls, id);
		odict_entry_add(o, "peer", ODICT_STRING, call_peeruri(call));
		odict_entry_add(o, "state", ODICT_STRING, state);
#ifdef SLPLUGIN
		odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch+1);
#else
		odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch);
#endif
		odict_entry_add(o, "track", ODICT_INT, (int64_t)sess->track);
		odict_entry_add(webapp_calls, id, ODICT_OBJECT, o);
	}

	ws_send_json(WS_CALLS, webapp_calls);
	mem_deref(o);
	return track;
}


static void jitter_buffer(void *arg)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	char bufsz[10] = {0};
	char talk[3] = {0};
	char buffers[200] = {0};
	char talks[200] = {0};
	char json[1024] = {0};
	int n;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local || sess->stream)
			continue;
	
		re_snprintf(bufsz, sizeof(bufsz), "%d ", sess->bufsz);
		re_snprintf(talk, sizeof(talk), "%d ", sess->talk);
		strcat((char*)buffers, bufsz);
		strcat((char*)talks, talk);
	}
	n = strlen(buffers); 
	buffers[n-1] = '\0'; /* remove trailing space */

	n = strlen(talks); 
	talks[n-1] = '\0'; /* remove trailing space */

	re_snprintf(json, sizeof(json),
			"{ \"callback\": \"JITTER\",\
			\"buffers\": \"%s\",\
			\"talks\": \"%s\"}",
			buffers, talks);

	ws_send_all(WS_CALLS, json);
	tmr_start(&tmr_jitter_buffer, 200, jitter_buffer, NULL);
}


void webapp_sessions_init(void)
{
	tmr_init(&tmr_stream);
	tmr_init(&tmr_jitter_buffer);
	tmr_start(&tmr_jitter_buffer, 2000, jitter_buffer, NULL);
}


void webapp_sessions_close(void)
{
	tmr_cancel(&tmr_stream);
	tmr_cancel(&tmr_jitter_buffer);
}
