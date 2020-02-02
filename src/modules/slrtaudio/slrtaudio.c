/**
 * @file rtaudio.c RTAUDIO
 *
 * Copyright (C) 2018-2020 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <rtaudio_c.h>
#include <samplerate.h>
#include "slrtaudio.h"

#define BUFFER_LEN 7680 /* max buffer_len = 192kHz*2ch*20ms */

enum
{
	MAX_REMOTE_CHANNELS = 6,
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

struct slrtaudio_st
{
	int16_t *inBuffer;
	int16_t *outBufferTmp;
	float *inBufferFloat;
	float *inBufferOutFloat;
	float *outBufferFloat;
	float *outBufferInFloat;
};

static struct slrtaudio_st *slrtaudio;

struct auplay_st
{
	const struct auplay *ap; /* pointer to base-class (inheritance) */
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

struct ausrc_st
{
	const struct ausrc *as; /* pointer to base-class (inheritance) */
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

static rtaudio_t audio_in = NULL;
static rtaudio_t audio_out = NULL;
static int driver = -1;
static int input = -1;
static int input_channels = 2;
static int output_channels = 2;
static int first_input_channel = 0;
static int preferred_sample_rate_in = 0;
static int preferred_sample_rate_out = 0;
static int output = -1;
static struct odict *interfaces = NULL;
struct list sessionl;
static SRC_STATE *src_state_in;
static SRC_STATE *src_state_out;
static unsigned int samples = 0;
static struct lock *rtaudio_lock;
static bool mismatch_samplerates = false;

static int slrtaudio_stop(void);
static int slrtaudio_start(void);
static int slrtaudio_devices(void);
static int slrtaudio_drivers(void);
void webapp_ws_rtaudio_sync(void);

static int32_t *playmix;

static bool mono = false;
static bool mute = false;


void slrtaudio_mono_set(bool active)
{
	mono = active;
}


void slrtaudio_mute_set(bool active)
{
	mute = active;
}

struct list* sl_sessions(void);
struct list* sl_sessions(void)
{
	return &sessionl;
}

static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	if (sess->run_record)
	{
		sess->run_record = false;
		(void)pthread_join(sess->record_thread, NULL);
	}

	if (sess->flac)
	{
		FLAC__stream_encoder_finish(sess->flac);
		FLAC__stream_encoder_delete(sess->flac);
		sess->flac = NULL;
	}

	mem_deref(sess->sampv);
	mem_deref(sess->pcm);
	mem_deref(sess->aubuf);
	mem_deref(sess->vumeter);

