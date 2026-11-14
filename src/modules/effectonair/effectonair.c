/**
 * @file effectonair.c DAW EffectLive Overlay Plugin
 *
 * The DAW input is sent to all active calls (one audio source per call),
 * nothing is played back.
 *
 * Copyright (C) 2013-2026 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <math.h>
#include <slplugin.h>


#define SAMPLE_16BIT_SCALING  32767.0f
#define SAMPLE_16BIT_MAX  32767
#define SAMPLE_16BIT_MIN  -32767

#define NORMALIZED_FLOAT_MIN -1.0f
#define NORMALIZED_FLOAT_MAX  1.0f

#define f_round(f) lrintf(f)

#define float_16(s, d)\
	if ((s) <= NORMALIZED_FLOAT_MIN) {\
		(d) = SAMPLE_16BIT_MIN;\
	}\
	else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_16BIT_MAX;\
	}\
	else {\
		(d) = f_round ((s) * SAMPLE_16BIT_SCALING);\
	}


enum {
	MAX_SAMPC = SL_PLUGIN_MAX_FRAMES * 2, /* stereo */
};

struct auplay_st {
	const struct auplay *ap;  /* pointer to base-class (inheritance) */
};

struct ausrc_st {
	const struct ausrc *as;  /* pointer to base-class (inheritance) */
	struct le le;
	int16_t *sampv;
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	uint64_t frames;
};

static struct ausrc *ausrc;
static struct auplay *auplay;

/* Active sources (one per call), guarded by src_lock (audio thread vs.
 * re thread) */
static struct list srcl;
static mtx_t *src_lock;


static void sample_move_d16_sS(char *dst, const float *src,
			       unsigned long nsamples, unsigned long dst_skip)
{
	while (nsamples--) {
		float_16 (*src, *((int16_t*) dst));
		dst += dst_skip;
		++src;
	}
}


static void src_process(struct ausrc_st *st, const float *const input0,
			const float *const input1, unsigned long nframes)
{
	struct auframe af;

	sample_move_d16_sS((char*)st->sampv, input0, nframes, 4);
	sample_move_d16_sS((char*)st->sampv+2, input1, nframes, 4);

	auframe_init(&af, st->prm.fmt, st->sampv, nframes * 2,
		     st->prm.srate, st->prm.ch);
	af.timestamp = st->frames * AUDIO_TIMEBASE / st->prm.srate;

	st->frames += nframes;

	st->rh(&af, st->arg);
}


void effectlive_src(const float *const input0, const float *const input1,
		    unsigned long nframes)
{
	struct le *le;

	if (!src_lock)
		return;

	mtx_lock(src_lock);

	for (le = srcl.head; le; le = le->next) {
		struct ausrc_st *st = le->data;
		unsigned long done = 0;

		/* split blocks bigger than the sample buffer */
		while (done < nframes) {
			unsigned long n = nframes - done;
			if (n > SL_PLUGIN_MAX_FRAMES)
				n = SL_PLUGIN_MAX_FRAMES;

			src_process(st, input0 + done, input1 + done, n);
			done += n;
		}
	}

	mtx_unlock(src_lock);
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;

	/* wait for a running effectlive_src() */
	mtx_lock(src_lock);
	list_unlink(&st->le);
	mtx_unlock(src_lock);

	mem_deref(st->sampv);
}


static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st;
	(void)errh;
	(void)device;

	if (!stp || !as || !prm || !rh)
		return EINVAL;

	if (prm->fmt != AUFMT_S16LE || prm->ch != 2 || !prm->srate) {
		warning("effectonair: unsupported source format %s/%u/%u\n",
			aufmt_name(prm->fmt), prm->srate, prm->ch);
		return ENOTSUP;
	}

	st = mem_zalloc(sizeof(*st), NULL);
	if (!st)
		return ENOMEM;

	st->as  = as;
	st->rh  = rh;
	st->arg = arg;
	st->prm = *prm;

	st->sampv = mem_zalloc(MAX_SAMPC * sizeof(int16_t), NULL);
	if (!st->sampv) {
		mem_deref(st);
		return ENOMEM;
	}

	mem_destructor(st, ausrc_destructor);

	mtx_lock(src_lock);
	list_append(&srcl, &st->le, st);
	mtx_unlock(src_lock);

	*stp = st;

	return 0;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st;
	(void)device;
	(void)wh;
	(void)arg;

	if (!stp || !ap || !prm)
		return EINVAL;

	/* OnAir only sends, the player is a dummy */
	st = mem_zalloc(sizeof(*st), NULL);
	if (!st)
		return ENOMEM;

	st->ap = ap;
	*stp = st;

	return 0;
}


static int effect_init(void)
{
	int err;

	list_init(&srcl);

	err = mutex_alloc(&src_lock);
	if (err)
		return err;

	err  = ausrc_register(&ausrc, baresip_ausrcl(), "effectonair",
			      src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(), "effectonair",
			       play_alloc);

	return err;
}


static int effect_close(void)
{
	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	/* all sources are destroyed with their calls before */
	src_lock = mem_deref(src_lock);

	return 0;
}


const struct mod_export DECL_EXPORTS(effectonair) = {
	"effectonair",
	"sound",
	effect_init,
	effect_close
};
