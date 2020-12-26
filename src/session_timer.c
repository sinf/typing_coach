#include <stdio.h>
#include "microsec.h"
#include "session_timer.h"

static char buf[256]="00:00:00";

const char *fmt_session_time()
{
	static uint64_t t_start=0, prev_dt;
	int s=0, m=0, h=0;
	uint64_t dt, tmp;

	if (t_start==0) {
		t_start = get_microsec();
		prev_dt = 0;
		dt = 0;
	} else {
		dt = get_microsec() - t_start;
		if (dt - prev_dt > 1000000) {
			prev_dt = dt;
			tmp = dt / 1000000;
			s = tmp % 60;
			tmp /= 60;
			m = tmp % 60;
			tmp /= 60;
			h = tmp;

			int n = snprintf(buf, sizeof buf - 1, "%02d:%02d:%02d", h, m, s);
			if (n >= 0) buf[n] = '\0';
		}
	}

	return buf;
}

