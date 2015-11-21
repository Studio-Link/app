/**
 * @file jack.c  Jack sound driver
 *
 * Copyright (C) 2014 studio-connect.de
 */
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <math.h>

#define SAMPLE_16BIT_SCALING  32767.0f
#define SAMPLE_16BIT_MAX  32767
#define SAMPLE_16BIT_MIN  -32767
#define SAMPLE_16BIT_MAX_F  32767.0f
#define SAMPLE_16BIT_MIN_F  -32767.0f

#define NORMALIZED_FLOAT_MIN -1.0f
#define NORMALIZED_FLOAT_MAX  1.0f

#define f_round(f) lrintf(f)

#define float_16(s, d)\
	if ((s) <= NORMALIZED_FLOAT_MIN) {\
		(d) = SAMPLE_16BIT_MIN;\
	} else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_16BIT_MAX;\
	} else {\
		(d) = f_round ((s) * SAMPLE_16BIT_SCALING);\
	}


static struct ausrc *ausrc;
static struct auplay *auplay;

jack_port_t *input_port0;
jack_port_t *input_port1;
jack_port_t *output_port0;
jack_port_t *output_port1;

uint16_t spike_detect_threshold = 1920 * 7;

struct aurx {
	struct auplay_st *auplay;     /**< Audio Player                    */
	const struct aucodec *ac;     /**< Current audio decoder           */
	struct audec_state *dec;      /**< Audio decoder state (optional)  */
	struct aubuf *aubuf;          /**< Incoming audio buffer           */
	struct auresamp resamp;       /**< Optional resampler for DSP      */
	struct list filtl;            /**< Audio filters in decoding order */
	char device[64];              /**< Audio player device name        */
	int16_t *sampv;               /**< Sample buffer                   */
	int16_t *sampv_rs;            /**< Sample buffer for resampler     */
	uint32_t ptime;               /**< Packet time for receiving       */
	int pt;                       /**< Payload type for incoming RTP   */
	int pt_tel;                   /**< Event payload type - receive    */
};

struct ausrc_st {
	struct ausrc *as;      /* inheritance */
	ausrc_read_h *rh;
	int16_t *sampv;
        size_t sampc;
	void *arg;
	volatile bool ready;
	struct ausrc_prm prm;
	unsigned ch;
	jack_client_t *src_client;
};

struct auplay_st {
	struct auplay *ap;      /* inheritance */
	auplay_write_h *wh;
	int16_t *sampv;
        size_t sampc;
	void *arg;
	volatile bool ready;
	struct auplay_prm prm;
	unsigned ch;
	jack_client_t *play_client;
	int16_t aubuf_last_size;
};


void sample_move_d16_sS (char *dst,  jack_default_audio_sample_t *src, unsigned long nsamples, unsigned long dst_skip)	
{
	while (nsamples--) {
		float_16 (*src, *((int16_t*) dst));
		dst += dst_skip;
		src++;
	}
}

void sample_move_dS_s16 (jack_default_audio_sample_t *dst, char *src, unsigned long nsamples, unsigned long src_skip) 
{
	/* ALERT: signed sign-extension portability !!! */
	const jack_default_audio_sample_t scaling = 1.0/SAMPLE_16BIT_SCALING;
	while (nsamples--) {
		*dst = (*((short *) src)) * scaling;
		dst++;
		src += src_skip;
	}
}	


int src_callback (jack_nframes_t nframes, void *arg);

int src_callback (jack_nframes_t nframes, void *arg)
{
	struct ausrc_st *st = arg;

	jack_default_audio_sample_t *in0 = 
		(jack_default_audio_sample_t *) 
		jack_port_get_buffer (input_port0, nframes);
	jack_default_audio_sample_t *in1 = 
		(jack_default_audio_sample_t *) 
		jack_port_get_buffer (input_port1, nframes);
//	formats[format].jack_to_soundcard( outbuf + format[formats].sample_size * chn, resampbuf, src.output_frames_gen, num_channels*format[formats].sample_size, NULL);
	sample_move_d16_sS((char*)st->sampv, in0, (double)nframes, 4);
	sample_move_d16_sS((char*)st->sampv+2, in1, (double)nframes, 4);

	st->rh(st->sampv, nframes * 2, st->arg);
        //warning ("nframes=%d;sampc=%d\n", nframes, st->sampc);

	return 0;
}


int play_callback (jack_nframes_t nframes, void *arg);

