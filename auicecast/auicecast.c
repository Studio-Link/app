/**
 * @file auice.c
 *
 * Copyright (C) 2016 IT-Service Sebastian Reimers
 */
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1
#include <pthread.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <shout/shout.h>
#include <lame/lame.h>


/**
 * @defgroup auice auice
 *
 * Audio module for ICECAST
 */

struct auplay_st {
	const struct auplay *ap;  /* pointer to base-class (inheritance) */
	pthread_t thread;
	bool run;
	void *write;
	int16_t *sampv;
	size_t sampc;
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
	char *device;
};

static struct auplay *auplay;


static void destructor(void *arg)
{
	struct auplay_st *st = arg;

	if (st->run) {
		st->run = false;
		pthread_join(st->thread, NULL);
	}
	mem_deref(st->sampv);
}


static void *play_thread(void *arg)
{
	struct auplay_st *st = arg;
	shout_t *shout;
	int write;
	int num_frames;
	const int MP3_SIZE = 4096;
	long ret;
	unsigned char mp3_buffer[MP3_SIZE];

	num_frames = st->prm.srate * st->prm.ptime / 1000;

	shout_init();

	if (!(shout = shout_new())) {
		printf("Could not allocate shout_t\n");
		return NULL;
	}

	if (shout_set_host(shout, "127.0.0.1") != SHOUTERR_SUCCESS) {
		printf("Error setting hostname: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
		printf("Error setting protocol: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_port(shout, 8000) != SHOUTERR_SUCCESS) {
		printf("Error setting port: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_password(shout, "hackme") != SHOUTERR_SUCCESS) {
		printf("Error setting password: %s\n", shout_get_error(shout));
		return NULL;
	}
	if (shout_set_mount(shout, "/example.mp3") != SHOUTERR_SUCCESS) {
		printf("Error setting mount: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_user(shout, "source") != SHOUTERR_SUCCESS) {
		printf("Error setting user: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_format(shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
		printf("Error setting format: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_name(shout, "testcast") != SHOUTERR_SUCCESS) {
		printf("Error setting name: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_set_description(shout, "description") != SHOUTERR_SUCCESS) {
		printf("Error setting genre: %s\n", shout_get_error(shout));
		return NULL;
	}

	if (shout_open(shout) != SHOUTERR_SUCCESS) {
		return NULL;
	}

	lame_t lame = lame_init();
	lame_set_in_samplerate(lame, 48000);
	lame_set_num_channels(lame, 2);
	lame_set_brate(lame, 128);
	lame_set_mode(lame, 1);
	lame_set_quality(lame, 2);
	lame_set_VBR(lame, vbr_default);
	lame_init_params(lame);

	warning("start stream\n");
	while (st->run) {
		st->wh(st->sampv, st->sampc, st->arg);

		write = lame_encode_buffer_interleaved(lame, st->sampv, num_frames, mp3_buffer, MP3_SIZE);

		ret = shout_send(shout, mp3_buffer, write);
		if (ret != SHOUTERR_SUCCESS) {
			printf("DEBUG: Send error: %s\n", shout_get_error(shout));
			break;
		}
		shout_sync(shout);
	}

	warning("stop stream\n");
	lame_close(lame);
        shout_close(shout);
        shout_shutdown();

	return NULL;
}


static int alloc_handler(struct auplay_st **stp, const struct auplay *ap,
		struct auplay_prm *prm, const char *device,
		auplay_write_h *wh, void *arg)
{
	struct auplay_st *st;
	int err;

	if (!stp || !ap || !prm || !wh)
		return EINVAL;

	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;

	st->ap   = ap;
	st->prm  = *prm;
	st->wh   = wh;
	st->arg  = arg;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st->sampv = mem_alloc(st->sampc * sizeof(int16_t), NULL);
	if (!st->sampv) {
		err = ENOMEM;
		goto out;
	}

	st->run = true;
	err = pthread_create(&st->thread, NULL, play_thread, st);
	if (err) {
		st->run = false;
		goto out;
	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}


static int module_init(void)
{
	return auplay_register(&auplay, "auice", alloc_handler);
}


static int module_close(void)
{
	auplay = mem_deref(auplay);

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(auicecast) = {
	"auicecast",
	"auplay",
	module_init,
	module_close
};
