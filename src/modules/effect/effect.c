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
#include <webapp.h>
#include <slplugin.h>


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
	MAX_CHANNELS = 10,
	MAX_SAMPC    = SL_PLUGIN_MAX_FRAMES * 2, /* stereo */
};

static mtx_t *sync_lock;

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
	uint64_t frames;
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;

static bool bypass = false;


/**
 * Get the timer jiffies in microseconds
 *
 * @return Jiffies in [us]
 */

static uint64_t sl_jiffies(void)
{
	uint64_t jfs;

#if defined(WIN32)
	LARGE_INTEGER li;
	static LARGE_INTEGER freq;

	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);

	QueryPerformanceCounter(&li);
	li.QuadPart *= 1000000;
	li.QuadPart /= freq.QuadPart;

	jfs = li.QuadPart;
#else
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	jfs  = (long)now.tv_sec * (uint64_t)1000000;
	jfs += now.tv_nsec/1000;
#endif

	return jfs;
}


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
	mem_deref(sess->dstmix);
	info("effect: destruct session\n");
}


/* The sample buffers hold max. SL_PLUGIN_MAX_FRAMES, the wrappers split
 * bigger host blocks. Never write behind the buffers if they don't. */
static unsigned long frames_limit(unsigned long nframes)
{
	if (nframes > SL_PLUGIN_MAX_FRAMES)
		return SL_PLUGIN_MAX_FRAMES;

	return nframes;
}


struct list* sl_sessions(void);
struct list* sl_sessions(void)
{
	return &sessionl;
}


struct session* effect_session_start(void)
{
	struct session *sess = NULL;
	struct le *le;
	int pos = 0;

	/* called from the host thread */
	re_thread_enter();

	for (le = sessionl.head; le; le = le->next) {
		struct session *s = le->data;

		if (s->ch == -1) {
			s->ch = pos * 2;
			s->effect_ready = false;
			s->primary = false;
			sess = s;
			break;
		}
		++pos;
	}

	re_thread_leave();

	if (!sess)
		warning("effect: max. sessions (%d) reached\n", MAX_CHANNELS);

	return sess;
}


int effect_session_stop(struct session *session)
{
	struct session *sess;
	struct le *le;
	int count = 0;
	char sess_id[64] = {0};

	if (!session)
		return MAX_CHANNELS;

	/* called from the host thread */
	re_thread_enter();

	session->ch = -1;

	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;

		if (sess->ch > -1) {
			++count;
		}
	}

	re_snprintf(sess_id, sizeof(sess_id), "%d", session->track);
	webapp_session_delete(sess_id, NULL);

	re_thread_leave();

	info("effect: debug session_stop count: %d\n", count);

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


static void play_process(struct session *sess, unsigned long nframes)
{
	struct auplay_st *st_play = sess->st_play;

	webapp_session_play_read(sess, st_play->sampv,
			st_play->wh, nframes*2, st_play->arg);

	sess->effect_ready = true;
}


void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes)
{
	(void)output0;
	(void)output1;

	/* check max sessions reached*/
	if (!sess)
		return;

	if (!sess->run_play)
		return;

	play_process(sess, frames_limit(nframes));
}


static bool check_sessions_stopped(struct session *sess)
{
	struct session *msess;
	struct le *le;

	for (le = sessionl.head; le; le = le->next) {
		msess = le->data;

		if (msess->run_play && msess != sess && msess->ch != -1) {
			if (msess->effect_ready)
				return false;
		}
	}

	return true;
}


