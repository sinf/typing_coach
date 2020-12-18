#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "prog_util.h"
#include "wordlist.h"
#include "win.h"
#include "microsec.h"
#include "database.h"
#include "dpy.h"
#include "spambox.h"
#include "cpm_counter.h"
#include "session_timer.h"

#define MAX_TOP_SEQ 20
static KSeq top_seq[MAX_TOP_SEQ];
static size_t top_seq_n=0;

void my_repaint()
{
	const int c0 = C_STATUS;
	const int c1 = C_NORMAL;

	dpy_print(0, c0, "Database: %s", database_path);
	dpy_print(1, c0, "%s", fmt_session_time());
	dpy_print(2, c0, "cpm: %8s | wpm: %8s | keystrokes : %08ld", cpm_str, wpm_str, the_typing_counter);

	int y = sb_paint(4);

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
}

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

void loading_screen()
{
	dpy_begin();
	dpy_print(0, C_NORMAL, "Loading...");
	dpy_refresh();
}

void get_more_words()
{
	loading_screen();
	// potentially freeze for a moment when submitting keystrokes, querying keystrokes, and querying more training words

	sb_clear();
	db_trans_end();

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
	db_trans_begin();
}

int check_input()
{
	static uint64_t last_ts = 0;
	int delay_ms = 0;
	int c = read_key();
	uint64_t ts = get_microsec();
	int expected = sb_expected();

	if (last_ts != 0) {
		delay_ms = (ts - last_ts) / 1000UL;
	}

	sb_putc(c);

	if (sb_end_reached()) {
		get_more_words();
		// ignore the first character since the user spends a second reading the new words
		last_ts = 0;
	} else {
		last_ts = ts;
	}

	db_put(c, expected, delay_ms);

	if (delay_ms != 0)
		calc_cpm(delay_ms, c==expected);

	return 1;
}

