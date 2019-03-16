#include <pthread.h>
#include "FLAC/stream_encoder.h"

struct session {
	struct le le;
	struct ausrc_st *st_src;
	struct auplay_st *st_play;
	bool run_src;
	bool run_play;
	bool run_record;
	int32_t *dstmix;
	struct aubuf *aubuf;
	pthread_t record_thread;
	FLAC__StreamEncoder *flac;
	int16_t *sampv;
	FLAC__int32 *pcm;
};

extern struct list sessionl;
void slrtaudio_record_init(void); 

void slrtaudio_record_set(bool active);
