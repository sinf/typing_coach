#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistr.h>
#include <unicase.h>
#include <unictype.h>

#include "wordlist.h"
#include "database.h"
#include "prog_util.h"
#include "sz_mult.h"
#include "kseq.h"

Word w_strip(Word *in)
{
	Word out;
	int l, r;
	out.len = 0;
	if (in->len > 0) {
		for(l=0; is_whitespace(in->s[l]); ++l) {}
		for(r=0; is_whitespace(in->s[in->len-1-r]); ++r) {}
		out.len = in->len - l - r;
		memcpy(out.s, in->s + l, out.len*4);
	}
	return out;
}

void w_to_lower(Word *w)
{
	size_t n=0;
	uint32_t *tmp = u32_tolower(w->s, w->len, iso639_lang, NULL, NULL, &n);
	if (n > WORD_MAX) n = WORD_MAX;
	memcpy(w->s, tmp, n*4);
	free(tmp);
}

void read_wordlist(const char *fn)
{
	FILE *fp = fopen(fn, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open file %s:\n%s\n", fn, strerror(errno));
		exit(1);
	}
	const int k = 256;
	char buf[k];
	unsigned long total_words=0;
	size_t total_seqs=0;
	int tmp=0;
	#ifdef BATCH
	int tmp2=0;
	#endif

	db_trans_begin();
	while (fgets(buf, sizeof(buf), fp)) {

		int buf_bytes = strlen(buf);

		if (buf_bytes > 0) {
			Word w0, w;
			w0 = utf8_to_word(buf, buf_bytes);

			// do stripping and lowercasing in UTF32
			w = w_strip(&w0);
			w_to_lower(&w);

			buf_bytes = word_to_utf8(&w, buf, k);
			if (buf_bytes > 0) {
				if (db_put_word(buf, buf_bytes, &total_seqs)) {
					total_words += 1;
					tmp += 1;
					if (tmp >= 1000) {
						tmp = 0;
						printf("\r%-lu words / %-lu sequences", total_words, (unsigned long) total_seqs);
						fflush(stdout);
					}
					#ifdef BATCH
					tmp2 += 1;
					if (tmp2 >= BATCH) {
						tmp2=0;
						db_trans_end();
						db_trans_begin();
					}
					#endif
				}
			}
		}
	}
	db_trans_end();
	fclose(fp);

	printf("\rNew words added: %lu           \n", total_words);
	printf("New sequences added: %lu\n", (unsigned long) total_seqs);
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

Word utf8_to_word(const char u8[], int u8_bytes)
{
	Word w;
	assert(u8_bytes >= 0);
	size_t l=0;
	uint32_t *tmp = u8_to_u32((const uint8_t*) u8, u8_bytes, NULL, &l);
	if (l > WORD_MAX) l = WORD_MAX;
	memcpy(w.s, tmp, sz_mult(l, sizeof *tmp, 0));
	w.len = l;
	free(tmp);
	return w;
}

int word_to_utf8(Word *w, char buf[], int bufsize)
{
	assert(bufsize >= 0);
	size_t l=0;
	uint8_t *p = u32_to_u8(w->s, w->len, NULL, &l);
	if (l > bufsize-1) l = bufsize-1;
	memcpy(buf, p, l);
	buf[l] = '\0';
	return l;
}

