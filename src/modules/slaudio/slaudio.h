#include <pthread.h>
#include "FLAC/stream_encoder.h"
#include <rtaudio_c.h>

struct session {
	struct le le;
	struct ausrc_st *st_src;
	struct auplay_st *st_play;
	bool local;
	bool stream;
	bool run_src;
	bool run_play;
	bool run_record;
	int32_t *dstmix;
	struct aubuf *aubuf;
	pthread_t record_thread;
	FLAC__StreamEncoder *flac;
	int16_t *sampv;
	FLAC__int32 *pcm;
	int8_t ch;
	float *vumeter;
	struct call *call;
	int8_t track;
	bool talk;
	int16_t bufsz;
	int16_t jb_max;
};

extern struct list sessionl;
void slaudio_record_init(void);
void slaudio_record_set(bool active);
int slaudio_record_get_timer(void);
void slaudio_mono_set(bool active);
void slaudio_mute_set(bool active);
const struct odict* slaudio_get_interfaces(void);
void slaudio_set_driver(int value);
void slaudio_set_input(int value);
void slaudio_set_first_input_channel(int value);
void slaudio_set_output(int value);
int slaudio_callback_in(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata);
int slaudio_callback_out(void *out, void *in, unsigned int nframes,
		double stream_time, rtaudio_stream_status_t status,
		void *userdata);

/* extern */
void ws_meter_process(unsigned int ch, float *in, unsigned long nframes);
