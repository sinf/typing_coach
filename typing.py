#!/usr/bin/python3
import pandas as pd
import numpy as np
import sys
import traceback
import curses as cu
import sqlite3
import textwrap
from time import monotonic, time
import argparse

ATT=lambda:None
the_sqc=None
the_wordlist=None

def get_timestamp():
	return int(time())

def series_to_text(word_series, max_ch):
	out=""
	x=0
	for index, word in word_series.items():
		s=" " + word
		x += len(s)
		if x > max_ch:
			break
		out += s
	if len(out)==0:
		raise Exception('empty word series')
	return out[1:] + "."

class Wordlist:
	def __init__(self):
		self.df = pd.DataFrame()
	def append_plain(self, path):
		df = pd.read_table(path,
			skipinitialspace = True,
			sep = '\0',
			names = ['word'],
			dtype = str,
			engine = 'c',
			#memory_map = True
			encoding = 'utf-8')
		df.word = df.word.str.strip()
		self.df = self.df.append(df)
	def get(self, max_chars=80):
		s=series_to_text(self.df['word'].sample(n=max_chars/2), max_chars)
		assert(len(s) <= max_chars)
		return s

class ScrollingWindow:
	def __init__(self, win):
		self.win = win
		self.text = ""
		self.attr = []
		self.rows = []
		self.top_row = 0 # first visible row
	def n_rows(self):
		return self.win.getmaxyx()[0] - 2
	def n_cols(self):
		return self.win.getmaxyx()[1] - 2
	def set_content(self, text, a=cu.A_NORMAL):
		self.text = text
		self.rows = textwrap.wrap(text, self.n_cols(), drop_whitespace=False)
		self.attr = [a] * len(text)
	def find_row(self, target_pos):
		row_nr=0
		row_end=0
		for r in self.rows:
			row_end += len(r)
			if target_pos < row_end:
				return row_nr
			row_nr += 1
		return 0
	def center_at(self, target_pos):
		r = self.find_row(target_pos) - self.n_rows()//2
		r = min(r, len(self.rows) - self.n_rows())
		r = max(r, 0)
		self.top_row = r
	def paint(self):
		pos = sum(len(r) for r in self.rows[:self.top_row])
		y=0
		self.win.clear()
		for row in self.rows[self.top_row:]:
			y += 1
			if y > self.n_rows():
				break
			for j in range(len(row)):
				self.win.addch(y, 1+j, row[j], self.attr[pos])
				pos += 1
		self.win.border()
		self.win.noutrefresh()

class InputBox:
	def __init__(self):
		self.sql_data= []
		self.t_begin = None    # key press time is calculated from this
		self.delay_buffer = [] # used for typing speed calculation
		self.delay_buffer_limit = 100
		self.last_key = None   # show key name for testing
		self.halt_on_mistake = False
		self.max_chars = 300
	def win_init(self):
		height, width = 3, 50
		y, x = 2, 5
		self.swin = ScrollingWindow(cu.newwin(height+2, width+2, y, x))
	def set_text(self, text):
		self.text = text
		self.caret = 0
		with open('textwall.txt', 'wb') as f:
			f.write(self.text.encode('utf8'))
		self.swin.set_content(self.text, a=ATT.untyped)
		self.swin.attr[0] = ATT.cursor
	def poll_input(self):
		try:
			key = self.swin.win.getch(0,0)
		except cu.error:
			return
		self.last_key = key
		ch = chr(key) if key >= 0 and key <= 255 else None
		if key is None:
			self.next_words()
			return
		ch_exp = self.text[self.caret]
		key_exp = ord(ch_exp)
		mistake = ch != ch_exp
		if mistake:
			self.swin.attr[self.caret] = ATT.cursor_mistake
		if self.halt_on_mistake == False or not mistake:
			self.swin.attr[self.caret] = ATT.typed if self.swin.attr[self.caret] is ATT.cursor else ATT.mistake
			self.caret += 1
			if self.caret >= len(self.text):
				self.next_words()
			self.swin.attr[self.caret] = ATT.cursor
		now = monotonic()
		if self.t_begin is not None:
			# start recording from the 2nd character
			delay = now - self.t_begin
			delay_ms = int(delay * 1000)
			self.sql_data += [(key, key_exp, delay_ms, get_timestamp())]
			if not mistake:
				# let correct keystrokes contribute to typing speed
				self.delay_buffer.insert(0, delay)
				while len(self.delay_buffer) > self.delay_buffer_limit:
					self.delay_buffer.pop()
		self.t_begin = now
	def cpm(self):
		if len(self.delay_buffer) == 0 or self.t_begin is None:
			return 0
		samples = self.delay_buffer + [monotonic() - self.t_begin]
		t = sum(samples)
		n = len(samples)
		return n/t*60
	def save_data(self):
		if len(self.sql_data) > 0 and the_sqc is not None:
			the_sqc.executemany( \
				'insert into keystrokes (pressed,expected,delay_ms,timestamp) values (?,?,?,?)',
					self.sql_data)
			the_sqc.commit()
			self.sql_data = []
	def next_words(self):
		self.save_data()
		self.caret = 0
		self.data_buffer = []
		self.set_text(the_wordlist.get(self.max_chars))
	def paint(self):
		self.swin.center_at(self.caret)
		self.swin.paint()