void effect_bypass(struct session *sess,
		float* const output0,
		float* const output1,
		const float* const input0,
		const float* const input1,
		unsigned long nframes)
{
	uint64_t loop_start;
	struct auplay_st *st_play;
	unsigned long nlimit = frames_limit(nframes);

	/* check max sessions reached*/
	if (!sess)
		return;

	if (sess->primary) {
		/* wait until all threads finished */
		loop_start = sl_jiffies();
		while (1) {
			if (check_sessions_stopped(sess))
				break;
			if ((sl_jiffies() - loop_start) > 100)
				break;
		}
		sess->primary = false;
	}

	sess->effect_ready = false;

	if (sess->run_play) {
		st_play = sess->st_play;
		sample_move_dS_s16(output0, (char*)st_play->sampv,
				nlimit, 4);
		sample_move_dS_s16(output1, (char*)st_play->sampv+2,
				nlimit, 4);
		for (unsigned long pos = nlimit; pos < nframes; pos++) {
			output0[pos] = 0;
			output1[pos] = 0;
		}
		ws_meter_process(sess->ch+1, (float*)output0, nlimit);
		return;
	}

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


static void promote_primary(struct session *sess)
{
	struct session *msess;
	struct le *le;

	/* check for another active primary */
	for (le = sessionl.head; le; le = le->next) {
		msess = le->data;
		if (msess->primary) {
			sess->primary = false;
			return;
		}
	}

	sess->primary = true;
}


static bool check_sessions_ready(struct session *sess)
{
	struct session *msess;
	struct le *le;

	for (le = sessionl.head; le; le = le->next) {
		msess = le->data;

		if (msess->run_play && msess != sess && msess->ch != -1) {
			if (!msess->effect_ready)
				return false;
		}
	}

	return true;
}


static void mix_n_minus_1(struct session *sess, int16_t *dst,
		unsigned long nframes)
{
	struct le *le;
	int32_t *dstmixv;
	int16_t *dstv = dst;
	int16_t *mixv;
	unsigned active = 0;
	unsigned long nsamples = nframes * 2;
	unsigned i;
	uint64_t loop_start;

	loop_start = sl_jiffies();
	mtx_lock(sync_lock);
	promote_primary(sess);

	while (1) {
		if (!sess->primary)
			break;
		if (check_sessions_ready(sess))
			break;
		if ((sl_jiffies() - loop_start) > 800)
			break;
	}

	mtx_unlock(sync_lock);

	for (le = sessionl.head; le; le = le->next) {
		struct session *msess = le->data;

		if (msess->run_play && msess != sess) {
			mixv = msess->st_play->sampv;
			dstmixv = sess->dstmix;

			for (i = 0; i < nsamples; i++) {
				*dstmixv = *dstmixv + *mixv;
				++mixv;
				++dstmixv;
			}

			++active;
		}
	}

	if (active) {
		dstmixv = sess->dstmix;
		for (i = 0; i < nsamples; i++) {
			*dstmixv = *dstmixv + *dstv;
			++dstv;
			++dstmixv;
		}

		dstmixv = sess->dstmix;
		for (i = 0; i < nsamples; i++) {
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
		const float* const input1, unsigned long nframes)
{
	struct auframe af;

	/* check max sessions reached*/
	if (!sess)
		return;

	nframes = frames_limit(nframes);

	if (sess->run_src && sess->run_play) {
		struct ausrc_st *st_src = sess->st_src;

		sample_move_d16_sS((char*)st_src->sampv, (float*)input0,
				nframes, 4);
		sample_move_d16_sS((char*)st_src->sampv+2, (float*)input1,
				nframes, 4);
		if (sess->run_auto_mix)
			mix_n_minus_1(sess, st_src->sampv, nframes);

		auframe_init(&af, st_src->prm.fmt, st_src->sampv,
			     nframes * 2, st_src->prm.srate, st_src->prm.ch);
		af.timestamp = st_src->frames * AUDIO_TIMEBASE /
			       st_src->prm.srate;

		st_src->frames += nframes;

		st_src->rh(&af, st_src->arg);
	}
	ws_meter_process(sess->ch, (float*)input0, nframes);

}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	sess->run_play = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	(void)errh;
	(void)device;
	struct ausrc_st *st_src = NULL;
	struct le *le;
	int err = 0;

	if (!stp || !as || !prm)
		return EINVAL;

	if (prm->fmt != AUFMT_S16LE || prm->ch != 2 || !prm->srate) {
		warning("effect: unsupported source format %s/%u/%u\n",
			aufmt_name(prm->fmt), prm->srate, prm->ch);
		return ENOTSUP;
	}

	struct session *sess = NULL;
	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;
		if (sess->ch == -1)
			continue;

		if (!sess->run_src && sess->call) {
			st_src = mem_zalloc(sizeof(*st_src), NULL);
			if (!st_src)
				return ENOMEM;
			st_src->sess = sess;
			break;
		}
	}

	if (!st_src || !sess)
		return EINVAL;

	st_src->run = false;
	st_src->as  = as;
	st_src->rh  = rh;
	st_src->arg = arg;
	st_src->prm = *prm;

	st_src->sampc = MAX_SAMPC;
	st_src->sampv = mem_zalloc(st_src->sampc * sizeof(int16_t), NULL);
	if (!st_src->sampv) {
		err = ENOMEM;
		goto out;
	}

out:
	if (err) {
		mem_deref(st_src);
		return err;
	}

	/* the destructor resets the session, only set it if in use */
	mem_destructor(st_src, ausrc_destructor);
	sess->st_src = st_src;

	st_src->run = true;
	sess->run_src = true;
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
	(void)device;

	if (!stp || !ap || !prm)
		return EINVAL;

	if (prm->fmt != AUFMT_S16LE || prm->ch != 2 || !prm->srate) {
		warning("effect: unsupported player format %s/%u/%u\n",
			aufmt_name(prm->fmt), prm->srate, prm->ch);
		return ENOTSUP;
	}

	struct session *sess = NULL;
	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;
		if (sess->ch == -1)
			continue;

		if (!sess->run_play && sess->call) {
			st_play = mem_zalloc(sizeof(*st_play), NULL);
			if (!st_play)
				return ENOMEM;
			st_play->sess = sess;
			break;
		}
	}

	if (!st_play || !sess)
		return EINVAL;

	st_play->run = false;
	st_play->ap  = ap;
	st_play->wh  = wh;
	st_play->arg = arg;
	st_play->prm = *prm;
	st_play->sampc = MAX_SAMPC;
	st_play->sampv = mem_zalloc(st_play->sampc * sizeof(int16_t), NULL);
	if (!st_play->sampv) {
		err = ENOMEM;
		goto out;
	}

out:
	if (err) {
		mem_deref(st_play);
		return err;
	}

	/* the destructor resets the session, only set it if in use */
	mem_destructor(st_play, auplay_destructor);
	sess->st_play = st_play;

	st_play->run = true;
	sess->run_play = true;
	*stp = st_play;

	return err;

}


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
	struct session *sess;

	err  = ausrc_register(&ausrc, baresip_ausrcl(), "effect", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(), "effect",
			play_alloc);

	err |= mutex_alloc(&sync_lock);
	if (err)
		return err;

	for (uint32_t cnt = 0; cnt < MAX_CHANNELS; cnt++)
	{
		sess = mem_zalloc(sizeof(*sess), sess_destruct);
		if (!sess)
			return ENOMEM;

		sess->dstmix = mem_zalloc(MAX_SAMPC * sizeof(int32_t), NULL);
		if (!sess->dstmix) {
			mem_deref(sess);
			return ENOMEM;
		}

		sess->run_play = false;
		sess->run_src = false;
		sess->bypass = false;
		sess->ch = -1;
		sess->call = NULL;

		sess->run_auto_mix = true;
		sess->stream = false;
		sess->local = false;
		sess->track = cnt + 1;
		sess->effect_ready = false;
		sess->primary = false;
		sess->volume = 1.0;
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

	mem_deref(sync_lock);

	return 0;
}


const struct mod_export DECL_EXPORTS(effect) = {
	"effect",
	"sound",
	effect_init,
	effect_close
};
