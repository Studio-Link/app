/**
 * @file slaudio.c SLAUDIO (PortAudio)
 *
 * Copyright (C) 2020 - 2026 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <samplerate.h>
#include <portaudio.h>
#include <pa_ringbuffer.h>
#include "slaudio.h"
#ifdef WIN32
#include <windows.h>
#endif
#include <webapp.h>

#define SRATE 48000
#define BUFFER_LEN 19200 /* max buffer_len = 192kHz*2ch*25ms*2frames */
#define FLOAT_FRAME_BYTES 8 /* sizeof(float) * 2ch */
#define RING_FRAMES 16384 /* power of 2, ~340ms @ 48kHz */
#define BUFFER_MS 10 /* PortAudio callback buffer size */
#define MONO_MIX_ALL 99 /* first_input_channel: mix all inputs to mono */
#define MAX_IN_CHANNELS 64

enum
{
	MAX_REMOTE_CHANNELS = 6,
};

struct slaudio_st
{
	int16_t *inBuffer;
	float *inBufferFloat;
	float *inBufferOutFloat;
	float *outBufferFloat;
	float *ringData;
	PaUtilRingBuffer ring;
	bool ring_primed; /* output callback only, set on first full read */
	PaStream *instream;
	PaStream *outstream;
	int in_channels;
	int out_channels;
	double in_srate;
	double out_srate;
};

static struct slaudio_st *slaudio;
static bool pa_initialized = false;

int16_t *empty_buffer;

struct auplay_st
{
	bool run;
	int16_t *sampv;
	size_t sampc;
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
	struct session *sess;
};

struct ausrc_st
{
	bool run;
	int16_t *sampv;
	size_t sampc;
	ausrc_read_h *rh;
	void *arg;
	struct ausrc_prm prm;
	struct session *sess;
};

static struct ausrc *ausrc;
static struct auplay *auplay;

static PaHostApiIndex driver = -1;
static PaDeviceIndex input = -1;
static int first_input_channel = 0;
static PaDeviceIndex output = -1;
static struct odict *interfaces = NULL;
struct list sessionl;
static SRC_STATE *src_state_in;
static SRC_STATE *src_state_out;

static int slaudio_stop(void);
static int slaudio_start(void);
static int slaudio_devices(void);
static int slaudio_drivers(void);
void webapp_ws_rtaudio_sync(void);

static int16_t *playmix;

static bool monorecord = true;
static bool monostream = true;
static bool mute = false;
static bool monitor = true;
static bool fatal_error = false;
char error_msg[512] = {0};

static int16_t startup_count = 0;

/* Protects the session audio state (st_src, st_play, dstmix, run_src,
 * run_play) shared between the PortAudio input callback and the main
 * thread (ausrc/auplay alloc and destruct). */
static mtx_t *sess_lock;

static struct sl_debug {
	RE_ATOMIC uint64_t out_ur;
	RE_ATOMIC uint64_t in_or;
	RE_ATOMIC uint64_t ring_ur;
	RE_ATOMIC uint64_t ring_or;
	RE_ATOMIC bool changed;
} sl_debug;

static struct tmr sl_debug_tmr;
static struct tmr sl_watch_tmr;

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


static void debug_count(RE_ATOMIC uint64_t *cnt)
{
	re_atomic_rlx_add(cnt, 1);
	re_atomic_rlx_set(&sl_debug.changed, true);
}


