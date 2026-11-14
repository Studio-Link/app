/**
 * @file slplugin.h Studio Link DAW plugin interface
 *
 * C interface between the DAW plugin wrappers (JUCE AU/VST3, LV2 ...) and
 * the Studio Link core. Can be included from C and C++.
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef SLPLUGIN_H
#define SLPLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/** Max. frames per effect_* call, wrappers must split larger blocks */
	SL_PLUGIN_MAX_FRAMES = 4096,

	/** The only sample rate Studio Link processes */
	SL_PLUGIN_SRATE      = 48000,
};


/*
 * Lifecycle (slcore)
 *
 * Every plugin instance calls sl_plugin_init() on creation and
 * sl_plugin_close() on destruction. The first init starts libre/baresip in
 * its own thread, the last close stops it again. Not real-time safe, call
 * them from the host message thread only.
 */
int  sl_plugin_init(void);
void sl_plugin_close(void);

/*
 * Report the host sample rate, call it after a successful sl_plugin_init()
 * and whenever the host (re)activates the plugin with a new rate. Logs a
 * warning if it is not SL_PLUGIN_SRATE and emits the baresip module event
 * "slplugin,samplerate,<Hz>" for the user interface. Not real-time safe,
 * call it from the host message thread only.
 */
void sl_plugin_samplerate(double srate);


/*
 * Studio Link plugin (effect module, SLPLUGIN)
 *
 * effect_play(), effect_src() and effect_bypass() are called from the audio
 * thread in this order for each block of max. SL_PLUGIN_MAX_FRAMES frames.
 * The audio format is 48 kHz stereo.
 */
struct session;

struct session *effect_session_start(void);
int  effect_session_stop(struct session *session);
void effect_play(struct session *sess, float *const output0,
		 float *const output1, unsigned long nframes);
void effect_src(struct session *sess, const float *const input0,
		const float *const input1, unsigned long nframes);
void effect_bypass(struct session *sess, float *const output0,
		   float *const output1, const float *const input0,
		   const float *const input1, unsigned long nframes);


/*
 * Studio Link OnAir plugin (effectonair module, SLIVE)
 */
void effectlive_src(const float *const input0, const float *const input1,
		    unsigned long nframes);

#ifdef __cplusplus
}
#endif

#endif /* SLPLUGIN_H */
