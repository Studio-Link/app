#include <re.h>
#include <baresip.h>
#include <string.h>
#include <math.h>

#include "webapp.h"


#define MAX_METERS 2

static float bias = 1.0f;
static float peaks[MAX_METERS];
static float sent_peaks[MAX_METERS];

static bool run = false;


/* Read and reset the recent peak sample */
static void webapp_read_peaks(void)
{
	memcpy(sent_peaks, peaks, sizeof(peaks));
	memset(peaks, 0, sizeof(peaks));
}


void webapp_ws_meter(const struct websock_hdr *hdr,
				     struct mbuf *mb, void *arg)
{
	struct odict *cmd = NULL;
	int err = 0;

	err = json_decode_odict(&cmd, 32, (const char *)mbuf_buf(mb),
			mbuf_get_left(mb), 8);
	if (err)
		goto out;

out:
	mem_deref(cmd);
}


static void write_ws(void)
{
	int n;
	int i;
	float db;
	char one_peak[100];
	char p[1024];

	p[0] = '\0';
	for (i=0; i<MAX_METERS; i++) {
		db = 20.0f * log10f(sent_peaks[i] * bias);
		snprintf(one_peak, 100, "%f ", db);
		strcat((char*)p, one_peak);
	}
	n = strlen(p);
	p[n-1] = '\0'; /* remove trailing space */

	ws_send_all(WS_METER, p);
	warning("ws_send %d\n", p);
}


void ws_meter_process(unsigned int ch, float *in, unsigned long nframes)
{
	unsigned int i;
	static unsigned int callrun = 0;

	for (i = 0; i < nframes; i=i+2) {
		const float s = fabs(in[i]);
		if (s > peaks[ch]) {
			peaks[ch] = s;
		}
	}

	if (callrun == 10) {
		if (run) {
			webapp_read_peaks();
			write_ws();
		}
		callrun = 0;
	}
	else {
		++callrun;
	}

}


void webapp_ws_meter_init(void)
{
	run = true;
}


void webapp_ws_meter_close(void)
{
	run = false;
}
