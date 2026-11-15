#include <stdio.h>
#include <string.h>
#include <time.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>

#if defined (WIN32)
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <lmaccess.h>
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

#define SRATE 48000
#define SAMPC 1920  /* Encoder chunk, 48000Hz 2ch at 20ms */
#define REC_MAX_BUF (SRATE * 2 * sizeof(int16_t) * 10) /* 10s per track */
#include "slaudio.h"
#include <slopen.h>
#include <webapp.h>

/*
 * Recording state machine
 *
 * All tracks must be sample aligned, so the audio thread decides which
 * audio period is the first and the last one of a recording and writes the
 * same number of samples to every track buffer. The record threads only
 * encode what they get, and close the file exactly at rec.end samples.
 *
 *  IDLE      -> ARMED     main thread, all files are opened
 *  ARMED     -> RUNNING   audio thread, first recorded period
 *  ARMED     -> STOPPING  main thread, stopped before audio started
 *  RUNNING   -> STOPPING  audio thread (main thread if no audio is running)
 *  STOPPING  -> IDLE      last record thread that closed its file
 */
enum rec_state {
	REC_IDLE = 0,
	REC_ARMED,
	REC_RUNNING,
	REC_STOPPING,
};

enum rec_error {
	REC_ERR_NONE = 0,
	REC_ERR_OPEN,
	REC_ERR_ENCODE,
	REC_ERR_OVERRUN,
	REC_ERR_BUSY,
};

static struct {
	RE_ATOMIC int state;      /* enum rec_state */
	RE_ATOMIC bool req;       /* main thread: recording requested */
	RE_ATOMIC unsigned gen;   /* main thread: recording generation */
	RE_ATOMIC uint64_t end;   /* samples per track, valid if STOPPING */
	RE_ATOMIC unsigned pending; /* record threads with an open recording */
	RE_ATOMIC int error;      /* enum rec_error, reported by rec_tmr */
	RE_ATOMIC int timer;      /* recorded time [ms] */
	uint64_t written;         /* samples per track, audio thread only */
	unsigned nthreads;
} rec;

static char basefolder[256] = {0};
static char record_folder[256] = {0};
static struct tmr rec_tmr;

static int timestamp_print(struct re_printf *pf, const struct tm *tm)
{
	if (!tm)
		return 0;

	return re_hprintf(pf, "%d-%02d-%02d-%02d-%02d-%02d",
			1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
}


static void rec_error_set(enum rec_error err)
{
	int none = REC_ERR_NONE;

	/* keep the first error */
	(void)re_atomic_compare_exchange_strong(&rec.error, &none, (int)err,
						re_memory_order_acq_rel,
						re_memory_order_acquire);
}


void slaudio_record_closefile(struct session *sess)
{
	if (sess->flac) {
		FLAC__stream_encoder_finish(sess->flac);
		FLAC__stream_encoder_delete(sess->flac);
		sess->flac = NULL;
	}

	if (sess->meta[0]) {
		FLAC__metadata_object_delete(sess->meta[0]);
		sess->meta[0] = NULL;
	}

	if (sess->meta[1]) {
		FLAC__metadata_object_delete(sess->meta[1]);
		sess->meta[1] = NULL;
	}
}


static void rec_filename(const struct session *sess, char *buf, size_t sz)
{
	if (sess->local) {
		(void)re_snprintf(buf, sz, "%s" DIR_SEP "local.flac",
				  record_folder);
	}
	else {
		(void)re_snprintf(buf, sz, "%s" DIR_SEP "remote-track-%d.flac",
				  record_folder, sess->track);
	}
}


static int openfile(struct session *sess)
{
	char filename[256];
	FLAC__bool ok = true;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	rec_filename(sess, filename, sizeof(filename));

	/* Basic Encoder */
	if((sess->flac = FLAC__stream_encoder_new()) == NULL) {
		warning("slaudio/record: allocating FLAC encoder\n");
		return ENOMEM;
	}

	ok &= FLAC__stream_encoder_set_verify(sess->flac, true);
	ok &= FLAC__stream_encoder_set_compression_level(sess->flac, 5);
	ok &= FLAC__stream_encoder_set_channels(sess->flac, 2);
	ok &= FLAC__stream_encoder_set_bits_per_sample(sess->flac, 16);
	ok &= FLAC__stream_encoder_set_sample_rate(sess->flac, SRATE);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(sess->flac, 0);

	if (!ok) {
		warning("slaudio/record: FLAC__stream_encoder_set\n");
		goto err;
	}

	/* METADATA */
	sess->meta[0] = FLAC__metadata_object_new(
			FLAC__METADATA_TYPE_VORBIS_COMMENT);
	sess->meta[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
	if (!sess->meta[0] || !sess->meta[1]) {
		warning("slaudio/record: FLAC METADATA: out of memory\n");
		goto err;
	}

	ok = FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
			&entry, "ENCODED_BY", "STUDIO LINK");

	ok &= FLAC__metadata_object_vorbiscomment_append_comment(
			sess->meta[0], entry, /*copy=*/false);

	if (!ok) {
		warning("slaudio/record: "
			"FLAC METADATA ERROR: out of memory or tag error\n");
		goto err;
	}

	sess->meta[1]->length = 1234; /* padding length */

	ok = FLAC__stream_encoder_set_metadata(sess->flac, sess->meta, 2);
	if (!ok) {
		warning("slaudio/record: FLAC__stream_encoder_set_metadata\n");
		goto err;
	}

	init_status = FLAC__stream_encoder_init_file(sess->flac, filename,
						     NULL, NULL);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		warning("slaudio/record: "
			"FLAC ERROR: initializing encoder %s: %s\n", filename,
			FLAC__StreamEncoderInitStatusString[init_status]);
		goto err;
	}

	return 0;

err:
	slaudio_record_closefile(sess);
	return EIO;
}


