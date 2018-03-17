/**
 * @file rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <rtaudio_c.h>

struct auplay_st {
	const struct auplay *ap;  /* pointer to base-class (inheritance) */
	bool run;
	void *write;
	int16_t *sampv;
	size_t sampc;
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
	char *device;
};

struct ausrc_st {
	const struct ausrc *as;  /* pointer to base-class (inheritance) */
	bool run;
	void *read;
	int16_t *sampv;
	size_t sampc;
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	char *device;
	rtaudio_t audio;
};

int webapp_ws_rtaudio_get_driver(void);
int webapp_ws_rtaudio_get_input(void);
int webapp_ws_rtaudio_get_output(void);

static struct ausrc *ausrc;
static struct auplay *auplay;

struct ausrc_st *st_src;
struct auplay_st *st_play;

int rtaudio_callback(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata);

int rtaudio_callback(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata) {

	unsigned int samples = nframes * 2;
	int16_t *outBuffer = (int16_t *) out;
	int16_t *inBuffer = (int16_t *) in;
	int16_t *inBuffer2 = (int16_t *) in;

	if (status == RTAUDIO_STATUS_INPUT_OVERFLOW) {
		warning("rtaudio: Buffer Overflow\n");
	}

	if (status == RTAUDIO_STATUS_OUTPUT_UNDERFLOW) {
		warning("rtaudio: Buffer Underrun\n");
	}

	for (uint32_t pos = 0; pos < samples; pos++) {
		st_src->sampv[pos] = *inBuffer++;
	}

	st_play->wh(st_play->sampv, samples, st_play->arg);
	for (uint32_t pos = 0; pos < samples; pos++) {
	//	*outBuffer++ = st_play->sampv[pos];
		//with echo
		*outBuffer++ = st_play->sampv[pos] + *inBuffer2++;
	}

	st_src->rh(st_src->sampv, samples, st_src->arg);


	return 0;
}


static void ausrc_destructor(void *arg)
{
	if (st_src->run) {
		rtaudio_stop_stream(st_src->audio);
		rtaudio_close_stream(st_src->audio);
	}
	rtaudio_destroy(st_src->audio);
	mem_deref(st_src->sampv);
	st_src = NULL;
}


static void auplay_destructor(void *arg)
{
	mem_deref(st_play->sampv);
	st_play = NULL;
}


static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct media_ctx **ctx,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	(void)ctx;
	(void)errh;
	(void)device;
	int err = 0;

	if (st_src)
		return 0;

	if (!stp || !as || !prm)
		return EINVAL;

	if ((st_src = mem_zalloc(sizeof(*st_src), ausrc_destructor)) == NULL)
		return ENOMEM;

	st_src->run = false;
	st_src->as  = as;
	st_src->rh  = rh;
	st_src->arg = arg;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_alloc(10 * st_src->sampc, NULL);
	if (!st_src->sampv) {
		err = ENOMEM;
		goto out;
	}


	st_src->audio = rtaudio_create(webapp_ws_rtaudio_get_driver());
	if (rtaudio_error(st_src->audio) != NULL) {
		err = 1;
		goto out;
	}

	rtaudio_stream_parameters_t out_params = {
		.device_id = webapp_ws_rtaudio_get_output(),
		.num_channels = 2,
		.first_channel = 0,
	};

	rtaudio_stream_parameters_t in_params = {
		.device_id = webapp_ws_rtaudio_get_input(),
		.num_channels = 2,
		.first_channel = 0,
	};

	unsigned int bufsz = 256;
	rtaudio_open_stream(st_src->audio, &out_params, &in_params,
			RTAUDIO_FORMAT_SINT16, 48000, &bufsz,
			rtaudio_callback, NULL, NULL, NULL);
	if (rtaudio_error(st_src->audio) != NULL) {
		err = 1;
		goto out;
	}

	rtaudio_start_stream(st_src->audio);
	if (rtaudio_error(st_src->audio) != NULL) {
		err = 1;
		goto out;
	}

out:
	if (err) {
		warning("error: %s\n", rtaudio_error(st_src->audio));
		mem_deref(st_src);
	}
	else {
		st_src->run = true;
	}

	*stp = st_src;

	return err;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	int err = 0;

	if (st_play)
		return 0;

	if (!stp || !ap || !prm)
		return EINVAL;

	if ((st_play = mem_zalloc(sizeof(*st_play),
					auplay_destructor)) == NULL)
		return ENOMEM;


	st_play->run = false;
	st_play->ap  = ap;
	st_play->wh  = wh;
	st_play->arg = arg;
	st_play->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_play->sampv = mem_alloc(10 * st_play->sampc, NULL);
	if (!st_play->sampv) {
		err = ENOMEM;
		goto out;
	}

out:
	if (err)
		mem_deref(st_play);
	else
		st_play->run = true;
	*stp = st_play;

	return err;

}


static int rtaudio_init(void)
{
	int err;
	err  = ausrc_register(&ausrc, baresip_ausrcl(), "rtaudio", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(),
			"rtaudio", play_alloc);

	return err;
}


static int rtaudio_close(void)
{
	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);

	return 0;
}


const struct mod_export DECL_EXPORTS(rtaudio) = {
	"rtaudio",
	"sound",
	rtaudio_init,
	rtaudio_close
};
