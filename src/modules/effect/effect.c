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
#include <re_atomic.h>
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

	/* play ring per session, must be a power of two */
	RING_SAMPC   = MAX_SAMPC * 8,
	RING_MASK    = RING_SAMPC - 1,

	DETACH_POLL_US   = 100,
	DETACH_WARN_US   = 1000000,
};


/*
 * N-1 mix synchronisation
 *
 * Every plugin instance runs in its own (host) audio thread or sequentially
 * on the same thread, with independent and possibly different block sizes.
 * Instead of waiting for each other, every session publishes its played
 * samples into a single-writer ring with a running sample counter (wpos).
 * Every session reads the rings of the other sessions with its own read
 * cursor (rpos), so it always mixes the next contiguous samples, regardless
 * of block size, processing order or threading. Nothing blocks or spins.
 */
struct play_ring {
	int16_t sampv[RING_SAMPC];
	RE_ATOMIC unsigned wpos;  /* total samples written (wraps) */
	RE_ATOMIC unsigned last;  /* samples of the last write */
};


/*
 * Audio state lifetime
 *
 * The audio thread of a session uses its auplay_st/ausrc_st (handlers and
 * their args, owned by baresip) only between audio_enter() and
 * audio_leave(). The destructors close the gate with audio_detach() and
 * wait until the audio thread left, before baresip frees anything.
 *
 * Gate and busy flag form a Dekker style handshake, both sides store their
 * flag and then load the other one (sequentially consistent): either the
 * audio thread sees the closed gate, or the destructor sees busy and waits.
 *
 * All sample buffers are owned by the module (static), never freed while
 * an audio thread can run, so peers and the own audio thread can always
 * access them.
 */
struct chan_st {
	struct play_ring ring;          /* written by this session only */
	int16_t play_sampv[MAX_SAMPC];  /* own audio thread only */
	int16_t src_sampv[MAX_SAMPC];   /* own audio thread only */
	int32_t mix_acc[MAX_SAMPC];     /* own audio thread only */
	unsigned rpos[MAX_CHANNELS];    /* read cursor per peer session */
	bool rsync[MAX_CHANNELS];       /* read cursor valid per peer */
	bool played;                    /* play_sampv filled this block */

	RE_ATOMIC unsigned play_on;     /* gate: sess->st_play usable */
	RE_ATOMIC unsigned src_on;      /* gate: sess->st_src usable */
	RE_ATOMIC unsigned busy;        /* audio thread inside a gate */
};

static struct chan_st chanv[MAX_CHANNELS];

struct auplay_st {
	const struct auplay *ap;  /* pointer to base-class (inheritance) */
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
	struct session *sess;
};

struct ausrc_st {
	const struct ausrc *as;  /* pointer to base-class (inheritance) */
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	struct session *sess;
	uint64_t frames;
};

static struct list sessionl;

static struct ausrc *ausrc;
static struct auplay *auplay;

static bool bypass = false;


static struct chan_st *sess_chan(const struct session *sess)
{
	return &chanv[sess->track - 1];
}


/* audio thread: returns true if the gate is open, must call audio_leave() */
static bool audio_enter(struct chan_st *chan, RE_ATOMIC unsigned *gate)
{
	re_atomic_store(&chan->busy, 1, re_memory_order_seq_cst);

	if (re_atomic_load(gate, re_memory_order_seq_cst))
		return true;

	re_atomic_store(&chan->busy, 0, re_memory_order_release);

	return false;
}


static void audio_leave(struct chan_st *chan)
{
	re_atomic_store(&chan->busy, 0, re_memory_order_release);
}


/* not real-time: close the gate and wait until the audio thread left */
static void audio_detach(struct chan_st *chan, RE_ATOMIC unsigned *gate)
{
	uint32_t waited = 0;

	re_atomic_store(gate, 0, re_memory_order_seq_cst);

	while (re_atomic_load(&chan->busy, re_memory_order_seq_cst)) {
		sys_usleep(DETACH_POLL_US);
		waited += DETACH_POLL_US;

		if (waited == DETACH_WARN_US)
			warning("effect: audio thread still busy after 1s\n");
	}
}


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
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

	re_snprintf(sess_id, sizeof(sess_id), "%u", session->track);
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


/* Single writer: only the audio thread of this session */
static void ring_write(struct play_ring *ring, const int16_t *sampv,
		       unsigned sampc)
{
	unsigned wpos = re_atomic_load(&ring->wpos, re_memory_order_relaxed);
	unsigned idx  = wpos & RING_MASK;
	unsigned n1   = RING_SAMPC - idx;

	if (n1 > sampc)
		n1 = sampc;

	memcpy(&ring->sampv[idx], sampv, n1 * sizeof(int16_t));
	memcpy(&ring->sampv[0], sampv + n1, (sampc - n1) * sizeof(int16_t));

	re_atomic_store(&ring->last, sampc, re_memory_order_relaxed);
	/* publish the samples */
	re_atomic_store(&ring->wpos, wpos + sampc, re_memory_order_release);
}


