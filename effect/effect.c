/**
 * @file effect.c DAW Effect Overlay Plugin
 *
 * Copyright (C) 2016 studio-link.de
 */
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>


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
	}\
	else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_16BIT_MAX;\
	}\
	else {\
		(d) = f_round ((s) * SAMPLE_16BIT_SCALING);\
	}


enum {
	MAX_CHANNELS = 6
};

struct auplay_st {
	const struct auplay *ap;  /* pointer to base-class (inheritance) */
	pthread_t thread;
	bool run;
	void *write;
	int16_t *sampv;
	size_t sampc;
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
	char *device;
	struct session *sess;
};

struct ausrc_st {
	const struct ausrc *as;  /* pointer to base-class (inheritance) */
	pthread_t thread;
	bool run;
	void *read;
	int16_t *sampv;
	size_t sampc;
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	char *device;
	struct session *sess;
};

struct session {
	struct le le;
	struct ausrc_st *st_src;
	struct auplay_st *st_play;
	int32_t *dstmix;
	int16_t *mix;
	uint8_t ch;
	bool run_src;
	bool run_play;
	bool mix_lock_read;
	bool mix_lock_write;
	bool run_auto_mix;
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;

static bool channels[MAX_CHANNELS] = {false};


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
#if 0
	mem_deref(sess->st_src);
	mem_deref(sess->st_play);
#endif
	warning("DESTRUCT SESSION\n");
}


static int calc_channel(struct session *sess)
{
	for (uint8_t pos = 0; pos < MAX_CHANNELS; pos++) {
		if (!channels[pos]) {
			channels[pos] = true;
			sess->ch = pos * 2;
			return 0;
		}
	}

	return 1;
}


struct session* effect_session_start(void);
struct session* effect_session_start(void)
{
	struct session *sess;

	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return NULL;

	if (calc_channel(sess)) {
		/* Max Channels reached */
		sess->ch = MAX_CHANNELS * 2;
	}

	sess->run_play = false;
	sess->run_src = false;

	list_append(&sessionl, &sess->le, sess);
	warning("SESSION STARTED\n");

	return sess;
}


int effect_session_stop(struct session *session);
int effect_session_stop(struct session *session)
{
	uint8_t pos = session->ch / 2;
	channels[pos] = false;
	mem_deref(session);
	return (int)list_count(&sessionl);
}


static void sample_move_d16_sS(char *dst, float *src, unsigned long nsamples,
		unsigned long dst_skip)
{
	while (nsamples--) {
		float_16 (*src, *((int16_t*) dst));
		dst += dst_skip;
		++src;
	}
}


static void sample_move_dS_s16(float *dst, char *src, unsigned long nsamples,
		unsigned long src_skip)
{
	/* ALERT: signed sign-extension portability !!! */
	const float scaling = 1.0/SAMPLE_16BIT_SCALING;
	while (nsamples--) {
		*dst = (*((short *) src)) * scaling;
		++dst;
		src += src_skip;
	}
}


static void mix_save(int16_t *src, int16_t *dst, unsigned long nsamples)
{
	while (nsamples--) {
		*dst = *src;
		++dst;
		++src;
	}
}


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes)
{

	if (!sess->run_play)
		goto out;

	struct auplay_st *st_play = sess->st_play;

	st_play->wh(st_play->sampv, nframes * 2, st_play->arg);
	for (unsigned i = 1; i<100; ++i) {
		if (!sess->mix_lock_read)
		{
			sess->mix_lock_write = true;
			mix_save(st_play->sampv, sess->mix,
					nframes * 2);
			sess->mix_lock_write = false;
			if (i > 1)
				warning("Write Busy loop runs %d\n", i);
			break;
		}
		usleep(1);
	}
	sample_move_dS_s16(output0, (char*)st_play->sampv,
			nframes, 4);
	sample_move_dS_s16(output1, (char*)st_play->sampv+2,
			nframes, 4);

out:
	ws_meter_process(sess->ch+1, (float*)output0, nframes);
}


void effect_bypass(struct session *sess,
		float* const output0,
		float* const output1,
		const float* const input0,
		const float* const input1,
		unsigned long nframes);

void effect_bypass(struct session *sess,
		float* const output0,
		float* const output1,
		const float* const input0,
		const float* const input1,
		unsigned long nframes)
{
	if (sess->run_play)
		return;

	if (sess->run_auto_mix) {
		for (uint32_t pos = 0; pos < nframes; pos++) {
			output0[pos] = input0[pos];
			output1[pos] = input1[pos];
		}
	}
	else {
		for (uint32_t pos = 0; pos < nframes; pos++) {
			output0[pos] = 0;
			output1[pos] = 0;
		}
	}
}


