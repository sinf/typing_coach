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

static uint64_t last_key_press_time = 0;
static int last_key_code = 0;
static int last_key_status = 0;

static int draw_status_bar()
{
	const int c0 = C_STATUS;
	dpy_print(0, c0, "Database: %s", database_path);
	dpy_print(1, c0, "%s", fmt_session_time());
	dpy_print(2, c0, "cpm: %8s | wpm: %8s | keystrokes : %08ld | key: %lc %d %d", cpm_str, wpm_str, the_typing_counter, (wchar_t) last_key_code, last_key_code, last_key_status);
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

	db_trans_end();

	sb_submit_sequences();
	sb_clear();

	tm_words();

	db_trans_begin();
}

static void tm_process_input(int c)
{
	int delay_ms = 0;

	uint64_t ts = get_microsec();
	int expected = sb_expected();

	if (last_key_press_time != 0) {
		delay_ms = (ts - last_key_press_time) / 1000UL;
	}

	sb_putc(c, delay_ms);

	if (opt_auto_space)
		sb_skip_spaces();

	if (sb_end_reached()) {
		get_more_words();
		// ignore the first character since the user spends a second reading the new words
		last_key_press_time = 0;
	} else {
		last_key_press_time = ts;
	}

	if (delay_ms != 0) {
		db_put(c, expected, delay_ms);
		calc_cpm(delay_ms, c==expected);
	}
}

void training_session(void)
{
	if (sb_end_reached())
		get_more_words();
	
	last_key_press_time = 0;
	timeout(1000);

	for(;;)
	{
		tm_repaint();

		wint_t c;
		int ret = get_wch(&c);
		last_key_code = c;
		last_key_status = ret;

		if (ret == ERR)
			continue;
	
		if (c == KEY_BACKSPACE || c == 127)
			break;

		tm_process_input(c);
	}

	notimeout(stdscr, 1);
	db_trans_end();
	sb_submit_sequences();
	sb_clear();
}

