#ifndef _STATS_H
#define _STATS_H
#include <wchar.h>
#include <stdint.h>

typedef uint32_t KeyCode;

#define MAX_SEQ 4
struct KSeq {
	KeyCode s[MAX_SEQ];
	int len;
	int samples, samples_raw;
	double cost, cost_var, weight;
};
typedef struct KSeq KSeq;
typedef struct WStr WStr;

int kseq_to_wchar(KSeq *s, wchar_t buf[], int buflen);

// need #include <unictype.h>
#define k_is_space uc_is_c_whitespace

int kseq_equal(const KSeq *a, const KSeq *b);

#define is_whitespace uc_is_property_white_space

#endif

