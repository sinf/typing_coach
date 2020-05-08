#!/usr/bin/python3
import pandas as pd
import numpy as np
from numpy.random import choice
import curses as cu
import sqlite3
import textwrap
from time import monotonic, time
import argparse
import locale

ATT=lambda:None
the_sqc=None
the_wordlist=None

keys_lh = 'qwertasdfgzxcvb<>!@#$%^\t 123456`~'
keys_rh = 'yuiop[]hjkl;\'\\bnm,./7890-^&*()_= ~\r'

def get_timestamp():
	return int(time())

def series_to_text(word_series, max_ch):
	out=""
	x=0
	sep=tuple(' .,-:;/\\+=*()[]{}@%#&^!$<>')
	prb=tuple(1/(x*x*1.4 + 1) for x in range(len(sep)))
	prb_total=sum(prb)
	prb=tuple(x/prb_total for x in prb)
	for index, word in word_series.items():
		s = choice(sep, 1, p=prb)[0]
		w = s + str(word).strip()
		x += len(w)
		if x > max_ch:
			break
		out += w
	if len(out)==0:
		raise Exception('empty word series')
	return out[1:] + "."

class Wordlist:
	def __init__(self):
		self.df = pd.DataFrame()
	def append_lines(self, path):
		df = pd.read_table(path,
			skipinitialspace = True,
			sep = '\0',
			names = ['word'],
			dtype = str,
			engine = 'c',
			memory_map = True,
			encoding = 'utf-8')
		df.word = df.word.str.strip()
		self.df = self.df.append(df)
	def append_file(self, path):
		with open(path,"r") as f:
			self.df = self.df.append({'word':f.read()}, ignore_index=True)
	def get(self, max_chars=300):
		if self.df['word'].size == 1:
			return self.df['word'][0]
		s=series_to_text(self.df['word'].sample(n=max_chars/3, replace=True), max_chars)
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
		text = text \
			.replace('\n', '↲') \
			.replace('\t', '→') \
			.replace('\v', '↓')
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
				c = row[j]
				self.win.addch(y, 1+j, c, self.attr[pos])
				pos += 1
		self.win.border()
		self.win.noutrefresh()

class InputBox:
	def __init__(self):
		self.sql_data= []
		self.t_begin = None    # key press time is calculated from this
		self.delay_buffer = [] # used for typing speed calculation
		self.delay_buffer_limit = 80
		self.last_key = None   # show key name for testing
	def win_init(self):
		height, width = 3, 50
		y, x = 2, 5
		self.swin = ScrollingWindow(cu.newwin(height+2, width+2, y, x))
	def set_text(self, text):
		self.text = text
		self.caret = 0
		self.swin.set_content(self.text, a=ATT.untyped)
		self.swin.attr[0] = ATT.cursor
	def process_input(self, key):
		self.last_key = key
		t1 = monotonic()
		t0 = self.t_begin
		delay = np.nan if (t0 is None) else (t1 - t0)
		delay_ms = np.nan if (t0 is None) else int(delay*1000)
		ch = chr(key) if key >= 0 and key <= 255 else None
		if key in (cu.KEY_BREAK, cu.KEY_HOME, cu.KEY_END):
			self.t_begin = None
			return
		if key in (cu.KEY_NPAGE, cu.KEY_PPAGE):
			self.next_words()
			self.t_begin = None
			return
		ch_exp = self.text[self.caret]
		key_exp = ord(ch_exp)
		mistake = ch != ch_exp
		if mistake:
			self.swin.attr[self.caret] = ATT.cursor_mistake
			cu.beep()
		if not mistake:
			self.swin.attr[self.caret] = ATT.typed if self.swin.attr[self.caret] is ATT.cursor else ATT.mistake
			self.caret += 1
			if self.caret >= len(self.text):
				self.next_words()
			if len(self.swin.text)>0:
				self.swin.attr[self.caret] = ATT.cursor
		self.sql_data += [(key, key_exp, delay_ms, get_timestamp())]
		if not mistake:
			# update delay buffer for realtime wpm feedback
			if t0 is not None:
				self.delay_buffer.insert(0, delay)
				while len(self.delay_buffer) > self.delay_buffer_limit:
					self.delay_buffer.pop()
			if not mistake:
				self.t_begin = t1
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
				'INSERT INTO keystrokes (pressed,expected,delay_ms,timestamp) VALUES (?,?,?,?)',
					self.sql_data)
			the_sqc.commit()
			self.sql_data = []
	def next_words(self):
		self.save_data()
		self.caret = 0
		self.data_buffer = []
		self.set_text(the_wordlist.get())
	def paint(self):
		self.swin.center_at(self.caret)
		self.swin.paint()
	

def practice(stdscr, args):

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
				ks = 'PAUSE' if input_box.t_begin is None else str(input_box.last_key)
				stdscr.addstr(1, 0, "%-30s" % ks)
				stdscr.noutrefresh()
				input_box.paint()
				cu.doupdate()

			try:
				key = stdscr.getch()
				input_box.process_input(key)
			except cu.error:
				return

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
			tail = the_sqc.execute('SELECT * FROM keystrokes ORDER BY sequence DESC LIMIT 3').fetchall()
			the_sqc.close()
			print("Data saved")
			print("Last 3 rows:")
			for r in tail:
				r_ = tuple(0 if x is None else x for x in r)
				print("sequence=%d pressed=%3d expected=%3d delay_ms=%4d timestamp=%d" % r_)
		print("Done")

def main():
	locale.setlocale(locale.LC_ALL, '')

	p = argparse.ArgumentParser()
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database (or "none")', default=['keystrokes.db'])
	p.add_argument("-p", "--page-size", nargs=1, help='characters per page', type=int, default=[300])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file(s)', default=[])
	p.add_argument("-s", "--source", nargs='+', help='utf8 source file(s)', default=[])
	args = p.parse_args()

	if len(args.wordlist)==0 and len(args.source)==0:
		args.wordlist = ['words/ascii_english']

	if args.db[0] != "none":
		global the_sqc
		the_sqc = sqlite3.connect(args.db[0])
		the_sqc.execute('CREATE TABLE IF NOT EXISTS keystrokes' +
			' ( sequence integer primary key,' +
			' pressed integer,' + # keycode
			' expected integer,' + # keycode
			' delay_ms integer,' +
			' timestamp integer );') # unix timestamp (seconds)
		the_sqc.commit()

	global the_wordlist
	the_wordlist = Wordlist()
	for path in args.wordlist:
		the_wordlist.append_lines(path)
	for path in args.source:
		the_wordlist.append_file(path)
	
	cu.wrapper(lambda stdscr: practice(stdscr, args))

if __name__=="__main__":
	main()