static void sess_destruct(void *arg)
{
	struct session *sess = arg;

	if (sess->run_record)
	{
		sess->run_record = false;
		(void)thrd_join(sess->record_thread, NULL);
	}

	slaudio_record_closefile(sess);

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


/* Main thread: true if the audio callbacks may run */
bool slaudio_audio_running(void)
{
	return slaudio != NULL;
}


/* Main thread: show an error message in the audio interface settings */
void slaudio_error_set(const char *msg)
{
	if (!interfaces || !msg)
		return;

	str_ncpy(error_msg, msg, sizeof(error_msg));
	odict_entry_del(interfaces, "error_msg");
	odict_entry_add(interfaces, "error_msg", ODICT_STRING, error_msg);
	webapp_ws_rtaudio_sync();
}


/*
 * PortAudio only scans devices on initialization, so it is re-initialized
 * on every reset to pick up newly connected devices.
 */
static int pa_reinit(void)
{
	PaError perr;

	if (pa_initialized) {
		Pa_Terminate();
		pa_initialized = false;
	}

	perr = Pa_Initialize();
	if (perr != paNoError) {
		warning("slaudio: Pa_Initialize: %s\n", Pa_GetErrorText(perr));
		return ENODEV;
	}

	pa_initialized = true;
	info("slaudio: %s\n", Pa_GetVersionText());

	return 0;
}


int slaudio_reset(void)
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

	if (fatal_error) {
		output = -1;
		input = -1;
		first_input_channel = 0;
		sys_msleep(400);
		fatal_error = false;
	}

	err = pa_reinit();
	if (!err) {
		slaudio_drivers();
		slaudio_devices();
	}
	webapp_ws_rtaudio_sync();
	if (!err)
		slaudio_start();

	return err;
}


void slaudio_set_driver(int value)
{
	driver = value;
	output = -1;
	input = -1;
	first_input_channel = 0;
	fatal_error = false;
	slaudio_reset();
}


/*
 * An explicit user selection clears fatal_error, otherwise slaudio_reset()
 * would discard it and fall back to the (maybe broken) default devices.
 */
void slaudio_set_input(int value)
{
	input = value;
	first_input_channel = 0;
	fatal_error = false;
	slaudio_reset();
}


void slaudio_set_first_input_channel(int value)
{
	first_input_channel = value;
	fatal_error = false;
	slaudio_reset();
}


void slaudio_set_output(int value)
{
	output = value;
	fatal_error = false;
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


/* Select the input channel(s) of the interleaved device buffer as stereo */
static void downsample_first_ch(float *outv, const float *in,
		unsigned long nframes, int channels)
{
	float left;
	float right;

	for (unsigned long frame = 0; frame < nframes; frame++) {
		left = 0.0;
		right = 0.0;

		for (int ch = 0; ch < channels; ch++) {
			if (first_input_channel == MONO_MIX_ALL) {
				left += in[ch];
				right = left;
				continue;
			}

			if (ch == first_input_channel)
				left = in[ch];

			if (ch == first_input_channel + 1)
				right = in[ch];
		}

		if (monorecord)
			right = left;

		in += channels;

		*outv++ = left;
		*outv++ = right;
	}
}


static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;
	struct session *sess = st->sess;
	info("slaudio: destruct ausrc\n");

	/* waits for a running input callback to finish */
	mtx_lock(sess_lock);
	if (sess->st_src == st) {
		sess->run_src = false;
		sess->st_src = NULL;
	}
	mtx_unlock(sess_lock);

	mem_deref(st->sampv);
}


static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;
	struct session *sess = st->sess;
	int32_t *dstmix = NULL;
	info("slaudio: destruct auplay\n");

	/* waits for a running input callback to finish */
	mtx_lock(sess_lock);
	if (sess->st_play == st) {
		sess->run_play = false;
		sess->st_play = NULL;
		dstmix = sess->dstmix;
		sess->dstmix = NULL;
	}
	mtx_unlock(sess_lock);

	mem_deref(dstmix);
	mem_deref(st->sampv);
}


static int src_alloc(struct ausrc_st **stp, const struct ausrc *as,
		struct ausrc_prm *prm, const char *device,
		ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st_src = NULL;
	struct session *sess = NULL;
	struct le *le;
	(void)errh;
	(void)device;
	(void)as;

	if (!stp || !prm)
		return EINVAL;

	if (prm->srate != SRATE || prm->ch != 2 || prm->fmt != AUFMT_S16LE) {
		warning("slaudio: unsupported source format %uHz/%uch/%s, "
			"check ausrc_srate/ausrc_channels config\n",
			prm->srate, prm->ch, aufmt_name(prm->fmt));
		return ENOTSUP;
	}

	for (le = sessionl.head; le; le = le->next)
	{
		struct session *s = le->data;

		if (!s->run_src && !s->local && s->call)
		{
			sess = s;
			break;
		}
	}

	if (!sess)
		return EINVAL;

	st_src = mem_zalloc(sizeof(*st_src), ausrc_destructor);
	if (!st_src)
		return ENOMEM;

	st_src->sess = sess;
	st_src->rh = rh;
	st_src->arg = arg;
	st_src->prm = *prm;

	st_src->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_src->sampv = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	if (!st_src->sampv) {
		/* destructor expects a session */
		mem_deref(st_src);
		return ENOMEM;
	}

	st_src->run = true;

	mtx_lock(sess_lock);
	sess->st_src = st_src;
	sess->run_src = true;
	mtx_unlock(sess_lock);

	*stp = st_src;

	return 0;
}


