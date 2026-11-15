/**
 * @file conf.c Studio Link configuration handling
 *
 * Copyright (C) 2026 studio-link.de
 */
#include <stdio.h>
#include <re.h>
#include <baresip.h>
#include "slconf.h"

#if defined (SLPLUGIN)
#define SL_CONF_DIR      ".studio-link-plugin"
#define SL_MAX_CALLS     10
#define SL_AUDIO_DEVICE  "effect"
#define SL_AUDIO_MODULE  "effect"
#define SL_APP_MODULE    "webapp"
#elif defined (SLIVE)
#define SL_CONF_DIR      ".studio-link-onair"
#define SL_MAX_CALLS     7
#define SL_AUDIO_DEVICE  "effectonair"
#define SL_AUDIO_MODULE  "effectonair"
#define SL_APP_MODULE    "apponair"
#else
#define SL_CONF_DIR      ".studio-link"
#define SL_MAX_CALLS     7
#define SL_AUDIO_DEVICE  "slaudio,default"
#define SL_AUDIO_MODULE  "slaudio"
#define SL_APP_MODULE    "webapp"
#endif

/* Windows/macOS: no console UI (GUI app / DAW plugin). On macOS stdin is
 * /dev/null if started from Finder or inside a DAW, stdio would spin on it */
#if !defined (WIN32) && !defined (DARWIN)
#define SL_UI_MODULE "stdio"
#endif

#ifndef SLVERSION
#define SLVERSION "dev"
#endif

/* not exported by baresip/re anymore */
#define SL_PATH_MAX 512
#ifdef WIN32
#define SL_DIR_SEP "\\"
#else
#define SL_DIR_SEP "/"
#endif


static const char *conf_path_override = NULL;


void sl_conf_path_set(const char *path)
{
	conf_path_override = path;
}


const char *sl_software(void)
{
	return "Studio Link " SLVERSION " (" ARCH "/" OS ")";
}


static int sl_conf_path(char *path, size_t sz)
{
	char home[SL_PATH_MAX];
	int err;

	if (conf_path_override) {
		if (re_snprintf(path, sz, "%s", conf_path_override) < 0)
			return ENOMEM;
		return 0;
	}

	err = fs_gethome(home, sizeof(home));
	if (err)
		return err;

	if (re_snprintf(path, sz, "%s" SL_DIR_SEP SL_CONF_DIR, home) < 0)
		return ENOMEM;

	return 0;
}


static int template_print(struct re_printf *pf, void *arg)
{
	int err;
	(void)arg;

	err = re_hprintf(pf,
		"#\n"
		"# Studio Link configuration\n"
		"#\n"
		"# This file is written by Studio Link and replaced on\n"
		"# version updates (the old file is kept as config.old)\n"
		"#\n"
		"sl_version\t\t" SLVERSION "\n"
		"\n"
		"# Call\n"
		"call_max_calls\t\t%u\n"
		"\n"
		"# Audio\n"
		"audio_player\t\t" SL_AUDIO_DEVICE "\n"
		"audio_source\t\t" SL_AUDIO_DEVICE "\n"
		"audio_alert\t\t" SL_AUDIO_DEVICE "\n"
		"audio_buffer\t\t20-160\t\t# ms\n"
		"# the Studio Link audio modules work with 48kHz stereo "
		"only,\n"
		"# other codec rates/channels are converted by auresamp\n"
		"ausrc_srate\t\t48000\n"
		"auplay_srate\t\t48000\n"
		"ausrc_channels\t\t2\n"
		"auplay_channels\t\t2\n"
		"\n"
		"# AVT - Audio/Video Transport\n"
		"audio_jitter_buffer_type\tadaptive\n"
		"audio_jitter_buffer_ms\t\t20-300\n"
		"audio_jitter_buffer_size\t50\t\t# [packets]\n"
		"rtp_stats\t\tno\n"
		"\n"
		"# Network\n"
		"dns_fallback\t\t8.8.8.8:53\n"
		"\n"
		"# Modules\n"
#ifndef SLIVE
		"module\t\t\tslogging.so\n"
#ifdef SL_UI_MODULE
		"module\t\t\t" SL_UI_MODULE ".so\n"
#endif
#endif
		"module\t\t\topus.so\n"
		"module\t\t\tg722.so\n"
		"module\t\t\tg711.so\n"
		"module\t\t\tauresamp.so\n"
		"module\t\t\t" SL_AUDIO_MODULE ".so\n"
		"module\t\t\tstun.so\n"
		"module\t\t\tturn.so\n"
		"module\t\t\tice.so\n"
		"module\t\t\tdtls_srtp.so\n"
		"module\t\t\tuuid.so\n"
		"\n"
		"module_app\t\t" SL_APP_MODULE ".so\n"
		"\n"
		"# Opus codec parameters\n"
		"opus_bitrate\t\t64000\t# 6000-510000\n"
		"opus_application\taudio\t# {voip,audio}\n"
		"opus_samplerate\t\t48000\n"
		"\n",
		SL_MAX_CALLS);

	return err;
}


static int template_write(const char *file)
{
	FILE *f = NULL;
	int err;

	err = fs_fopen(&f, file, "w");
	if (err) {
		warning("slconf: could not write %s (%m)\n", file, err);
		return err;
	}

	err = re_fprintf(f, "%H", template_print, NULL) < 0 ? EIO : 0;

	fclose(f);

	return err;
}


static bool version_matches(const char *file)
{
	struct conf *conf = NULL;
	char version[64] = "";
	int err;

	err = conf_alloc(&conf, file);
	if (err)
		return false;

	err = conf_get_str(conf, "sl_version", version, sizeof(version));
	mem_deref(conf);
	if (err)
		return false;

	return 0 == str_cmp(version, SLVERSION);
}


int sl_conf_configure(bool reset)
{
	char path[SL_PATH_MAX], file[SL_PATH_MAX], old[SL_PATH_MAX];
	int err;

	err = sl_conf_path(path, sizeof(path));
	if (err)
		return err;

	err = conf_path_set(path);
	if (err)
		return err;

	if (re_snprintf(file, sizeof(file), "%s" SL_DIR_SEP "config",
			path) < 0)
		return ENOMEM;

	if (fs_isfile(file) && (reset || !version_matches(file))) {
		info("slconf: configuration reset (" SLVERSION ")\n");

		if (re_snprintf(old, sizeof(old), "%s.old", file) < 0)
			return ENOMEM;

		(void)remove(old);
		if (rename(file, old) != 0)
			(void)remove(file);
	}

	if (!fs_isfile(file)) {
		(void)fs_mkdir(path, 0700);

		err = template_write(file);
		if (err)
			return err;
	}

	return conf_configure();
}
