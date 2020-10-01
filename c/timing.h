#pragma once
#ifndef _TIMING_H
#define _TIMING_H
#include <wchar.h>

extern char *database_path;
extern long the_typing_counter;

#define MAX_SEQ 4
struct KSeq {
	wchar_t s[MAX_SEQ+1];
	int len;
	int samples, samples_raw;
	double cost, cost_var, weight;
};
typedef struct KSeq KSeq;

void db_open();
void db_close();
void db_put(wchar_t pressed, wchar_t expected, int delay_ms);

void db_trans_begin();
void db_trans_end();

size_t db_get_sequences(
int ch_limit,
int sl_min,
int sl_max,
KSeq *out[1]
);

typedef int (*CmpFunc)(const void*,const void*);
int cmp_seq_cost(const KSeq *a, const KSeq *b);

double calc_cost(int len, int delay[], int mist[]);
double calc_weight(int len, int age[]);

size_t remove_duplicate_sequences(KSeq *s, size_t count);
size_t remove_neg_cost(KSeq *s, size_t count);

#endif