void effect_play(struct session *sess, float* const output0,
		float* const output1, unsigned long nframes)
{
	struct chan_st *chan;
	struct auplay_st *st_play;
	unsigned sampc;
	(void)output0;
	(void)output1;

	/* check max sessions reached*/
	if (!sess)
		return;

	chan = sess_chan(sess);
	chan->played = false;

	if (!audio_enter(chan, &chan->play_on))
		return;

	st_play = sess->st_play;
	sampc   = (unsigned)frames_limit(nframes) * 2;

	/* baresip's write handler leaves the frame untouched if it misses
	 * its aubuf trylock: play silence, never a stale block (the buffer
	 * is module owned and may hold the last block of a previous call) */
	memset(chan->play_sampv, 0, sampc * sizeof(int16_t));

	webapp_session_play_read(sess, chan->play_sampv,
			st_play->wh, sampc, st_play->arg);

	audio_leave(chan);

	ring_write(&chan->ring, chan->play_sampv, sampc);
	chan->played = true;
}


void effect_bypass(struct session *sess,
		float* const output0,
		float* const output1,
		const float* const input0,
		const float* const input1,
		unsigned long nframes)
{
	struct chan_st *chan;
	unsigned long nlimit = frames_limit(nframes);

	/* max sessions reached: the host output buffers must always be
	 * written, otherwise they keep stale data (feedback noise) */
	if (!sess) {
		memset(output0, 0, nframes * sizeof(float));
		memset(output1, 0, nframes * sizeof(float));
		return;
	}

	chan = sess_chan(sess);

	/* filled by effect_play() of this block, module owned memory */
	if (chan->played) {
		chan->played = false;

		sample_move_dS_s16(output0, (char*)chan->play_sampv,
				nlimit, 4);
		sample_move_dS_s16(output1, (char*)chan->play_sampv+2,
				nlimit, 4);
		for (unsigned long pos = nlimit; pos < nframes; pos++) {
			output0[pos] = 0;
			output1[pos] = 0;
		}
		ws_meter_process_planar(sess->ch+1, output0, output1, nlimit);
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


static void mix_reset(struct chan_st *chan)
{
	for (unsigned j = 0; j < MAX_CHANNELS; j++)
		chan->rsync[j] = false;
}


/*
 * Read the next contiguous samples of a peer ring and add them to the
 * accumulator. Returns the number of samples added (0..sampc).
 */
static unsigned ring_mix(struct chan_st *chan, unsigned peer,
			 int32_t *acc, unsigned sampc)
{
	const struct play_ring *ring = &chanv[peer].ring;
	unsigned wpos = re_atomic_load(&ring->wpos, re_memory_order_acquire);
	unsigned last = re_atomic_load(&ring->last, re_memory_order_relaxed);
	unsigned avail;
	unsigned rpos;

	if (!chan->rsync[peer]) {
		/* start with the next samples the peer writes */
		chan->rpos[peer]  = wpos;
		chan->rsync[peer] = true;
		return 0;
	}

	rpos  = chan->rpos[peer];
	avail = wpos - rpos;

	/* Too far behind (stalled reader or peer, or a burst after a
	 * resync): skip to the latest block to keep the latency bounded
	 * and to never read where the peer is writing. Bigger than any
	 * regular backlog with mixed block sizes (sampc vs. last). */
	if (avail > 2 * (sampc + last) || avail > RING_SAMPC / 2) {
		avail = last < sampc ? last : sampc;
		rpos  = wpos - avail;
	}

	/* underrun (peer not processed yet or suspended by the host):
	 * mix what is there, the cursor keeps the stream contiguous */
	if (avail > sampc)
		avail = sampc;

	for (unsigned i = 0; i < avail; i++)
		acc[i] += ring->sampv[(rpos + i) & RING_MASK];

	chan->rpos[peer] = rpos + avail;

	return avail;
}


static void mix_n_minus_1(struct session *sess, int16_t *dst,
		unsigned long nframes)
{
	struct chan_st *chan = sess_chan(sess);
	int32_t *acc = chan->mix_acc;
	unsigned sampc = (unsigned)nframes * 2;
	unsigned mixed = 0;
	struct le *le;

	for (le = sessionl.head; le; le = le->next) {
		struct session *msess = le->data;
		unsigned peer = msess->track - 1;
		unsigned n;

		if (msess == sess)
			continue;

		/* the peer ring is module owned, the gate is only a hint */
		if (!re_atomic_load(&chanv[peer].play_on,
				    re_memory_order_relaxed) ||
		    msess->ch == -1) {
			chan->rsync[peer] = false;
			continue;
		}

		n = ring_mix(chan, peer, acc, sampc);
		if (n > mixed)
			mixed = n;
	}

	if (!mixed)
		return;

	for (unsigned i = 0; i < sampc; i++) {
		int32_t v = acc[i] + dst[i];

		if (v > SAMPLE_16BIT_MAX)
			v = SAMPLE_16BIT_MAX;
		if (v < SAMPLE_16BIT_MIN)
			v = SAMPLE_16BIT_MIN;

		dst[i] = (int16_t)v;
		acc[i] = 0;
	}
}


void effect_src(struct session *sess, const float* const input0,
		const float* const input1, unsigned long nframes)
{
	struct chan_st *chan;
	struct ausrc_st *st_src;
	struct auframe af;

	/* check max sessions reached*/
	if (!sess)
		return;

	chan    = sess_chan(sess);
	nframes = frames_limit(nframes);

	/* send only while the call also plays (same as before) */
	if (!re_atomic_load(&chan->play_on, re_memory_order_relaxed) ||
	    !audio_enter(chan, &chan->src_on)) {
		mix_reset(chan);
		goto out;
	}

	st_src = sess->st_src;

	sample_move_d16_sS((char*)chan->src_sampv, (float*)input0,
			nframes, 4);
	sample_move_d16_sS((char*)chan->src_sampv+2, (float*)input1,
			nframes, 4);
	if (sess->run_auto_mix)
		mix_n_minus_1(sess, chan->src_sampv, nframes);
	else
		mix_reset(chan);

	auframe_init(&af, st_src->prm.fmt, chan->src_sampv,
		     nframes * 2, st_src->prm.srate, st_src->prm.ch);
	af.timestamp = st_src->frames * AUDIO_TIMEBASE /
		       st_src->prm.srate;

	st_src->frames += nframes;

	st_src->rh(&af, st_src->arg);

	audio_leave(chan);

out:
	ws_meter_process_planar(sess->ch, input0, input1, nframes);
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;

	/* the read handler and its arg are freed by baresip after return */
	audio_detach(sess_chan(sess), &sess_chan(sess)->src_on);

	sess->st_src  = NULL;
	sess->run_src = false;
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;

	/* the write handler and its arg are freed by baresip after return */
	audio_detach(sess_chan(sess), &sess_chan(sess)->play_on);

	sess->st_play  = NULL;
	sess->run_play = false;
}


/*
 * Find the session for a new audio source/player. The webapp sets the track
 * number as device name of the call audio (webapp_call_update()), so the
 * audio of a call always ends up in the session (plugin instance) of its
 * call, independent of the order in which baresip starts the audio of
 * several calls.
 */
static struct session *sess_find(bool play, const char *device)
{
	struct pl pl;
	unsigned track = 0;
	struct le *le;

	if (!re_regex(device, str_len(device), "^[0-9]+$", &pl))
		track = pl_u32(&pl);

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (track && sess->track != track)
			continue;

		if (sess->ch == -1 || !sess->call)
			continue;

		if (play ? !sess->run_play : !sess->run_src)
			return sess;
	}

	if (track)
		warning("effect: no session for track %u (%s)\n", track,
			play ? "play" : "src");

	return NULL;
}


static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st_src;
	struct session *sess;
	(void)errh;

	if (!stp || !as || !prm)
		return EINVAL;

	if (prm->fmt != AUFMT_S16LE || prm->ch != 2 || !prm->srate) {
		warning("effect: unsupported source format %s/%u/%u\n",
			aufmt_name(prm->fmt), prm->srate, prm->ch);
		return ENOTSUP;
	}

	sess = sess_find(false, device);
	if (!sess)
		return EINVAL;

	st_src = mem_zalloc(sizeof(*st_src), ausrc_destructor);
	if (!st_src)
		return ENOMEM;

	st_src->sess = sess;
	st_src->as   = as;
	st_src->rh   = rh;
	st_src->arg  = arg;
	st_src->prm  = *prm;

	sess->st_src  = st_src;
	sess->run_src = true;

	/* open the gate last, publishes sess->st_src */
	re_atomic_store(&sess_chan(sess)->src_on, 1, re_memory_order_seq_cst);

	*stp = st_src;

	return 0;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st_play;
	struct session *sess;

	if (!stp || !ap || !prm)
		return EINVAL;

	if (prm->fmt != AUFMT_S16LE || prm->ch != 2 || !prm->srate) {
		warning("effect: unsupported player format %s/%u/%u\n",
			aufmt_name(prm->fmt), prm->srate, prm->ch);
		return ENOTSUP;
	}

	sess = sess_find(true, device);
	if (!sess)
		return EINVAL;

	st_play = mem_zalloc(sizeof(*st_play), auplay_destructor);
	if (!st_play)
		return ENOMEM;

	st_play->sess = sess;
	st_play->ap   = ap;
	st_play->wh   = wh;
	st_play->arg  = arg;
	st_play->prm  = *prm;

	sess->st_play  = st_play;
	sess->run_play = true;

	/* open the gate last, publishes sess->st_play */
	re_atomic_store(&sess_chan(sess)->play_on, 1, re_memory_order_seq_cst);

	*stp = st_play;

	return 0;
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

	if (err)
		return err;

	/* no audio thread uses a session yet, safe to reset */
	memset(chanv, 0, sizeof(chanv));

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
		sess->track = cnt + 1;
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

	return 0;
}


const struct mod_export DECL_EXPORTS(effect) = {
	"effect",
	"sound",
	effect_init,
	effect_close
};
