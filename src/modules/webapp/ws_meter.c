#include <re.h>
#include <baresip.h>
#include <string.h>
#include <math.h>

#include "webapp.h"


#define MAX_METERS 32

enum { METER_L = 0, METER_R = 1 };

static float bias = 1.0f;
/* per meter slot: left and right channel peak */
static float peaks[MAX_METERS][2] = {{0}};
static float sent_peaks[MAX_METERS][2] = {{0}};

static struct tmr tmr;


/* Read and reset the recent peak sample */
static void webapp_read_peaks(void)
{
	memcpy(sent_peaks, peaks, sizeof(peaks));
	memset(peaks, 0, sizeof(peaks));
}


void webapp_ws_meter(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	(void)hdr;
	(void)arg;
	struct odict *cmd = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, DICT_BSIZE, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), MAX_LEVELS);
	if (err)
		goto out;

out:
	mem_deref(cmd);
}


#ifndef SLPLUGIN
int slaudio_record_get_timer(void);
#endif
static void write_ws(void)
{
	int n;
	int i;
	float db;
	char one_peak[100];
	char p[4096];

	p[0] = '\0';
	/* Record time */
#ifndef SLPLUGIN
	int hours;
	int min;
	int sec;
	int record_time;

	record_time = slaudio_record_get_timer();
	hours = record_time / 1000 / 3600;
	min = (record_time / 1000 / 60) - (hours * 60);
	sec = (record_time / 1000) - (hours * 3600) - (min * 60);

	re_snprintf(one_peak, 100, "%d:%02d:%02d 0 ", hours, min, sec);
#else
	re_snprintf(one_peak, 100, "0 0 ");
#endif
	strcat((char*)p, one_peak);

	/* "<time> 0 <m0 L> <m0 R> <m1 L> <m1 R> ..." */
	for (i=0; i<MAX_METERS; i++) {
		for (int c = METER_L; c <= METER_R; c++) {
			db = 20.0f * log10f(sent_peaks[i][c] * bias);
			re_snprintf(one_peak, 100, "%.1f ", db);
			strcat((char*)p, one_peak);
		}
	}
	n = strlen(p);
	p[n-1] = '\0'; /* remove trailing space */

	ws_send_all(WS_METER, p);
}


static void tmr_handler(void *arg)
{
	(void)arg;
	tmr_start(&tmr, 150, tmr_handler, NULL);
	webapp_read_peaks();
	write_ws();
}


static inline void peak_update(float *peak, float sample)
{
	const float s = fabsf(sample);

	if (s > *peak)
		*peak = s;
}


/* Interleaved stereo (L R L R ...), nframes = number of frames */
void ws_meter_process(unsigned int ch, const float *in, unsigned long nframes)
{
	if (!in || ch >= MAX_METERS)
		return;

	for (unsigned long i = 0; i < nframes; i++) {
		peak_update(&peaks[ch][METER_L], in[i * 2]);
		peak_update(&peaks[ch][METER_R], in[i * 2 + 1]);
	}
}


/* Planar stereo (separate left/right buffers, e.g. plugin host buffers) */
void ws_meter_process_planar(unsigned int ch, const float *l, const float *r,
			     unsigned long nframes)
{
	if (!l || ch >= MAX_METERS)
		return;

	if (!r)
		r = l;

	for (unsigned long i = 0; i < nframes; i++) {
		peak_update(&peaks[ch][METER_L], l[i]);
		peak_update(&peaks[ch][METER_R], r[i]);
	}
}


void webapp_ws_meter_init(void)
{
	tmr_init(&tmr);
	tmr_start(&tmr, 150, tmr_handler, NULL);
}


void webapp_ws_meter_close(void)
{
	tmr_cancel(&tmr);
}
