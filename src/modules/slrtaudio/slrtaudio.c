/**
 * @file rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018-2019 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <rtaudio_c.h>
#include "slrtaudio.h"

enum {
	MAX_REMOTE_CHANNELS = 5,
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

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
	struct session *sess;
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
	struct session *sess;
};

static struct ausrc *ausrc;
static struct auplay *auplay;


static rtaudio_t audio;
static int driver = -1;
static int input = -1;
static int output = -1;
static struct odict *interfaces = NULL;
struct list sessionl;

static int slrtaudio_stop(void);
static int slrtaudio_start(void);
static int slrtaudio_devices(void);
static int slrtaudio_drivers(void);
void webapp_ws_rtaudio_sync(void);

static int32_t *playmix;

static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	if (sess->run_record) {
		sess->run_record = false;
		(void)pthread_join(sess->record_thread, NULL);
	}

	if (sess->flac) {
		FLAC__stream_encoder_finish(sess->flac);
		FLAC__stream_encoder_delete(sess->flac);
	}

	mem_deref(sess->sampv);
	mem_deref(sess->pcm);
	mem_deref(sess->aubuf);

	list_unlink(&sess->le);
}


const struct odict* slrtaudio_get_interfaces(void);
const struct odict* slrtaudio_get_interfaces(void)
{
	return (const struct odict *)interfaces;
}


static int slrtaudio_reset(void)
{
	int err;

	if(interfaces)
		mem_deref(interfaces);

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	slrtaudio_stop();
	slrtaudio_drivers();
	slrtaudio_devices();
	webapp_ws_rtaudio_sync();
	slrtaudio_start();

	return err;
}


void slrtaudio_set_driver(int value);
void slrtaudio_set_driver(int value)
{
	driver = value;
	slrtaudio_reset();
}


void slrtaudio_set_input(int value);
void slrtaudio_set_input(int value)
{
	input = value;
	slrtaudio_reset();
}


void slrtaudio_set_output(int value);
void slrtaudio_set_output(int value)
{
	output = value;
	slrtaudio_reset();
}


static void convert_float_mono(int16_t *sampv, float *f_sampv, size_t sampc)
{
	const float scaling = 1.0/32767.0f;
	int16_t i;
	float f;

	while (sampc--) {
		i = sampv[0] + sampv[1];
		f = i * scaling;
		f_sampv[0] = f;
		f_sampv[1] = f;
		++f_sampv;
		++sampv;
	}
}


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);

int slrtaudio_callback(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata);

int slrtaudio_callback(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata) {
	unsigned int samples = nframes * 2;
	int16_t *outBuffer = (int16_t *) out;
	int16_t *inBuffer = (int16_t *) in;
	float vumeter[samples];
	struct le *le;
	struct le *mle;
	struct session *sess;
	struct auplay_st *st_play;
	struct auplay_st *mst_play;
	struct ausrc_st *st_src;
	int cntplay = 0;
	int msessplay = 0;


	if (status == RTAUDIO_STATUS_INPUT_OVERFLOW) {
		warning("rtaudio: Buffer Overflow\n");
	}

	if (status == RTAUDIO_STATUS_OUTPUT_UNDERFLOW) {
		warning("rtaudio: Buffer Underrun\n");
	}
	
	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;
		msessplay = 0;

		if (!sess->run_play || sess->local) {
			continue;
		}

		st_play = sess->st_play;
		st_play->wh(st_play->sampv, samples, st_play->arg);
	}
	
	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;
		msessplay = 0;

		if (!sess->run_play || sess->local) {
			continue;
		}

		st_play = sess->st_play;

		for (uint32_t pos = 0; pos < samples; pos++) {
			if (cntplay < 1) {
				playmix[pos] = st_play->sampv[pos];
			} else {
				playmix[pos] = playmix[pos] + st_play->sampv[pos];
			}
		}

		/* write remote streams to flac record buffer */
		(void)aubuf_write_samp(sess->aubuf, st_play->sampv, samples);

		//mix n-1
		for (mle = sessionl.head; mle; mle = mle->next) {
			struct session *msess = mle->data;

			if (!msess->run_play || msess == sess || sess->local) {
				continue;
			}
			mst_play = msess->st_play;

			for (uint32_t pos = 0; pos < samples; pos++) {
				if (msessplay < 1) {
					sess->dstmix[pos] = mst_play->sampv[pos];
				} else {
					sess->dstmix[pos] = mst_play->sampv[pos] + sess->dstmix[pos];
				}
			}
			msessplay++;
		}
		cntplay++;
	}

	for (le = sessionl.head; le; le = le->next) {
		sess = le->data;
		if (sess->run_src && !sess->local) {
			st_src = sess->st_src;

			for (uint32_t pos = 0; pos < samples; pos++) {
				st_src->sampv[pos] = inBuffer[pos] + sess->dstmix[pos];
			}

			st_src->rh(st_src->sampv, samples, st_src->arg);
		}

		if (sess->local) {
			/* write local audio to flac record buffer */
			(void)aubuf_write_samp(sess->aubuf, inBuffer, samples);
		}
	}

	if (!cntplay) {
		for (uint32_t pos = 0; pos < samples; pos++) {
			playmix[pos] = 0;
		}
	}
	
	for (uint32_t pos = 0; pos < samples; pos++) {
		*outBuffer++ = playmix[pos];
	}

	//vumeter

	convert_float_mono(inBuffer, vumeter, samples);

	ws_meter_process(0, vumeter, (unsigned long)samples);


	return 0;
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	warning("DESTRUCT ausrc\n");
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	warning("DESTRUCT auplay\n");
	sess->run_play = false;
	sys_msleep(20);
	mem_deref(st->sampv);

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
	struct ausrc_st *st_src = NULL;
	struct le *le;

	if (!stp || !as || !prm)
		return EINVAL;

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!sess->run_src && !sess->local) {
			sess->st_src = mem_zalloc(sizeof(*st_src),
					ausrc_destructor);
			if (!sess->st_src)
				return ENOMEM;
			st_src = sess->st_src;
			st_src->sess = sess;
			sess->run_src = true;
			break;
		}
	}

	if (!st_src)
		return EINVAL;

	st_src->run = false;
	st_src->as  = as;
	st_src->rh  = rh;
	st_src->arg = arg;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_alloc(10 * st_src->sampc, NULL);
	if (!st_src->sampv) {
		err = ENOMEM;
	}

	if (err) {
		mem_deref(st_src);
		return err;
	}

	st_src->run = true;
	*stp = st_src;

	return err;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	int err = 0;
	struct auplay_st *st_play = NULL;
	struct le *le;

	if (!stp || !ap || !prm)
		return EINVAL;

	size_t sampc = prm->srate * prm->ch * prm->ptime / 1000;

	for (le = sessionl.head; le; le = le->next) {
		struct session *sess = le->data;

		if (!sess->run_play && !sess->local) {
			sess->st_play = mem_zalloc(sizeof(*st_play),
					auplay_destructor);

			sess->dstmix = mem_zalloc(20 * sampc, NULL);
			if (!sess->st_play)
				return ENOMEM;
			st_play = sess->st_play;
			st_play->sess = sess;
			sess->run_play = true;
			break;
		}
	}

	if (!st_play)
		return EINVAL;

	st_play->run = false;
	st_play->ap  = ap;
	st_play->wh  = wh;
	st_play->arg = arg;
	st_play->sampc = sampc;
	st_play->sampv = mem_alloc(10 * st_play->sampc, NULL);
	if (!st_play->sampv) {
		err = ENOMEM;
	}

	if (err) {
		mem_deref(st_play);
		return err;
	}

	st_play->run = true;
	*stp = st_play;

	return err;
}


