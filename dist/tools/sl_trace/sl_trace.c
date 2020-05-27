#define _POSIX_C_SOURCE 199309L

#include <re.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdint.h>

#include "sl_trace.h"

static uint64_t profile_start, profile_diff;
static uint64_t profile_max = 0, profile_sum = 0, profile_min = 0;
static int counter = 0;
static struct tmr tmr;
static int trace_arg;


static uint64_t microseconds(void)
{
	uint64_t usecs;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	usecs = (long)now.tv_sec * (uint64_t)1000000;
	usecs += (long)now.tv_nsec/1000;

	return usecs;
}


static void sl_trace_reset(void)
{
	profile_sum = 0;
	profile_max = 0;
	profile_min = 0;
	counter = 0;
}


void sl_trace_start(void)
{
	profile_start = microseconds();
}


void sl_trace_end(int arg)
{
	uint64_t jfs;

	jfs = microseconds();
	++counter;

	profile_diff = jfs - profile_start;

	if (profile_diff > profile_max) {
		profile_max = profile_diff;
		trace_arg = arg;
	}

	if (profile_diff < profile_min || !profile_min) {
		profile_min = profile_diff;
	}

	profile_sum += profile_diff;
}


void sl_trace_debug(void)
{
	double profile_avg = 0.0;
	if (counter)
		profile_avg = profile_sum/counter;

	printf("SL_TRACE_MAX: (min)%4ld (max)%4ld (avg)%5.2f (arg)%d (count)%3d\n", 
			profile_min, profile_max, profile_avg, trace_arg, counter);
	sl_trace_reset();
}


void sl_trace_debug_tmr(void *arg)
{
	sl_trace_debug();
	tmr_init(&tmr);
	tmr_start(&tmr, 250, sl_trace_debug_tmr, NULL);
}
