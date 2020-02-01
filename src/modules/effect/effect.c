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
	int32_t *dstmix;
	int8_t ch;
	bool run_src;
	bool run_play;
	bool run_auto_mix;
	bool bypass;
	struct call *call;
	bool stream; /* only for standalone */
	bool local; /* only for standalone */
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;

static bool bypass = false;
static bool ready = false;

static uint8_t run_sessions = 0;
static uint8_t active_sessions = 0;
static struct lock *lock;

static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
	info("destruct session\n");
}


struct list* sl_sessions(void);
struct list* sl_sessions(void)
{
	return &sessionl;
}


struct session* effect_session_start(void);
struct session* effect_session_start(void)
{
	struct session *sess;
	struct le *le;
	int pos = 0;

	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;

		if (sess->ch == -1) {
			sess->ch = pos * 2;
			lock_write_get(lock);
			++active_sessions;
			lock_rel(lock);
			return sess;
		}
		pos++;
	}

	return NULL;
}


int effect_session_stop(struct session *session);
int effect_session_stop(struct session *session)
{
	struct session *sess;
	struct le *le;
	int count = 0;

	if (!session)
		return MAX_CHANNELS;

	session->ch = -1;

	lock_write_get(lock);
	--active_sessions;
	lock_rel(lock);


	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;

		if (sess->ch > -1) {
			count++;
		}
	}
	warning("effect: debug session_stop count: %d", count);

	return count;
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


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes);

void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes)
{
	if(!sess) 
		return;

	if (!sess->run_play) {
		for (uint32_t pos = 0; pos < nframes; pos++) {
			output0[pos] = 0;
			output1[pos] = 0;
		}
		return;
	}

	struct auplay_st *st_play = sess->st_play;
	sample_move_dS_s16(output0, (char*)st_play->sampv,
			nframes, 4);
	sample_move_dS_s16(output1, (char*)st_play->sampv+2,
			nframes, 4);

	if (sess->ch > -1)
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
	if (!sess)
		return;

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
}


static void mix_n_minus_1(struct session *sess, unsigned long samples)
{
	struct ausrc_st *st_src = sess->st_src;
	struct auplay_st *st_play;
	struct auplay_st *mst_play;
	struct le *le;
	struct le *mle;
	int msessplay = 0;
	int sessplay = 0;

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (!sess->run_play)
			continue;

		st_play = sess->st_play;
		st_play->wh(st_play->sampv, samples, st_play->arg);
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		msessplay = 0;

		if (!sess->run_play)
			continue;

		st_play = sess->st_play;

		/* mix n-1 */
		for (mle = sessionl.head; mle; mle = mle->next)
		{
			struct session *msess = mle->data;

			if (!msess->run_play || msess == sess)
			{
				continue;
			}
			mst_play = msess->st_play;

			for (uint16_t pos = 0; pos < samples; pos++)
			{
				if (msessplay < 1)
				{
					sess->dstmix[pos] =
						mst_play->sampv[pos];
				}
				else
				{
					sess->dstmix[pos] =
						mst_play->sampv[pos] +
						sess->dstmix[pos];
				}
			}
			++msessplay;
			++sessplay;
		}
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (sess->run_src)
		{
			st_src = sess->st_src;

			for (uint16_t pos = 0; pos < samples; pos++)
			{
				/* ignore on last call the dstmix */
				if (sessplay) {
					st_src->sampv[pos] =
						st_src->sampv[pos] +
						sess->dstmix[pos];
				}
			}

			st_src->rh(st_src->sampv, samples, st_src->arg);
		}
	}

}


void effect_src(struct session *sess, const float* const input0,
		const float* const input1, unsigned long nframes);

void effect_src(struct session *sess, const float* const input0,
		const float* const input1, unsigned long nframes)
{
	if (!sess)
		return;

	if (sess->ch > -1)
		ws_meter_process(sess->ch, (float*)input0, nframes);

	lock_write_get(lock);
	if(!ready) {
		mix_n_minus_1(sess, nframes * 2);
		ready = true;
	}

	++run_sessions;

	if (run_sessions >= active_sessions)
	{
		ready = false;
		run_sessions = 0;
	}
	lock_rel(lock);

	if (!sess->run_src)
	{
		return;
	}
	struct ausrc_st *st_src = sess->st_src;

	sample_move_d16_sS((char *)st_src->sampv, (float *)input0,
					   nframes, 4);
	sample_move_d16_sS((char *)st_src->sampv + 2, (float *)input1,
					   nframes, 4);
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	info("effect: ausrc\n");
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	info("effect: destruct auplay\n");
	sess->run_play = false;
	sys_msleep(20);
	mem_deref(st->sampv);
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

		if (sess->ch == -1)
			continue;

		if (!sess->run_src && sess->call) {
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

		if (sess->ch == -1)
			continue;

		if (!sess->run_play && sess->call) {
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
	lock_alloc(&lock);

	for (uint32_t cnt = 0; cnt < MAX_CHANNELS; cnt++)
	{
		sess = mem_zalloc(sizeof(*sess), sess_destruct);
		if (!sess)
			return ENOMEM;

		sess->run_play = false;
		sess->run_src = false;
		sess->bypass = false;
		sess->ch = -1;
		sess->call = NULL;

		sess->run_auto_mix = true;
		sess->stream = false;
		sess->local = false;
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
	lock = mem_deref(lock);

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
