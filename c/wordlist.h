#pragma once
#ifndef _WORDLIST_H
#define _WORDLIST_H
#include <stddef.h>
#include <wchar.h>
#include <stdint.h>
#include "kseq.h"

#define WORD_MAX 63

struct Word {
	uint32_t s[WORD_MAX];
	int len;
};

struct Wordlist {
	size_t len;
	size_t alloc;
	struct Word words[];
};

typedef struct Word Word;
typedef struct Word Word32;
typedef struct Wordlist Wordlist;

Word w_strip(Word *in);
void w_lowercase(Word *w);

Wordlist *append_wordlist(Wordlist *wl, const Word *w);
Wordlist *read_wordlist(Wordlist *, const char *path);

void shuffle_words(Word *array, size_t n);

void get_words(int count, int (*func)(Word *w));
// return # of words containing sequence (seq) processed
int get_words_s(int count, int (*func)(Word *w), KSeq *seq);

void utf8_to_word(const char u8[], int u8_bytes, Word w[1]);

#endif


