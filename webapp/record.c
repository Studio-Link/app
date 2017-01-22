#include <time.h>
#include <re.h>
#include <baresip.h>
#include "webapp.h"
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

static bool record = false;

struct record_enc {
	struct aufilt_enc_st af;  /* inheritance */
	FLAC__StreamEncoder *enc;
	uint32_t srate;
	uint32_t ch;
	bool run;
	struct lock *lock;
	bool ready;
	size_t sampc;
	FLAC__int32 *pcm;
	pthread_t thread;
};


static void record_enc_destructor(void *arg)
{
	struct record_enc *st = arg;

	st->run = false;
	(void)pthread_join(st->thread, NULL);
	if (st->enc) {
		FLAC__stream_encoder_finish(st->enc);
		/*
		FLAC__metadata_object_delete(metadata[0]);
		FLAC__metadata_object_delete(metadata[1]);
		*/
		FLAC__stream_encoder_delete(st->enc);
	}
	list_unlink(&st->af.le);
	mem_deref(st->lock);
}


static int timestamp_print(struct re_printf *pf, const struct tm *tm)
{
	if (!tm)
		return 0;

	return re_hprintf(pf, "%d-%02d-%02d-%02d-%02d-%02d",
			  1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec);
}


static int openfile(struct record_enc *st)
{
	char filename[256];
	char buf[256];
	time_t tnow = time(0);
	struct tm *tm = localtime(&tnow);
	FLAC__bool ok = true;
	FLAC__StreamMetadata *metadata[2];
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	int err;
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

	(void)re_snprintf(filename, sizeof(filename), "%s" DIR_SEP "studio-link-%H.flac", 
			buf, timestamp_print, tm);

	/* Basic Encoder */
	if((st->enc = FLAC__stream_encoder_new()) == NULL) {
		warning("ERROR: allocating FLAC encoder\n");
		return 1;
	}

	ok &= FLAC__stream_encoder_set_verify(st->enc, true);
	ok &= FLAC__stream_encoder_set_compression_level(st->enc, 5);
	ok &= FLAC__stream_encoder_set_channels(st->enc, st->ch);
	ok &= FLAC__stream_encoder_set_bits_per_sample(st->enc, 16);
	ok &= FLAC__stream_encoder_set_sample_rate(st->enc, st->srate);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(st->enc, 0);

	/* METADATA */
	if(ok) {
		if(
			(metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
			(metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			/* there are many tag (vorbiscomment) functions but these are convenient for this particular use: */
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "STUDIO LINK") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false) || /* copy=false: let metadata object take control of entry's allocated       string */
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "2016") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false)
		) {
			warning("FLAC METADATA ERROR: out of memory or tag error\n");
			ok = false;
		}

		metadata[1]->length = 1234; /* set the padding length */

		ok = FLAC__stream_encoder_set_metadata(st->enc, metadata, 2);
	}

	/* initialize encoder */
	if(ok) {
		init_status = FLAC__stream_encoder_init_file(st->enc, filename, NULL, NULL);
		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
			warning("FLAC ERROR: initializing encoder: %s\n", 
					FLAC__StreamEncoderInitStatusString[init_status]);
			ok = false;
		}
	}



	return 0;
}


static void *process_thread(void *arg)
{
	struct record_enc *sf = arg;
	FLAC__bool ok = true;

	while (sf->run) {

		if (sf->ready) {
			ok = FLAC__stream_encoder_process_interleaved(sf->enc, sf->pcm, sf->sampc/2);

			if (!ok) {
				warning("FLAC ENCODE ERROR: %s\n", 
						FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(sf->enc)]);

				warning("samples: %d\n", sf->sampc);
			}
			lock_write_get(sf->lock);
			sf->ready = false;
			lock_rel(sf->lock);
		}

		sys_msleep(1);
	}

	return NULL;
}


int webapp_record_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct record_enc *st;
	(void)ctx;

	if (!stp || !af || !prm)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), record_enc_destructor);
	st->pcm = mem_alloc(2 * 4096, NULL);
	st->ready = false;
	lock_alloc(&st->lock);
	if (!st)
		return ENOMEM;

	st->srate = prm->srate;
	st->ch = prm->ch;

	*stp = (struct aufilt_enc_st *)st;

	return 0;
}


int webapp_record_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	struct record_enc *sf = (struct record_enc *)st;
	unsigned i;

	if (record) {
		lock_write_get(sf->lock);
		sf->sampc = *sampc;
		for(i = 0; i < *sampc; i++) {
			sf->pcm[i] = (FLAC__int32)sampv[i];
		}
		sf->ready = true;
		lock_rel(sf->lock);

		if (!sf->enc) {
			openfile(sf);
			if (!sf->enc)
				return ENOMEM;
			//START PROCESS THREAD
			sf->run = true;
			pthread_create(&sf->thread, NULL, process_thread, sf);
		}

		return 0;
	}	

	if (sf->enc) {
		//STOP PROCESS THREAD
		sf->run = false;
		(void)pthread_join(sf->thread, NULL);

		FLAC__stream_encoder_finish(sf->enc);
		FLAC__stream_encoder_delete(sf->enc);
		sf->enc = NULL;
		warning("Close record\n");
	}

	return 0;
}


void webapp_record_set(bool active)
{
	record = active;
}
