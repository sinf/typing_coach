#pragma once
#ifndef _WORDLIST_H
#define _WORDLIST_H
#include <stddef.h>
#include <stdint.h>
#include "kseq.h"

#define WORD_MAX 63

struct Word {
	uint32_t s[WORD_MAX];
	int len;
};

typedef struct Word Word;
typedef struct Word Word32;

Word w_strip(Word *in);
void w_lowercase(Word *w);

void read_wordlist(const char *path);

void shuffle_words(Word *array, size_t n);

void get_words(int count, int (*func)(Word *w));
// return # of words containing sequence (seq) processed
int get_words_s(int count, int (*func)(Word *w), KSeq *seq);

Word utf8_to_word(const char u8[], int u8_bytes);
int word_to_utf8(Word *w, char buf[], int bufsize);

#endif


