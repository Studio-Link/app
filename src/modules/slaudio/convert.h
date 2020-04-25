#include <math.h>

#define SAMPLE_32BIT_SCALING 2147483647.0f
#define SAMPLE_32BIT_MAX  2147483647
#define SAMPLE_32BIT_MIN  -2147483647

#define SAMPLE_24BIT_SCALING 8388607.0f
#define SAMPLE_24BIT_MAX 8388607
#define SAMPLE_24BIT_MIN -8388607

#define SAMPLE_16BIT_SCALING 32767.0f
#define SAMPLE_16BIT_MAX 32767
#define SAMPLE_16BIT_MIN -32767

#define NORMALIZED_FLOAT_MIN -1.0f
#define NORMALIZED_FLOAT_MAX 1.0f

#define f_round(f) lrintf (f)

#define float_32(s, d)\
	if ((s) <= NORMALIZED_FLOAT_MIN) {\
		(d) = SAMPLE_32BIT_MIN;\
	}\
	else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_32BIT_MAX;\
	}\
	else {\
		(d) = f_round ((s) * SAMPLE_32BIT_SCALING);\
	}

#define float_24(s, d)\
	if ((s) <= NORMALIZED_FLOAT_MIN) {\
		(d) = SAMPLE_24BIT_MIN;\
	}\
	else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_24BIT_MAX;\
	}\
	else {\
		(d) = f_round ((s) * SAMPLE_24BIT_SCALING);\
	}

#define float_16(s, d)\
	if ((s) <= NORMALIZED_FLOAT_MIN) {\
		(d) = SAMPLE_16BIT_MIN;\
	}\
	else if ((s) >= NORMALIZED_FLOAT_MAX) {\
		(d) = SAMPLE_16BIT_MAX;\
	}\
	else {\
		(d) = f_round ((s) * SAMPLE_16BIT_SCALING);\
	}
