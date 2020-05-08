#!/usr/bin/python3
import pandas as pd
import numpy as np
import sys
import re
import traceback
import curses as cu
import sqlite3
import textwrap
from time import monotonic, time
import argparse

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
	for index, word in word_series.items():
		s=" " + str(word).strip()
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
		s=series_to_text(self.df['word'].sample(n=max_chars/3, replace=True), max_chars)
		assert(len(s) <= max_chars)
		return s

class ChSeqGen:
	def __init__(self):
		pass
	def get(self, max_chars=80):
		an = Analysis(1000)
		df=pd.DataFrame()
		for n in (4,5):
			an.gen_seq(n)
			df=df.append(an.mseq[n])
		w=df.typo + df.delay/400.0
		words=df.sample(n=max_chars, weights=w, replace=True)
		words=words.index.to_series()
		s=series_to_text(words, max_chars*2)
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
			cu.beep()
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
		else:
			delay = np.nan
			delay_ms = np.nan
		self.sql_data += [(key, key_exp, delay_ms, get_timestamp())]
		if self.t_begin is not None and not mistake:
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
				'INSERT INTO keystrokes (pressed,expected,delay_ms,timestamp) VALUES (?,?,?,?)',
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
	

def practice(stdscr, args):

	global the_wordlist

	if args.gen:
		the_wordlist = ChSeqGen()
	else:
		the_wordlist = Wordlist()
		for path in args.wordlist:
			the_wordlist.append_plain(path)

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
			tail = the_sqc.execute('SELECT * FROM keystrokes ORDER BY sequence DESC LIMIT 3').fetchall()
			the_sqc.close()
			print("Data saved")
			print("Last 3 rows:")
			for r in tail:
				r_ = tuple(0 if x is None else x for x in r)
				print("sequence=%d pressed=%3d expected=%3d delay_ms=%4d timestamp=%d" % r_)
		print("Done")

class Analysis:
	def __init__(self, n=100000):
		df = pd.read_sql_query('SELECT * FROM keystrokes LIMIT %d' % n, the_sqc)
		df['delay'] = df['delay_ms']
		df['typo'] = df['pressed'] != df['expected']
		df['ch'] = df['expected'].apply(lambda x: chr(x))
		df = df.drop(['delay_ms'], axis=1)
		df = df.sort_values(by='sequence')
		self.df = df

		# ignore afk inputs
		df2 = df[df['delay'] < 5000]

		g = df2.groupby(by='ch')
		std = g.std()
		mean = g.mean()
		mean = mean.drop(['pressed', 'expected', 'timestamp', 'sequence'], axis=1)
		mean['delay_std'] = std['delay']
		mean['typo_std'] = std['typo']
		mean['samples'] = g.count()['typo']

		# drop undersampled keys
		mean = mean[mean['samples'] >= 5]

		self.mean = mean
		self.by_error = mean.sort_values(by='typo', ascending=False)
		self.by_delay = mean.sort_values(by='delay', ascending=False)

		self.seq = {}
		self.mseq = {}
		self.mseq_by_error = {}
		self.mseq_by_delay = {}

	def gen_seq(self, n=2):
		rows = self.df.shape[0]
		seq = pd.DataFrame()
		seq['str'] = [''] * rows
		seq['delay'] = [0.0] * rows
		seq['typo'] = [False] * rows
		seq['ok'] = [True] * rows

		for i in range(n):
			sh = self.df.shift(i)
			seq['str'] = sh['ch'].str.cat(seq['str'])
			seq['typo'] |= sh['typo']
			seq['delay'] += sh['delay']

			# key presses separated by more than 30s aren't part of same session
			# and thus sequences containing such breaks are invalid
			seq['ok'] &= abs(self.df['timestamp'] - sh['timestamp']) < 30

			# repeats>=3 are just annoying

		# drop invalid sequences
		seq = seq[seq['ok']].drop(['ok'], axis=1)
		seq = seq.dropna()
		seq = seq[seq['delay'] < 10000]
		seq = seq[seq['str'].str.contains(' ') ^ True]

		g = seq.groupby(by='str')
		count, std = g.count(), g.std()
		mseq = g.sum()
		mseq['delay'] /= count['delay']
		mseq['typo'] /= count['typo']
		mseq['delay_std'] = std['delay']
		mseq['typo_std'] = std['typo']
		mseq['samples'] = count['delay']

		# filter out undersampled sequences
		#mseq = mseq[mseq['samples'] >= 5]

		self.mseq[n] = mseq
		self.mseq_by_delay[n] = mseq.sort_values(by='delay', ascending=False)
		self.mseq_by_error[n] = mseq.sort_values(by='typo', ascending=False)
	
	def print_misc(self):
		print("\nKeys most often typo:")
		print(self.by_error.head(5))
		print("\nKeys that are slow:")
		print(self.by_delay.head(5))
	
	def print_seq(self,i):
		print("\nSequences (%d) most often typo:" % i)
		print(self.mseq_by_error[i].head(5))
		print("\nSequences (%d) that are slow:" % i)
		print(self.mseq_by_delay[i].head(5))

def analyze(args):
	print("Pandas:", pd.__version__)
	print("Analyzing...")
	seqs = [2, 3]
	a = Analysis(100000)
	for n in seqs:
		a.gen_seq(n)
	a.print_misc()
	for n in seqs:
		a.print_seq(n)

def main():

	p = argparse.ArgumentParser()
	p.add_argument("-c", "--cont", help="continue on mistakes", action="store_true")
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database (or "none")', default=['keystrokes.db'])
	p.add_argument("-p", "--page-size", nargs=1, help='characters per page', type=int, default=[300])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file', default=['words/ascii_english'])
	p.add_argument("-g", "--gen", help='enable sequence generator', action="store_true")
	p.add_argument("mode", default="practice", choices=('practice', 'analyze'), nargs='?')
	args = p.parse_args()

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
	
	if args.mode == 'practice':
		cu.wrapper(lambda stdscr: practice(stdscr, args))
	elif args.mode == 'analyze':
		analyze(args)

if __name__=="__main__":
	main()