static int play_alloc(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st_play = NULL;
	struct session *sess = NULL;
	int32_t *dstmix;
	struct le *le;
	(void)device;
	(void)ap;

	if (!stp || !prm)
		return EINVAL;

	if (prm->srate != SRATE || prm->ch != 2 || prm->fmt != AUFMT_S16LE) {
		warning("slaudio: unsupported player format %uHz/%uch/%s, "
			"check auplay_srate/auplay_channels config\n",
			prm->srate, prm->ch, aufmt_name(prm->fmt));
		return ENOTSUP;
	}

	for (le = sessionl.head; le; le = le->next)
	{
		struct session *s = le->data;

		if (!s->run_play && !s->local && s->call)
		{
			sess = s;
			break;
		}
	}

	if (!sess)
		return EINVAL;

	st_play = mem_zalloc(sizeof(*st_play), auplay_destructor);
	if (!st_play)
		return ENOMEM;

	st_play->sess = sess;
	st_play->wh = wh;
	st_play->arg = arg;
	st_play->prm = *prm;
	st_play->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st_play->sampv = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	dstmix = mem_zalloc(sizeof(int32_t) * BUFFER_LEN, NULL);
	if (!st_play->sampv || !dstmix) {
		mem_deref(dstmix);
		mem_deref(st_play);
		return ENOMEM;
	}

	st_play->run = true;

	mtx_lock(sess_lock);
	sess->dstmix = dstmix;
	sess->st_play = st_play;
	sess->run_play = true;
	mtx_unlock(sess_lock);

	*stp = st_play;

	return 0;
}


static const char *driver_display_name(const PaHostApiInfo *api)
{
	switch (api->type) {

	case paALSA:        return "ALSA";
	case paPulseAudio:  return "PulseAudio";
	case paJACK:        return "JACK";
	case paWASAPI:      return "WASAPI";
	case paWDMKS:       return "WDM-KS";
	case paMME:         return "MME";
	case paDirectSound: return "DirectSound";
	case paCoreAudio:   return "CoreAudio";
	default:            return api->name;
	}
}


static PaHostApiIndex driver_default(void)
{
	static const PaHostApiTypeId prio[] = {
		paPulseAudio, paWASAPI, paCoreAudio
	};

	for (size_t i = 0; i < RE_ARRAY_SIZE(prio); i++) {
		PaHostApiIndex idx = Pa_HostApiTypeIdToHostApiIndex(prio[i]);
		if (idx >= 0 && Pa_GetHostApiInfo(idx)->deviceCount > 0)
			return idx;
	}

	return Pa_GetDefaultHostApi();
}


static int slaudio_drivers(void)
{
	int err = 0;
	struct odict *o;
	struct odict *array;
	char idx[16];
	PaHostApiIndex count = Pa_GetHostApiCount();

	if (driver >= count || driver < -1)
		driver = -1;

	if (driver == -1)
		driver = driver_default();

	err = odict_alloc(&array, DICT_BSIZE);
	if (err)
		return ENOMEM;

	for (PaHostApiIndex i = 0; i < count; i++)
	{
		const PaHostApiInfo *api = Pa_GetHostApiInfo(i);
		const char *name;

		if (!api)
			continue;

		name = driver_display_name(api);
		info("slaudio/drivers detected: %s (%d devices)\n", name,
				api->deviceCount);

		err = odict_alloc(&o, DICT_BSIZE);
		if (err)
			break;

		(void)re_snprintf(idx, sizeof(idx), "%d", i);

		odict_entry_add(o, "display", ODICT_STRING, name);
		odict_entry_add(o, "selected", ODICT_BOOL, driver == i);
		odict_entry_add(o, "id", ODICT_INT, (int64_t)i);
		odict_entry_add(array, idx, ODICT_OBJECT, o);

		if (driver == i)
			info("slaudio/drivers: %s selected\n", name);

		mem_deref(o);
	}

	odict_entry_add(interfaces, "drivers", ODICT_ARRAY, array);
	mem_deref(array);

	return err;
}


