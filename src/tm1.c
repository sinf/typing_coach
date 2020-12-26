#include <stdlib.h>
#include "dpy.h"
#include "kseq.h"
#include "spambox.h"
#include "wordlist.h"
#include "database.h"
#include "tm.h"

#define MAX_WORDS 30
#define MAX_TOP_SEQ 20
static KSeq top_seq[MAX_TOP_SEQ];
static size_t top_seq_n=0;

static int next_count=0;
static Word next_words[MAX_WORDS];
static int add_word_(Word *w) {
	if (next_count < MAX_WORDS) {
		next_words[next_count++] = *w;
		return 1;
	}
	return 0;
}
static void flush_next() {
	shuffle_words(next_words, next_count);
	for(int i=0; i<next_count; ++i)
		sb_add_word(next_words+i);
	next_count = 0;
}

void tm1_words(void)
{
	top_seq_n = 0;
	KSeq *sq;
	size_t i,n=db_get_sequences(20000,1,MAX_SEQ,&sq);

	if (n<5) {
		get_words(MAX_WORDS, sb_add_word);
	} else {
		// try all sequences starting from most expensive
		for(i=0; i<n; i++) {
			if (
			get_words_s(5, add_word_, sq+i)
			&& top_seq_n < MAX_TOP_SEQ) {
				top_seq[top_seq_n++] = sq[i];
			}
			if (next_count >= MAX_WORDS*2/3)
				break;
		}
		get_words(MAX_WORDS, add_word_);
		flush_next();
	}
	free(sq);
}

int tm1_info(int y)
{
	const int c1 = C_NORMAL;

	if (top_seq_n) {
		dpy_print(++y, c1, "%8s  %8s  %8s  %7s  %s", "Sequence", "Cost", "Sd", "Samples", "Samples (raw)");
		for(size_t i=0; i<top_seq_n; ++i) {
			KSeq *s = top_seq + i;
			dpy_print(++y, c1,
				"%8.*ls  %8.2f  %8.2f  %7d  %d",
				s->len, s->s,
				s->cost, s->cost_var,
				s->samples, s->samples_raw);
		}
	} else {
		dpy_print(y+1, c1, "Random words");
	}
	return y;
}

