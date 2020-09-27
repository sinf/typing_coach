#pragma once
#ifndef _WORDLIST_H
#define _WORDLIST_H
#include <stddef.h>
#include <wchar.h>

#define WORD_MAX 63

struct Word {
	wchar_t s[WORD_MAX];
	int len;
};

struct Wordlist {
	size_t len;
	size_t alloc;
	struct Word words[];
};

typedef struct Word Word;
typedef struct Wordlist Wordlist;

wchar_t *stripw(wchar_t *s);
wchar_t *lower(wchar_t *s, size_t len);

Wordlist *append_wordlist(Wordlist *, const wchar_t *word);
Wordlist *read_wordlist(Wordlist *, const char *path);

void get_words(Wordlist *, int count, int (*func)(Word *w));
void get_words_s(Wordlist *, int count, int (*func)(Word *w), wchar_t seq[]);

void shuffle_words(Word *array, size_t n);

#endif