static void slaudio_destruct(void *arg)
{
	struct slaudio_st *st = arg;

	if (st->instream) {
		Pa_AbortStream(st->instream);
		Pa_CloseStream(st->instream);
	}

	if (st->outstream) {
		Pa_AbortStream(st->outstream);
		Pa_CloseStream(st->outstream);
	}

	mem_deref(st->inBuffer);
	mem_deref(st->inBufferFloat);
	mem_deref(st->inBufferOutFloat);
	mem_deref(st->outBufferFloat);
	mem_deref(st->ringData);
}


static int slaudio_construct(void)
{
	int err = 0;

	slaudio = mem_zalloc(sizeof(*slaudio), slaudio_destruct);
	if (!slaudio)
		return ENOMEM;

	slaudio->inBuffer = mem_zalloc(sizeof(int16_t) * BUFFER_LEN, NULL);
	slaudio->inBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN, NULL);
	slaudio->inBufferOutFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
					       NULL);
	slaudio->outBufferFloat = mem_zalloc(sizeof(float) * BUFFER_LEN,
					     NULL);
	slaudio->ringData = mem_zalloc(FLOAT_FRAME_BYTES * RING_FRAMES, NULL);

	if (!slaudio->inBuffer || !slaudio->inBufferFloat ||
	    !slaudio->inBufferOutFloat || !slaudio->outBufferFloat ||
	    !slaudio->ringData) {
		slaudio = mem_deref(slaudio);
		return ENOMEM;
	}

	if (PaUtil_InitializeRingBuffer(&slaudio->ring, FLOAT_FRAME_BYTES,
					RING_FRAMES, slaudio->ringData) < 0) {
		slaudio = mem_deref(slaudio);
		return EINVAL;
	}

	/** Initialize the sample rate converter for input */
	if ((src_state_in = src_new(SRC_SINC_FASTEST, 2, &err)) == NULL)
	{
		warning("slaudio/start: Samplerate::src_new failed : %s.\n",
				src_strerror(err));
		return EINVAL;
	};

	/** Initialize the sample rate converter for output */
	if ((src_state_out = src_new(SRC_SINC_FASTEST, 2, &err)) == NULL)
	{
		warning("slaudio/start: Samplerate::src_new failed : %s.\n",
				src_strerror(err));
		return EINVAL;
	};

	return 0;
}


static bool device_valid(PaDeviceIndex dev, bool in)
{
	const PaDeviceInfo *dinfo;

	if (dev < 0 || dev >= Pa_GetDeviceCount())
		return false;

	dinfo = Pa_GetDeviceInfo(dev);
	if (!dinfo || dinfo->hostApi != driver)
		return false;

	return in ? dinfo->maxInputChannels > 0 :
		dinfo->maxOutputChannels > 0;
}


/*
 * Usable channel count of a device. PortAudio reports a placeholder of 32
 * channels (PA_CHANNELS_MAX) for the virtual PulseAudio "Default" devices,
 * opening them with 32 channels fails, so they are used as stereo.
 */
static int device_channels(PaDeviceIndex dev, bool in)
{
	const PaDeviceInfo *dinfo = Pa_GetDeviceInfo(dev);
	const PaHostApiInfo *api;
	int channels;

	if (!dinfo)
		return 0;

	channels = in ? dinfo->maxInputChannels : dinfo->maxOutputChannels;

	api = Pa_GetHostApiInfo(dinfo->hostApi);
	if (api && api->type == paPulseAudio &&
	    (dev == api->defaultInputDevice ||
	     dev == api->defaultOutputDevice))
		channels = MIN(channels, 2);

	return MIN(channels, MAX_IN_CHANNELS);
}


