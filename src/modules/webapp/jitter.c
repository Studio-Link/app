#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"


#define STIME 192 /* 192 = 48 Samples * 2ch * 2 Bytes = 1ms */
#define STARTUP_COUNT 200

static int16_t *dummy[1920];

/* src/audio.c */
struct timestamp_recv {
	uint32_t first;
	uint32_t last;
	bool is_set;
	unsigned num_wraps;
};

struct aurx {
	struct auplay_st *auplay;     /**< Audio Player                    */
	struct auplay_prm auplay_prm; /**< Audio Player parameters         */
	const struct aucodec *ac;     /**< Current audio decoder           */
	struct audec_state *dec;      /**< Audio decoder state (optional)  */
	struct aubuf *aubuf;          /**< Incoming audio buffer           */
	size_t aubuf_maxsz;           /**< Maximum aubuf size in [bytes]   */
	volatile bool aubuf_started;  /**< Aubuf was started flag          */
	struct auresamp resamp;       /**< Optional resampler for DSP      */
	struct list filtl;            /**< Audio filters in decoding order */
	char *module;                 /**< Audio player module name        */
	char *device;                 /**< Audio player device name        */
	void *sampv;                  /**< Sample buffer                   */
	int16_t *sampv_rs;            /**< Sample buffer for resampler     */
	uint32_t ptime;               /**< Packet time for receiving       */
	int pt;                       /**< Payload type for incoming RTP   */
	double level_last;            /**< Last audio level value [dBov]   */
	bool level_set;               /**< True if level_last is set       */
	enum aufmt play_fmt;          /**< Sample format for audio playback*/
	enum aufmt dec_fmt;           /**< Sample format for decoder       */
	bool need_conv;               /**< Sample format conversion needed */
	struct timestamp_recv ts_recv;/**< Receive timestamp state         */
	size_t last_sampc;

	struct {
		uint64_t aubuf_overrun;
		uint64_t aubuf_underrun;
		uint64_t n_discard;
	} stats;
};


static void stereo_mono_ch_select(int16_t *buf, size_t sampc, enum sess_chmix chmix)
{
	while (sampc--) {
		if (chmix == CH_MONO_L) {
			buf[1] = buf[0];
		}
		if (chmix == CH_MONO_R) {
			buf[0] = buf[1];
		}
		buf += 2;
	}
}


void webapp_jitter_reset(struct session *sess)
{
	sess->jitter.startup = 0;
	sess->jitter.max = 0;
	sess->jitter.max_l = 0;
	sess->jitter.max_r = 0;
	sess->chmix = CH_STEREO;
}


void webapp_jitter(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, unsigned int sampc, void *arg)
{
	struct aurx *rx = arg;
	size_t frames = sampc / 2;

	int16_t max_l = 0, max_r = 0, max = 0;
	size_t bufsz = 0, pos = 0;

	/* set threshold to 500ms */
	int16_t silence_threshold = 48000/sampc;

	bufsz = aubuf_cur_size(rx->aubuf);

	if (bufsz <= (sess->buffer * STIME) &&
	    sess->jitter.startup < STARTUP_COUNT) {
		memset(sampv, 0, sampc * sizeof(int16_t));
		return;
	}

	if (bufsz <= (size_t)(sess->buffer * 0.8) * STIME && !sess->jitter.talk) {
		if (sess->jitter.delay_count >= 960) {
			/* only slightly increase buffer */
			sess->jitter.talk = true;
			sess->jitter.silence_count = 0;
			sess->jitter.delay_count = 0;
		}
		memset(sampv, 0, sampc * sizeof(int16_t));
		sess->jitter.delay_count += frames;
		return;
	}

	sess->jitter.bufsz = bufsz/STIME;
	wh(sampv, sampc, arg);

	/* GAIN */
	pos = 0;
	for (uint16_t frame = 0; frame < sampc; frame++)
	{
		sampv[pos] = (int16_t)(sampv[pos] * sess->volume);
		++pos;
	}

	if (sess->jitter.startup == STARTUP_COUNT) {
		if (sess->jitter.max_l > 400 && sess->jitter.max_r < 400) {
			sess->chmix = CH_MONO_L;
			sess->changed = true;
		}
		if (sess->jitter.max_r > 400 && sess->jitter.max_l < 400) {
			sess->chmix = CH_MONO_R;
			sess->changed = true;
		}
		sess->jitter.startup = STARTUP_COUNT+1;
	}

	/* Mono chmix */
	if (sess->chmix != CH_STEREO) {
		stereo_mono_ch_select(sampv, sampc, sess->chmix);
	}

	/* Detect Talk spurt and channel usage */
	pos = 0;
	for (uint16_t frame = 0; frame < frames; frame++)
	{
		if (sampv[pos] > max_l)
			max_l = sampv[pos];
		if (sampv[pos+1] > max_r)
			max_r = sampv[pos+1];
		pos += 2;
	}

	if (max_l > max_r)
		max = max_l;
	else
		max = max_r;

	sess->jitter.max = (sess->jitter.max + max) / 2;
	if (max_l > sess->jitter.max_l)
		sess->jitter.max_l = max_l;
	if (max_r > sess->jitter.max_r)
 	       sess->jitter.max_r = max_r;

	if (sess->jitter.startup <= STARTUP_COUNT)
		sess->jitter.startup++;

	if (sess->jitter.max < 400) {
		if (sess->jitter.silence_count < silence_threshold) {
			sess->jitter.silence_count++;
		}
		else {
			sess->jitter.talk = false;
		}
	}
	else {
		sess->jitter.talk = true;
		sess->jitter.silence_count = 0;
	}

	if (sess->jitter.talk)
		return;

	bufsz = aubuf_cur_size(rx->aubuf);
	/* Reduce latency */
	if (bufsz >= (sess->buffer + 100) * STIME) {
		/* only slightly reduce buffer */
		sess->jitter.talk = true;
		sess->jitter.silence_count = 0;
		wh(dummy, 1920, arg);
	}
}