def main(stdscr):

	p = argparse.ArgumentParser()
	p.add_argument("-c", "--cont", help="continue on mistakes", action="store_true")
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database (or "none")', default=['keystrokes.db'])
	p.add_argument("-p", "--page-size", nargs=1, help='characters per page', type=int, default=[300])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file', default=['words/ascii_english'])
	args = p.parse_args()

	global the_wordlist
	the_wordlist = Wordlist()
	for path in args.wordlist:
		the_wordlist.append_plain(path)

	if args.db[0] != "none":
		global the_sqc
		the_sqc = sqlite3.connect(args.db[0])

	input_box = InputBox()

	try:
		cu.cbreak()
		cu.curs_set(0)
		cu.start_color()
		cu.init_pair(1, cu.COLOR_GREEN, cu.COLOR_BLACK)
		cu.init_pair(2, cu.COLOR_BLACK, cu.COLOR_GREEN)
		cu.init_pair(3, cu.COLOR_BLACK, cu.COLOR_RED)
		cu.init_pair(4, cu.COLOR_BLACK, cu.COLOR_WHITE)
		cu.init_pair(5, cu.COLOR_BLACK, cu.COLOR_MAGENTA)
		cu.init_pair(6, cu.COLOR_BLACK, cu.COLOR_YELLOW)
		setattr(ATT, 'untyped', cu.color_pair(1))
		setattr(ATT, 'typed', cu.color_pair(2))
		setattr(ATT, 'mistake', cu.color_pair(3))
		setattr(ATT, 'cursor', cu.color_pair(4))
		setattr(ATT, 'cursor_mistake', cu.color_pair(5))
		setattr(ATT, 'marker', cu.color_pair(6))
		input_box.halt_on_mistake = not args.cont
		input_box.max_chars = args.page_size[0]
		input_box.win_init()
		input_box.next_words()
		#input_box.win.nodelay(True)

		redraw_delay = 0.2
		redraw_t = monotonic() + redraw_delay

		while True:

			if input_box.swin.win.nodelay is True:
				should_redraw = monotonic() >= redraw_t
				redraw_t += redraw_delay if should_redraw else 0
			else:
				should_redraw = True

			if should_redraw:
				cpm = input_box.cpm()
				wpm = cpm / 1700 * 378
				stdscr.addstr(0, 0, "cpm: %5.0f    wpm : %4.0f" % (cpm,wpm))
				stdscr.addstr(1, 0, "key: %-30s" % str(input_box.last_key))
				stdscr.noutrefresh()
				input_box.paint()
				cu.doupdate()

			input_box.poll_input()

	except KeyboardInterrupt:
		print("kKeyboardInterrupt")
	finally:
		cu.echo()
		cu.nocbreak()
		cu.endwin()
		cu.curs_set(1)
		if input_box is not None:
			input_box.save_data()
		if the_sqc is not None:
			the_sqc.execute('PRAGMA optimize')
			the_sqc.close()
			print("Data saved")
		print("Done")

if __name__=="__main__":
	cu.wrapper(main)

