#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <string.h>
#include "webapp.h"


#define STIME 192 /* 192 = 48 Samples * 2ch * 2 Bytes = 1ms */

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


void webapp_jitter(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, unsigned int sampc, void *arg)
{
	int16_t max = 0;
	struct aurx *rx = arg;
	size_t bufsz = 0;

	/* set threshold to 500ms */
	int16_t silence_threshold = 48000/sampc;

	bufsz = aubuf_cur_size(rx->aubuf);

	if (bufsz <= 7680 && !sess->talk) { /* >=40ms (1920*2*2) */
		debug("webapp_jitter: increase latency %dms\n",
				bufsz/STIME);
		memset(sampv, 0, sampc * sizeof(int16_t));
		return;
	}

	sess->bufsz = bufsz/STIME;

	wh(sampv, sampc, arg);

	/* Detect Talk spurt */
	for (uint16_t pos = 0; pos < sampc; pos++)
	{
		if (sampv[pos] > max)
			max = sampv[pos];
	}
	sess->jb_max = (sess->jb_max + max) / 2;

	if (sess->jb_max < 400) {
		if (sess->silence_count < silence_threshold) {
			sess->silence_count++;
		}
		else {
			sess->talk = false;
		}
	}
	else {
		sess->talk = true;
		sess->silence_count = 0;
	}

	if (sess->talk)
		return;

	bufsz = aubuf_cur_size(rx->aubuf);
	/* Reduce latency <= 120ms (1920*2*6) */
	if (bufsz >= 23040) {
		debug("webapp_jitter: reduce latency %dms\n",
				bufsz/STIME);
		wh(dummy, 1920, arg);
	}
}