int play_callback (jack_nframes_t nframes, void *arg)
{
	struct auplay_st *st = arg;
	struct aurx *rx = st->arg;
	jack_default_audio_sample_t *out0 = 
		(jack_default_audio_sample_t *) 
		jack_port_get_buffer (output_port0, nframes);
	jack_default_audio_sample_t *out1 = 
		(jack_default_audio_sample_t *) 
		jack_port_get_buffer (output_port1, nframes);

	// Workaround spike detection
	// @TODO: This should be better placed in src/stream.c with PLC trigger
	if ((aubuf_cur_size(rx->aubuf) - st->aubuf_last_size) > spike_detect_threshold) {
		while(aubuf_cur_size(rx->aubuf) > 1920) {
			st->wh(st->sampv, nframes * 2, st->arg);
		}
		warning("playback buffer network spike detected, samples removed\n");
	}

	st->wh(st->sampv, nframes * 2, st->arg);

	sample_move_dS_s16(out0, (char*)st->sampv, (double)nframes, 4);
	sample_move_dS_s16(out1, (char*)st->sampv+2, (double)nframes, 4);
	st->aubuf_last_size = aubuf_cur_size(rx->aubuf);

	return 0;
}

static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;

	jack_client_close (st->src_client);
	mem_deref(st->sampv);
	mem_deref(st->as);

}

static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;

	jack_client_close (st->play_client);
	mem_deref(st->sampv);
	mem_deref(st->ap);
}

static int jack_src_alloc(struct ausrc_st **stp, struct ausrc *as,
		struct media_ctx **ctx,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st;
	jack_status_t status;
	(void)ctx;
	(void)errh;
	(void)device;

	if (!stp || !as || !prm)
		return EINVAL;

	st = mem_zalloc(sizeof(*st), ausrc_destructor);
	if (!st)
		return ENOMEM;

	st->prm = *prm;
	st->as  = mem_ref(as);
	st->rh  = rh;
	st->arg = arg;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;
//	st->sampv = mem_alloc(2 * st->sampc, NULL);
	st->sampv = mem_alloc(10 * st->sampc, NULL);
	if (!st->sampv)
		return ENOMEM;

	if ((st->src_client = jack_client_open ("sip-src", JackNoStartServer, &status)) == 0) {
		warning ("JACK server not running?\n");
		return 1;
	}
	if (st->src_client == NULL) {
		warning ("jack_client_open() failed, "
				"status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			warning ("Unable to connect to JACK server\n");
		}
	}
	jack_set_process_callback(st->src_client, src_callback, st);
	input_port0 = jack_port_register(st->src_client, "in0", 
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	input_port1 = jack_port_register(st->src_client, "in1", 
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

	if (jack_activate(st->src_client)) {
		warning("cannot activate jack src_client");
		return 1;
	}

	*stp = st;
	return 0;
}

static int jack_play_alloc(struct auplay_st **stp, struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st;
	jack_status_t status;

	(void)device;

	if (!stp || !ap || !prm)
		return EINVAL;

	st = mem_zalloc(sizeof(*st), auplay_destructor);
	if (!st)
		return ENOMEM;

	st->prm = *prm;
	st->ap  = mem_ref(ap);
	st->wh  = wh;
	st->arg = arg;
	st->aubuf_last_size = 0;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st->sampv = mem_alloc(10 * st->sampc, NULL);
	if (!st->sampv)
		return ENOMEM;

	if ((st->play_client = jack_client_open ("sip-play", JackNoStartServer, &status)) == 0) {
		warning ("JACK server not running?\n");
		return 1;
	}
	if (st->play_client == NULL) {
		warning ("jack_client_open() failed, "
				"status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			warning ("Unable to connect to JACK server\n");
		}
	}

	jack_set_process_callback(st->play_client, play_callback, st);
	output_port0 = jack_port_register(st->play_client, "out0", 
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	
	output_port1 = jack_port_register(st->play_client, "out1", 
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate(st->play_client)) {
		warning("cannot activate jack play_client");
		return 1;
	}
	*stp = st;
	return 0;
}


static int jack_init(void)
{
	int err;

	err  = ausrc_register(&ausrc, "jack", jack_src_alloc);
	err |= auplay_register(&auplay, "jack", jack_play_alloc);

	return err;
}


static int jack_close(void)
{
	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	return 0;
}


const struct mod_export DECL_EXPORTS(jack) = {
	"jack",
	"sound",
	jack_init,
	jack_close
};