static int slrtaudio_drivers(void) {
	int err;
	rtaudio_api_t const *apis = rtaudio_compiled_api();
	struct odict *o;
	struct odict *array;
	char idx[2];

	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;


	for(unsigned int i = 0; i < sizeof(apis)/sizeof(rtaudio_api_t); i++) {
		(void)re_snprintf(idx, sizeof(idx), "%d", i);
		int detected = 0;

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		if(apis[i] == RTAUDIO_API_LINUX_PULSE) {
			warning("driver detected PULSE\n");
			if (driver == -1) {
				driver = RTAUDIO_API_LINUX_PULSE;
			}
			odict_entry_add(o, "display", ODICT_STRING, "Pulseaudio");
			detected = 1;
		}
		if(apis[i] == RTAUDIO_API_LINUX_ALSA) {
			warning("driver detected ALSA\n");
			odict_entry_add(o, "display", ODICT_STRING, "ALSA");
			if (driver == -1) {
				driver = RTAUDIO_API_LINUX_ALSA;
			}
			detected = 1;
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_WASAPI) {
			odict_entry_add(o, "display", ODICT_STRING, "WASAPI");
			if (driver == -1) {
				driver = RTAUDIO_API_WINDOWS_WASAPI;
			}
			detected = 1;
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_DS) {
			odict_entry_add(o, "display", ODICT_STRING, "DirectSound");
			detected = 1;
		}
		if(apis[i] == RTAUDIO_API_WINDOWS_ASIO) {
			odict_entry_add(o, "display", ODICT_STRING, "ASIO");
			detected = 1;
		}
		if(apis[i] == RTAUDIO_API_MACOSX_CORE) {
			odict_entry_add(o, "display", ODICT_STRING, "Coreaudio");
			if (driver == -1) {
				driver = RTAUDIO_API_MACOSX_CORE;
			}
			detected = 1;
		}

		if(apis[i] == (unsigned int)driver) {
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		} else {
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		if (detected) {
			odict_entry_add(o, "id", ODICT_INT, apis[i]);
			odict_entry_add(array, idx, ODICT_OBJECT, o);
		}

		mem_deref(o);

	}
	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

	return 0;
}


static int slrtaudio_devices(void) {
	int err = 0;
	struct odict *o_in;
	struct odict *o_out;
	struct odict *array_in;
	struct odict *array_out;
	char idx[2];

	rtaudio_device_info_t info;
	audio = rtaudio_create(driver);

	err = odict_alloc(&array_in, DICT_BSIZE);
	if (err)
		goto out2;
	err = odict_alloc(&array_out, DICT_BSIZE);
	if (err)
		goto out2;

	for (int i = 0; i < rtaudio_device_count(audio); i++) {
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		info = rtaudio_get_device_info(audio, i);
		if (rtaudio_error(audio) != NULL) {
			//re_snprintf(errmsg, sizeof(errmsg), "%s", rtaudio_error(audio));
			//warning("rtaudio error: %s\n", errmsg);
			err = 1;
			goto out1;
		}

		if (!info.probed)
			continue;

		warning("%c%d: %s: %d\n",
				info.is_default_input ? '*' : ' ', i,
				info.name, info.preferred_sample_rate);
		warning("%c%d: %s: %d\n",
				info.is_default_output ? '*' : ' ', i,
				info.name, info.preferred_sample_rate);

		err = odict_alloc(&o_in, DICT_BSIZE);
		if (err)
			goto out1;
		err = odict_alloc(&o_out, DICT_BSIZE);
		if (err)
			goto out1;

		if (output == -1 && info.is_default_output) {
			output = i;
		}

		if (input == -1 && info.is_default_input) {
			input = i;
		}

		if (info.output_channels > 0) {
			if (output == i) {
				odict_entry_add(o_out, "selected", ODICT_BOOL, true);
			} else {
				odict_entry_add(o_out, "selected", ODICT_BOOL, false);
			}
			odict_entry_add(o_out, "id", ODICT_INT, i);
			odict_entry_add(o_out, "display", ODICT_STRING, info.name);
			odict_entry_add(array_out, idx, ODICT_OBJECT, o_out);
		}
		
		if (info.input_channels > 0) {
			if (input == i) {
				odict_entry_add(o_in, "selected", ODICT_BOOL, true);
			} else {
				odict_entry_add(o_in, "selected", ODICT_BOOL, false);
			}
			odict_entry_add(o_in, "id", ODICT_INT, i);
			odict_entry_add(o_in, "display", ODICT_STRING, info.name);
			odict_entry_add(array_in, idx, ODICT_OBJECT, o_in);
		}

		mem_deref(o_out);
		mem_deref(o_in);
	}

	odict_entry_add(interfaces, "input", ODICT_ARRAY, array_in);
	odict_entry_add(interfaces, "output", ODICT_ARRAY, array_out);

out1:
	mem_deref(array_in);
	mem_deref(array_out);

out2:
	rtaudio_destroy(audio);
	audio = NULL;

	return err;
}


static int slrtaudio_start(void)
{
	int err = 0;
	char errmsg[512];
	unsigned int bufsz = 1024; 

	audio = rtaudio_create(driver);
	if (rtaudio_error(audio) != NULL) {
		err = ENODEV;
		goto out;
	}

	rtaudio_stream_parameters_t out_params = {
		.device_id = output,
		.num_channels = 2,
		.first_channel = 0,
	};

	rtaudio_stream_parameters_t in_params = {
		.device_id = input,
		.num_channels = 2,
		.first_channel = 0,
	};

	rtaudio_stream_options_t options = {
		.flags = RTAUDIO_FLAGS_SCHEDULE_REALTIME + RTAUDIO_FLAGS_MINIMIZE_LATENCY,
	};

	rtaudio_open_stream(audio, &out_params, &in_params,
			RTAUDIO_FORMAT_SINT16, 48000, &bufsz,
			slrtaudio_callback, NULL, &options, NULL);
	if (rtaudio_error(audio) != NULL) {
		err = EINVAL;
		goto out;
	}

	rtaudio_start_stream(audio);
	if (rtaudio_error(audio) != NULL) {
		err = EINVAL;
		goto out;
	}

out:
	if (err) {
		re_snprintf(errmsg, sizeof(errmsg), "%s",
				rtaudio_error(audio));
		warning("error: %s\n", errmsg);
	//	webapp_ws_rtaudio_set_err(errmsg);
	}

	return err;
}


static int slrtaudio_stop(void)
{
	if (audio) {
		rtaudio_stop_stream(audio);
		rtaudio_close_stream(audio);
		rtaudio_destroy(audio);
		audio = NULL;
	}

	return 0;
}


static int slrtaudio_init(void)
{
	int err;
	struct session *sess;

	err  = ausrc_register(&ausrc, baresip_ausrcl(), "slrtaudio", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(),
			"slrtaudio", play_alloc);

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	playmix = mem_zalloc(10 * 1920, NULL);
	if (!playmix)
		return ENOMEM;


	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return ENOMEM;
	sess->local = true;
	list_append(&sessionl, &sess->le, sess);

	for (uint32_t cnt = 0; cnt < MAX_REMOTE_CHANNELS; cnt++) {
		sess = mem_zalloc(sizeof(*sess), sess_destruct);
		if (!sess)
			return ENOMEM;

		sess->local = false;
		list_append(&sessionl, &sess->le, sess);
	}

	slrtaudio_drivers();
	slrtaudio_devices();
	slrtaudio_record_init();
	slrtaudio_start();

	warning("slrtaudio\n");

	return err;
}


static int slrtaudio_close(void)
{
	struct le *le;
	struct session *sess;

	slrtaudio_stop();

	ausrc  = mem_deref(ausrc);
	auplay = mem_deref(auplay);
	interfaces = mem_deref(interfaces);

	for (le = sessionl.head; le;) {
		sess = le->data;
		le = le->next;
		mem_deref(sess);
	}

	playmix = mem_deref(playmix);

	return 0;
}


const struct mod_export DECL_EXPORTS(slrtaudio) = {
	"slrtaudio",
	"sound",
	slrtaudio_init,
	slrtaudio_close
};