static bool rec_session(const struct session *sess)
{
	return !sess->stream && sess->aubuf;
}


/* Main thread: report errors from the audio and record threads */
static void rec_tmr_handler(void *arg)
{
	int err;
	const char *msg;
	(void)arg;

	tmr_start(&rec_tmr, 100, rec_tmr_handler, NULL);

	err = re_atomic_exchange(&rec.error, REC_ERR_NONE,
				 re_memory_order_acq_rel);
	if (err == REC_ERR_NONE)
		return;

	switch (err) {

	case REC_ERR_OPEN:
		msg = "Recording failed: could not create the record files";
		break;
	case REC_ERR_ENCODE:
		msg = "Recording stopped: could not write the record files "
		      "(disk full?)";
		break;
	case REC_ERR_OVERRUN:
		msg = "Recording stopped: the disk is too slow";
		break;
	default:
		msg = "Recording failed: previous recording is still "
		      "finishing, please try again";
		break;
	}

	warning("slaudio/record: %s\n", msg);
	slaudio_error_set(msg);

	/* resets the UI state and calls slaudio_record_set(false) */
	webapp_options_set("record", "false");
}


/* Audio thread (or main thread without running audio) */
static void rec_stop(void)
{
	re_atomic_rls_set(&rec.end, rec.written);
	re_atomic_rls_set(&rec.state, REC_STOPPING);
	info("slaudio/record: stop after %llu samples\n", rec.written);
}


/**
 * Called by the audio thread once per period, before the samples are
 * written to the record buffers.
 *
 * @param samples Samples per track of this period
 *
 * @return true if this period has to be written to all record buffers
 */
bool slaudio_record_period(size_t samples)
{
	int st = re_atomic_acq(&rec.state);
	struct le *le;

	if (st == REC_ARMED) {
		if (!re_atomic_compare_exchange_strong(&rec.state, &st,
					REC_RUNNING, re_memory_order_acq_rel,
					re_memory_order_acquire))
			return false; /* stopped by main thread */

		rec.written = 0;
		st = REC_RUNNING;
	}

	if (st != REC_RUNNING)
		return false;

	if (!re_atomic_acq(&rec.req)) {
		rec_stop();
		return false;
	}

	/* A track buffer is full (slow disk): stop all tracks at the same
	 * position instead of dropping samples on a single track */
	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!rec_session(sess))
			continue;

		if (aubuf_cur_size(sess->aubuf) > REC_MAX_BUF) {
			rec_error_set(REC_ERR_OVERRUN);
			rec_stop();
			return false;
		}
	}

	rec.written += samples;

	return true;
}


/**
 * Called by the main thread when the audio streams are stopped, so no
 * audio period can finish a requested stop anymore.
 */
