/**
 * @file slaudio.c SLAUDIO (libsoundio)
 *
 * Copyright (C) 2020 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <samplerate.h>
#include <soundio/soundio.h>
#include "slaudio.h"
#include "convert.h"
#ifdef WIN32
#include <windows.h>
#endif

#define BUFFER_LEN 15360 /* max buffer_len = 192kHz*2ch*20ms*2frames */
#define FLOAT_FRAME_BYTES 8 /* sizeof(float) * 2ch */

enum
{
	MAX_REMOTE_CHANNELS = 6,
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

static enum SoundIoFormat prioritized_formats[] = {
	SoundIoFormatFloat32LE,
	SoundIoFormatS32LE,
	SoundIoFormatS24LE,
	SoundIoFormatS16LE,
	SoundIoFormatInvalid,
};

struct slaudio_st
{
	int16_t *inBuffer;
	int16_t *outBufferTmp;
	float *inBufferFloat;
	float *inBufferOutFloat;
	float *outBufferFloat;
	struct SoundIo *soundio;
	struct SoundIoDevice *dev_in;
	struct SoundIoDevice *dev_out;
	struct SoundIoInStream *instream;
	struct SoundIoOutStream *outstream;
};

int16_t *empty_buffer;

static struct slaudio_st *slaudio;

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

static enum SoundIoBackend backend;
static int driver = -1;
static int input = -1;
static int first_input_channel = 0;
static int preferred_sample_rate_in = 0;
static int preferred_sample_rate_out = 0;
static int output = -1;
static struct odict *interfaces = NULL;
struct list sessionl;
static SRC_STATE *src_state_in;
static SRC_STATE *src_state_out;

static int slaudio_stop(void);
static int slaudio_start(void);
static int slaudio_devices(void);
static int slaudio_drivers(void);
static struct SoundIoRingBuffer *ring_buffer = NULL;
void webapp_ws_rtaudio_sync(void);

static int16_t *playmix;

static bool monorecord = true;
static bool monostream = true;
static bool mute = false;
static bool monitor = true;
static bool fatal_error = false;

static int16_t startup_count = 0;


/* webapp/jitter.c */
void webapp_jitter(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, unsigned int sampc, void *arg);


void slaudio_monorecord_set(bool status)
{
	monorecord = status;
}


void slaudio_monostream_set(bool status)
{
	monostream = status;
}


void slaudio_mute_set(bool status)
{
	mute = status;
}


void slaudio_monitor_set(bool status)
{
	monitor = status;
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


const struct odict *slaudio_get_interfaces(void)
{
	return (const struct odict *)interfaces;
}


static int slaudio_reset(void)
{
	int err;

	if (interfaces)
		mem_deref(interfaces);

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	info("slaudio/reset\n");

	slaudio_stop();
	startup_count = 0;

	slaudio_drivers();
	slaudio_devices();
	webapp_ws_rtaudio_sync();
	slaudio_start();

	return err;
}


void slaudio_set_driver(int value)
{
	driver = value;
	output = -1;
	input = -1;
	first_input_channel = 0;
	slaudio_reset();
}


void slaudio_set_input(int value)
{
	input = value;
	first_input_channel = 0;
	slaudio_reset();
}


void slaudio_set_first_input_channel(int value)
{
	first_input_channel = value;
	slaudio_reset();
}


void slaudio_set_output(int value)
{
	output = value;
	slaudio_reset();
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


static void stereo2mono(int16_t *buf, size_t sampc)
{
	while (sampc--) {
		buf[0] = buf[0]/2 + buf[1]/2;
		buf[1] = buf[0];

		buf += 2;
	}
}


static float format_float(char *src, enum SoundIoFormat fmt)
{
	float value = 0.0;

	if (fmt == SoundIoFormatS32LE) {
		value = (*((int32_t*)src)) / SAMPLE_32BIT_SCALING;
	}
	else if (fmt == SoundIoFormatS24LE) {
		int x;
		memcpy ((char*)&x + 1, src, 3);
		x >>= 8;
		value = x / SAMPLE_24BIT_SCALING;
	}
	else if (fmt == SoundIoFormatS16LE) {
		value = (*((int16_t*)src)) / SAMPLE_16BIT_SCALING;
	}
	else {
		value = *((float*)src);
	}
	return value;
}


static void downsample_first_ch(float *outv, struct SoundIoChannelArea *areas,
		int nframes, struct SoundIoInStream *instream)
{
	float left;
	float right;

	for (int frame = 0; frame < nframes; frame++) {
		left = 0.0;
		right = 0.0;

		for (int ch = 0; ch < instream->layout.channel_count; ch++) {
			if (ch == first_input_channel) {
				left = format_float(areas[ch].ptr,
						instream->format);
			}

			if (ch == first_input_channel + 1) {
				right = format_float(areas[ch].ptr,
						instream->format);
			}

			if (first_input_channel == 99) {
				left += format_float(areas[ch].ptr,
						instream->format);
				right = left;
			}

			if (monorecord)
				right = left;

			areas[ch].ptr += areas[ch].step;
		}

		/*stereo ch left*/
		*outv = left;
		outv += 1;

		/*stereo ch right*/
		*outv = right;
		outv += 1;
	}
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	info("slaudio: destruct ausrc\n");
	sess->run_src = false;
	sys_msleep(20);
	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	info("slaudio: destruct auplay\n");
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

	struct session *sess = NULL;
	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;

		if (!sess->run_src && !sess->local && sess->call)
		{
			sess->st_src = mem_zalloc(sizeof(*st_src),
					ausrc_destructor);
			if (!sess->st_src)
				return ENOMEM;
			st_src = sess->st_src;
			st_src->sess = sess;
			break;
		}
	}

	if (!st_src || !sess)
		return EINVAL;

	st_src->run = false;
	st_src->as = as;
	st_src->rh = rh;
	st_src->arg = arg;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_zalloc(sizeof(int16_t) * st_src->sampc * 10, NULL);
	if (!st_src->sampv)
	{
		err = ENOMEM;
	}

	if (err)
	{
		mem_deref(st_src);
		return err;
	}

	sess->run_src = true;
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

	struct session *sess;
	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;

		if (!sess->run_play && !sess->local && sess->call)
		{
			sess->st_play = mem_zalloc(sizeof(*st_play),
					auplay_destructor);

			sess->dstmix = mem_zalloc(sizeof(int16_t) * sampc * 10,
					NULL);
			if (!sess->st_play)
				return ENOMEM;
			st_play = sess->st_play;
			st_play->sess = sess;
			break;
		}
	}

	if (!st_play || !sess)
		return EINVAL;

	st_play->run = false;
	st_play->ap = ap;
	st_play->wh = wh;
	st_play->arg = arg;
	st_play->sampc = sampc;
	st_play->sampv = mem_zalloc(sizeof(int16_t) * sampc * 10, NULL);
	if (!st_play->sampv)
	{
		err = ENOMEM;
	}

	if (err)
	{
		mem_deref(st_play);
		return err;
	}

	sess->run_play = true;
	st_play->run = true;
	*stp = st_play;

	return err;
}


static void underflow_callback(struct SoundIoOutStream *outstream) {
	static int count = 0;
	warning("slaudio/underflow %d\n", ++count);
}


static void outstream_error_callback(struct SoundIoOutStream *os, int err) {
	warning("slaudio/out_err_call: %s\n", soundio_strerror(err));
	fatal_error = true;
	slaudio_reset();
}


static void instream_error_callback(struct SoundIoInStream *is, int err) {
	warning("slaudio/in_err_call: %s\n", soundio_strerror(err));
	fatal_error = true;
	slaudio_reset();
}


static int slaudio_drivers(void)
{
	int err = 0;
	static enum SoundIoBackend backend_tmp;
	struct SoundIo *soundio;
	struct odict *o;
	struct odict *array;
	char backend_name[30];
	char idx[2];

	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;

	if (!(soundio = soundio_create()))
		return ENOMEM;

	for (int i = 0; i < soundio_backend_count(soundio); i++)
	{
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		backend_tmp = soundio_get_backend(soundio, i);

		(void)re_snprintf(backend_name, sizeof(backend_name), "%s",
				soundio_backend_name(backend_tmp));

		if (!str_cmp(backend_name, "Dummy"))
			continue;

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		odict_entry_add(o, "display", ODICT_STRING,
				backend_name);

		info("slaudio/drivers detected: %s\n", backend_name);

		if (driver == -1 && !str_cmp(backend_name, "PulseAudio"))
			driver = i;

		if (driver == -1 && !str_cmp(backend_name, "WASAPI"))
			driver = i;

		if (driver == -1 && !str_cmp(backend_name, "CoreAudio"))
			driver = i;

		if (driver == i) {
			odict_entry_add(o, "selected", ODICT_BOOL, true);
			backend = backend_tmp;
			info("slaudio/drivers: %s selected\n", backend_name);
		}
		else {
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);
		odict_entry_add(array, idx, ODICT_OBJECT, o);

		mem_deref(o);
	}

	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

	soundio_destroy(soundio);

	return err;
}


static int slaudio_devices(void)
{
	int err = 0;
	struct SoundIo *soundio;
	int output_count, input_count, default_output, default_input;
	bool default_input_err;
	bool default_output_err;
	struct odict *o;
	struct odict *array_in;
	struct odict *array_out;
	struct SoundIoDevice *device;
	char idx[2];
	char device_name[256];

	err = odict_alloc(&array_in, DICT_BSIZE);
	if (err)
		return ENOMEM;

	err = odict_alloc(&array_out, DICT_BSIZE);
	if (err)
		return ENOMEM;

	if (!(soundio = soundio_create()))
		return ENOMEM;

	err = soundio_connect_backend(soundio, backend);
	if (err) {
		warning("slaudio/soundio_connect_backend err: %s\n",
				soundio_strerror(err));
		goto out;
	}

	soundio_flush_events(soundio);

	output_count = soundio_output_device_count(soundio);
	input_count = soundio_input_device_count(soundio);

	default_output = soundio_default_output_device_index(soundio);
	default_output_err = false;
	default_input = soundio_default_input_device_index(soundio);
	default_input_err = false;

	for (int i = 0; i < input_count; i += 1) {

		device = soundio_get_input_device(soundio, i);
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		if (backend == SoundIoBackendAlsa && !device->is_raw) {
			soundio_device_unref(device);
			if (i == default_input)
				default_input_err = true;
			continue;
		}

		if (device->probe_error) {
			info("slaudio/input device %s probe error\n",
					device->name);
			soundio_device_unref(device);
			if (i == default_input)
				default_input_err = true;
			continue;
		}

		debug("slaudio/input device %s formats:\n", device->name);
		for (int y = 0; y < device->format_count; y += 1) {
			const char *comma =
				(y == device->format_count - 1) ? "\n" : ", ";
			debug("%s%s",
				soundio_format_string(device->formats[y]),
				comma);
		}


		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			goto out;

		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);

		if (device->is_raw) {
			(void)re_snprintf(device_name, sizeof(device_name),
					"%s (Exclusiv)", device->name);
		}
		else {
			(void)re_snprintf(device_name,
					sizeof(device_name), "%s",
					device->name);
		}

		odict_entry_add(o, "display", ODICT_STRING, device_name);
		info("slaudio: device: %s\n", device_name);

		int64_t channels = device->current_layout.channel_count;
		info("slaudio: default channels: %d\n", channels);
		if (!channels) {
			const struct SoundIoChannelLayout *layout =
				&device->layouts[0];
			channels = layout->channel_count;
			info("slaudio: fallback channels: %d\n", channels);
		}

		odict_entry_add(o, "channels", ODICT_INT, channels);
		odict_entry_add(o, "first_input_channel",
				ODICT_INT,
				(int64_t)first_input_channel);

		if (input == -1 && default_input == i)
		{
			info("slaudio: set input %s (%d)\n", device_name, i);
			input = i;
		}

		if (input == -1 && default_input_err) {
			info("slaudio: set fallback input %s (%d)\n",
					device_name, i);
			input = i;
		}

		if (input == i)
		{
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		}
		else
		{
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		odict_entry_add(array_in, idx, ODICT_OBJECT, o);

		soundio_device_unref(device);
		mem_deref(o);
	}

	for (int i = 0; i < output_count; i += 1) {

		device = soundio_get_output_device(soundio, i);
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		if (backend == SoundIoBackendAlsa && !device->is_raw) {
			soundio_device_unref(device);
			if (i == default_output)
				default_output_err = true;
			continue;
		}

		if (device->probe_error) {
			info("slaudio/input device %s probe error\n",
					device->name);
			soundio_device_unref(device);
			if (i == default_output)
				default_output_err = true;
			continue;
		}

		debug("slaudio/output device %s formats:\n", device->name);
		for (int y = 0; y < device->format_count; y += 1) {
			const char *comma =
				(y == device->format_count - 1) ? "\n" : ", ";
			debug("%s%s",
				soundio_format_string(device->formats[y]),
				comma);
		}

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			goto out;

		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);
		if (device->is_raw) {
			(void)re_snprintf(device_name, sizeof(device_name),
					"%s (Exclusiv)", device->name);
		}
		else {
			(void)re_snprintf(device_name, sizeof(device_name),
					"%s", device->name);
		}
		odict_entry_add(o, "display", ODICT_STRING, device_name);
		info("slaudio: output device %s\n", device_name);

		if (output == -1 && default_output == i)
		{
			info("slaudio: set ouput %s (%d)\n", device_name, i);
			output = i;
		}

		if (input == -1 && default_output_err) {
			info("slaudio: set fallback ouput %s (%d)\n",
					device_name, i);
			output = i;
		}

		if (output == i)
		{
			odict_entry_add(o, "selected", ODICT_BOOL, true);
		}
		else
		{
			odict_entry_add(o, "selected", ODICT_BOOL, false);
		}

		odict_entry_add(array_out, idx, ODICT_OBJECT, o);

		soundio_device_unref(device);
		mem_deref(o);
	}