	list_unlink(&sess->le);
}


const struct odict *slrtaudio_get_interfaces(void)
{
	return (const struct odict *)interfaces;
}


static int slrtaudio_reset(void)
{
	int err;

	if (interfaces)
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


void slrtaudio_set_driver(int value)
{
	driver = value;
	output = -1;
	input = -1;
	slrtaudio_reset();
}


void slrtaudio_set_input(int value)
{
	input = value;
	slrtaudio_reset();
}


void slrtaudio_set_first_input_channel(int value)
{
	first_input_channel = value;
	slrtaudio_reset();
}


void slrtaudio_set_output(int value)
{
	output = value;
	slrtaudio_reset();
}


static void convert_float(int16_t *sampv, float *f_sampv, size_t sampc)
{
	const float scaling = 1.0 / 32767.0f;

	while (sampc--)
	{
		f_sampv[0] = sampv[0] * scaling;
		++f_sampv;
		++sampv;
	}
}


static void downsample_first_ch(int16_t *outv, const int16_t *inv, size_t inc)
{
	unsigned ratio = input_channels;
	int16_t mix = 0;

	/** Mix Channels */
	if (first_input_channel == 99) {
		while (inc >= 1)
		{
			mix = 0;
			for (uint16_t ch = 0; ch < input_channels; ch++)
			{
				mix += inv[ch];
			}
			outv[0] = mix;
			outv[1] = mix;

			outv += 2;
			inv += ratio;
			inc -= ratio;
		}
		return;
	}

	while (inc >= 1)
	{
		outv[0] = inv[first_input_channel];
		outv[1] = inv[first_input_channel];

		outv += 2;
		inv += ratio;
		inc -= ratio;
	}
}


static void convert_out_channels(int16_t *outv, const int16_t *inv, size_t inc)
{
	while (inc >= 1)
	{
		outv[0] = inv[0];
		outv[1] = inv[1];

		outv += 2;

		for (uint16_t ch = 0; ch < output_channels - 2; ch++)
		{
			outv += 1;
		}

		inv += 2;
		inc -= 2;
	}


}


int slrtaudio_callback_in(void *out, void *in, unsigned int nframes,
			double stream_time, rtaudio_stream_status_t status,
			void *userdata)
{
	unsigned int in_samples = nframes * 2;
	struct le *le;
	struct le *mle;
	struct session *sess;
	struct auplay_st *st_play;
	struct auplay_st *mst_play;
	struct ausrc_st *st_src;
	int cntplay = 0, msessplay = 0, sessplay = 0, error = 0;

	SRC_DATA src_data_in;
	lock_write_get(rtaudio_lock);

	if (status == RTAUDIO_STATUS_INPUT_OVERFLOW)
	{
		warning("slrtaudio: Buffer Overflow frames: %d\n", nframes);
	}

	if (status == RTAUDIO_STATUS_OUTPUT_UNDERFLOW)
	{
		warning("slrtaudio: Buffer Underrun frames: %d\n", nframes);
	}

	downsample_first_ch(slrtaudio->inBuffer, in, nframes * input_channels);

	if (mute)
	{
		for (uint16_t pos = 0; pos < in_samples; pos++)
		{
			slrtaudio->inBuffer[pos] = 0;
		}
	}

	/** vumeter */
	convert_float(slrtaudio->inBuffer,
			slrtaudio->inBufferFloat, in_samples);
	ws_meter_process(0, slrtaudio->inBufferFloat,
			(unsigned long)in_samples);

	samples = nframes * 2;

	/**<-- Input Samplerate conversion */
	if (preferred_sample_rate_in != 48000)
	{
		if (!src_state_in)
			return 1;

		src_data_in.data_in = slrtaudio->inBufferFloat;
		src_data_in.data_out = slrtaudio->inBufferOutFloat;
		src_data_in.input_frames = nframes;
		src_data_in.output_frames = BUFFER_LEN / 2;
		src_data_in.src_ratio =
				48000 / (double)preferred_sample_rate_in;
		src_data_in.end_of_input = 0;

		if ((error = src_process(src_state_in, &src_data_in)) != 0)
		{
			error_msg("slrtaudio: Samplerate::src_process_in :"
					"returned error : %s %f\n",
					src_strerror(error),
					src_data_in.src_ratio);
			return 1;
		};
		samples = src_data_in.output_frames_gen * 2;
		auconv_to_s16(slrtaudio->inBuffer, AUFMT_FLOAT,
				slrtaudio->inBufferOutFloat,
				samples);
	}
	/** Input Samplerate conversion -->*/

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (!sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;
		st_play->wh(st_play->sampv, samples, st_play->arg);
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		msessplay = 0;

		if (!sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;

		for (uint16_t pos = 0; pos < samples; pos++)
		{
			if (cntplay < 1)
			{
				playmix[pos] = st_play->sampv[pos];
			}
			else
			{
				playmix[pos] =
					playmix[pos] + st_play->sampv[pos];
			}
		}

		/* write remote streams to flac record buffer */
		(void)aubuf_write_samp(sess->aubuf, st_play->sampv, samples);

		/* vumeter */
		if (!sess->stream) {
			convert_float(st_play->sampv,
					sess->vumeter, samples);
			ws_meter_process(sess->ch, sess->vumeter,
					  (unsigned long)samples);
		}

		/* mix n-1 */
		for (mle = sessionl.head; mle; mle = mle->next)
		{
			struct session *msess = mle->data;

			if (!msess->run_play || msess == sess || sess->local)
			{
				continue;
			}
			mst_play = msess->st_play;

			for (uint16_t pos = 0; pos < samples; pos++)
			{
				if (msessplay < 1)
				{
					sess->dstmix[pos] =
						mst_play->sampv[pos];
				}
				else
				{
					sess->dstmix[pos] =
						mst_play->sampv[pos] +
						sess->dstmix[pos];
				}
			}
			++msessplay;
			++sessplay;
		}
		++cntplay;
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (sess->run_src && !sess->local)
		{
			st_src = sess->st_src;

			for (uint16_t pos = 0; pos < samples; pos++)
			{
				if (!sessplay) {
					/* ignore on last call the dstmix */
					st_src->sampv[pos] =
						slrtaudio->inBuffer[pos];
				}
				else
				{
					st_src->sampv[pos] =
						slrtaudio->inBuffer[pos] +
						sess->dstmix[pos];
				}
			}

			st_src->rh(st_src->sampv, samples, st_src->arg);
		}

		if (sess->local)
		{
			/* write local audio to flac record buffer */
			(void)aubuf_write_samp(sess->aubuf,
					slrtaudio->inBuffer, samples);
		}
	}

	if (!cntplay)
	{
		for (uint16_t pos = 0; pos < samples; pos++)
		{
			playmix[pos] = 0;
		}
	}

	lock_rel(rtaudio_lock);

	if (!mismatch_samplerates)
		slrtaudio_callback_out(out, in, nframes,
				stream_time, status, userdata);

	return 0;
}


int slrtaudio_callback_out(void *out, void *in, unsigned int nframes,
			double stream_time, rtaudio_stream_status_t status,
			void *userdata)
{
	int16_t *outBuffer = (int16_t *)out;
	SRC_DATA src_data_out;
	int error;

	lock_write_get(rtaudio_lock);

	if (preferred_sample_rate_out != 48000)
	{
		for (uint16_t pos = 0; pos < samples; pos++)
		{
			slrtaudio->outBufferTmp[pos] = playmix[pos];
		}

		convert_float(slrtaudio->outBufferTmp,
				slrtaudio->outBufferFloat, samples);

		src_data_out.data_in = slrtaudio->outBufferFloat;
		src_data_out.data_out = slrtaudio->outBufferInFloat;
		src_data_out.input_frames = samples / 2;
		src_data_out.output_frames = nframes;
		src_data_out.src_ratio =
			(double)preferred_sample_rate_out / 48000;
		src_data_out.end_of_input = 0;

		if (!src_state_out)
			return 1;

		if ((error = src_process(src_state_out, &src_data_out)) != 0)
		{
			error_msg("slrtaudio: Samplerate::src_process_out :"
				"returned error : %s\n", src_strerror(error));
			return 1;
		};
		if (output_channels > 2) {
			auconv_to_s16(slrtaudio->outBufferTmp, AUFMT_FLOAT,
					slrtaudio->outBufferInFloat,
					src_data_out.output_frames_gen * 2);
		}
		else
		{
			auconv_to_s16(outBuffer, AUFMT_FLOAT,
					slrtaudio->outBufferInFloat,
					src_data_out.output_frames_gen * 2);
		}
	}
	else
	{
		if (output_channels > 2) {
			for (uint16_t pos = 0; pos < nframes * 2; pos++)
			{
				slrtaudio->outBufferTmp[pos] = playmix[pos];
			}
		}
		else
		{
			for (uint16_t pos = 0; pos < nframes * 2; pos++)
			{
				outBuffer[pos] = playmix[pos];
			}
		}
	}

	if (output_channels > 2) {
		convert_out_channels(outBuffer, slrtaudio->outBufferTmp,
					nframes * 2);
	}

	lock_rel(rtaudio_lock);
	return 0;
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	info("slrtaudio: destruct ausrc\n");
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	info("slrtaudio: destruct auplay\n");
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

	for (le = sessionl.head; le; le = le->next)
	{
		struct session *sess = le->data;

		if (!sess->run_src && !sess->local && sess->call)
		{
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
	st_src->as = as;
	st_src->rh = rh;
	st_src->arg = arg;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_zalloc(10 * st_src->sampc, NULL);
	if (!st_src->sampv)
	{
		err = ENOMEM;
	}

	if (err)
	{
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

	for (le = sessionl.head; le; le = le->next)
	{
		struct session *sess = le->data;

		if (!sess->run_play && !sess->local && sess->call)
		{
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
	st_play->ap = ap;
	st_play->wh = wh;
	st_play->arg = arg;
	st_play->sampc = sampc;
	st_play->sampv = mem_zalloc(10 * st_play->sampc, NULL);
	if (!st_play->sampv)
	{
		err = ENOMEM;
	}

	if (err)
	{
		mem_deref(st_play);
		return err;
	}

	st_play->run = true;
	*stp = st_play;

	return err;
}


static int slrtaudio_drivers(void)
{
	int err;
	rtaudio_api_t const *apis = rtaudio_compiled_api();
	struct odict *o;
	struct odict *array;
	char idx[2];

	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;

	for (unsigned int i = 0; i < rtaudio_get_num_compiled_apis(); i++)
	{
		(void)re_snprintf(idx, sizeof(idx), "%d", i);
		int detected = 0;

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		if (apis[i] == RTAUDIO_API_LINUX_PULSE)
		{
			info("slrtaudio: driver detected PULSE\n");
			if (driver == -1)
			{
				driver = RTAUDIO_API_LINUX_PULSE;
			}
			odict_entry_add(o, "display", ODICT_STRING,
					"Pulseaudio");
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_LINUX_ALSA)
		{
			info("slrtaudio: driver detected ALSA\n");
			odict_entry_add(o, "display", ODICT_STRING, "ALSA");
			if (driver == -1)
			{
				driver = RTAUDIO_API_LINUX_ALSA;
			}
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_UNIX_JACK)
		{
			info("slrtaudio: driver detected JACK\n");
			odict_entry_add(o, "display", ODICT_STRING, "JACK");
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_WINDOWS_WASAPI)
		{
			info("slrtaudio: driver detected WASAPI\n");
			odict_entry_add(o, "display", ODICT_STRING, "WASAPI");
			if (driver == -1)
			{
				driver = RTAUDIO_API_WINDOWS_WASAPI;
			}
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_WINDOWS_DS)
		{
			info("slrtaudio: driver detected Directsound\n");
			odict_entry_add(o, "display", ODICT_STRING,
					"DirectSound");
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_WINDOWS_ASIO)
		{
			info("slrtaudio: driver detected ASIO\n");
			odict_entry_add(o, "display", ODICT_STRING, "ASIO");
			detected = 1;
		}
		if (apis[i] == RTAUDIO_API_MACOSX_CORE)
		{
			info("slrtaudio: driver detected Coreaudio\n");
			odict_entry_add(o, "display", ODICT_STRING,
					"Coreaudio");
			if (driver == -1)
			{
				driver = RTAUDIO_API_MACOSX_CORE;
			}
			detected = 1;
		}

		if (apis[i] == (unsigned int)driver)
		{
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		}
		else
		{
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		if (detected)
		{
			odict_entry_add(o, "id", ODICT_INT, (int64_t)apis[i]);
			odict_entry_add(array, idx, ODICT_OBJECT, o);
		}

		mem_deref(o);
	}
	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

	return 0;
}


static int slrtaudio_devices(void)
{
	int err = 0;
	struct odict *o_in;
	struct odict *o_out;
	struct odict *array_in;
	struct odict *array_out;
	char idx[2];
	rtaudio_t audio;
	rtaudio_device_info_t device_info;
	char errmsg[512];

	audio = rtaudio_create(driver);

	err = odict_alloc(&array_in, DICT_BSIZE);
	if (err)
		goto out2;
	err = odict_alloc(&array_out, DICT_BSIZE);
	if (err)
		goto out2;

	for (int i = 0; i < rtaudio_device_count(audio); i++)
	{
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		device_info = rtaudio_get_device_info(audio, i);
		if (rtaudio_error(audio) != NULL)
		{
			re_snprintf(errmsg, sizeof(errmsg), "%s",
					rtaudio_error(audio));
			error_msg("slrtaudio rtaudio_error: %s\n", errmsg);
			err = 1;
			goto out1;
		}

		if (!device_info.probed)
			continue;

		err = odict_alloc(&o_in, DICT_BSIZE);
		if (err)
			goto out1;
		err = odict_alloc(&o_out, DICT_BSIZE);
		if (err)
			goto out1;

		if (device_info.output_channels > 0)
		{
			if (output == -1)
			{
				output = i;
			}
			info("slrtaudio: device out %c%d: %s: %d (ch %d)\n",
				device_info.is_default_output ? '*' : ' ', i,
				device_info.name,
				device_info.preferred_sample_rate,
				device_info.output_channels);
			if (output == i)
			{
				preferred_sample_rate_out =
					device_info.preferred_sample_rate;
				odict_entry_add(o_out, "selected",
						ODICT_BOOL, true);
				output_channels = device_info.output_channels;
				info("slrtaudio output"
						": %s (%dhz/%dch)\n",
						device_info.name,
						preferred_sample_rate_out,
						output_channels);
			}
			else
			{
				odict_entry_add(o_out, "selected",
						ODICT_BOOL, false);
			}
			odict_entry_add(o_out, "id", ODICT_INT, (int64_t)i);
			odict_entry_add(o_out, "display", ODICT_STRING,
					device_info.name);
			odict_entry_add(array_out, idx, ODICT_OBJECT, o_out);
		}

		if (device_info.input_channels > 0)
		{
			if (input == -1)
			{
				input = i;
			}
			info("slrtaudio: device in %c%d: %s: %d (ch %d)\n",
				device_info.is_default_input ? '*' : ' ', i,
				device_info.name,
				device_info.preferred_sample_rate,
				device_info.input_channels);
			if (input == i)
			{
				odict_entry_add(o_in, "selected",
						ODICT_BOOL, true);
				input_channels = device_info.input_channels;
				preferred_sample_rate_in =
					device_info.preferred_sample_rate;
				info("slrtaudio input: %s (%dhz/%dch)\n",
						device_info.name,
						preferred_sample_rate_in,
						input_channels);
			}
			else
			{
				odict_entry_add(o_in, "selected",
						ODICT_BOOL, false);
			}
			odict_entry_add(o_in, "id", ODICT_INT, (int64_t)i);
			odict_entry_add(o_in, "channels", ODICT_INT,
					(int64_t)device_info.input_channels);
			odict_entry_add(o_in, "first_input_channel",
					ODICT_INT,
					(int64_t)first_input_channel);
			odict_entry_add(o_in, "display", ODICT_STRING,
					device_info.name);
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
	char errmsg[512];
	int err = 0;
	int error = 0;

	slrtaudio = mem_zalloc(sizeof(*slrtaudio), NULL);
	slrtaudio->inBuffer = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	slrtaudio->outBufferTmp = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	slrtaudio->inBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN, NULL);
	slrtaudio->inBufferOutFloat = mem_zalloc(sizeof(float) * BUFFER_LEN, NULL);
	slrtaudio->outBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN, NULL);
	slrtaudio->outBufferInFloat = mem_zalloc(sizeof(float) * BUFFER_LEN, NULL);

	/* calculate 20ms buffersize/nframes  */
	unsigned int bufsz_in = preferred_sample_rate_in * 20 / 1000;
	unsigned int bufsz_out = preferred_sample_rate_out * 20 / 1000;

#ifdef DARWIN
	/* workaround for buffer underrun on macos */
	mismatch_samplerates = true;
#else
	mismatch_samplerates = false;
	if (preferred_sample_rate_in != preferred_sample_rate_out)
		mismatch_samplerates = true;
#endif

	audio_in = rtaudio_create(driver);
	if (rtaudio_error(audio_in) != NULL)
	{
		err = ENODEV;
		goto out;
	}

	if (mismatch_samplerates) {
		audio_out = rtaudio_create(driver);
		if (rtaudio_error(audio_out) != NULL)
		{
			err = ENODEV;
			goto out;
		}
	}

	rtaudio_stream_parameters_t out_params = {
		.device_id = output,
		.num_channels = output_channels,
		.first_channel = 0,
	};

	rtaudio_stream_parameters_t in_params = {
		.device_id = input,
		.num_channels = input_channels,
		.first_channel = 0,
	};

	rtaudio_stream_options_t options = {
		.flags = RTAUDIO_FLAGS_SCHEDULE_REALTIME +
			RTAUDIO_FLAGS_MINIMIZE_LATENCY,
	};

	/** Initialize the sample rate converter for input */
	if ((src_state_in = src_new(SRC_SINC_FASTEST, 2, &error)) == NULL)
	{
		error_msg("Samplerate::src_new failed : %s.\n",
				src_strerror(error));
		return 1;
	};

	/** Initialize the sample rate converter for output */
	if ((src_state_out = src_new(SRC_SINC_FASTEST, 2, &error)) == NULL)
	{
		error_msg("slrtaudio: Samplerate::src_new failed : %s.\n",
				src_strerror(error));
		return 1;
	};

	info("slrtaudio: samplerate/ch: in %d/%d out %d/%d\n",
			preferred_sample_rate_in,
			input_channels,
			preferred_sample_rate_out,
			output_channels);

	if (mismatch_samplerates) {
		info("slrtaudio: MISMATCH START STREAM: %i/%i \n",
				input, output);
		rtaudio_open_stream(audio_in, NULL,
				&in_params, RTAUDIO_FORMAT_SINT16,
				preferred_sample_rate_in, &bufsz_in,
				slrtaudio_callback_in, NULL, NULL, NULL);
		if (rtaudio_error(audio_in) != NULL)
		{
			err = EINVAL;
			goto out;
		}

		rtaudio_open_stream(audio_out, &out_params,
				NULL, RTAUDIO_FORMAT_SINT16,
				preferred_sample_rate_out, &bufsz_out,
				slrtaudio_callback_out, NULL, NULL, NULL);
		if (rtaudio_error(audio_out) != NULL)
		{
			err = EINVAL;
			goto out;
		}

		rtaudio_start_stream(audio_in);
		if (rtaudio_error(audio_in) != NULL)
		{
			err = EINVAL;
			goto out;
		}

		rtaudio_start_stream(audio_out);
		if (rtaudio_error(audio_out) != NULL)
		{
			err = EINVAL;
			goto out;
		}
	}
	else {
		info("slrtaudio: START STREAM: %i/%i \n", input, output);
		rtaudio_open_stream(audio_in, &out_params,
				&in_params, RTAUDIO_FORMAT_SINT16,
				preferred_sample_rate_in, &bufsz_in,
				slrtaudio_callback_in, NULL,
				&options, NULL);
		if (rtaudio_error(audio_in) != NULL)
		{
			err = EINVAL;
			goto out;
		}

		rtaudio_start_stream(audio_in);
		if (rtaudio_error(audio_in) != NULL)
		{
			err = EINVAL;
			goto out;
		}
	}

out:
	if (err) {
		re_snprintf(errmsg, sizeof(errmsg), "%s",
					rtaudio_error(audio_in));
		error_msg("slrtaudio: error: %s\n", errmsg);
		if (audio_out) {
			re_snprintf(errmsg, sizeof(errmsg), "%s",
					rtaudio_error(audio_out));
			error_msg("slrtaudio out error: %s\n", errmsg);
		}
		/**	webapp_ws_rtaudio_set_err(errmsg);*/
	}

	return err;
}


static int slrtaudio_stop(void)
{
	if (audio_in)
	{
		rtaudio_stop_stream(audio_in);
		rtaudio_close_stream(audio_in);
		rtaudio_destroy(audio_in);
		audio_in = NULL;
	}
	if (audio_out)
	{
		rtaudio_stop_stream(audio_out);
		rtaudio_close_stream(audio_out);
		rtaudio_destroy(audio_out);
		audio_out = NULL;
	}

	if (src_state_in)
		src_state_in = src_delete(src_state_in);

	if (src_state_out)
		src_state_out = src_delete(src_state_out);

	if (slrtaudio) {
		mem_deref(slrtaudio->inBuffer);
		mem_deref(slrtaudio->outBufferTmp);
		mem_deref(slrtaudio->inBufferFloat);
		mem_deref(slrtaudio->inBufferOutFloat);
		mem_deref(slrtaudio->outBufferFloat);
		mem_deref(slrtaudio->outBufferInFloat);
		slrtaudio = mem_deref(slrtaudio);
	}

	return 0;
}


static int slrtaudio_init(void)
{
	int err;
	struct session *sess;

	lock_alloc(&rtaudio_lock);

	err = ausrc_register(&ausrc, baresip_ausrcl(), "slrtaudio", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(),
						   "slrtaudio", play_alloc);

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	playmix = mem_zalloc(10 * 1920, NULL);
	if (!playmix)
		return ENOMEM;

	/* add local/recording session
	 */
	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return ENOMEM;
	sess->local = true;
	sess->stream = false;
	list_append(&sessionl, &sess->le, sess);

	/* add remote sessions
	 */
	for (uint32_t cnt = 0; cnt < MAX_REMOTE_CHANNELS; cnt++)
	{
		sess = mem_zalloc(sizeof(*sess), sess_destruct);
		if (!sess)
			return ENOMEM;
		sess->vumeter = mem_zalloc(BUFFER_LEN, NULL);

		sess->local = false;
		sess->stream = false;
		sess->ch = cnt * 2 + 1;
		sess->call = NULL;

		list_append(&sessionl, &sess->le, sess);
	}

	/* add stream session
	 */
	sess = mem_zalloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return ENOMEM;
	sess->local = false;
	sess->stream = true;
	sess->call = NULL;
	sess->ch = MAX_REMOTE_CHANNELS + 1;
	list_append(&sessionl, &sess->le, sess);

	slrtaudio_drivers();
	slrtaudio_devices();
	slrtaudio_record_init();
	slrtaudio_start();

	info("slrtaudio ready\n");

	return err;
}


static int slrtaudio_close(void)
{
	struct le *le;
	struct session *sess;

	slrtaudio_stop();

	rtaudio_lock = mem_deref(rtaudio_lock);
	ausrc = mem_deref(ausrc);
	auplay = mem_deref(auplay);
	interfaces = mem_deref(interfaces);

	for (le = sessionl.head; le;)
	{
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