void slaudio_record_audio_stopped(void)
{
	int st = REC_ARMED;

	if (re_atomic_acq(&rec.req))
		return;

	if (re_atomic_compare_exchange_strong(&rec.state, &st, REC_STOPPING,
					      re_memory_order_acq_rel,
					      re_memory_order_acquire))
		return; /* end is 0 */

	if (st == REC_RUNNING)
		rec_stop();
}


static void rec_start(void)
{
	time_t tnow = time(0);
	struct tm *tm = localtime(&tnow);
	struct le *le;
	int err = 0;

	if (re_atomic_acq(&rec.state) != REC_IDLE) {
		rec_error_set(REC_ERR_BUSY);
		return;
	}

	if (!rec.nthreads) {
		rec_error_set(REC_ERR_OPEN);
		return;
	}

	(void)re_snprintf(record_folder, sizeof(record_folder),
		"%s" DIR_SEP "%H", basefolder, timestamp_print, tm);

	fs_mkdir(record_folder, 0700);

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!rec_session(sess))
			continue;

		re_atomic_rlx_set(&sess->rec_used, false);

		err = openfile(sess);
		if (err)
			break;
	}

	if (err) {
		for (le = sessionl.head; le; le = le->next) {
			struct session *sess = le->data;

			if (rec_session(sess))
				slaudio_record_closefile(sess);
		}

		rec_error_set(REC_ERR_OPEN);
		return;
	}

	info("slaudio/record: start %s\n", record_folder);

	re_atomic_rlx_set(&rec.timer, 0);
	re_atomic_rlx_set(&rec.end, 0);
	re_atomic_rlx_set(&rec.pending, rec.nthreads);
	re_atomic_rlx_add(&rec.gen, 1);
	re_atomic_rls_set(&rec.req, true);
	re_atomic_rls_set(&rec.state, REC_ARMED);
}


void slaudio_record_set(bool status)
{
	int st = REC_ARMED;

	if (status) {
		if (!re_atomic_acq(&rec.req))
			rec_start();
		return;
	}

	re_atomic_rls_set(&rec.req, false);

	/* not started by the audio thread yet */
	if (re_atomic_compare_exchange_strong(&rec.state, &st, REC_STOPPING,
					      re_memory_order_acq_rel,
					      re_memory_order_acquire))
		return;

	/* no audio period will stop it */
	if (st == REC_RUNNING && !slaudio_audio_running())
		rec_stop();
}


int slaudio_record_get_timer(void)
{
	return re_atomic_rlx(&rec.timer);
}


static int record_open_folder(void *arg)
{
	int err;
	(void)arg;

#if defined (WIN32)
	err = sl_shell_open(record_folder);
#else
	char command[256] = {0};

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open %s", record_folder);
#else
	re_snprintf(command, sizeof(command), "xdg-open %s", record_folder);
#endif
	err = system(command);
#endif
	if (err)
		warning("slaudio: could not open %s\n", record_folder);

	return 0;
}


void slaudio_record_open_folder(void)
{
	thrd_t thr;

	if (thread_create_name(&thr, "slaudio_open_folder",
			       record_open_folder, NULL) == 0)
		thrd_detach(thr);
}


static int folder_init(void)
{
	int err;
	char buf[256];
#ifdef WIN32
	char win32_path[MAX_PATH];

	if (S_OK != SHGetFolderPath(NULL,
				CSIDL_DESKTOP,
				NULL,
				0,
				win32_path)) {
		return ENOENT;
	}
	str_ncpy(buf, win32_path, sizeof(buf));
#else
	err = fs_gethome(buf, sizeof(buf));
	if (err)
		return err;
#endif

	(void)re_snprintf(basefolder, sizeof(basefolder), "%s"
			DIR_SEP "studio-link", buf);
	fs_mkdir(basefolder, 0700);

	str_ncpy(record_folder, basefolder, sizeof(record_folder));

	return 0;
}


