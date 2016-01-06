#include <re.h>
#include <baresip.h>
#include "webapp.h"

struct vumeter_enc {
	struct aufilt_enc_st af;  /* inheritance */
	int ch;
};

struct vumeter_dec {
	struct aufilt_dec_st af;  /* inheritance */
	int ch;
};

static int channels_enc = 0;
static int channels_dec = 0;

static void vu_enc_destructor(void *arg)
{
	struct vumeter_enc *st = arg;
	channels_enc = channels_enc - 2;
	list_unlink(&st->af.le);
}


static void vu_dec_destructor(void *arg)
{
	struct vumeter_dec *st = arg;
	channels_dec = channels_dec - 2;
	list_unlink(&st->af.le);
}


int webapp_vu_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct vumeter_enc *st;
	(void)ctx;
	(void)prm;

	if (!stp || !af)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), vu_enc_destructor);
	if (!st)
		return ENOMEM;

	st->ch = channels_enc;
	channels_enc = channels_enc + 2;

	*stp = (struct aufilt_enc_st *)st;

	return 0;
}


int webapp_vu_decode_update(struct aufilt_dec_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct vumeter_dec *st;
	(void)ctx;
	(void)prm;

	if (!stp || !af)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), vu_dec_destructor);
	if (!st)
		return ENOMEM;

	st->ch = channels_dec+1;
	channels_dec = channels_dec + 2;

	*stp = (struct aufilt_dec_st *)st;

	return 0;
}

static void convert_float(int16_t *sampv, float *f_sampv, size_t sampc)
{
	const float scaling = 1.0/32767.0f;
	while (sampc--) {
		*f_sampv = (*((short *) sampv)) * scaling;
		++f_sampv;
	}

}

int webapp_vu_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	struct vumeter_enc *vu = (struct vumeter_enc *)st;

	float f_sampv[*sampc];

	convert_float(sampv, f_sampv, *sampc);

	ws_meter_process(vu->ch, f_sampv, (unsigned long)*sampc);

	return 0;
}


int webapp_vu_decode(struct aufilt_dec_st *st, int16_t *sampv, size_t *sampc)
{
	struct vumeter_dec *vu = (struct vumeter_dec *)st;
	float f_sampv[*sampc];

	convert_float(sampv, f_sampv, *sampc);

	ws_meter_process(vu->ch, f_sampv, (unsigned long)*sampc);

	return 0;
}
