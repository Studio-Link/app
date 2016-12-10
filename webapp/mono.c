#include <re.h>
#include <baresip.h>
#include "webapp.h"

static bool mono = false;

struct mono_enc {
	struct aufilt_enc_st af;  /* inheritance */
};


static void mono_enc_destructor(void *arg)
{
	struct mono_enc *st = arg;
	list_unlink(&st->af.le);
}


static void downsample_stereo2mono(int16_t *outv, const int16_t *inv,
		size_t inc)
{
	unsigned ratio = 2;
	int16_t i;

	while (inc >= 1) {

		i = inv[0] + inv[1];
		outv[0] = i;
		outv[1] = i;

		outv += ratio;
		inv += ratio;
		inc -= ratio;
	}
}


int webapp_mono_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct mono_enc *st;
	(void)ctx;
	(void)prm;

	if (!stp || !af)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), mono_enc_destructor);
	if (!st)
		return ENOMEM;

	*stp = (struct aufilt_enc_st *)st;

	return 0;
}


int webapp_mono_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	if (mono)
		downsample_stereo2mono(sampv, sampv, *sampc);

	return 0;
}


void webapp_mono_set(bool active)
{
	mono = active;
}
