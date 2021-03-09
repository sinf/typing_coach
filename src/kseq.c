#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistr.h>
#include <unicase.h>
#include "kseq.h"
#include "prog_util.h"

int kseq_cmp(const KSeq *a, const KSeq *b)
{
	int res;
	if (u32_casecmp(a->s, a->len, b->s, b->len, iso639_lang, NULL, &res))
		return 0;
	return res;
}

int kseq_hist_validate(const KSeqHist *hist)
{
	if (hist->start_pos >= KSEQ_HIST) return 0;
	if (hist->samples > KSEQ_HIST) return 0;
	return 1;
}

void kseq_hist_push(KSeqHist *hist, int16_t delay)
{
	int i = hist->start_pos;

	if (delay < 0 && hist->delay_ms[i] < 0) {
		// avoid inserting more than one sequential typo
		// to prevent accidentally clearing the short buffer with a typo burst
		return;
	}

	if (hist->samples < KSEQ_HIST) {
		hist->samples += 1;
	} else {
		i = i==0 ? KSEQ_HIST-1 : i-1;
	}

	hist->start_pos = i;
	hist->delay_ms[i] = delay;
}

KSeqStats kseq_hist_stats(KSeqHist *h)
{
	double delay_d[KSEQ_HIST], var=0;
	uint64_t typos=0, delays=0;
	KSeqStats s={0};
	unsigned i, n = h->samples, delay_n=0;

	for(i=0; i<n; ++i) {
		unsigned j = (h->start_pos + i) % KSEQ_HIST;
		unsigned d = h->delay_ms[j];
		delay_d[i] = d;
		if (d <= 0) {
			typos += 1;
		} else {
			delays += d;
			delay_n += 1;
		}
	}

	s.delay_mean = 280;
	s.delay_stdev = 100;

	if (delay_n) {
		s.delay_mean = delays / (double) delay_n;
		if (delay_n > 1) {
			for(i=0; i<n; ++i) {
				if (delay_d[i] > 0) {
					double d = delay_d[i] - s.delay_mean;
					var += d*d;
				}
			}
			var /= (delay_n-1);
			s.delay_stdev = sqrt(var);
		}
	}

	s.typo_mean = typos / (double) n;
	s.cost_func = delays*0.01 + typos*10.0;
	return s;
}

