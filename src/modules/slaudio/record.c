#include <string.h>
#include <time.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>
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

static volatile bool record = false;
static char basefolder[256] = {0};
static char record_folder[256] = {0};
static int record_timer = 0;
static pthread_t open_folder;

static int timestamp_print(struct re_printf *pf, const struct tm *tm)
{
	if (!tm)
		return 0;

	return re_hprintf(pf, "%d-%02d-%02d-%02d-%02d-%02d",
			1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
}


void slaudio_record_set(bool status)
{
	time_t tnow = time(0);
	struct tm *tm = localtime(&tnow);

	if (status) {
		(void)re_snprintf(record_folder, sizeof(record_folder),
			"%s" DIR_SEP "%H", basefolder, timestamp_print, tm);

		fs_mkdir(record_folder, 0700);
	}
	record = status;
}


int slaudio_record_get_timer(void)
{
	return record_timer;
}


static void *record_open_folder(void *arg)
{
	int err;
	char command[256] = {0};

#if defined (DARWIN)
	re_snprintf(command, sizeof(command), "open %s", record_folder);
#elif defined (WIN32)
	re_snprintf(command, sizeof(command), "explorer.exe %s", record_folder);
#else
	re_snprintf(command, sizeof(command), "xdg-open %s", record_folder);
#endif
	err = system(command);
	if (err) {};

	return NULL;
}


void slaudio_record_open_folder(void)
{
	pthread_create(&open_folder, NULL, record_open_folder, NULL);
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

	(void)re_snprintf(basefolder, sizeof(basefolder), "%s"
			DIR_SEP "studio-link", buf);


	return 0;
}


static int openfile(struct session *sess)
{
	char filename[256];
	FLAC__bool ok = true;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	str_ncpy(filename, record_folder, sizeof(filename));

	if (sess->local) {
		(void)re_snprintf(filename, sizeof(filename), "%s"
			DIR_SEP "local.flac", filename);
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
	sess->meta[0] = FLAC__metadata_object_new(
			FLAC__METADATA_TYPE_VORBIS_COMMENT);
	sess->meta[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);

	ok = FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
			&entry, "ENCODED_BY", "STUDIO LINK");

	ok &= FLAC__metadata_object_vorbiscomment_append_comment(
			sess->meta[0], entry, /*copy=*/false);

	if (!ok) {
		warning("slaudio/record: \
			   FLAC METADATA ERROR: out of memory or tag error\n");
		return ENOMEM;
	}

	sess->meta[1]->length = 1234; /* padding length */

	ok = FLAC__stream_encoder_set_metadata(sess->flac, sess->meta, 2);

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
				FLAC__metadata_object_delete(sess->meta[0]);
				FLAC__metadata_object_delete(sess->meta[1]);

				sess->flac = NULL;
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
	int err;

	err = folder_init();
	if (err) {
		warning("slaudio/record: folder_init error %d\n", err);
		return;
	}

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