static int add_device(struct odict *array, PaDeviceIndex dev, bool in)
{
	const PaDeviceInfo *dinfo = Pa_GetDeviceInfo(dev);
	struct odict *o;
	char idx[16];
	int err;

	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return err;

	(void)re_snprintf(idx, sizeof(idx), "%d", dev);

	odict_entry_add(o, "id", ODICT_INT, (int64_t)dev);
	odict_entry_add(o, "display", ODICT_STRING, dinfo->name);

	if (in) {
		int channels = device_channels(dev, true);
		info("slaudio: input device: %s (%d channels)\n",
				dinfo->name, channels);
		odict_entry_add(o, "channels", ODICT_INT, (int64_t)channels);
		odict_entry_add(o, "first_input_channel", ODICT_INT,
				(int64_t)first_input_channel);
		odict_entry_add(o, "selected", ODICT_BOOL, input == dev);
	}
	else {
		info("slaudio: output device: %s\n", dinfo->name);
		odict_entry_add(o, "selected", ODICT_BOOL, output == dev);
	}

	err = odict_entry_add(array, idx, ODICT_OBJECT, o);
	mem_deref(o);

	return err;
}


static int slaudio_devices(void)
{
	int err = 0;
	struct odict *array_in = NULL;
	struct odict *array_out = NULL;
	const PaHostApiInfo *api;

	err = slaudio_construct();
	if (err)
		return err;

	api = Pa_GetHostApiInfo(driver);
	if (!api) {
		warning("slaudio/devices: invalid driver %d\n", driver);
		return EINVAL;
	}

	err  = odict_alloc(&array_in, DICT_BSIZE);
	err |= odict_alloc(&array_out, DICT_BSIZE);
	if (err)
		goto out;

	/* Fallback to the driver defaults, then to the first device */
	if (!device_valid(input, true))
		input = api->defaultInputDevice;
	if (!device_valid(output, false))
		output = api->defaultOutputDevice;

	for (int i = 0; i < api->deviceCount; i++) {
		PaDeviceIndex dev = Pa_HostApiDeviceIndexToDeviceIndex(driver,
								       i);
		if (!device_valid(input, true) && device_valid(dev, true))
			input = dev;
		if (!device_valid(output, false) && device_valid(dev, false))
			output = dev;
	}

	if (first_input_channel != MONO_MIX_ALL && device_valid(input, true)
	    && first_input_channel >= device_channels(input, true))
		first_input_channel = 0;

	for (int i = 0; i < api->deviceCount; i++) {
		PaDeviceIndex dev = Pa_HostApiDeviceIndexToDeviceIndex(driver,
								       i);
		if (device_valid(dev, true))
			err |= add_device(array_in, dev, true);

		if (device_valid(dev, false))
			err |= add_device(array_out, dev, false);
	}

	odict_entry_add(interfaces, "input", ODICT_ARRAY, array_in);
	odict_entry_add(interfaces, "output", ODICT_ARRAY, array_out);

out:
	mem_deref(array_in);
	mem_deref(array_out);

	return err;
}


