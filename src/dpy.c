#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <curses.h>
#include <unictype.h>
#include <unistr.h>
#include "kseq.h"
#include "dpy.h"

int need_endwin = 0;
static int dpy_rows=-1, dpy_cols=-1;

void dpy_begin(void)
{
	erase();
	clrtobot();
	getmaxyx(stdscr, dpy_rows, dpy_cols);
	if (dpy_cols > 80) dpy_cols = 80;
}

static int dpy_line_start(int y)
{
	if (y<0 || y>=dpy_rows) return 0;
	move(y, 0);
	clrtoeol();
	return 1;
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
		init_pair(C_NORMAL, COLOR_WHITE, COLOR_BLACK);
		init_pair(C_UNTYPED, COLOR_GREEN, COLOR_BLACK);
		init_pair(C_TYPED, COLOR_BLACK, COLOR_GREEN);
		init_pair(C_MISTAKE, COLOR_BLACK, COLOR_RED);
		init_pair(C_STATUS, COLOR_YELLOW, COLOR_BLACK);

		init_pair(C_NORMAL|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_UNTYPED|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_TYPED|CURSOR_BIT, COLOR_BLACK, COLOR_WHITE);
		init_pair(C_MISTAKE|CURSOR_BIT, COLOR_BLACK, COLOR_MAGENTA);
		init_pair(C_STATUS|CURSOR_BIT, COLOR_YELLOW, COLOR_BLACK);
	}
}

void dpy_write_color(int y, const uint32_t s[], const int colors[], int len, int curs) {
	if (!dpy_line_start(y)) return;
	// curs: relative cursor position
	int c = -1;
	for(int i=0; i<len; ++i) {
		if (colors[i] != c) {
			c = colors[i];
			attron(COLOR_PAIR(c));
		}
		if (curs == i) attron(COLOR_PAIR(c|CURSOR_BIT));
		size_t l=0;
		char *s8 = (char*) u32_to_u8(s+i, 1, NULL, &l);
		addnstr(s8, l);
		free(s8);
		if (curs == i) attron(COLOR_PAIR(c));
	}
	// note: whatever color was last used is still on
}

// returns how many characters can be printed before needing line break
static int find_line_end(const uint32_t s[], int s_len, int line_w) {
	int stop = s_len;
	for(int x=0; x<line_w && x<s_len; ++x) {
		if (is_whitespace(s[x]))
			stop = x;
		if (x > line_w)
			break;
	}
	return stop;
}

int dpy_write_color_multiline(int y, const uint32_t ch[], const int color[], int len, int cursor)
{
	int i=0;
	while(y < dpy_rows && i < len) {
		int n = find_line_end(ch+i, len+1-i, dpy_cols);
		dpy_write_color(y, ch+i, color+i, n, cursor-i);
		i += n;
		y += 1;
	}

	return y;
}

void dpy_print(int y, int color, const char *s, ...)
{
	if (!dpy_line_start(y)) return;
	va_list ap;
	va_start(ap, s);
	attron(COLOR_PAIR(color));
	vw_printw(stdscr, s, ap);
	va_end(ap);
}

void dpy_refresh(void)
{
	refresh();
	doupdate();
}

int read_key(void)
{
	wint_t c=0;
	while (get_wch(&c) == ERR) ;
	return c;
}

