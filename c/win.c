#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <curses.h>
#include <stdint.h>
#include "prog_util.h"
#include "wordlist.h"
#include "win.h"
#include "microsec.h"
#include "timing.h"

#define C_DEFAULT 1
#define C_TYPED 2
#define C_MISTAKE 3
#define C_INFO 4
#define CURSOR_BIT 8

#define BUFLEN 300

#define MAX_TOP_SEQ 20
static KSeq top_seq[MAX_TOP_SEQ];
static size_t top_seq_n=0;

struct Buf {
	// characters the user needs to type
	wchar_t ch[BUFLEN];

	// Each character is identified as part of some word. Word separators use NULL
	struct Word *wp[BUFLEN];

	// each character has a color C_...
	int color[BUFLEN];

	int pos; //cursor position
	int len; //how many characters in buffer
};

static struct Buf cbuf = {{0},{0},{0},0,0};

#define SNAN L"---"
static wchar_t cpm_str[32]=SNAN, wpm_str[32]=SNAN;
static double cpm=0, wpm=0;

#define CPM_WIN 30
static void calc_cpm(int ms, int correct)
{
	static int buf[CPM_WIN], pos=0, n=0;
	if (ms > 10000) {
		pos=n=0;
		wcscpy(cpm_str, SNAN);
		wcscpy(wpm_str, SNAN);
	} else {
		buf[pos] = ms;

		if (correct) {
			if (++pos > CPM_WIN)
				pos = 0;
			if (++n > CPM_WIN)
				n = CPM_WIN;
		}

		int sum=0;
		for(int i=0; i<n; ++i) {
			int j=(CPM_WIN+pos-i) % CPM_WIN;
			sum += buf[j];
		}

		cpm = (double) sum / n;
		wpm = cpm * 0.2;
		swprintf(cpm_str, 32, L"%.0f", cpm);
		swprintf(wpm_str, 32, L"%.0f", wpm);
	}
}

void buf_clear()
{
	cbuf.pos = 0;
	cbuf.len = 0;
	for(int i=0; i<BUFLEN; ++i) {
		cbuf.ch[i] = L' ';
		cbuf.wp[i] = NULL;
		cbuf.color[i] = C_DEFAULT;
	}
}

void buf_write(int len, const uint32_t *s, Word *w)
{
	int i0 = cbuf.len;
	int i1 = i0 + len;
	if (i1 > BUFLEN) return;
	cbuf.len = i1;
	for(int i=0; i<len; ++i) {
		int j = i0 + i;
		cbuf.ch[j] = s[i];
		cbuf.color[j] = C_DEFAULT;
		cbuf.wp[j] = w;
	}
}

int add_word(struct Word *w)
{
	int l0 = cbuf.len;
	if (cbuf.len > 0) {
		uint32_t space[] = {' '};
		buf_write(1, space, NULL);
	}
	buf_write(w->len, w->s, w);
	return l0 != cbuf.len;
}

void cu_setup()
{
	need_endwin = 1;
	initscr();
	nonl();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	if (has_colors()) {
		curs_set(0);
		start_color();
		init_pair(C_DEFAULT, COLOR_GREEN, COLOR_BLACK);
		init_pair(C_TYPED, COLOR_BLACK, COLOR_GREEN);
		init_pair(C_MISTAKE, COLOR_BLACK, COLOR_RED);
		init_pair(C_INFO, COLOR_YELLOW, COLOR_BLACK);

		init_pair(C_DEFAULT|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_TYPED|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_MISTAKE|CURSOR_BIT, COLOR_BLACK, COLOR_MAGENTA);
		init_pair(C_INFO|CURSOR_BIT, COLOR_YELLOW, COLOR_BLACK);
	}
}

void addstr_color(const wchar_t s[], int colors[], int len, int curs) {
	// curs: relative cursor position
	int c = -1;
	for(int i=0; i<len; ++i) {
		if (colors[i] != c) {
			c = colors[i];
			attron(COLOR_PAIR(c));
		}
		if (curs == i) attron(COLOR_PAIR(c|CURSOR_BIT));
		addch(s[i]);
		if (curs == i) attron(COLOR_PAIR(c));
	}
}

int word_wrap_scan(const wchar_t s[], int s_len, int line_w) {
	int stop = s_len;
	for(int x=0; x<line_w && x<s_len; ++x) {
		if (iswspace(s[x]))
			stop = x;
		if (x > line_w)
			break;
	}
	return stop;
}

void my_repaint()
{
	int y=4;
	int i=0;

	attron(COLOR_PAIR(C_INFO));
	mvprintw(0, 0, "Database: %s", database_path);
	mvprintw(1, 0, "Wordlist: %s", wordlist_path);
	mvprintw(2, 0, "cpm: %8ls | wpm: %8ls | keystrokes : %08ld", cpm_str, wpm_str, the_typing_counter);

	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	if (cols > 80) cols = 80;

	while(y < rows && i < cbuf.len) {
		int n = word_wrap_scan(cbuf.ch+i, cbuf.len+1-i, cols);
		move(y, 0);
		clrtoeol();
		addstr_color(cbuf.ch+i, cbuf.color+i, n, cbuf.pos-i);
		i += n;
		y += 1;
	}

	if (top_seq_n) {
		mvprintw(++y, 0, "%8s  %8s  %8s  %7s  %s", "Sequence", "Cost", "Sd", "Samples", "Samples (raw)");
		for(size_t i=0; i<top_seq_n && y < rows; ++i) {
			KSeq *s = top_seq + i;
			move(++y, 0);
			clrtoeol();
			printw("%8.*ls  %8.2f  %8.2f  %7d  %d",
				s->len, s->s,
				s->cost, s->cost_var,
				s->samples, s->samples_raw);
		}
	} else {
		mvprintw(y+1, 0, "Randow words");
	}

	refresh();
	doupdate();
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
		add_word(next_words+i);
	next_count = 0;
}

void get_more_words()
{
	buf_clear();
	db_trans_end();

	top_seq_n = 0;
	KSeq *sq;
	size_t i,n=db_get_sequences(20000,1,MAX_SEQ,&sq);
	if (n<5) {
		get_words(MAX_WORDS, add_word);
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

	while(iswspace(cbuf.ch[cbuf.len-1]))
		cbuf.len -= 1;

	free(sq);
	erase();
	db_trans_begin();
}

int check_input()
{
	static uint64_t last_ts = 0;
	int delay_ms = 0;
	int c = getch();
	uint64_t ts = get_microsec();
	int expected = cbuf.ch[cbuf.pos];

	if (last_ts != 0) {
		delay_ms = (ts - last_ts) / 1000UL;
	}

	if (c == expected) {
		// typed ok
		if (cbuf.color[cbuf.pos] != C_MISTAKE)
			cbuf.color[cbuf.pos] = C_TYPED;
		cbuf.pos += 1;
		if (cbuf.pos >= cbuf.len) {
			get_more_words();
			// ignore the first character of a screen since the user spends a few seconds reading
			last_ts = 0;
		}
		last_ts = ts;
	} else {
		// typed wrong
		cbuf.color[cbuf.pos] = C_MISTAKE;
	}

	db_put(c, expected, delay_ms);

	if (delay_ms != 0)
		calc_cpm(delay_ms, c==expected);

	return 1;
}

