#ifndef _STATS_H
#define _STATS_H
#include <wchar.h>

#define MAX_SEQ 4
struct KSeq {
	wchar_t s[MAX_SEQ+1];
	int len;
	int samples, samples_raw;
	double cost, cost_var, weight;
};
typedef struct KSeq KSeq;

void 

#endif