static void process_input(const float *in, unsigned long nframes)
{
	int err;
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
	struct auframe af;
	static uint64_t frames = 0;
	bool record;

	if (!nframes)
		return;

	if (startup_count < 16) {
		/* drop first frames (keeps buffers small) */
		++startup_count;
		return;
	}

	samples = (int)nframes * 2; /* Stereo (2 ch) only */
	if (samples > BUFFER_LEN / 2)
		return;

	if (!in) {
		memset(slaudio->inBufferFloat, 0, samples * sizeof(float));
		debug_count(&sl_debug.in_or);
	}
	else {
		downsample_first_ch(slaudio->inBufferFloat, in, nframes,
				slaudio->in_channels);
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
	if (slaudio->in_srate != SRATE)
	{
		if (!src_state_in)
			return;

		src_data_in.data_in = slaudio->inBufferFloat;
		src_data_in.data_out = slaudio->inBufferOutFloat;
		src_data_in.input_frames = nframes;
		src_data_in.output_frames = BUFFER_LEN / 2;
		src_data_in.src_ratio = SRATE / slaudio->in_srate;
		src_data_in.end_of_input = 0;

		if ((err = src_process(src_state_in, &src_data_in)) != 0)
		{
			warning("slaudio: Samplerate::src_process_in :"
					"returned error : %s %f\n",
					src_strerror(err),
					src_data_in.src_ratio);
			return;
		};

		samples = (int)src_data_in.output_frames_gen * 2;
		auconv_to_s16(slaudio->inBuffer, AUFMT_FLOAT,
				slaudio->inBufferOutFloat,
				samples);
	}
	else {
		auconv_to_s16(slaudio->inBuffer, AUFMT_FLOAT,
				slaudio->inBufferFloat, samples);
	}
	/** Input Samplerate conversion -->*/

	if (!samples)
		return;

	mtx_lock(sess_lock);

	/* same period for all tracks (sample aligned recording) */
	record = slaudio_record_period(samples);

	for (le = sessionl.head; le; le = le->next)
	{
		sess = le->data;
		if (sess->local && record)
		{
			/* write local audio to flac record buffer */
			(void)aubuf_write_samp(sess->aubuf,
					slaudio->inBuffer, samples);
		}

		if (!sess->run_play || sess->local)
			continue;

		st_play = sess->st_play;

		webapp_session_play_read(sess, st_play->sampv,
				st_play->wh, samples, st_play->arg);
	}

	if (monostream && !monorecord)
	{
		stereo2mono(slaudio->inBuffer, samples / 2);
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

		if (sess->local)
			continue;

		if (!sess->run_play) {
			if (record)
				(void)aubuf_write_samp(sess->aubuf,
						empty_buffer, samples);
			continue;
		}

		st_play = sess->st_play;

		for (int pos = 0; pos < samples; pos++)
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
		if (record)
			(void)aubuf_write_samp(sess->aubuf, st_play->sampv,
					samples);

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

			if (!msess->run_play || msess == sess || msess->local)
			{
				continue;
			}
			mst_play = msess->st_play;

			for (int pos = 0; pos < samples; pos++)
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
		if (sess->run_src && sess->run_play && !sess->local)
		{
			st_src = sess->st_src;

			for (int pos = 0; pos < samples; pos++)
			{
				int32_t val = slaudio->inBuffer[pos];

				/* ignore on last call the dstmix */
				if (sessplay)
					val += sess->dstmix[pos];

				if (val >= 32767)
					val = 32767;
				if (val <= -32767)
					val = -32767;

				st_src->sampv[pos] = (int16_t)val;
			}

			auframe_init(&af, AUFMT_S16LE, st_src->sampv, samples,
				     SRATE, 2);
			af.timestamp = frames * AUDIO_TIMEBASE / SRATE;

			st_src->rh(&af, st_src->arg);
		}
	}

	mtx_unlock(sess_lock);

	frames += samples / 2;

	if (!cntplay && !monitor)
	{
		memset(playmix, 0, samples * sizeof(int16_t));
	}

	/* Output: mix -> (resampler) -> ring buffer -> output callback */
	auconv_from_s16(AUFMT_FLOAT, slaudio->outBufferFloat, playmix,
			samples);

	float *outv = slaudio->outBufferFloat;
	long out_frames = samples / 2;

	if (slaudio->out_srate != SRATE)
	{
		if (!src_state_out)
			return;

		src_data_out.data_in = slaudio->outBufferFloat;
		src_data_out.data_out = slaudio->inBufferOutFloat;
		src_data_out.input_frames = samples / 2;
		src_data_out.output_frames = BUFFER_LEN / 2;
		src_data_out.src_ratio = slaudio->out_srate / SRATE;
		src_data_out.end_of_input = 0;

		if ((err = src_process(src_state_out, &src_data_out)) != 0)
		{
			warning("slaudio: Samplerate::src_process_out :"
				"returned error : %s\n", src_strerror(err));
			return;
		};

		outv = slaudio->inBufferOutFloat;
		out_frames = src_data_out.output_frames_gen;
	}

	if (PaUtil_GetRingBufferWriteAvailable(&slaudio->ring) < out_frames)
		debug_count(&sl_debug.ring_or);

	(void)PaUtil_WriteRingBuffer(&slaudio->ring, outv, out_frames);
}


static int read_callback(const void *input_buf, void *output_buf,
		unsigned long nframes,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags flags, void *arg)
{
	(void)output_buf;
	(void)time_info;
	(void)arg;

	if (flags & paInputOverflow)
		debug_count(&sl_debug.in_or);

	process_input(input_buf, nframes);

	return paContinue;
}


static int write_callback(const void *input_buf, void *output_buf,
		unsigned long nframes,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags flags, void *arg)
{
	float *out = output_buf;
	float frame[2];
	(void)input_buf;
	(void)time_info;
	(void)arg;

	if (flags & paOutputUnderflow)
		debug_count(&sl_debug.out_ur);

	if (PaUtil_GetRingBufferReadAvailable(&slaudio->ring) <
	    (ring_buffer_size_t)nframes) {
		/* Ring buffer does not have enough data, fill with zeroes.
		 * Don't count underruns before the input stream delivered
		 * its first data (startup priming) */
		if (slaudio->ring_primed)
			debug_count(&sl_debug.ring_ur);
		memset(out, 0,
		       nframes * slaudio->out_channels * sizeof(float));
		return paContinue;
	}

	slaudio->ring_primed = true;

	if (slaudio->out_channels == 2) {
		(void)PaUtil_ReadRingBuffer(&slaudio->ring, out,
					    (ring_buffer_size_t)nframes);
		return paContinue;
	}

	/* mono output */
	for (unsigned long i = 0; i < nframes; i++) {
		(void)PaUtil_ReadRingBuffer(&slaudio->ring, frame, 1);
		out[i] = frame[0];
	}

	return paContinue;
}


/* Prefer 48kHz, fallback to the device default rate (resampled) */
static double stream_srate(const PaStreamParameters *in,
		const PaStreamParameters *out, double fallback)
{
	if (Pa_IsFormatSupported(in, out, SRATE) == paFormatIsSupported)
		return SRATE;

	return fallback;
}


static int open_stream(PaStream **stream, const PaStreamParameters *in,
		const PaStreamParameters *out, double srate,
		PaStreamCallback *cb)
{
	PaError perr;

	perr = Pa_OpenStream(stream, in, out, srate,
			     (unsigned long)(srate * BUFFER_MS / 1000),
			     paClipOff | paDitherOff, cb, NULL);
	if (perr != paNoError) {
		warning("slaudio/start: Pa_OpenStream: %s\n",
				Pa_GetErrorText(perr));
		return EINVAL;
	}

	return 0;
}


static int slaudio_start(void)
{
	int err = 0;
	PaError perr;
	PaStreamParameters in_prm, out_prm;
	const PaDeviceInfo *dev_in, *dev_out;

	if (!slaudio) {
		err = EINVAL;
		goto err_out;
	}

	if (!device_valid(input, true)) {
		warning("slaudio/start: invalid input device\n");
		err = EINVAL;
		goto err_out;
	}

	if (!device_valid(output, false)) {
		warning("slaudio/start: invalid output device\n");
		err = EINVAL;
		goto err_out;
	}

	dev_in = Pa_GetDeviceInfo(input);
	dev_out = Pa_GetDeviceInfo(output);

	info("slaudio/start: in dev: %s\n", dev_in->name);
	info("slaudio/start: out dev: %s\n", dev_out->name);

	slaudio->in_channels = device_channels(input, true);
	slaudio->out_channels = MIN(device_channels(output, false), 2);

	memset(&in_prm, 0, sizeof(in_prm));
	in_prm.device = input;
	in_prm.channelCount = slaudio->in_channels;
	in_prm.sampleFormat = paFloat32;
	in_prm.suggestedLatency = dev_in->defaultLowInputLatency;

	memset(&out_prm, 0, sizeof(out_prm));
	out_prm.device = output;
	out_prm.channelCount = slaudio->out_channels;
	out_prm.sampleFormat = paFloat32;
	out_prm.suggestedLatency = dev_out->defaultLowOutputLatency;

	slaudio->in_srate = stream_srate(&in_prm, NULL,
					 dev_in->defaultSampleRate);
	slaudio->out_srate = stream_srate(NULL, &out_prm,
					  dev_out->defaultSampleRate);

	err = open_stream(&slaudio->instream, &in_prm, NULL,
			  slaudio->in_srate, read_callback);
	if (err)
		goto err_out;

	err = open_stream(&slaudio->outstream, NULL, &out_prm,
			  slaudio->out_srate, write_callback);
	if (err)
		goto err_out;

	info("slaudio/start: in: %dhz (%dch) out: %dhz (%dch)\n",
			(int)slaudio->in_srate, slaudio->in_channels,
			(int)slaudio->out_srate, slaudio->out_channels);

	slaudio->ring_primed = false;

	perr = Pa_StartStream(slaudio->instream);
	if (perr == paNoError)
		perr = Pa_StartStream(slaudio->outstream);
	if (perr != paNoError) {
		warning("slaudio/start: Pa_StartStream: %s\n",
				Pa_GetErrorText(perr));
		err = EIO;
		goto err_out;
	}

	info("slaudio/start: success\n");
	return 0;

err_out:
	re_snprintf(error_msg, sizeof(error_msg),
		"Error starting Audio, please try reload "
		"or select another device...");
	odict_entry_add(interfaces, "error_msg", ODICT_STRING, error_msg);
	webapp_ws_rtaudio_sync();
	fatal_error = true;
	slaudio = mem_deref(slaudio);
	warning("slaudio/start: error %m\n", err);
	return err;
}


static int slaudio_stop(void)
{
	slaudio = mem_deref(slaudio);

	/* no audio period can finish a requested record stop anymore */
	slaudio_record_audio_stopped();

	if (src_state_in)
		src_state_in = src_delete(src_state_in);

	if (src_state_out)
		src_state_out = src_delete(src_state_out);

	return 0;
}


/* PortAudio has no error callback: restart if a stream stopped by itself
 * (e.g. the device was removed) */
static void slaudio_watch(void *arg)
{
	(void)arg;

	tmr_start(&sl_watch_tmr, 1000, slaudio_watch, NULL);

	if (!slaudio || !slaudio->instream || !slaudio->outstream)
		return;

	if (Pa_IsStreamActive(slaudio->instream) == 1 &&
	    Pa_IsStreamActive(slaudio->outstream) == 1)
		return;

	warning("slaudio: audio stream stopped, restarting\n");
	fatal_error = true;
	slaudio_reset();
}


static void slaudio_debug(void *arg)
{
	(void)arg;

	if (!re_atomic_rlx(&sl_debug.changed))
		goto out;

	warning("slaudio_debug\n"
		"\tout_ur: %llu \n"
		"\tin_or: %llu \n"
		"\tring_ur: %llu \n"
		"\tring_or: %llu \n",
		re_atomic_rlx(&sl_debug.out_ur),
		re_atomic_rlx(&sl_debug.in_or),
		re_atomic_rlx(&sl_debug.ring_ur),
		re_atomic_rlx(&sl_debug.ring_or));

	re_atomic_rlx_set(&sl_debug.changed, false);

out:
	tmr_start(&sl_debug_tmr, 10000, slaudio_debug, NULL);
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

	err = mutex_alloc(&sess_lock);
	if (err)
		return err;

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
		sess->run_src = false;
		sess->run_play = false;
		sess->volume = 1.0;

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
	sess->volume = 1.0;
	sess->ch = MAX_REMOTE_CHANNELS * 2 + 1;
	list_append(&sessionl, &sess->le, sess);

	slaudio_record_init();

	err = pa_reinit();
	if (!err) {
		slaudio_drivers();
		slaudio_devices();
		slaudio_start();
	}

	tmr_init(&sl_debug_tmr);
	tmr_start(&sl_debug_tmr, 10000, slaudio_debug, NULL);

	tmr_init(&sl_watch_tmr);
	tmr_start(&sl_watch_tmr, 1000, slaudio_watch, NULL);

#ifdef WIN32
	/* Activate windows low latency timer */
	timeBeginPeriod(1);
#endif
	info("slaudio ready\n");

	return 0;
}


static int slaudio_close(void)
{
	struct le *le;
	struct session *sess;

	tmr_cancel(&sl_watch_tmr);

	slaudio_stop();

	if (pa_initialized) {
		Pa_Terminate();
		pa_initialized = false;
	}

	slaudio_record_close();

	re_atomic_rlx_set(&sl_debug.changed, true);
	slaudio_debug(NULL);
	tmr_cancel(&sl_debug_tmr);

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
	sess_lock = mem_deref(sess_lock);

#ifdef WIN32
	timeEndPeriod(1);
#endif

	return 0;
}


const struct mod_export DECL_EXPORTS(slaudio) = {
	"slaudio",
	"sound",
	slaudio_init,
	slaudio_close
};