static int record_thread(void *arg)
{
	struct session *sess = arg;
	unsigned my_gen = re_atomic_acq(&rec.gen);
	uint64_t done = 0;
	bool failed = false;

	while (sess->run_record) {
		FLAC__StreamEncoderState encstate;
		uint64_t end = UINT64_MAX;
		size_t n;
		int st;

		/* state before gen and end (release order of the writers) */
		st = re_atomic_acq(&rec.state);
		if (st == REC_IDLE || re_atomic_acq(&rec.gen) == my_gen)
			goto sleep;

		if (st == REC_STOPPING)
			end = re_atomic_acq(&rec.end);

		n = aubuf_cur_size(sess->aubuf) / sizeof(int16_t);
		n = MIN(n, SAMPC);

		if (end != UINT64_MAX)
			n = (size_t)MIN((uint64_t)n, end - done);
		else if (n < SAMPC)
			goto sleep;

		if (n) {
			aubuf_read(sess->aubuf, (uint8_t *)sess->sampv,
				   n * sizeof(int16_t));

			for (size_t i = 0; i < n; i++)
				sess->pcm[i] = (FLAC__int32)sess->sampv[i];

			if (!failed &&
			    !FLAC__stream_encoder_process_interleaved(
				    sess->flac, sess->pcm, (unsigned)n / 2)) {
				encstate = FLAC__stream_encoder_get_state(
						sess->flac);
				warning("slaudio/record: FLAC ENCODE ERROR "
					"track %d: %s\n", sess->track,
				    FLAC__StreamEncoderStateString[encstate]);
				/* keep draining to stay in sync, the
				 * recording is stopped by rec_tmr */
				failed = true;
				rec_error_set(REC_ERR_ENCODE);
			}

			done += n;

			if (sess->local)
				re_atomic_rlx_set(&rec.timer,
					(int)(done * 1000 / (SRATE * 2)));
		}

		if (st == REC_STOPPING && done == end) {
			slaudio_record_closefile(sess);
			info("slaudio/record: track %d closed "
			     "(%llu samples)\n", sess->track, done);

			/* no call on this track while recording */
			if (!sess->local &&
			    !re_atomic_acq(&sess->rec_used)) {
				char filename[256];

				rec_filename(sess, filename,
					     sizeof(filename));
				if (remove(filename) == 0)
					info("slaudio/record: track %d "
					     "unused, removed\n",
					     sess->track);
				else
					warning("slaudio/record: could not "
						"remove %s\n", filename);
			}
			my_gen = re_atomic_acq(&rec.gen);
			done = 0;
			failed = false;

			if (sess->local)
				re_atomic_rlx_set(&rec.timer, 0);

			if (re_atomic_acq_sub(&rec.pending, 1) == 1)
				re_atomic_rls_set(&rec.state, REC_IDLE);

			continue;
		}

		/* more data waiting, drain without sleeping */
		if (n == SAMPC)
			continue;
sleep:
		sys_msleep(10);
	}

	return 0;
}


void slaudio_record_init(void) {

	struct session *sess;
	struct le *le;
	int err;

	memset(&rec, 0, sizeof(rec));

	tmr_init(&rec_tmr);
	tmr_start(&rec_tmr, 100, rec_tmr_handler, NULL);

	err = folder_init();
	if (err) {
		warning("slaudio/record: folder_init error %d\n", err);
		return;
	}

	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;

		if (sess->stream)
			continue;

		sess->pcm = mem_zalloc(SAMPC * sizeof(FLAC__int32), NULL);
		sess->sampv = mem_zalloc(SAMPC * sizeof(int16_t), NULL);

		/* no jitter buffering (min_sz 0) and no size limit, the
		 * limit is checked by slaudio_record_period() for all
		 * tracks */
		err = aubuf_alloc(&sess->aubuf, 0, 0);
		if (err || !sess->pcm || !sess->sampv) {
			warning("slaudio/record: alloc error\n");
			sess->aubuf = mem_deref(sess->aubuf);
			continue;
		}

		aubuf_set_live(sess->aubuf, false);

		sess->run_record = true;
		if (thread_create_name(&sess->record_thread, "slaudio_record",
				       record_thread, sess)) {
			sess->run_record = false;
			sess->aubuf = mem_deref(sess->aubuf);
			continue;
		}

		++rec.nthreads;
	}
}


/* Main thread, after the audio streams are stopped */
void slaudio_record_close(void)
{
	tmr_cancel(&rec_tmr);

	slaudio_record_set(false);
	slaudio_record_audio_stopped();

	/* give the record threads time to finish the files */
	for (int i = 0; i < 300; i++) {
		if (re_atomic_acq(&rec.state) == REC_IDLE)
			break;
		sys_msleep(10);
	}
}
