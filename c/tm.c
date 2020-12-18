#include <stdint.h>
#include <curses.h>

#include "wordlist.h"
#include "kseq.h"
#include "dpy.h"
#include "spambox.h"
#include "cpm_counter.h"
#include "session_timer.h"
#include "microsec.h"
#include "database.h"
#include "menu.h"
#include "tm.h"

int opt_auto_space = 0;
void (*tm_words)(void) = tm1_words;
int (*tm_info)(int) = tm1_info;

static int draw_status_bar()
{
	const int c0 = C_STATUS;
	dpy_print(0, c0, "Database: %s", database_path);
	dpy_print(1, c0, "%s", fmt_session_time());
	dpy_print(2, c0, "cpm: %8s | wpm: %8s | keystrokes : %08ld", cpm_str, wpm_str, the_typing_counter);
	dpy_print(3, c0, "Access menu with backspace");
	dpy_print(4, c0, "-------");
	return 5;
}

static void tm_repaint()
{
	int y;
	y = draw_status_bar();
	y = sb_paint(y);
	y = tm_info(y);
}

static void get_more_words()
{
	loading_screen();
	// potentially freeze for a moment when submitting keystrokes, querying keystrokes, and querying more training words

	sb_clear();
	db_trans_end();

	tm_words();

	db_trans_begin();
}

static int tm_process_input()
{
	static uint64_t last_ts = 0;
	int delay_ms = 0;
	int c = read_key();

	if (c == KEY_BACKSPACE) {
		last_ts = 0;
		return 1;
	}

	uint64_t ts = get_microsec();
	int expected = sb_expected();

	if (last_ts != 0) {
		delay_ms = (ts - last_ts) / 1000UL;
	}

	sb_putc(c);

	if (opt_auto_space)
		sb_skip_spaces();

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
	
	return 0;
}

void training_session(void)
{
	if (sb_end_reached())
		get_more_words();
	
	do {
		tm_repaint();
	} while(!tm_process_input());
}

