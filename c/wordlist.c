#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include "wordlist.h"
#include "timing.h"
#include "prog_util.h"
#include "win.h"

Wordlist *append_wordlist(Wordlist *wl, const wchar_t *wd)
{
	// overflow bug and unchecked realloc
	size_t count=0, alloc=32000;
	Word *w;
	if (wl) {
		count = wl->len;
		alloc = wl->alloc + 16000;
	}
	wl = Realloc(wl, alloc, sizeof(*w), sizeof *wl);
	wl->len = count + 1;
	wl->alloc = alloc;

	w = wl->words + count;
	wcsncpy(w->s, wd, WORD_MAX-1);
	w->s[WORD_MAX-1] = L'\0';
	w->len = wcslen(w->s);
	return wl;
}

wchar_t *stripw(wchar_t *s)
{
	// remove spaces from start and end
	while(iswspace(*s)) ++s;
	size_t n = wcslen(s);
	if (n) {
		wchar_t *end = s + n - 1;
		while(iswspace(*end)) --end;
		end[1] = L'\0';
	}
	return s;
}

wchar_t *lower(wchar_t *s, size_t len)
{
	for(size_t i=0; i<len && s[i]!=L'\0'; ++i)
		s[i] = towlower(s[i]);
	return s;
}

Wordlist* read_wordlist(Wordlist *wl, const char *fn)
{
	FILE *fp = fopen(fn, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open file %s:\n%s\n", fn, strerror(errno));
		exit(1);
	}
	const int k = 256;
	char buf[k];
	wchar_t wbuf[k];
	while (fgets(buf, sizeof(buf), fp)) {
		mbstowcs(wbuf, buf, k);
		wchar_t *w = stripw(wbuf);
		w = lower(w, k);
		wl = append_wordlist(wl, w);
	}
	fclose(fp);
	return wl;
}

void shuffle_words(Word *array, size_t n)
{
	if (n > 1) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		int usec = tv.tv_usec;
		srand48(usec);
		size_t i;
		for (i = n - 1; i > 0; i--) {
			size_t j = (unsigned int) (drand48()*(i+1));
			Word t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}


void get_words(Wordlist *wl, int count, int (*func)(Word *))
{
	shuffle_words(wl->words, wl->len);
	for(size_t i=0; i<wl->len; ++i) {
		if (!func(wl->words+i)) break;
		if (--count < 1) break;
	}
}

void get_words_s(Wordlist *wl, int count, int (*func)(Word *), wchar_t seq[])
{
	lower(seq, MAX_SEQ);
	shuffle_words(wl->words, wl->len);
	for(size_t i=0; i<wl->len; ++i) {
		if (wcsstr(wl->words[i].s, seq)) {
			if (!func(wl->words+i)) break;
			if (--count < 1) break;
		}
	}
}

