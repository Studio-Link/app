/**
 * @file sl_clap.c Studio Link CLAP plugin
 *
 * Studio Link (SL_ONAIR=0) or Studio Link OnAir (SL_ONAIR=1) as CLAP plugin.
 * VST3 and AUv2 are built from it by clap-wrapper (see CMakeLists.txt).
 *
 * The user interface is the web app, the plugin has no GUI, no parameters
 * and no state.
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <clap/clap.h>
#include <slplugin.h>
#include "sl_clap.h"

#ifndef SL_ONAIR
#define SL_ONAIR 0
#endif

#ifndef SL_PLUGIN_VERSION
#define SL_PLUGIN_VERSION "0.0.0"
#endif

#if SL_ONAIR
#define SL_CLAP_ID   "de.studio-link.onair"
#define SL_CLAP_NAME "Studio Link OnAir"
#else
#define SL_CLAP_ID   "de.studio-link.plugin"
#define SL_CLAP_NAME "Studio Link"
#endif


struct sl_clap {
	clap_plugin_t plugin;
	const clap_host_t *host;
	bool ready;            /* sl_plugin_init() succeeded */
	struct session *sess;  /* NULL with OnAir or max. sessions reached */
};


static const char *const features[] = {
	CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
	CLAP_PLUGIN_FEATURE_UTILITY,
	CLAP_PLUGIN_FEATURE_STEREO,
	NULL
};

static const clap_plugin_descriptor_t descriptor = {
	.clap_version = CLAP_VERSION_INIT,
	.id           = SL_CLAP_ID,
	.name         = SL_CLAP_NAME,
	.vendor       = "ITSR",
	.url          = "https://studio-link.de",
	.manual_url   = "https://doku.studio-link.de",
	.support_url  = "https://studio-link.de",
	.version      = SL_PLUGIN_VERSION,
	.description  = SL_CLAP_NAME " DAW plugin",
	.features     = features,
};


/*
 * Audio ports: one stereo in, one stereo out (in place)
 */

static uint32_t audio_ports_count(const clap_plugin_t *plugin, bool is_input)
{
	(void)plugin;
	(void)is_input;

	return 1;
}


static bool audio_ports_get(const clap_plugin_t *plugin, uint32_t index,
			    bool is_input, clap_audio_port_info_t *info)
{
	(void)plugin;

	if (index > 0)
		return false;

	info->id = 0;
	strncpy(info->name, is_input ? "Input" : "Output",
		sizeof(info->name) - 1);
	info->name[sizeof(info->name) - 1] = '\0';
	info->flags = CLAP_AUDIO_PORT_IS_MAIN;
	info->channel_count = 2;
	info->port_type = CLAP_PORT_STEREO;
	info->in_place_pair = 0;

	return true;
}


static const clap_plugin_audio_ports_t audio_ports = {
	.count = audio_ports_count,
	.get   = audio_ports_get,
};


/*
 * Plugin
 */

static bool plugin_init(const clap_plugin_t *plugin)
{
	struct sl_clap *sl = plugin->plugin_data;

	/* starts libre/baresip with the first instance */
	sl->ready = sl_plugin_init() == 0;
	if (!sl->ready)
		return false;

#if !SL_ONAIR
	sl->sess = effect_session_start();
#endif

	return true;
}


static void plugin_destroy(const clap_plugin_t *plugin)
{
	struct sl_clap *sl = plugin->plugin_data;

	if (sl->ready) {
#if !SL_ONAIR
		effect_session_stop(sl->sess);
#endif
		/* stops libre/baresip with the last instance */
		sl_plugin_close();
	}

	free(sl);
}


static bool plugin_activate(const clap_plugin_t *plugin, double sample_rate,
			    uint32_t min_frames, uint32_t max_frames)
{
	(void)plugin;
	(void)sample_rate; /* Studio Link expects 48 kHz */
	(void)min_frames;
	(void)max_frames;

	return true;
}


static void plugin_deactivate(const clap_plugin_t *plugin)
{
	(void)plugin;
}


static bool plugin_start_processing(const clap_plugin_t *plugin)
{
	(void)plugin;
	return true;
}


static void plugin_stop_processing(const clap_plugin_t *plugin)
{
	(void)plugin;
}


static void plugin_reset(const clap_plugin_t *plugin)
{
	(void)plugin;
}


