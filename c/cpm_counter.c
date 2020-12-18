#include <stdio.h>
#include <string.h>
#include "cpm_counter.h"

// CPM_WIN: how many last characters contribute to CPM calculation
#define CPM_WIN 30

#define SNAN "---"

char cpm_str[CPM_STR_MAX]=SNAN, wpm_str[CPM_STR_MAX]=SNAN;
static double cpm=0, wpm=0;

void calc_cpm(int ms, int correct)
{
	static int buf[CPM_WIN], pos=0, n=0;
	if (ms > 10000) {
		pos=n=0;
		strcpy(cpm_str, SNAN);
		strcpy(wpm_str, SNAN);
	} else {
		buf[pos] = ms;

		if (correct) {
			if (++pos > CPM_WIN)
				pos = 0;
			if (++n > CPM_WIN)
				n = CPM_WIN;
		}

		int sum=0;
		for(int i=0; i<n; ++i) {
			int j=(CPM_WIN+pos-i) % CPM_WIN;
			sum += buf[j];
		}

		cpm = (double) sum / n;
		wpm = cpm * 0.2;
		snprintf(cpm_str, CPM_STR_MAX, "%.0f", cpm);
		snprintf(wpm_str, CPM_STR_MAX, "%.0f", wpm);
	}
}


