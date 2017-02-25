#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "webapp.h"

#define SAMPLE_16BIT_MAX  32767
#define SAMPLE_16BIT_MIN  -32767

struct session {
	struct le le;
	int16_t *sampv;
	uint32_t trev;
	uint32_t prev;
	int32_t *dstmix;
	struct lock *plock;
	bool run_src;
	bool run_play;
};

struct routing_enc {
	struct aufilt_enc_st af;  /* inheritance */
	struct session *sess;
};

struct routing_dec {
	struct aufilt_dec_st af;  /* inheritance */
	struct session *sess;
};

static struct list sessionl;



static void routing_enc_destructor(void *arg)
{
	struct routing_enc *st = arg;

	list_unlink(&st->af.le);
}


static void routing_dec_destructor(void *arg)
{
	struct routing_dec *st = arg;

	mem_deref(st->sess);
	list_unlink(&st->af.le);
}


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	list_unlink(&sess->le);
}


static int routing_alloc(struct session **stp, void **ctx, struct aufilt_prm *prm)
{
	struct session *sess;
	int err = 0;

	if (!stp || !ctx || !prm)
		return EINVAL;

	if (*ctx) {
		*stp = mem_ref(*ctx);
		return 0;
	}

	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return ENOMEM;

	sess->dstmix = mem_zalloc(20 * 1920, NULL);
	if (!sess->dstmix) {
		err = ENOMEM;
		goto out;
	}

	sess->sampv = mem_zalloc(20 * 1920, NULL);
	if (!sess->sampv) {
		err = ENOMEM;
		goto out;
	}

	sess->trev = 0;
	sess->prev = 0;
	lock_alloc(&sess->plock);

	list_append(&sessionl, &sess->le, sess);

out:
	if (err)
		mem_deref(sess);
	else
		*ctx = *stp = sess;

	return err;
}


int webapp_routing_encode_update(struct aufilt_enc_st **stp, void **ctx,
			 const struct aufilt *af, struct aufilt_prm *prm)
{
	struct routing_enc *st;
	int err = 0;

	if (!stp || !ctx || !af || !prm)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), routing_enc_destructor);
	if (!st)
		return ENOMEM;
	
	err = routing_alloc(&st->sess, ctx, prm);

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

	if (!stp || !ctx || !af || !prm)
		return EINVAL;

	if (*stp)
		return 0;

	st = mem_zalloc(sizeof(*st), routing_dec_destructor);
	if (!st)
		return ENOMEM;

	err = routing_alloc(&st->sess, ctx, prm);

	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_dec_st *)st;

	return err;
}


int webapp_routing_encode(struct aufilt_enc_st *st, int16_t *sampv, size_t *sampc)
{
	struct routing_enc *sf = (struct routing_enc *)st;
	struct session *sess = sf->sess;
	struct le *le;
	int32_t *dstmixv;
	int16_t *dstv = sampv;
	int16_t *mixv;
	unsigned active = 0;

	//Mix this with play sessions
	for (le = sessionl.head; le; le = le->next) {
		struct session *msess = le->data;

		if (msess != sess) {
			mixv = msess->sampv;
			dstmixv = sess->dstmix;

			for (unsigned n = 0; n < *sampc; n++) {
				*dstmixv = *dstmixv + *mixv;
				++mixv;
				++dstmixv;
			}

			++active;
		}
	}

	//Mix only if other calls active
	if (!active)
		return 0;

	dstmixv = sess->dstmix;
	for (unsigned i = 0; i < *sampc; i++) {
		*dstmixv = *dstmixv + *dstv;
		if (*dstmixv > SAMPLE_16BIT_MAX)
			*dstmixv = SAMPLE_16BIT_MAX;
		if (*dstmixv < SAMPLE_16BIT_MIN)
			*dstmixv = SAMPLE_16BIT_MIN;
		*dstv = *dstmixv;
		++dstv;
		++dstmixv;
	}

	return 0;
}


int webapp_routing_decode(struct aufilt_dec_st *st, int16_t *sampv, size_t *sampc)
{
	struct routing_dec *sf = (struct routing_dec *)st;
	int16_t *org_sampv = sampv;
	int16_t *sess_sampv = sf->sess->sampv;

	//Save this to mix
	for (unsigned i = 0; i < *sampc; i++) {
		*sess_sampv = *org_sampv;
		++org_sampv;
		++sess_sampv;
	}

	return 0;
}
