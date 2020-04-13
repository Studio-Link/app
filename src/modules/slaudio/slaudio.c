/**
 * @file slaudio.c RTAUDIO
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

#define BUFFER_LEN 7680 /* max buffer_len = 192kHz*2ch*20ms */

enum
{
	MAX_REMOTE_CHANNELS = 6,
	DICT_BSIZE = 32,
	MAX_LEVELS = 8,
};

struct slaudio_st
{
	int16_t *inBuffer;
	int16_t *outBufferTmp;
	float *inBufferFloat;
	float *inBufferOutFloat;
	float *outBufferFloat;
	float *outBufferInFloat;
	struct SoundIo *soundio;
	struct SoundIoDevice *dev_in;
	struct SoundIoDevice *dev_out;
	struct SoundIoInStream *instream;
	struct SoundIoOutStream *outstream;
};

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

static int slaudio_stop(void);
static int slaudio_start(void);
static int slaudio_devices(void);
static int slaudio_drivers(void);
static struct SoundIoRingBuffer *ring_buffer = NULL;
void webapp_ws_rtaudio_sync(void);

static int16_t *playmix;

static bool mono = false;
static bool mute = false;
static bool monitor = true;

/* webapp/jitter.c */
void webapp_jitter(struct session *sess, int16_t *sampv,
		auplay_write_h *wh, unsigned int sampc, void *arg);


void slaudio_mono_set(bool active)
{
	mono = active;
}


void slaudio_mute_set(bool active)
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

	slaudio_stop();
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
	slaudio_reset();
}


