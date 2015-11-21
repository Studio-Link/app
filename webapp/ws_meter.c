#include <re.h>
#include <baresip.h>
#include <string.h>
#include <jack/jack.h>
#include <math.h>

#include "webapp.h"


#define MAX_METERS 2

static float bias = 1.0f;
static float peaks[MAX_METERS];
static float sent_peaks[MAX_METERS];
static jack_port_t *input_ports[MAX_METERS];
static jack_client_t *client = NULL;


/* Read and reset the recent peak sample */
static void webapp_read_peaks_jackmeter(void)
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
}


/* Callback called by JACK when audio is available.
 *  *    Stores value of peak sample */
static int process_peak(jack_nframes_t nframes, void *arg)
{
	unsigned int i, port;
	static unsigned int callrun = 0;

	for (port = 0; port < MAX_METERS; port++) {
		jack_default_audio_sample_t *in;

		/* just incase the port isn't registered yet */
		if (input_ports[port] == 0) {
			break;
		}

		in = (jack_default_audio_sample_t *) jack_port_get_buffer(input_ports[port], nframes);

		for (i = 0; i < nframes; i=i+2) {
			const float s = fabs(in[i]);
			if (s > peaks[port]) {
				peaks[port] = s;
			}
		}
	}

	if (callrun == 10) {
		webapp_read_peaks_jackmeter();
		write_ws();
		callrun = 0;
	} else {
		callrun++;
	}

	return 0;
}


int webapp_ws_meter_init(void)
{
	jack_status_t status;
	if ((client = jack_client_open ("wsmeter", JackNoStartServer, &status)) == 0) {
		warning ("JACK server not running [jackwsmeter]?\n");
		return 1;
	}

	if (client == NULL) {
		warning ("jack_client_open() failed, "
				"status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			warning ("Unable to connect to JACK server\n");
		}
	}

	jack_set_process_callback(client, process_peak, 0);

	if (jack_activate(client)) {
		warning("cannot activate jackwsmeter");
		return 1;
	}

	if(!(input_ports[0] = jack_port_register(client, "mic",
					JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0))) {
		warning ("Can not connect jack port\n");
		return 1;
	}

	if(!(input_ports[1] = jack_port_register(client, "head",
					JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0))) {
		warning ("Can not connect jack port\n");
		return 1;
	}

	warning("jackmeter ready\n");
	return 0;
}

void webapp_ws_meter_close(void)
{
	if (client != NULL) {
		jack_client_close(client);
	}
}
