#pragma once
#ifndef _TIMING_H
#define _TIMING_H
#include <stdint.h>
#include "kseq.h"
#include "wordlist.h"

extern long the_typing_counter;

void db_open();
void db_close();
void db_put(KeyCode pressed, KeyCode expected, int delay_ms);

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

/* put one sequence:word pair into database
for later fetching a list of words that contain that sequence */
void db_put_word_seq(const char seq[], int seq_bytes, const char word[], int word_bytes);

/* break a word into all sequences and call db_put_word_seq() for each */
void db_put_word_seqs(const char word[], int word_bytes);

/* Fetches words that contain a sequence (substring)
seq, seq_bytes: UTF-8 string
words[limit]: output buffer for returned words
returns count of words written to output buffer
*/
int db_get_words(const char seq[], int seq_bytes, Word32 words[], int limit);

/* Fetches words at random, return count of words written to buffer */
int db_get_words_random(Word32 word_buffer[], int limit);

#endif


