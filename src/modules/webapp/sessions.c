#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <slua.h>
#include <string.h>
#include "webapp.h"
#include <stdlib.h>

static struct tmr tmr_stream;
static struct tmr tmr_buffer_report;
static bool streaming = false;
struct list* sl_sessions(void);

static void stream_check(void *arg)
{
	struct session *sess = arg;


	/* reconnect stream if active */
	if (sess->stream && streaming) {
		info("webapp/stream_check: reconnect\n");
		sl_ua_connect(webapp_get_main_ua(), &sess->call,
				"stream@studio.link");
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
			tmr_start(&tmr_stream, 2000, stream_check,
					(void *)sess);
		}

		re_snprintf(id, sizeof(id), "%d", sess->track);

		if (sess_id) {
			if (!str_cmp(id, sess_id)) {
				ua_hangup(call_get_ua(sess->call), sess->call,
						0, NULL);
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
	if (!str_cmp(call_peeruri(call),
			"sip:stream@studio.link;transport=tls")) {
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
			sess->chmix = CH_STEREO;
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


static void stereo_mono_ch_select(int16_t *buf, size_t sampc,
				  enum sess_chmix chmix)
{
	while (sampc > 1) {
		if (chmix == CH_MONO_L)
			buf[1] = buf[0];
		else if (chmix == CH_MONO_R)
			buf[0] = buf[1];

		buf += 2;
		sampc -= 2;
	}
}


/*
 * Read decoded remote audio (S16LE, 48kHz, stereo) from baresip, which
 * handles jitter buffering, and apply the per-call volume and channel mix.
 */
void webapp_session_play_read(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, size_t sampc, void *arg)
{
	struct auframe af;

	auframe_init(&af, AUFMT_S16LE, sampv, sampc, 48000, 2);
	wh(&af, arg);

	if (sess->volume != 1.0) {
		for (size_t i = 0; i < sampc; i++) {
			double v = sampv[i] * sess->volume;
			if (v > 32767)
				v = 32767;
			if (v < -32767)
				v = -32767;
			sampv[i] = (int16_t)v;
		}
	}

	if (sess->chmix != CH_STEREO)
		stereo_mono_ch_select(sampv, sampc, sess->chmix);
}


/* Report the receive buffer level per track (measured by baresip) */
static void buffer_report(void *arg)
{
	struct list *tsession;
	struct session *sess;
	struct le *le;
	char buffers[200] = {0};
	char json[512] = {0};
	size_t n = 0;
	(void)arg;

	tsession = sl_sessions();

	for (le = tsession->head; le; le = le->next) {
		uint64_t ms = 0;
		sess = le->data;

		if (sess->local || sess->stream)
			continue;

		if (sess->call)
			ms = audio_jb_current_value(call_audio(sess->call));

		n += re_snprintf(buffers + n, sizeof(buffers) - n, "%s%llu",
				 n ? " " : "", ms);

		if (sess->changed) {
			session_to_webapp_calls(sess);
			sess->changed = false;
			ws_send_json(WS_CALLS, webapp_calls);
		}
	}

	re_snprintf(json, sizeof(json),
			"{ \"callback\": \"JITTER\", \"buffers\": \"%s\"}",
			buffers);

	ws_send_all(WS_CALLS, json);
	tmr_start(&tmr_buffer_report, 250, buffer_report, NULL);
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
	tmr_init(&tmr_buffer_report);
	tmr_start(&tmr_buffer_report, 2000, buffer_report, NULL);
}


void webapp_sessions_close(void)
{
	tmr_cancel(&tmr_stream);
	tmr_cancel(&tmr_buffer_report);
}
