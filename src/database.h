#pragma once
#ifndef _TIMING_H
#define _TIMING_H
#include <stdint.h>
#include "kseq.h"
#include "wordlist.h"

extern const char* database_path;
extern long the_typing_counter;

void db_open();
void db_close();
void db_put(KeyCode pressed, KeyCode expected, int delay_ms);

void db_trans_begin();
void db_trans_end();

/* get ch_limit most recent key presses, then compile a list of sequences out of them */
size_t db_get_sequences(
int ch_limit,
int sl_min,
int sl_max,
KSeq *out[1]
);

// look up history for each supplied sequence
void db_get_sequences_hist(size_t count, const KSeq seqs[], KSeqHist hist_out[]);

typedef int (*CmpFunc)(const void*,const void*);
int cmp_seq_cost(const KSeq *a, const KSeq *b);

double calc_cost(int len, int delay[], int mist[]);
double calc_weight(int len, int age[]);

size_t remove_duplicate_sequences(KSeq *s, size_t count);
size_t remove_neg_cost(KSeq *s, size_t count);

/*
put the word into database
also break the word into all sequences and put into database
increment num_seqs[0] accordingly
return 0 on failure */
int db_put_word(const char word[], int word_bytes);

/* Fetches words that contain a sequence (substring)
seq, seq_bytes: UTF-8 string
words[limit]: output buffer for returned words
returns count of words written to output buffer
*/
int db_get_words(const char seq[], int seq_bytes, Word32 words[], int limit);

/* Fetches words at random, return count of words written to buffer */
int db_get_words_random(Word32 word_buffer[], int limit);

void db_defrag();

void db_put_seq_samples(
	size_t num_ch,
	const uint32_t ch[SPAMBOX_BUFLEN],
	const int16_t delay_ms[SPAMBOX_BUFLEN] );

long db_total_word_count();

#endif


