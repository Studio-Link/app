#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "webapp.h"
	
static int16_t *dummy[3840];

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

	if (aubuf_cur_size(rx->aubuf) <= 1920*2*5 && !sess->talk) { /* 100ms */
		debug("webapp_jitter: increase latency %dms\n",
				aubuf_cur_size(rx->aubuf)/3840*20);
		return;
	}

	sess->bufsz = 100.0/76800.0*aubuf_cur_size(rx->aubuf); /* 100% = 400ms */

	wh(sampv, sampc, arg);

	/* Detect Talk spurt */
	for (uint16_t pos = 0; pos < sampc; pos++)
	{
		if (sampv[pos] > max)
			max = sampv[pos];
	}

	sess->talk = false;
	if (max > 400) {
		sess->talk = true;
		debug("talk %dms\n", aubuf_cur_size(rx->aubuf)/3840*20);
	} else {
		debug("webapp_jitter: latency %dms\n",
				aubuf_cur_size(rx->aubuf)/3840*20);
	}

	if (sess->talk)
		return;
#if 1
	/* Reduce latency */
	if (aubuf_cur_size(rx->aubuf) >= 1920*2*15) { //300ms
		debug("webapp_jitter: reduce latency %dms\n",
				aubuf_cur_size(rx->aubuf)/3840*20);
		wh(dummy, sampc, arg);
	}
#endif
}
