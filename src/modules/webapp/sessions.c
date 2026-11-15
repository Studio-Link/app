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
		int err = sl_ua_connect(webapp_get_main_ua(), &sess->call,
					"stream@studio.link");
		if (err)
			webapp_call_error("stream@studio.link", err);
	}
}


/**
 * Report a failed outgoing call to the webui (shown as notification)
 *
 * @param dial Dial string or SIP URI
 * @param err  Error code
 */
void webapp_call_error(const char *dial, int err)
{
	struct odict *o = NULL;
	char msg[256];

	if (odict_alloc(&o, 4))
		return;

	re_snprintf(msg, sizeof(msg), "%s: %m", dial, err);

	odict_entry_add(o, "callback", ODICT_STRING, "CLOSED");
	odict_entry_add(o, "message", ODICT_STRING, msg);
	ws_send_json(WS_CALLS, o);

	mem_deref(o);
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


/* Add the network/buffer statistics of one call as object to the array */
static void buffer_report_track(struct odict *arr, const struct session *sess)
{
	const struct rtcp_stats *rtcp;
	struct jbuf_stat jb;
	struct stream *strm;
	struct audio *au;
	struct odict *o;
	char key[8];

	if (odict_alloc(&o, 16))
		return;

	au   = sess->call ? call_audio(sess->call) : NULL;
	strm = au ? audio_strm(au) : NULL;

	odict_entry_add(o, "track", ODICT_INT, (int64_t)sess->track);
	odict_entry_add(o, "active", ODICT_BOOL, strm != NULL);

	if (!strm)
		goto out;

	/* receive audio buffer (aubuf) latency */
	odict_entry_add(o, "buffer", ODICT_INT,
			(int64_t)audio_jb_current_value(au));

	/* RTP bitrate [bit/s] and packet counters */
	odict_entry_add(o, "rx_bitrate", ODICT_INT,
			(int64_t)stream_metric_get_rx_bitrate(strm));
	odict_entry_add(o, "tx_bitrate", ODICT_INT,
			(int64_t)stream_metric_get_tx_bitrate(strm));
	odict_entry_add(o, "rx_packets", ODICT_INT,
			(int64_t)stream_metric_get_rx_n_packets(strm));
	odict_entry_add(o, "tx_packets", ODICT_INT,
			(int64_t)stream_metric_get_tx_n_packets(strm));

	/* RTCP: jitter [us], lost packets (cumulative), round-trip [us] */
	rtcp = stream_rtcp_stats(strm);
	if (rtcp) {
		odict_entry_add(o, "rx_lost", ODICT_INT,
				(int64_t)rtcp->rx.lost);
		odict_entry_add(o, "rx_jitter", ODICT_INT,
				(int64_t)rtcp->rx.jit);
		odict_entry_add(o, "tx_lost", ODICT_INT,
				(int64_t)rtcp->tx.lost);
		odict_entry_add(o, "tx_jitter", ODICT_INT,
				(int64_t)rtcp->tx.jit);
		odict_entry_add(o, "rtt", ODICT_INT, (int64_t)rtcp->rtt);
	}

	/* RTP jitter buffer */
	if (!stream_jbuf_stats(strm, &jb)) {
		struct odict *oj;

		if (odict_alloc(&oj, 16))
			goto out;

		odict_entry_add(oj, "delay", ODICT_INT, (int64_t)jb.c_delay);
		odict_entry_add(oj, "jitter", ODICT_INT,
				(int64_t)jb.c_jitter);
		odict_entry_add(oj, "skew", ODICT_INT, (int64_t)jb.c_skew);
		odict_entry_add(oj, "packets", ODICT_INT,
				(int64_t)jb.c_packets);
		odict_entry_add(oj, "put", ODICT_INT, (int64_t)jb.n_put);
		odict_entry_add(oj, "lost", ODICT_INT, (int64_t)jb.n_lost);
		odict_entry_add(oj, "late", ODICT_INT, (int64_t)jb.n_late);
		odict_entry_add(oj, "late_lost", ODICT_INT,
				(int64_t)jb.n_late_lost);
		odict_entry_add(oj, "oos", ODICT_INT, (int64_t)jb.n_oos);
		odict_entry_add(oj, "dups", ODICT_INT, (int64_t)jb.n_dups);
		odict_entry_add(oj, "overflow", ODICT_INT,
				(int64_t)jb.n_overflow);
		odict_entry_add(oj, "flush", ODICT_INT, (int64_t)jb.n_flush);

		odict_entry_add(o, "jbuf", ODICT_OBJECT, oj);
		mem_deref(oj);
	}

out:
	re_snprintf(key, sizeof(key), "%u", (unsigned)odict_count(arr, false));
	odict_entry_add(arr, key, ODICT_OBJECT, o);
	mem_deref(o);
}


/* Report the network and buffer statistics per track (measured by baresip) */
static void buffer_report(void *arg)
{
	struct list *tsession;
	struct session *sess;
	struct odict *o = NULL, *arr = NULL;
	struct le *le;
	(void)arg;

	tsession = sl_sessions();

	if (odict_alloc(&o, 4) || odict_alloc(&arr, 16))
		goto out;

	for (le = tsession->head; le; le = le->next) {
		sess = le->data;

		if (sess->local || sess->stream)
			continue;

		buffer_report_track(arr, sess);

		if (sess->changed) {
			session_to_webapp_calls(sess);
			sess->changed = false;
			ws_send_json(WS_CALLS, webapp_calls);
		}
	}

	odict_entry_add(o, "callback", ODICT_STRING, "STATS");
	odict_entry_add(o, "tracks", ODICT_ARRAY, arr);

	ws_send_json(WS_CALLS, o);

out:
	mem_deref(arr);
	mem_deref(o);
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
