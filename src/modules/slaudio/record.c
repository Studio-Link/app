#include <time.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "share/compat.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
#include <pthread.h>

#if defined (WIN32)
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <lmaccess.h>
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

#define PTIME 20
#define SAMPC 1920  /* Max samples, 48000Hz 2ch at 20ms */
#include "slaudio.h"

static bool record = false;
static char command[256];
static int record_timer = 0;


void slaudio_record_set(bool active)
{
	record = active;
}


int slaudio_record_get_timer()
{
	return record_timer;
}


static int timestamp_print(struct re_printf *pf, const struct tm *tm)
{
	if (!tm)
		return 0;

	return re_hprintf(pf, "%d-%02d-%02d-%02d-%02d-%02d",
			1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
}


static int openfile(struct session *sess)
{
	char filename[256];
	char buf[256];
	time_t tnow = time(0);
	struct tm *tm = localtime(&tnow);
	FLAC__bool ok = true;
	FLAC__StreamMetadata *meta[2];
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	int err;
	int ret;
#ifdef WIN32
	char win32_path[MAX_PATH];

	if (S_OK != SHGetFolderPath(NULL,
				CSIDL_DESKTOPDIRECTORY,
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


	(void)re_snprintf(filename, sizeof(filename), "%s"
			DIR_SEP "studio-link", buf, timestamp_print, tm);

	fs_mkdir(filename, 0700);

	(void)re_snprintf(filename, sizeof(filename), "%s"
			DIR_SEP "studio-link"
			DIR_SEP "%H", buf, timestamp_print, tm);

	fs_mkdir(filename, 0700);

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open %s",
			filename);
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "start %s",
			filename);
#else
	re_snprintf(command, sizeof(command), "xdg-open %s",
			filename);
#endif
	if (sess->local) {
		(void)re_snprintf(filename, sizeof(filename), "%s"
			DIR_SEP "local.flac", filename);
		ret = system(command);
		if (ret) {}
	}
	else {
		(void)re_snprintf(filename, sizeof(filename), "%s"
			DIR_SEP "remote-track-%d.flac", filename, sess->track);
	}


	/* Basic Encoder */
	if((sess->flac = FLAC__stream_encoder_new()) == NULL) {
		warning("slaudio/record: allocating FLAC encoder\n");
		return ENOMEM;
	}

	ok &= FLAC__stream_encoder_set_verify(sess->flac, true);
	ok &= FLAC__stream_encoder_set_compression_level(sess->flac, 5);
	ok &= FLAC__stream_encoder_set_channels(sess->flac, 2);
	ok &= FLAC__stream_encoder_set_bits_per_sample(sess->flac, 16);
	ok &= FLAC__stream_encoder_set_sample_rate(sess->flac, 48000);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(sess->flac, 0);

	if (!ok) {
		warning("slaudio/record: FLAC__stream_encoder_set\n");
		return EINVAL;
	}

	/* METADATA */
	meta[0] = FLAC__metadata_object_new(
			FLAC__METADATA_TYPE_VORBIS_COMMENT);
	meta[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);

	ok = FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
			&entry, "ENCODED_BY", "STUDIO LINK");

	ok &= FLAC__metadata_object_vorbiscomment_append_comment(
			meta[0], entry, /*copy=*/false);

	if (!ok) {
		warning("slaudio/record: \
			   FLAC METADATA ERROR: out of memory or tag error\n");
		return ENOMEM;
	}

	meta[1]->length = 1234; /* padding length */

	ok = FLAC__stream_encoder_set_metadata(sess->flac, meta, 2);

	if (!ok) {
		warning("slaudio/record: \
				FLAC__stream_encoder_set_metadata\n");
		return ENOMEM;
	}

	init_status = FLAC__stream_encoder_init_file(sess->flac, filename,
						     NULL, NULL);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		warning("slaudio/record: \
				FLAC ERROR: initializing encoder: %s\n",
			  FLAC__StreamEncoderInitStatusString[init_status]);
	}

	return 0;
}


void webapp_options_set(char *key, char *value);
static void *record_thread(void *arg)
{
	struct session *sess = arg;
	FLAC__bool ok = true;
	unsigned i;
	int ret;
	size_t bufsz;
	FLAC__StreamEncoderState encstate;
	size_t num_bytes = SAMPC * sizeof(int16_t);

	while (sess->run_record) {
		bufsz = aubuf_cur_size(sess->aubuf);
		if (bufsz < num_bytes) {
			goto sleep;
		}
		aubuf_read(sess->aubuf, (uint8_t *)sess->sampv, num_bytes);

		if (record) {
			if (!sess->flac) {
				if (sess->local)
					info("slaudio/record: open \
						session record file\n");
				ret = openfile(sess);
				if (ret) {
					warning("slaudio/record: \
						   FLAC open file error\n");
					webapp_options_set("record", "false");
					continue;
				}
			}

			for (i = 0; i < SAMPC; i++) {
				sess->pcm[i] = (FLAC__int32)sess->sampv[i];
			}

			ok = FLAC__stream_encoder_process_interleaved(
					sess->flac, sess->pcm, SAMPC/2);

			if (sess->local)
				record_timer += PTIME;

		}
		else {
			if (sess->flac) {
				if (sess->local)
					info("slaudio/record: \
						close session record file\n");
				FLAC__stream_encoder_finish(sess->flac);
				FLAC__stream_encoder_delete(sess->flac);

				sess->flac = NULL;

				/* open folder on stop record
				 * if record_time > 5min
				 */
				if (sess->local && record_timer > 300000)
					ret = system(command);
			}
			record_timer = 0;
		}

		if (!ok) {
			encstate = FLAC__stream_encoder_get_state(sess->flac);
			warning("slaudio/record: FLAC ENCODE ERROR: %s\n",
				FLAC__StreamEncoderStateString[encstate]);
			ok = true;
		}
sleep:
		sys_msleep(10);
	}
	return NULL;
}


void slaudio_record_init(void) {

	struct session *sess;
	struct le *le;

	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;

		if (sess->stream)
			continue;

		sess->pcm = mem_zalloc(10 * 1920, NULL);
		sess->sampv = mem_zalloc(10 * 1920, NULL);
		aubuf_alloc(&sess->aubuf, 1920 * 10, 1920 * 100);
		sess->run_record = true;
		pthread_create(&sess->record_thread, NULL,
			       record_thread, sess);
	}
}