	odict_entry_add(interfaces, "input", ODICT_ARRAY, array_in);
	odict_entry_add(interfaces, "output", ODICT_ARRAY, array_out);

out:
	mem_deref(array_in);
	mem_deref(array_out);

	soundio_destroy(soundio);

	return err;
}


static void read_callback(struct SoundIoInStream *instream,
				int frame_count_min, int frame_count_max)
{
	int err;
	struct SoundIoChannelArea *areas;

	int nframes = frame_count_max;
	int samples;

	struct le *le;
	struct le *mle;
	struct session *sess;
	struct auplay_st *st_play;
	struct auplay_st *mst_play;
	struct ausrc_st *st_src;
	int cntplay = 0, msessplay = 0, sessplay = 0;
	SRC_DATA src_data_in;
	SRC_DATA src_data_out;

	if ((err = soundio_instream_begin_read(instream, &areas, &nframes))) {
		warning("slaudio/read_callback:"
			"begin read error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	if (!nframes)
		return;


	if (startup_count < 16 || nframes > 1920) {
		/*
		 * drop first frames (keeps buffers small)
		 */
		debug("slaudio: drop %d frames due startup %d\n", nframes,
				startup_count);
		if ((err = soundio_instream_end_read(instream))) {
			warning("slaudio/read_callback:"
				"end read error: %s\n", soundio_strerror(err));
		}
		++startup_count;

		return;
	}

	samples = nframes * 2; /* Stereo (2 ch) only */

	if (!areas) {
		/* Due to an overflow there is a hole. Fill with silence */
		memset(slaudio->inBufferFloat, 0, samples * sizeof(float));
		warning("slaudio/read_callback:"
				"Dropped %d frames overflow\n", nframes);
	}
	else {
		downsample_first_ch(slaudio->inBufferFloat, areas, nframes,
				instream);
	}

	if ((err = soundio_instream_end_read(instream))) {
		warning("slaudio/read_callback:"
				"end read error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	if (mute)
	{
		memset(slaudio->inBufferFloat, 0, samples * sizeof(float));
	}

	ws_meter_process(0, slaudio->inBufferFloat, (unsigned long)samples);
	if (!monorecord)
		ws_meter_process(2, slaudio->inBufferFloat+1,
				(unsigned long)samples-1);

	/**<-- Input Samplerate conversion */
	if (preferred_sample_rate_in != 48000)
	{
		if (!src_state_in)
			return;

		src_data_in.data_in = slaudio->inBufferFloat;
		src_data_in.data_out = slaudio->inBufferOutFloat;
		src_data_in.input_frames = nframes;
		src_data_in.output_frames = BUFFER_LEN / 2;
		src_data_in.src_ratio =
				48000 / (double)preferred_sample_rate_in;
		src_data_in.end_of_input = 0;

		if ((err = src_process(src_state_in, &src_data_in)) != 0)
		{
			warning("slaudio: Samplerate::src_process_in :"
					"returned error : %s %f\n",
					src_strerror(err),
					src_data_in.src_ratio);
			return;
		};

		samples = src_data_in.output_frames_gen * 2;
		auconv_to_s16(slaudio->inBuffer, AUFMT_FLOAT,
				slaudio->inBufferOutFloat,
				samples);
	}
	else {
		auconv_to_s16(slaudio->inBuffer, AUFMT_FLOAT,
				slaudio->inBufferFloat, samples);
	}
	/** Input Samplerate conversion -->*/

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (sess && sess->local)
		{
			/* write local audio to flac record buffer */
			(void)aubuf_write_samp(sess->aubuf,
					slaudio->inBuffer, samples);
		}

		if (!sess || !sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;

		webapp_jitter(sess, st_play->sampv,
				st_play->wh, samples, st_play->arg);
	}

	if (monostream && !monorecord)
	{
		stereo2mono(slaudio->inBuffer, nframes);
	}

	if (monitor)
	{
		memcpy(playmix, slaudio->inBuffer, samples * sizeof(int16_t));
	}
	else {
		memset(playmix, 0, samples * sizeof(int16_t));
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		msessplay = 0;

		if (!sess || sess->local)
			continue;

		if (!sess->run_play) {
			(void)aubuf_write_samp(sess->aubuf, empty_buffer,
					samples);
			continue;
		}

		st_play = sess->st_play;

		for (uint16_t pos = 0; pos < samples; pos++)
		{
			int32_t val = 0;
			val = playmix[pos] + st_play->sampv[pos];
			if (val >= 32767)
				val = 32767;
			if (val <= -32767)
				val = -32767;
			playmix[pos] = val;
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
			int32_t val = 0;

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
					val = mst_play->sampv[pos] +
						sess->dstmix[pos];
					if (val >= 32767)
						val = 32767;
					if (val <= -32767)
						val = -32767;

					sess->dstmix[pos] = val;
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
		if (sess && sess->run_src && !sess->local)
		{
			st_src = sess->st_src;

			for (uint16_t pos = 0; pos < samples; pos++)
			{
				if (!sessplay) {
					/* ignore on last call the dstmix */
					st_src->sampv[pos] =
						slaudio->inBuffer[pos];
				}
				else
				{
					st_src->sampv[pos] =
						slaudio->inBuffer[pos] +
						sess->dstmix[pos];
				}
			}

			st_src->rh(st_src->sampv, samples, st_src->arg);
		}

	}

	if (!cntplay && !monitor)
	{
		memset(playmix, 0, samples * sizeof(int16_t));
	}

	char *write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
	int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
	int free_count = free_bytes / FLOAT_FRAME_BYTES;

	if (samples > free_count) {
		warning("ring buffer overflow %d/%d/%d\n", samples,
				free_count, frame_count_max);
		return;
	}

	if (preferred_sample_rate_out != 48000)
	{
		auconv_from_s16(AUFMT_FLOAT, slaudio->outBufferFloat,
				playmix, samples);

		src_data_out.data_in = slaudio->outBufferFloat;
		src_data_out.data_out = (float *)write_ptr;
		src_data_out.input_frames = samples / 2;
		src_data_out.output_frames = free_count;
		src_data_out.src_ratio =
			(double)preferred_sample_rate_out / 48000;
		src_data_out.end_of_input = 0;

		if (!src_state_out)
			return;

		if ((err = src_process(src_state_out, &src_data_out)) != 0)
		{
			warning("slrtaudio: Samplerate::src_process_out :"
				"returned error : %s\n", src_strerror(err));
			return;
		};

		samples = src_data_out.output_frames_gen * 2;
	}
	else {
		auconv_from_s16(AUFMT_FLOAT, write_ptr,
				playmix, samples);
	}

	int advance_bytes = samples * sizeof(float);
	soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_bytes);
}


static void write_callback(struct SoundIoOutStream *outstream,
				int frame_count_min, int frame_count_max)
{
	struct SoundIoChannelArea *areas;
	int err;
	int nframes = frame_count_max;

	char *read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
	int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
	int fill_count = fill_bytes / FLOAT_FRAME_BYTES;
	int frames_left;
	int frame_count;
	int samples;

	if (nframes > fill_count) {
		/* Ring buffer does not have enough data, fill with zeroes. */
		frames_left = nframes;
		for (;;) {
			frame_count = frames_left;
			if (frame_count <= 0)
				return;
			if ((err = soundio_outstream_begin_write(
							outstream,
							&areas,
							&frame_count))) {
				warning("slaudio/write_callback:"
					"begin write error: %s\n",
					soundio_strerror(err));
				return;
			}
			if (frame_count <= 0)
				return;
			for (int frame = 0; frame < frame_count; frame += 1) {
				for (int ch = 0;
					ch < outstream->layout.channel_count;
					ch += 1) {
					memset(areas[ch].ptr, 0,
						outstream->bytes_per_sample);
					areas[ch].ptr += areas[ch].step;
				}
			}
			if ((err = soundio_outstream_end_write(outstream))) {
				warning("slaudio/write_callback:"
					"end write error: %s\n",
					soundio_strerror(err));
				return;
			}
			frames_left -= frame_count;
		}
	}

	if ((err = soundio_outstream_begin_write(outstream, &areas,
					&nframes))) {
		warning("slaudio/write_callback:"
			"begin write error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	if (!nframes)
		return;

	samples = nframes * 2;

	memcpy(slaudio->outBufferFloat, read_ptr, samples * sizeof(float));
	soundio_ring_buffer_advance_read_ptr(ring_buffer,
			samples * sizeof(float));

	float *outBufferFloat = slaudio->outBufferFloat;

	for (int frame = 0; frame < nframes; frame += 1) {
		if (outstream->format == SoundIoFormatS32LE) {
			float_32(*outBufferFloat, *((int32_t*)areas[0].ptr));
		}
		else if (outstream->format == SoundIoFormatS24LE) {
			float_24(*outBufferFloat, *((int32_t*)areas[0].ptr));
		}
		else if (outstream->format == SoundIoFormatS16LE) {
			float_16(*outBufferFloat, *((int16_t*)areas[0].ptr));
		}
		else {
			memcpy(areas[0].ptr, outBufferFloat,
					outstream->bytes_per_sample);
		}
		outBufferFloat += 1;
		areas[0].ptr += areas[0].step;

		if (outstream->layout.channel_count == 1)
			continue;

		if (outstream->format == SoundIoFormatS32LE) {
			float_32(*outBufferFloat, *((int32_t*)areas[1].ptr));
		}
		else if (outstream->format == SoundIoFormatS24LE) {
			float_24(*outBufferFloat, *((int32_t*)areas[1].ptr));
		}
		else if (outstream->format == SoundIoFormatS16LE) {
			float_16(*outBufferFloat, *((int16_t*)areas[1].ptr));
		}
		else {
			memcpy(areas[1].ptr, outBufferFloat,
					outstream->bytes_per_sample);
		}

		outBufferFloat += 1;
		areas[1].ptr += areas[1].step;
	}

	if ((err = soundio_outstream_end_write(outstream))) {
		warning("slaudio/write_callback:"
			"end write error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}
}


static void slaudio_destruct(void *arg)
{
	if (slaudio) {
		if (!fatal_error) {
			soundio_instream_destroy(slaudio->instream);
			soundio_outstream_destroy(slaudio->outstream);
		}
		soundio_device_unref(slaudio->dev_in);
		soundio_device_unref(slaudio->dev_out);
		soundio_destroy(slaudio->soundio);
		mem_deref(slaudio->inBuffer);
		mem_deref(slaudio->inBufferFloat);
		mem_deref(slaudio->inBufferOutFloat);
		mem_deref(slaudio->outBufferFloat);
		slaudio = NULL;
	}
	if (fatal_error) {
		output = -1;
		input = -1;
		first_input_channel = 0;
		sys_msleep(500);
		fatal_error = false;
	}
}


static int slaudio_start(void)
{
	int err = 0;
	double microphone_latency = 0.02;
	enum SoundIoFormat *fmt;

	if (input == -1) {
		warning("slaudio/start: invalid input device\n");
		return 1;
	}

	if (output == -1) {
		warning("slaudio/start: invalid output device\n");
		return 1;
	}

	slaudio = mem_zalloc(sizeof(*slaudio), slaudio_destruct);
	slaudio->inBuffer = mem_zalloc(sizeof(int16_t) * BUFFER_LEN,
					NULL);
	slaudio->inBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
					NULL);
	slaudio->inBufferOutFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
					NULL);
	slaudio->outBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
					NULL);

	/** Initialize the sample rate converter for input */
	if ((src_state_in = src_new(SRC_SINC_FASTEST, 2, &err)) == NULL)
	{
		warning("slaudio/start: Samplerate::src_new failed : %s.\n",
				src_strerror(err));
		return err;
	};

	/** Initialize the sample rate converter for output */
	if ((src_state_out = src_new(SRC_SINC_FASTEST, 2, &err)) == NULL)
	{
		warning("slaudio/start: Samplerate::src_new failed : %s.\n",
				src_strerror(err));
		return err;
	};

	slaudio->soundio = soundio_create();

	if (!slaudio->soundio)
		return ENOMEM;

	slaudio->soundio->app_name = "StudioLink";

	err = soundio_connect_backend(slaudio->soundio, backend);
	if (err) {
		warning("slaudio/start: soundio_connect err: %s\n",
				soundio_strerror(err));
		goto err_out;
	}

	soundio_flush_events(slaudio->soundio);

	slaudio->dev_in = soundio_get_input_device(slaudio->soundio, input);
	if (!slaudio->dev_in) {
		warning("slaudio/start: error soundio_get_input_device\n");
		err = ENOMEM;
		goto err_out;
	}

	slaudio->dev_out = soundio_get_output_device(slaudio->soundio, output);
	if (!slaudio->dev_out) {
		warning("slaudio/start: error soundio_get_output_device\n");
		err = ENOMEM;
		goto err_out;
	}

	info("slaudio/start: in dev: %s\n", slaudio->dev_in->name);
	info("slaudio/start: out dev: %s\n", slaudio->dev_out->name);


	for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; fmt++) {
		if (soundio_device_supports_format(slaudio->dev_in, *fmt))
		{
			break;
		}
	}
	if (*fmt == SoundIoFormatInvalid) {
		warning("slaudio/start: instream SoundIoFormatInvalid\n");
		err = 42;
		goto err_out;
	}

	/* Create input stream */

	slaudio->instream = soundio_instream_create(slaudio->dev_in);
	if (!slaudio->instream) {
		warning("slaudio/start: error soundio_instream_create\n");
		err = ENOMEM;
		goto err_out;
	}

	slaudio->instream->name = "StudioLinkIn";

	slaudio->instream->format = *fmt;
	slaudio->instream->read_callback = read_callback;
	slaudio->instream->software_latency = microphone_latency;
	slaudio->instream->error_callback = instream_error_callback;

	err = soundio_instream_open(slaudio->instream);
	if (err) {
		warning("slaudio/start: soundio_instream_open: %s\n",
				soundio_strerror(err));
		goto err_out;
	}

	for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; ++fmt) {
		if (soundio_device_supports_format(slaudio->dev_out, *fmt))
		{
			break;
		}
	}
	if (*fmt == SoundIoFormatInvalid) {
		warning("slaudio/start: outstream SoundIoFormatInvalid\n");
		err = 42;
		goto err_out;
	}

	/* Create output stream */

	slaudio->outstream = soundio_outstream_create(slaudio->dev_out);
	if (!slaudio->outstream) {
		warning("slaudio/start: error soundio_outstream_create\n");
		err = ENOMEM;
		goto err_out;
	}
	slaudio->outstream->name = "StudioLinkOut";

	slaudio->outstream->format = *fmt;
	slaudio->outstream->write_callback = write_callback;
	slaudio->outstream->software_latency = microphone_latency;
	slaudio->outstream->underflow_callback = underflow_callback;
	slaudio->outstream->error_callback = outstream_error_callback;


	err = soundio_outstream_open(slaudio->outstream);
	if (err) {
		warning("slaudio/start: soundio_outstream_open: %s\n",
				soundio_strerror(err));
		goto err_out;
	}

	if (slaudio->outstream->layout_error) {
		warning("slaudio/start: unable to set channel layout: %s\n",
			soundio_strerror(slaudio->outstream->layout_error));
		goto err_out;
	}

	/* Prepare ring buffer */

	int capacity = microphone_latency * 2 * 3 *
		slaudio->outstream->sample_rate * 4;
	ring_buffer = soundio_ring_buffer_create(slaudio->soundio, capacity);
	if (!ring_buffer) {
		warning("slaudio/start: error ring_buffer\n");
		err = ENOMEM;
		goto err_out;
	}

	preferred_sample_rate_in = slaudio->instream->sample_rate;
	preferred_sample_rate_out = slaudio->outstream->sample_rate;
	info("slaudio/start: sample_rate in: %d out: %d\n",
			preferred_sample_rate_in, preferred_sample_rate_out);

	/* Start streams */

	err = soundio_instream_start(slaudio->instream);
	if (err) {
		warning("slaudio/start: soundio_instream_start: %s\n",
				soundio_strerror(err));
		goto err_out;
	}

	err = soundio_outstream_start(slaudio->outstream);
	if (err) {
		warning("slaudio/start: soundio_outstream_start: %s\n",
				soundio_strerror(err));
		goto err_out;
	}


	info("slaudio/start: success\n");
	return 0;

err_out:
	mem_deref(slaudio);

	warning("slaudio/start: error %d\n", err);
	return err;
}


static int slaudio_stop(void)
{
	if (src_state_in)
		src_state_in = src_delete(src_state_in);

	if (src_state_out)
		src_state_out = src_delete(src_state_out);

	if (slaudio)
		mem_deref(slaudio);

	if (ring_buffer) {
		soundio_ring_buffer_destroy(ring_buffer);
		ring_buffer = NULL;
	}

	return 0;
}


static int slaudio_init(void)
{
	int err;
	struct session *sess;

	err = ausrc_register(&ausrc, baresip_ausrcl(), "slaudio", src_alloc);
	err |= auplay_register(&auplay, baresip_auplayl(),
						   "slaudio", play_alloc);
	if (err)
		return ENOMEM;

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	playmix = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	if (!playmix)
		return ENOMEM;

	empty_buffer = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	if (!empty_buffer)
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
		sess->vumeter = mem_zalloc(BUFFER_LEN * sizeof(float), NULL);

		sess->local = false;
		sess->stream = false;
		sess->ch = cnt * 2 + 1;
		sess->track = cnt + 1;
		sess->call = NULL;
		sess->jb_max = 0;

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
	sess->jb_max = 0;
	sess->ch = MAX_REMOTE_CHANNELS + 1;
	list_append(&sessionl, &sess->le, sess);

	slaudio_drivers();
	slaudio_devices();
	slaudio_record_init();
	slaudio_start();


#ifdef WIN32
	/* Activate windows low latency timer */
	timeBeginPeriod(1);
#endif
	info("slaudio ready\n");

	return err;
}


static int slaudio_close(void)
{
	struct le *le;
	struct session *sess;

	slaudio_stop();

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
	empty_buffer = mem_deref(empty_buffer);

	return 0;
}


const struct mod_export DECL_EXPORTS(slaudio) = {
	"slaudio",
	"sound",
	slaudio_init,
	slaudio_close
};
