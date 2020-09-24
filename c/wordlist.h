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

char *strip(char *s);
wchar_t *stripw(wchar_t *s);
Wordlist *append_wordlist(Wordlist *, const wchar_t *word);
Wordlist *read_wordlist(Wordlist *, const char *path);

void get_words(Wordlist *, int count, int (*func)(Word *w));

#endif


