/**
 * @file effect.c DAW Effect Overlay Plugin
 *
 * Copyright (C) 2015 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <math.h>
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
	unsigned int ch;
	bool run_src;
	bool run_play;
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
	//mem_deref(sess->st_src);
	//mem_deref(sess->st_play);
	warning("DESTRUCT SESSION\n");
}


struct session* effect_session_start(void);
struct session* effect_session_start(void)
{
	struct session *sess;

	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return NULL;

	sess->ch = (unsigned int)list_count(&sessionl) * 2;
	sess->run_play = false;
	sess->run_src = false;

	list_append(&sessionl, &sess->le, sess);
	warning("SESSION STARTED\n");
	return sess;
}


int effect_session_stop(struct session *session);
int effect_session_stop(struct session *session)
{
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


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);

void effect_play(struct session *sess, float* const output0, 
		float* const output1, unsigned long nframes);

void effect_play(struct session *sess, float* const output0, 
		float* const output1, unsigned long nframes)
{

	if (sess->run_play) {
		struct auplay_st *st_play = sess->st_play;

		st_play->wh(st_play->sampv, nframes * 2, st_play->arg);
		sample_move_dS_s16(output0, (char*)st_play->sampv,
				nframes, 4);
		sample_move_dS_s16(output1, (char*)st_play->sampv+2,
				nframes, 4);
	}
	else {
		for (uint32_t pos = 0; pos < nframes; pos++) {
			output0[pos] = 0;
			output1[pos] = 0;
		}
	}
	ws_meter_process(sess->ch+1, (float*)output0, nframes);
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

		if(!sess->run_src) {
			sess->st_src = mem_zalloc(sizeof(*st_src),
					ausrc_destructor);
			if(!sess->st_src)
				return ENOMEM;
			st_src = sess->st_src;
			st_src->sess = sess;
			sess->run_src = true;
			break;
		}
	}
	
	if(!st_src)
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

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;
		
		if(!sess->run_play) {
			sess->st_play = mem_zalloc(sizeof(*st_play),
					auplay_destructor);
			if(!sess->st_play)
				return ENOMEM;
			st_play = sess->st_play;
			st_play->sess = sess;
			sess->run_play = true;
			break;
		}
	}
	
	if(!st_play)
		return EINVAL;

	st_play->run = false;
	st_play->ap  = ap;
	st_play->wh  = wh;
	st_play->arg = arg;
	st_play->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_play->sampv = mem_alloc(10 * st_play->sampc, NULL);
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
