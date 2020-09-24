#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include "wordlist.h"
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
	wl = realloc(wl, sizeof(*wl) + alloc * sizeof(*w));
	wl->len = count + 1;
	wl->alloc = alloc;

	w = wl->words + count;
	wcsncpy(w->s, wd, WORD_MAX-1);
	w->s[WORD_MAX-1] = L'\0';
	w->len = wcslen(w->s);
	return wl;
}

char *strip(char *s)
{
	// remove spaces from start and end
	while(isspace(*s)) ++s;
	size_t n = strlen(s);
	if (n) {
		char *end = s + n - 1;
		while(isspace(*end)) --end;
		end[1] = '\0';
	}
	return s;
}

wchar_t *stripw(wchar_t *s)
{
	// remove spaces from start and end
	while(iswspace(*s)) ++s;
	size_t n = wcslen(s);
	if (n) {
		wchar_t *end = s + n - 1;
		while(iswspace(*end)) --end;
		end[1] = '\0';
	}
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
		wl = append_wordlist(wl, stripw(wbuf));
	}
	fclose(fp);
	return wl;
}

static void shuffle(Word *array, size_t n)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int usec = tv.tv_usec;
	srand48(usec);

	if (n > 1) {
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
	shuffle(wl->words, wl->len);
	for(int i=0; i<count && func(wl->words+i); ++i) {}
}

