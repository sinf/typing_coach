#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistr.h>
#include <unicase.h>
#include <unictype.h>

#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include "wordlist.h"
#include "timing.h"
#include "prog_util.h"
#include "win.h"
#include "sz_mult.h"
#include "kseq.h"

Wordlist *append_wordlist(Wordlist *wl, const Word *w)
{
	size_t alloc, len;

	if (wl) {
		alloc = wl->alloc;
		len = wl->len;
	} else {
		alloc = 0;
		len = 0;
	}

	if (len >= alloc)
		alloc += 16000;

	wl = Realloc(wl, alloc, sizeof(Word), sizeof(Wordlist));
	wl->alloc = alloc;
	wl->words[len] = *w;
	wl->len = len + 1;

	return wl;
}

Word w_strip(Word *in)
{
	Word out;
	int l, r;
	out.len = 0;
	if (in->len > 0) {
		for(l=0; uc_is_space(in->s[l]); ++l) {}
		for(r=in->len-1; uc_is_space(in->s[r]); --r) {}
		out.len = in->len - l - r;
		memcpy(out.s, in->s + l, out.len*4);
	}
	return out;
}

void w_to_lower(Word *w)
{
	size_t n=0;
	uint32_t *tmp = u32_tolower(w->s, w->len, iso639_lang, NULL, NULL, &n);
	memcpy(w->s, tmp, n*4);
	free(tmp);
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

	while (fgets(buf, sizeof(buf), fp)) {

		int buf_bytes = u8_mblen((uint8_t*) buf, k);
		if (buf_bytes > 0) {
			Word w0, w;
			utf8_to_word(buf, buf_bytes, &w0);
			w = w_strip(&w0);
			w_to_lower(&w);
			if (w.len > 0) {
				db_put_word_seqs(buf, buf_bytes);
				append_wordlist(wl, &w);
			}
		}
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


void get_words(int count, int (*func)(Word *))
{
	Word w[count];
	int n = db_get_words_random(w, count);
	for(int i=0; i<n; ++i) {
		if (!func(w+i)) break;
	}
}

int get_words_s(int count, int (*func)(Word *), KSeq *seq)
{
	size_t lo_n=0;
	uint32_t *lo = u32_tolower(seq->s, seq->len, iso639_lang, NULL, NULL, &lo_n);

	size_t seq8_n=0;
	char *seq8 = (char*) u32_to_u8(lo, lo_n, NULL, &seq8_n);

	Word w[count];
	int n = db_get_words(seq8, seq8_n, w, count);

	int added = 0;
	for(int i=0; i<n; ++i) {
		if (!func(w+i)) break;
		added += 1;
	}

	free(seq8);
	return added;
}

void utf8_to_word(const char u8[], int u8_bytes, Word w[1])
{
	size_t l=0;
	uint32_t *tmp = u8_to_u32((const uint8_t*) u8, u8_bytes, NULL, &l);
	if (l > WORD_MAX) l = WORD_MAX;
	memcpy(w->s, tmp, sz_mult(l, sizeof *tmp, 0));
	w->len = l;
	free(tmp);
}

