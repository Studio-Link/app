/**
 * @file effect.c DAW Effect Overlay Plugin
 *
 * Copyright (C) 2013-2020 studio-link.de
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
	MAX_CHANNELS = 10
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
	uint32_t trev;
	uint32_t prev;
	int32_t *dstmix;
	uint8_t ch;
	bool run_src;
	bool run_play;
	struct lock *plock;
	bool run_auto_mix;
	bool bypass;
	struct call *call;
	bool stream; /* only for standalone */
	bool local; /* only for standalone */
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;

static bool channels[MAX_CHANNELS] = {false};

static bool bypass = false;

static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
	mem_deref(sess->plock);
#if 0
	mem_deref(sess->st_src);
	mem_deref(sess->st_play);
#endif
	warning("DESTRUCT SESSION\n");
}


static void calc_channel(struct session *sess)
{
	for (uint8_t pos = 0; pos < MAX_CHANNELS; pos++) {
		if (!channels[pos]) {
			channels[pos] = true;
			sess->ch = pos * 2;
			return;
		}
	}

	/* Max Channels reached */
	sess->ch = MAX_CHANNELS * 2;
}


char* webapp_options_getv(char *key);
struct session* effect_session_start(void);
struct session* effect_session_start(void)
{
	struct session *sess;



	return sess;
}


int effect_session_stop(struct session *session);
int effect_session_stop(struct session *session)
{
#if 0
	uint8_t pos = session->ch / 2;
	channels[pos] = false;
	mem_deref(session);
	return (int)list_count(&sessionl);
#endif
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


static void play_process(struct session *sess, unsigned long nframes)
{
	struct auplay_st *st_play = sess->st_play;

	st_play->wh(st_play->sampv, nframes, st_play->arg);
	++sess->prev;
}


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes)
{

	if (!sess->run_play)
		return;

	struct auplay_st *st_play = sess->st_play;

	lock_write_get(sess->plock);
	if (sess->trev > sess->prev)
		play_process(sess, nframes*2);

	sample_move_dS_s16(output0, (char*)st_play->sampv,
			nframes, 4);
	sample_move_dS_s16(output1, (char*)st_play->sampv+2,
			nframes, 4);
	lock_rel(sess->plock);

	ws_meter_process(sess->ch+1, (float*)output0, nframes);
	++sess->trev;
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
	struct le *le;
	struct session *msess;
	int8_t counter;

	if (sess->run_play)
		return;

	if (sess->run_auto_mix && sess->bypass) {
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


	for (le = sessionl.head; le; le = le->next) {
		msess = le->data;
		if (msess != sess) {
			counter = msess->trev - sess->trev;
			if (counter > 1) {
				sess->trev = msess->trev;
				sess->prev = msess->prev;
				warning("sync thread %d\n", counter);
				return;
			}
		}
	}

	++sess->trev;
	++sess->prev;
}


static void mix_n_minus_1(struct session *sess, int16_t *dst,
		unsigned long nsamples)
{
	struct le *le;
	int32_t *dstmixv;
	int16_t *dstv = dst;
	int16_t *mixv;
	unsigned active = 0;

	for (le = sessionl.head; le; le = le->next) {
		struct session *msess = le->data;

		if (msess->run_play && msess != sess) {
			mixv = msess->st_play->sampv;
			dstmixv = sess->dstmix;

			lock_write_get(msess->plock);
			if (sess->trev > msess->prev)
				play_process(msess, nsamples);
			for (unsigned n = 0; n < nsamples; n++) {
				*dstmixv = *dstmixv + *mixv;
				++mixv;
				++dstmixv;
			}
			lock_rel(msess->plock);

			++active;
		}
	}

	if (active) {
		dstmixv = sess->dstmix;
		for (unsigned i = 0; i < nsamples; i++) {
			*dstmixv = *dstmixv + *dstv;
			++dstv;
			++dstmixv;
		}

		dstmixv = sess->dstmix;
		for (unsigned i = 0; i < nsamples; i++) {
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
	//@fixme
	//if (sess->dstmix)
	//	mem_deref(sess->dstmix);
}


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
			sess->dstmix = mem_zalloc(20 * sampc, NULL);
			if (!sess->dstmix)
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


void effect_set_bypass(bool value);
void effect_set_bypass(bool value)
{
	struct le *le;
	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;
		sess->bypass = value;
	}
	bypass = value;
}


static int effect_init(void)
{
	int err;
	err  = ausrc_register(&ausrc, baresip_ausrcl(), "effect", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(), "effect", play_alloc);
	struct session *sess;


	for (uint32_t cnt = 0; cnt < MAX_CHANNELS; cnt++)
	{
		sess = mem_zalloc(sizeof(*sess), sess_destruct);
		if (!sess)
			return ENOMEM;

		calc_channel(sess);

		sess->run_play = false;
		sess->run_src = false;
		sess->bypass = bypass;
		sess->trev = 0;
		sess->prev = 0;
		sess->call = NULL;
		lock_alloc(&sess->plock);

		sess->run_auto_mix = true;
		sess->local = false;
		sess->stream = false;
		list_append(&sessionl, &sess->le, sess);
	}

	return err;
}


static int effect_close(void)
{
	struct le *le;
	struct session *sess;

	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	for (le = sessionl.head; le;)
	{
		sess = le->data;
		le = le->next;
		mem_deref(sess);
	}

	return 0;
}


const struct mod_export DECL_EXPORTS(effect) = {
	"effect",
	"sound",
	effect_init,
	effect_close
};
