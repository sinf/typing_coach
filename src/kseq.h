#ifndef _STATS_H
#define _STATS_H
#include <stdint.h>

typedef uint32_t KeyCode;

#define MAX_SEQ 4

// how many characters the user can type in one screen
#define SPAMBOX_BUFLEN 300

struct KSeq {
	KeyCode s[MAX_SEQ];
	int len;
	int samples, samples_raw;
	double cost, cost_var, weight;
};
typedef struct KSeq KSeq;
typedef struct WStr WStr;

// need #include <unictype.h>
#define k_is_space uc_is_c_whitespace

int kseq_cmp(const KSeq *a, const KSeq *b);
#define kseq_equal(a,b) (!kseq_cmp(a,b))

#define is_whitespace uc_is_property_white_space

/* KSEQ_HIST:
How much history to remember for each key sequence
should be enough to analyze the user's current typing ability,
but not too much: old results must be forgotten as user improves.
Also prefer to keep this small because of database size reasons
*/
#define KSEQ_HIST 32

#define ERR_QW ((KSEQ_HIST+63)/64)

typedef struct KSeqHist {
	uint16_t start_pos;
	uint16_t samples;
	// ring buffer of delay. <0 means typing mistake
	int16_t delay_ms[KSEQ_HIST];
} KSeqHist;

typedef struct KSeqStats {
	// mean delay for one keystroke in this sequence
	// so mean delay of sequences of different length are comparable
	double delay_mean;
	double delay_stdev;
	// typos/keystroke, so 1.00 means all typos, 0.00 means no typos
	double typo_mean;
	// delay*0.01 + typos*10
	double cost_func;
} KSeqStats;

void kseq_hist_push(KSeqHist *h, int16_t delay);
KSeqStats kseq_hist_stats(KSeqHist *h);

/*
we have a string of length n, and wish to know how many
substrings of length L we can extract, where 1 <= L <=k

n = number of characters in the string
k = maximum substring length
1 <= k <= n
z = total substrings

(n-0) substrings of length 1
(n-1) substrings of length 2
(n-2) substrings of length 3
(n-3) substrings of length 4
...
(n-(k-1)) substrings of length k

z = n + (n-1) + (n-2) + ... + (n-(k-1))
z = k*n - (1 + 2 + 3 + ... + (k-1))
z = k*n - (k-1)*(k-1+1)/2
z = k*n - k*(k-1)/2
*/
#define SUBSTR_COUNT(n,k) ((n)*(k) - (k)*((k)-1)/2)

// used for checking if blob from sqlite is usable
int kseq_hist_validate(const KSeqHist *hist);

#endif

