#include <pthread.h>
#include <FLAC/stream_encoder.h>

extern struct list sessionl;
void slaudio_record_init(void);
void slaudio_record_open_folder(void);

void slaudio_record_set(bool status);
void slaudio_monorecord_set(bool status);
void slaudio_monostream_set(bool status);
void slaudio_mute_set(bool status);
void slaudio_monitor_set(bool status);

int slaudio_record_get_timer(void);
const struct odict* slaudio_get_interfaces(void);
void slaudio_set_driver(int value);
void slaudio_set_input(int value);
void slaudio_set_first_input_channel(int value);
void slaudio_set_output(int value);
int slaudio_reset(void);