static clap_process_status plugin_process(const clap_plugin_t *plugin,
					  const clap_process_t *process)
{
	struct sl_clap *sl = plugin->plugin_data;
	const uint32_t nframes = process->frames_count;

	if (process->audio_inputs_count < 1 ||
	    process->audio_outputs_count < 1)
		return CLAP_PROCESS_CONTINUE;

	const clap_audio_buffer_t *in  = &process->audio_inputs[0];
	const clap_audio_buffer_t *out = &process->audio_outputs[0];

	if (!in->data32 || !out->data32 || in->channel_count < 1 ||
	    out->channel_count < 1)
		return CLAP_PROCESS_CONTINUE;

	const uint32_t in1  = in->channel_count > 1 ? 1 : 0;
	const uint32_t out1 = out->channel_count > 1 ? 1 : 0;

	/* the core takes max. SL_PLUGIN_MAX_FRAMES per call */
	for (uint32_t pos = 0; pos < nframes; pos += SL_PLUGIN_MAX_FRAMES) {
		uint32_t n = nframes - pos;
		if (n > SL_PLUGIN_MAX_FRAMES)
			n = SL_PLUGIN_MAX_FRAMES;

		const float *input0  = in->data32[0] + pos;
		const float *input1  = in->data32[in1] + pos;
		float *output0 = out->data32[0] + pos;
		float *output1 = out->data32[out1] + pos;

		if (!sl->ready) {
			memmove(output0, input0, n * sizeof(float));
			if (output1 != output0)
				memmove(output1, input1, n * sizeof(float));
			continue;
		}

#if SL_ONAIR
		effectlive_src(input0, input1, n);

		/* pass through */
		memmove(output0, input0, n * sizeof(float));
		if (output1 != output0)
			memmove(output1, input1, n * sizeof(float));
#else
		/* order matters: play -> src (N-1 mix) -> bypass (output) */
		effect_play(sl->sess, output0, output1, n);
		effect_src(sl->sess, input0, input1, n);
		effect_bypass(sl->sess, output0, output1, input0, input1, n);
#endif
	}

	return CLAP_PROCESS_CONTINUE;
}


static const void *plugin_get_extension(const clap_plugin_t *plugin,
					const char *id)
{
	(void)plugin;

	if (!strcmp(id, CLAP_EXT_AUDIO_PORTS))
		return &audio_ports;

	return NULL;
}


static void plugin_on_main_thread(const clap_plugin_t *plugin)
{
	(void)plugin;
}


static const clap_plugin_t *plugin_create(const clap_host_t *host)
{
	struct sl_clap *sl = calloc(1, sizeof(*sl));
	if (!sl)
		return NULL;

	sl->host = host;
	sl->plugin.desc             = &descriptor;
	sl->plugin.plugin_data      = sl;
	sl->plugin.init             = plugin_init;
	sl->plugin.destroy          = plugin_destroy;
	sl->plugin.activate         = plugin_activate;
	sl->plugin.deactivate       = plugin_deactivate;
	sl->plugin.start_processing = plugin_start_processing;
	sl->plugin.stop_processing  = plugin_stop_processing;
	sl->plugin.reset            = plugin_reset;
	sl->plugin.process          = plugin_process;
	sl->plugin.get_extension    = plugin_get_extension;
	sl->plugin.on_main_thread   = plugin_on_main_thread;

	return &sl->plugin;
}


/*
 * Factory and entry
 */

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *factory)
{
	(void)factory;
	return 1;
}


static const clap_plugin_descriptor_t *factory_get_plugin_descriptor(
	const clap_plugin_factory_t *factory, uint32_t index)
{
	(void)factory;
	return index == 0 ? &descriptor : NULL;
}


static const clap_plugin_t *factory_create_plugin(
	const clap_plugin_factory_t *factory, const clap_host_t *host,
	const char *plugin_id)
{
	(void)factory;

	if (!clap_version_is_compatible(host->clap_version))
		return NULL;

	if (strcmp(plugin_id, descriptor.id))
		return NULL;

	return plugin_create(host);
}


static const clap_plugin_factory_t factory = {
	.get_plugin_count      = factory_get_plugin_count,
	.get_plugin_descriptor = factory_get_plugin_descriptor,
	.create_plugin         = factory_create_plugin,
};


bool sl_clap_entry_init(const char *plugin_path)
{
	(void)plugin_path;
	return true;
}


void sl_clap_entry_deinit(void)
{
}


const void *sl_clap_entry_get_factory(const char *factory_id)
{
	if (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
		return &factory;

	return NULL;
}