static void mix_n_minus_1(struct session *sess, int16_t *dst,
		unsigned long nsamples)
{
	struct le *le;
	int32_t *dstmixv = sess->dstmix;
	int16_t *dstv = dst;
	unsigned active = 0;

	for (le = sessionl.head; le; le = le->next) {
		struct session *msess = le->data;
		int16_t *mixv = msess->mix;

		if (msess->run_play && msess != sess) {
			for (unsigned n = 1; n<100; ++n) {
				if (!sess->mix_lock_write)
				{
					sess->mix_lock_read = true;
					for (unsigned i = 0; i < nsamples; ++i) {
						*dstmixv = *dstmixv + *mixv;
						++mixv;
						++dstmixv;
					}
					sess->mix_lock_read = false;
					if (n > 1)
						warning("Read Busy loop runs %d\n", n);
					break;
				}
				usleep(1);
			}
			++active;
		}
	}

	if (active) {
		dstmixv = sess->dstmix;
		for (unsigned i = 0; i < nsamples; ++i) {
			*dstmixv = *dstmixv + *dstv;
			++dstv;
			++dstmixv;
		}

		dstmixv = sess->dstmix;
		for (unsigned i = 0; i < nsamples; ++i) {
			if (*dstmixv > SAMPLE_16BIT_MAX)
				*dstmixv = SAMPLE_16BIT_MAX;
			if (*dstmixv < SAMPLE_16BIT_MIN)
				*dstmixv = SAMPLE_16BIT_MIN;
			*dst = *dstmixv;
			*dstmixv = 0;
			++dst;
			++dstmixv;
		}
	}
}

void effect_src(struct session *sess, const float* const input0,
		const float* const input1, unsigned long nframes);

void effect_src(struct session *sess, const float* const input0,
		const float* const input1, unsigned long nframes)
{

	if (sess->run_src) {
		struct ausrc_st *st_src = sess->st_src;

		sample_move_d16_sS((char*)st_src->sampv, (float*)input0,
				nframes, 4);
		sample_move_d16_sS((char*)st_src->sampv+2, (float*)input1,
				nframes, 4);
		if (sess->run_auto_mix)
			mix_n_minus_1(sess, st_src->sampv, nframes * 2);
		st_src->rh(st_src->sampv, nframes * 2, st_src->arg);
	}
	ws_meter_process(sess->ch, (float*)input0, nframes);
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	warning("DESTRUCT ausrc\n");
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	warning("DESTRUCT auplay\n");
	sess->run_play = false;
	sys_msleep(20);
	mem_deref(st->sampv);
	mem_deref(sess->mix);
	mem_deref(sess->dstmix);
}

char* webapp_options_getv(char *key);

static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct media_ctx **ctx,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	(void)ctx;
	(void)errh;
	(void)device;
	struct ausrc_st *st_src = NULL;
	struct le *le;
	int err = 0;
	char *automixv = webapp_options_getv("auto-mix-n-1");

	if (!stp || !as || !prm)
		return EINVAL;

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!sess->run_src) {
			sess->st_src = mem_zalloc(sizeof(*st_src),
					ausrc_destructor);
			if (!sess->st_src)
				return ENOMEM;
			st_src = sess->st_src;
			st_src->sess = sess;
			sess->run_src = true;

			if (0 == str_cmp(automixv, "true")) {
				sess->run_auto_mix = true;
				warning("auto mix enabled\n");
			}
			else {
				sess->run_auto_mix = false;
				warning("auto mix disabled\n");
			}
			break;
		}
	}

	if (!st_src)
		return EINVAL;

	st_src->run = false;
	st_src->as  = as;
	st_src->rh  = rh;
	st_src->arg = arg;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_alloc(10 * st_src->sampc, NULL);
	if (!st_src->sampv) {
		err = ENOMEM;
		goto out;
	}

out:
	if (err)
		mem_deref(st_src);
	else
		st_src->run = true;
	*stp = st_src;

	return err;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st_play = NULL;
	struct le *le;
	int err = 0;
	if (!stp || !ap || !prm)
		return EINVAL;
	size_t sampc = prm->srate * prm->ch * prm->ptime / 1000;

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!sess->run_play) {
			sess->st_play = mem_zalloc(sizeof(*st_play),
					auplay_destructor);
			if (!sess->st_play)
				return ENOMEM;
			sess->mix = mem_zalloc(10 * sampc, NULL);
			if (!sess->mix)
				return ENOMEM;
			sess->dstmix = mem_zalloc(20 * sampc, NULL);
			if (!sess->mix)
				return ENOMEM;
			st_play = sess->st_play;
			st_play->sess = sess;
			sess->run_play = true;
			break;
		}
	}

	if (!st_play)
		return EINVAL;

	st_play->run = false;
	st_play->ap  = ap;
	st_play->wh  = wh;
	st_play->arg = arg;
	st_play->sampc = sampc;
	st_play->sampv = mem_alloc(10 * sampc, NULL);
	if (!st_play->sampv) {
		err = ENOMEM;
		goto out;
	}

out:
	if (err)
		mem_deref(st_play);
	else
		st_play->run = true;
	*stp = st_play;

	return err;

}


static int effect_init(void)
{
	int err;
	err  = ausrc_register(&ausrc, "effect", src_alloc);
	err |= auplay_register(&auplay, "effect", play_alloc);

	return err;
}


static int effect_close(void)
{
	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	return 0;
}


const struct mod_export DECL_EXPORTS(effect) = {
	"effect",
	"sound",
	effect_init,
	effect_close
};
