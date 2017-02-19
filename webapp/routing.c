#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "webapp.h"


struct routing_enc {
	struct aufilt_enc_st af;  /* inheritance */
};

struct routing_dec {
	struct aufilt_dec_st af;  /* inheritance */
};


static void routing_enc_destructor(void *arg)
{
	struct routing_enc *st = arg;
	list_unlink(&st->af.le);
}


static void routing_dec_destructor(void *arg)
{
	struct routing_dec *st = arg;
	list_unlink(&st->af.le);
}


int webapp_routing_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct routing_enc *st;
	int err = 0;
	(void)ctx;
	(void)af;

	st = mem_zalloc(sizeof(*st), routing_enc_destructor);
	if (!st)
		return EINVAL;

	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_enc_st *)st;

	return err;
}


int webapp_routing_decode_update(struct aufilt_dec_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct routing_dec *st;
	int err = 0;
	(void)ctx;
	(void)af;

	st = mem_zalloc(sizeof(*st), routing_dec_destructor);
	if (!st)
		return EINVAL;

	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_dec_st *)st;

	return err;
}


int webapp_routing_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	struct routing_enc *sf = (struct routing_enc *)st;

	return 0;
}


int webapp_routing_decode(struct aufilt_dec_st *st, int16_t *sampv, size_t *sampc)
{
	struct routing_dec *sf = (struct routing_dec *)st;

	return 0;
}