void slaudio_set_input(int value)
{
	input = value;
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


static void downsample_first_ch(float *outv, struct SoundIoChannelArea *areas,
		int nframes, struct SoundIoInStream *instream)
{
	for (int frame = 0; frame < nframes; frame++) {
		for (int ch = 0; ch < instream->layout.channel_count; ch++) {
			if (ch == first_input_channel) {
				/*stereo ch left*/
				memcpy(outv, areas[ch].ptr,
						instream->bytes_per_sample);
				outv += 1;

				/*stereo ch right*/
				memcpy(outv, areas[ch].ptr,
						instream->bytes_per_sample);
				outv += 1;
				areas[ch].ptr += areas[ch].step;
			}
		}
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

			sess->dstmix = mem_zalloc(sizeof(int16_t) * sampc * 10,
					NULL);
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

	st_play->run = true;
	*stp = st_play;

	return err;
}


static void underflow_callback(struct SoundIoOutStream *outstream) {
	static int count = 0;
	warning("slaudio/underflow %d\n", ++count);
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
		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			return ENOMEM;

		backend_tmp = soundio_get_backend(soundio, i);

		(void)re_snprintf(backend_name, sizeof(backend_name), "%s",
				soundio_backend_name(backend_tmp));

		odict_entry_add(o, "display", ODICT_STRING,
				backend_name);

		info("slaudio/drivers detected: %s\n", backend_name);

		if (driver == -1 && !str_cmp(backend_name, "PulseAudio"))
			driver = i;

		if (driver == -1 && !str_cmp(backend_name, "WASAPI"))
			driver = i;

		if (driver == -1 && !str_cmp(backend_name, "Coreaudio"))
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
	struct odict *o;
	struct odict *array_in;
	struct odict *array_out;
	struct SoundIoDevice *device;
	char idx[2];

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
	default_input = soundio_default_input_device_index(soundio);

	for (int i = 0; i < input_count; i += 1) {

		device = soundio_get_input_device(soundio, i);
		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		if (!device->sample_rate_current)
		{
			info("slaudio/input device %s has no sample_rate\n",
					device->name);
			soundio_device_unref(device);
			continue;
		}

		if (!device->current_layout.channel_count)
		{
			info("slaudio/input device %s has no inputs\n",
					device->name);
			soundio_device_unref(device);
			continue;
		}

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			goto out1;

		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);
		odict_entry_add(o, "display", ODICT_STRING, device->name);
		odict_entry_add(o, "channels", ODICT_INT,
				(int64_t)device->current_layout.channel_count);
		odict_entry_add(o, "first_input_channel",
				ODICT_INT,
				(int64_t)first_input_channel);

		if (input == -1 && default_input == i)
		{
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

		if (!device->sample_rate_current)
		{
			info("slaudio/output device %s has no sample_rate\n",
					device->name);
			soundio_device_unref(device);
			continue;
		}

		if (!device->current_layout.channel_count)
		{
			info("slaudio/output device %s has no outputs\n",
					device->name);
			soundio_device_unref(device);
			continue;
		}

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			goto out1;

		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);
		odict_entry_add(o, "display", ODICT_STRING, device->name);

		if (output == -1 && default_output == i)
		{
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

out1:
	mem_deref(array_in);
	mem_deref(array_out);

out:
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

//	warning("nframes read %d\n", nframes);
	
	if (frame_count_max > 960)
		nframes = 960;
		
	if ((err = soundio_instream_begin_read(instream, &areas, &nframes))) {
		warning("slaudio/read_callback:"
				"begin read error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}


	if (!nframes)
		return;

	samples = nframes * 2; /* Stereo (2 ch) only */

	if (!areas) {
		/* Due to an overflow there is a hole. Fill with silence */
		memset(slaudio->inBufferFloat, 0, samples * instream->bytes_per_frame);
		warning("slaudio/read_callback:"
				"Dropped %d frames overflow\n", nframes);
	} else {
		downsample_first_ch(slaudio->inBufferFloat, areas, nframes, instream);
	}

	if ((err = soundio_instream_end_read(instream))) {
		warning("slaudio/read_callback:"
				"end read error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	if (mute)
	{
		memset(slaudio->inBufferFloat, 0, samples * instream->bytes_per_frame);
	}
	
	ws_meter_process(0, slaudio->inBufferFloat, (unsigned long)samples);

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
	} else {
		auconv_to_s16(slaudio->inBuffer, AUFMT_FLOAT,
				slaudio->inBufferFloat, samples);
	}
	/** Input Samplerate conversion -->*/

	//warning("samples: %d %d\n", nframes, instream->layout.channel_count);
	
	if (monitor)
	{
		memcpy(playmix, slaudio->inBuffer, samples * sizeof(int16_t));
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (!sess || !sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;

		webapp_jitter(sess, st_play->sampv,
				st_play->wh, samples, st_play->arg);
	}

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		msessplay = 0;

		if (!sess || !sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;

		for (uint16_t pos = 0; pos < samples; pos++)
		{
			int32_t val = 0;
			if (cntplay < 1)
			{
				playmix[pos] = st_play->sampv[pos];
			}
			else
			{
				val = playmix[pos] + st_play->sampv[pos];
				if (val >= 32767)
					val = 32767;
				if (val <= -32767)
					val = -32767;
				playmix[pos] = val;
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

		if (sess && sess->local)
		{
			/* write local audio to flac record buffer */
			(void)aubuf_write_samp(sess->aubuf,
					slaudio->inBuffer, samples);
		}
	}

	if (!cntplay && !monitor)
	{
		memset(playmix, 0, samples * sizeof(int16_t));
	}

	char *write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
	int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
	int free_count = free_bytes / instream->bytes_per_sample;

	if (samples > free_count) {
		warning("ring buffer overflow %d/%d/%d\n", samples, free_count, frame_count_max);
		return;
	}

	auconv_from_s16(AUFMT_FLOAT, write_ptr,
			playmix, samples);
	int advance_bytes = samples * instream->bytes_per_sample;
	soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_bytes);
}


static void write_callback(struct SoundIoOutStream *outstream,
				int frame_count_min, int frame_count_max)
{
	struct SoundIoChannelArea *areas;
	int err;
	int nframes = frame_count_max;

//	warning("nframes write %d\n", nframes);
	if (frame_count_max > 960)
		nframes = 960;

	char *read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
	int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
	int fill_count = fill_bytes / outstream->bytes_per_frame;
	int frames_left;
	int frame_count;

	if (nframes > fill_count) {
		// Ring buffer does not have enough data, fill with zeroes.
		frames_left = nframes;
		for (;;) {
			frame_count = frames_left;
			if (frame_count <= 0)
				return;
			if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
				warning("slaudio/write_callback:"
						"begin write error: %s\n", soundio_strerror(err));
				return;
			}
			if (frame_count <= 0)
				return;
			for (int frame = 0; frame < frame_count; frame += 1) {
				for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
					memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
					areas[ch].ptr += areas[ch].step;
				}
			}
			if ((err = soundio_outstream_end_write(outstream))) {
				warning("slaudio/write_callback:"
						"end write error: %s\n", soundio_strerror(err));
				return;
			}
			frames_left -= frame_count;
		}
	}


	if ((err = soundio_outstream_begin_write(outstream, &areas, &nframes))) {
		warning("slaudio/write_callback:"
				"begin write error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	if (!nframes)
		return;

//	warning("out: nframes %d\n", nframes);

	for (int frame = 0; frame < nframes; frame += 1) {
		//@TODO: handle >2ch, read_ptr is 2ch only
		for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
			memcpy(areas[ch].ptr, read_ptr, outstream->bytes_per_sample);
			areas[ch].ptr += areas[ch].step;
			read_ptr += outstream->bytes_per_sample;
		}
	}

	if ((err = soundio_outstream_end_write(outstream))) {
		warning("slaudio/write_callback:"
				"end write error: %s\n", soundio_strerror(err));
		return; /*@TODO handle error */
	}

	soundio_ring_buffer_advance_read_ptr(ring_buffer, nframes * outstream->bytes_per_frame);

}


static int slaudio_start(void)
{
	int err = 0;
	double microphone_latency = 0.02;

	if (input == -1) {
		warning("slaudio/start: invalid input device\n");
		return 1;
	}

	if (output == -1) {
		warning("slaudio/start: invalid output device\n");
		return 1;
	}

	slaudio = mem_zalloc(sizeof(*slaudio), NULL);
	slaudio->inBuffer = mem_zalloc(sizeof(int16_t) * BUFFER_LEN,
					NULL);
	slaudio->inBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
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
		soundio_device_unref(slaudio->dev_in);
		goto err_out;
	}

	info("slaudio/start: in dev: %s\n", slaudio->dev_in->name);
	info("slaudio/start: out dev: %s\n", slaudio->dev_out->name);


	/* Create input stream */

	slaudio->instream = soundio_instream_create(slaudio->dev_in);
	if (!slaudio->instream) {
		warning("slaudio/start: error soundio_instream_create\n");
		err = ENOMEM;
		goto err_out_stream;
	}

	slaudio->instream->format = SoundIoFormatFloat32NE;
	slaudio->instream->read_callback = read_callback;
	slaudio->instream->software_latency = microphone_latency;

	err = soundio_instream_open(slaudio->instream);
	if (err) {
		warning("slaudio/start: soundio_instream_open: %s\n",
				soundio_strerror(err));
		goto err_out_open;
	}


	/* Create output stream */

	slaudio->outstream = soundio_outstream_create(slaudio->dev_out);
	if (!slaudio->outstream) {
		warning("slaudio/start: error soundio_outstream_create\n");
		err = ENOMEM;
		soundio_instream_destroy(slaudio->instream);
		goto err_out_stream;
	}

	slaudio->outstream->format = SoundIoFormatFloat32NE;
	slaudio->outstream->write_callback = write_callback;
	slaudio->outstream->software_latency = microphone_latency;
	slaudio->outstream->underflow_callback = underflow_callback;


	err = soundio_outstream_open(slaudio->outstream);
	if (err) {
		warning("slaudio/start: soundio_outstream_open: %s\n",
				soundio_strerror(err));
		goto err_out_open;
	}

	if (slaudio->outstream->layout_error)
		warning("slaudio/start: unable to set channel layout: %s\n",
			soundio_strerror(slaudio->outstream->layout_error));

	/* Prepare ring buffer */

	int capacity = microphone_latency * 2 * 3 * slaudio->instream->sample_rate * slaudio->instream->bytes_per_frame;
	ring_buffer = soundio_ring_buffer_create(slaudio->soundio, capacity);
	if (!ring_buffer) {
		warning("slaudio/start: error ring_buffer\n");
		err = ENOMEM;
		goto err_out_open;
	}

	preferred_sample_rate_in = slaudio->instream->sample_rate;
	preferred_sample_rate_out = slaudio->outstream->sample_rate;
	info("slaudio/start: sample_rate in: %d out: %d\n", preferred_sample_rate_in, preferred_sample_rate_out);

	/* Start streams */

	err = soundio_instream_start(slaudio->instream);
	if (err) {
		warning("slaudio/start: soundio_instream_start: %s\n",
				soundio_strerror(err));
		goto err_out_open;
	}

	err = soundio_outstream_start(slaudio->outstream);
	if (err) {
		warning("slaudio/start: soundio_outstream_start: %s\n",
				soundio_strerror(err));
		goto err_out_open;
	}


	info("slaudio/start: success\n");
	return 0;

err_out_open:
	soundio_instream_destroy(slaudio->instream);
	soundio_outstream_destroy(slaudio->outstream);

err_out_stream:
	soundio_device_unref(slaudio->dev_in);
	soundio_device_unref(slaudio->dev_out);
err_out:
	soundio_destroy(slaudio->soundio);

	warning("slaudio/start: error %d\n", err);
	return err;
}


static int slaudio_stop(void)
{
	if (src_state_in)
		src_state_in = src_delete(src_state_in);

	if (src_state_out)
		src_state_out = src_delete(src_state_out);

	if (slaudio) {
		soundio_instream_destroy(slaudio->instream);
		soundio_outstream_destroy(slaudio->outstream);
		soundio_device_unref(slaudio->dev_in);
		soundio_device_unref(slaudio->dev_out);
		soundio_destroy(slaudio->soundio);
		mem_deref(slaudio->inBuffer);
		mem_deref(slaudio->inBufferFloat);
		slaudio = mem_deref(slaudio);
	}

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

	err = odict_alloc(&interfaces, DICT_BSIZE);
	if (err)
		return ENOMEM;

	playmix = mem_zalloc(sizeof(int16_t) * 1920, NULL);
	if (!playmix)
		return ENOMEM;

	/* add local/recording session
	 */
	sess = mem_alloc(sizeof(*sess), sess_destruct);
	if (!sess)
		return ENOMEM;
	sess->local = true;
	sess->stream = false;
	list_append(&sessionl, &sess->le, sess);

	/* add remote sessions
	 */
	for (uint32_t cnt = 0; cnt < MAX_REMOTE_CHANNELS; cnt++)
	{
		sess = mem_alloc(sizeof(*sess), sess_destruct);
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

	return 0;
}


const struct mod_export DECL_EXPORTS(slaudio) = {
	"slaudio",
	"sound",
	slaudio_init,
	slaudio_close
};
