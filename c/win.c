#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <curses.h>
#include "prog_util.h"
#include "wordlist.h"
#include "win.h"

#define C_DEFAULT 1
#define C_TYPED 2
#define C_MISTAKE 3
#define CURSOR_BIT 8

#define BUFLEN 300

struct Buf {
	// characters the user needs to type
	wchar_t ch[BUFLEN];
	// each character is identified as part of some word
	// word separators use NULL
	struct Word *wp[BUFLEN];
	// each character has a color
	int color[BUFLEN];
	int pos; //cursor position
	int len; //how many characters in buffer
};

static struct Buf cbuf = {{0},{0},{0},0,0};

void buf_clear()
{
	cbuf.pos = 0;
	cbuf.len = 0;
	for(int i=0; i<BUFLEN; ++i) cbuf.color[i] = C_DEFAULT;
}

void buf_write(int len, const wchar_t *s, Word *w)
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
	if (cbuf.len > 0)
		buf_write(1, L" ", NULL);
	buf_write(w->len, w->s, w);
	return l0 != cbuf.len;
}

void cu_setup()
{
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

		init_pair(C_DEFAULT|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_TYPED|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_MISTAKE|CURSOR_BIT, COLOR_BLACK, COLOR_MAGENTA);
		//init_pair(6, COLOR_BLACK, COLOR_YELLOW);
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
	int y=3;
	int i=0;

	mvprintw(0, 0, "Database: %s | Wordlist: %s", database_path, wordlist_path);
	mvprintw(1, 0, "cpm: %5.0f  wpm : %4.0f  keystrokes : %08d", 0.0, 0.0, 0);

	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	if (cols > 80) cols = 80;

	while(y < rows && i < cbuf.len) {
		int n = word_wrap_scan(cbuf.ch+i, cbuf.len+1-i, cols);
		move(y, 0);
		addstr_color(cbuf.ch+i, cbuf.color+i, n, cbuf.pos-i);
		i += n;
		y += 1;
	}
	refresh();
	doupdate();
}

void get_more_words()
{
	buf_clear();
	get_words(the_wordlist, 30, add_word);
}

int check_input()
{
	int c = getch();

	if (c == cbuf.ch[cbuf.pos]) {
		// typed ok
		if (cbuf.color[cbuf.pos] != C_MISTAKE)
			cbuf.color[cbuf.pos] = C_TYPED;
		if (cbuf.pos < cbuf.len) {
			cbuf.pos += 1;
		} else {
			get_more_words();
		}
	} else {
		// typed wrong
		cbuf.color[cbuf.pos] = C_MISTAKE;
	}

	return 1;
}

