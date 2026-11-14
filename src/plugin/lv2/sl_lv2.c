/**
 * @file sl_lv2.c Studio Link LV2 plugin
 *
 * Studio Link (SL_ONAIR=0) or Studio Link OnAir (SL_ONAIR=1) as LV2 plugin,
 * ported from overlay-lv2 and overlay-onair-lv2.
 *
 * Copyright (C) 2015-2026 studio-link.de
 */
#include <stdlib.h>
#include <string.h>
#include <slplugin.h>
#include "lv2.h"

#ifndef SL_ONAIR
#define SL_ONAIR 0
#endif

#if SL_ONAIR
#define SL_LV2_URI "http://studio-link.de/plugins/lv2/studio-link-onair"
#else
#define SL_LV2_URI "http://studio-link.de/plugins/lv2/studio-link"
#endif


typedef enum {
	PORT_INPUT0  = 0,
	PORT_INPUT1  = 1,
	PORT_OUTPUT0 = 2,
	PORT_OUTPUT1 = 3
} PortIndex;

typedef struct {
	const float *input0;
	const float *input1;
	float *output0;
	float *output1;
	struct session *sess;
} SlLv2;


static LV2_Handle instantiate(const LV2_Descriptor *descriptor, double rate,
			      const char *bundle_path,
			      const LV2_Feature *const *features)
{
	SlLv2 *sl;
	(void)descriptor;
	(void)bundle_path;
	(void)features;

	sl = calloc(1, sizeof(*sl));
	if (!sl)
		return NULL;

	/* starts libre/baresip with the first instance */
	if (sl_plugin_init()) {
		free(sl);
		return NULL;
	}

#if !SL_ONAIR
	sl->sess = effect_session_start();
#endif

	/* LV2 fixes the rate per instance: Studio Link only works at
	 * 48 kHz, warn the user (log and web app) */
	sl_plugin_samplerate(rate);

	return (LV2_Handle)sl;
}


static void connect_port(LV2_Handle instance, uint32_t port, void *data)
{
	SlLv2 *sl = (SlLv2 *)instance;

	switch ((PortIndex)port) {

	case PORT_INPUT0:
		sl->input0 = (const float *)data;
		break;
	case PORT_INPUT1:
		sl->input1 = (const float *)data;
		break;
	case PORT_OUTPUT0:
		sl->output0 = (float *)data;
		break;
	case PORT_OUTPUT1:
		sl->output1 = (float *)data;
		break;
	}
}


static void activate(LV2_Handle instance)
{
	(void)instance;
}


/* Real-time safe, no locks or allocations */
static void run(LV2_Handle instance, uint32_t n_samples)
{
	const SlLv2 *sl = (const SlLv2 *)instance;

	/* the core takes max. SL_PLUGIN_MAX_FRAMES per call */
	for (uint32_t pos = 0; pos < n_samples; pos += SL_PLUGIN_MAX_FRAMES) {
		uint32_t n = n_samples - pos;
		if (n > SL_PLUGIN_MAX_FRAMES)
			n = SL_PLUGIN_MAX_FRAMES;

		const float *input0 = sl->input0 + pos;
		const float *input1 = sl->input1 + pos;
		float *output0 = sl->output0 + pos;
		float *output1 = sl->output1 + pos;

#if SL_ONAIR
		effectlive_src(input0, input1, n);

		/* pass through (the output ports must always be written) */
		memmove(output0, input0, n * sizeof(float));
		memmove(output1, input1, n * sizeof(float));
#else
		/* order matters: play -> src (N-1 mix) -> bypass (output) */
		effect_play(sl->sess, output0, output1, n);
		effect_src(sl->sess, input0, input1, n);
		effect_bypass(sl->sess, output0, output1, input0, input1, n);
#endif
	}
}


static void deactivate(LV2_Handle instance)
{
	(void)instance;
}


static void cleanup(LV2_Handle instance)
{
	SlLv2 *sl = (SlLv2 *)instance;

#if !SL_ONAIR
	effect_session_stop(sl->sess);
#endif
	/* stops libre/baresip with the last instance */
	sl_plugin_close();

	free(sl);
}


static const void *extension_data(const char *uri)
{
	(void)uri;
	return NULL;
}


static const LV2_Descriptor descriptor = {
	SL_LV2_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};


LV2_SYMBOL_EXPORT const LV2_Descriptor *lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
