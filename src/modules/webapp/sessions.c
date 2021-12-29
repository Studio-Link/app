#include <re.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"
#include <stdlib.h>

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
		ua_connect(webapp_get_main_ua(), &sess->call, NULL,
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
			ua_hangup(webapp_get_main_ua(), sess->call, 0, NULL);
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
				ua_hangup(call_get_ua(sess->call), sess->call, 0, NULL);
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				return err;
			}
		}
		if (call) {
			if (sess->call == call) {
				sess->call = NULL;
				odict_entry_del(webapp_calls, id);
				return err;
			}
		}
	}

	return err;
}


static void session_to_webapp_calls(struct session *sess)
{
	char id[64] = {0};
	struct odict *o;
	int err;

	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return;

	re_snprintf(id, sizeof(id), "%d", sess->track);

	odict_entry_del(webapp_calls, id);
	odict_entry_add(o, "peer", ODICT_STRING, call_peeruri(sess->call));
	odict_entry_add(o, "state", ODICT_STRING, sess->state);
#ifdef SLPLUGIN
	odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch+1);
#else
	odict_entry_add(o, "ch", ODICT_INT, (int64_t)sess->ch);
#endif
	if (sess->chmix == CH_STEREO)
		odict_entry_add(o, "chmix", ODICT_STRING, "Stereo");
	if (sess->chmix == CH_MONO_L)
		odict_entry_add(o, "chmix", ODICT_STRING, "Mono L");
	if (sess->chmix == CH_MONO_R)
		odict_entry_add(o, "chmix", ODICT_STRING, "Mono R");

	odict_entry_add(o, "buffer", ODICT_INT, (int64_t)sess->buffer);
	odict_entry_add(o, "volume", ODICT_DOUBLE, sess->volume);
	odict_entry_add(o, "track", ODICT_INT, (int64_t)sess->track);
	odict_entry_add(webapp_calls, id, ODICT_OBJECT, o);
	mem_deref(o);
}


int8_t webapp_call_update(struct call *call, char *state)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	int8_t track = 0;
	bool new = true;

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
			webapp_jitter_reset(sess);
		}

		if (sess->call != call)
			continue;

		track = sess->track;
		sess->state = state;
		session_to_webapp_calls(sess);
		goto out;
	}

out:
	ws_send_json(WS_CALLS, webapp_calls);
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

		re_snprintf(bufsz, sizeof(bufsz), "%d ", sess->jitter.bufsz);
		re_snprintf(talk, sizeof(talk), "%d ", sess->jitter.talk);
		strcat((char*)buffers, bufsz);
		strcat((char*)talks, talk);

		if (sess->changed) {
			session_to_webapp_calls(sess);
			sess->changed = false;
			ws_send_json(WS_CALLS, webapp_calls);
		}
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
	tmr_start(&tmr_jitter_buffer, 250, jitter_buffer, NULL);
}


void webapp_session_chmix(char *const sess_id)
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
			if (sess->chmix == CH_MONO_R) {
				sess->chmix = CH_STEREO;
			}
			else {
				sess->chmix++;
			}
			session_to_webapp_calls(sess);
			ws_send_json(WS_CALLS, webapp_calls);
			return;
		}
	}
}


void webapp_session_bufferinc(char *const sess_id)
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
			if (sess->buffer >= 300) {
				return;
			}
			sess->buffer += 20;
			sess->jitter.startup = 0;
			session_to_webapp_calls(sess);
			ws_send_json(WS_CALLS, webapp_calls);

			return;
		}
	}
}


void webapp_session_bufferdec(char *const sess_id)
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
			if (sess->buffer <= 40) {
				return;
			}
			sess->buffer -= 20;
			session_to_webapp_calls(sess);
			ws_send_json(WS_CALLS, webapp_calls);

			return;
		}
	}
}


void webapp_session_volume(char *const sess_id, char *const volume)
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
			sess->volume = atof(volume);
			session_to_webapp_calls(sess);
			ws_send_json(WS_CALLS, webapp_calls);

			return;
		}
	}
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
