#include <sndfile.h>
#include <time.h>
#include <re.h>
#include <baresip.h>
#include "webapp.h"
#include <string.h>

static bool record = false;

struct record_enc {
	struct aufilt_enc_st af;  /* inheritance */
	SNDFILE *enc;
	uint32_t srate;
	uint32_t ch;
};


static void record_enc_destructor(void *arg)
{
	struct record_enc *st = arg;
	if (st->enc)
		sf_close(st->enc);
	list_unlink(&st->af.le);
}


static int timestamp_print(struct re_printf *pf, const struct tm *tm)
{
	if (!tm)
		return 0;

	return re_hprintf(pf, "%d-%02d-%02d-%02d-%02d-%02d",
			  1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec);
}


static SNDFILE *openfile(const struct record_enc *st)
{
	char filename[128];
	SF_INFO sfinfo;
	time_t tnow = time(0);
	struct tm *tm = localtime(&tnow);
	SNDFILE *sf;

	(void)re_snprintf(filename, sizeof(filename),
			  "studio-link-%H.flac",
			  timestamp_print, tm);
	memset(&sfinfo, 0, sizeof(sfinfo));
	sfinfo.samplerate = st->srate;
	sfinfo.channels   = st->ch;
	sfinfo.format     = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;

	sf = sf_open(filename, SFM_WRITE, &sfinfo);
	if (!sf) {
		warning("sndfile: could not open: %s\n", filename);
		puts(sf_strerror(NULL));
		return NULL;
	}

	info("sndfile: dumping audio to %s\n", filename);

	return sf;
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

	if (record) {
		if (!sf->enc) {
			sf->enc = openfile(sf);
			if (!sf->enc)
				return ENOMEM;
		}
		sf_write_short(sf->enc, sampv, *sampc);
	} else {
		if (sf->enc) {
			sf_close(sf->enc);
			sf->enc = NULL;
			warning("Close record\n");
		}
	}

	return 0;
}


void webapp_record_set(bool active)
{
	record = active;
}
