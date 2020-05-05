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

the_sqc=None
the_wordlist=None

def get_timestamp():
	return int(time())

def key_code(k):
	if type(k) is str:
		return ord(k) if len(k)==1 else 0
	if type(k) is int:
		return k
	return 0

def series_to_text(word_series, max_ch):
	out=""
	x=0
	for index, word in word_series.items():
		x += 1 + len(word)
		if x > max_ch+1:
			break
		out += " " + word
	if len(out)==0:
		raise Exception('empty word series')
	return out[1:]

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
		self.df = self.df.append(df)
	def get(self, max_chars=80):
		return series_to_text(self.df['word'].sample(n=max_chars/2), max_chars)

class InputBox:
	def __init__(self):
		self.sql_data= []
		self.t_begin = None
		self.delay_buffer = [] # used for typing speed calculation
		# options
		self.halt_on_mistake = False
	def win_init(self):
		height = 2 + 2
		width = 68 + 2
		y = 2
		x = 5
		self.win = cu.newwin(height, width, y, x)
	def max_chars(self):
		max_y, max_x = self.win.getmaxyx()
		return (max_y-2) * (max_x-2)
	def set_text(self, text):
		self.text = text[:self.max_chars()]
		self.caret = 0
		self.oops = set()
	def type(self):
		k = self.win.getkey(0,0)
		k_exp = self.text[self.caret]
		if k != k_exp:
			self.set_oops()
		if self.halt_on_mistake == False or k == k_exp:
			self.caret += 1
		if self.caret == len(self.text):
			self.next_words()
		now = monotonic()
		if self.t_begin is not None:
			# start recording from the 2nd character
			delay = now - self.t_begin
			delay_ms = int(delay * 1000)
			self.sql_data += [(key_code(k), key_code(k_exp), delay_ms, get_timestamp())]
			if k == k_exp:
				# let correct keystrokes contribute to typing speed
				self.delay_buffer.insert(0, delay)
				if len(self.delay_buffer) > 500:
					self.delay_buffer.pop()
		self.t_begin = now
	def cpm(self):
		if len(self.delay_buffer) == 0:
			return 0
		t = sum(self.delay_buffer)
		n = len(self.delay_buffer)
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
		self.oops = set()
		self.caret = 0
		self.data_buffer = []
		self.set_text(the_wordlist.get(self.max_chars()))
	def set_oops(self):
		cu.flash()
		self.oops.add(self.caret)
	def repaint(self):
		i = 0
		y = 1
		x = 1
		max_x = self.win.getmaxyx()[1]
		self.win.clear()
		for row in textwrap.wrap(self.text, max_x, drop_whitespace=False):
			i1 = self.caret - i # caret position relative to current row
			i0 = min(len(row), max(0, i1))
			typed = row[:i0]
			untyped = row[i0:]
			if len(typed) > 0:
				self.win.addstr(y, x, typed, cu.color_pair(2))
			if len(untyped) > 0:
				self.win.addstr(y, x + i0, untyped, cu.color_pair(1))
			for j in range(len(row)):
				if i+j in self.oops:
					self.win.addch(y, x+j, row[j], cu.color_pair(3))
			if 0 <= i1 and i1 < len(row):
				self.win.addch(y, x + i0, row[i1],
					cu.color_pair(5 if self.caret in self.oops else 4))
			i += len(row)
			y += 1
		self.win.border()
		self.win.noutrefresh()

def main():

	p = argparse.ArgumentParser()
	p.add_argument("-c", "--cont", help="continue on mistakes", action="store_true")
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database (or "none")', default=['keystrokes.db'])
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
		stdscr = cu.initscr()
		cu.cbreak()
		cu.curs_set(0)
		cu.start_color()
		cu.init_pair(1, cu.COLOR_GREEN, cu.COLOR_BLACK) # untyped
		cu.init_pair(2, cu.COLOR_BLACK, cu.COLOR_GREEN) # typed
		cu.init_pair(3, cu.COLOR_BLACK, cu.COLOR_RED) # mistake
		cu.init_pair(4, cu.COLOR_BLACK, cu.COLOR_WHITE) # cursor
		cu.init_pair(5, cu.COLOR_BLACK, cu.COLOR_MAGENTA) # cursor+mistake
		input_box.halt_on_mistake = not args.cont
		input_box.win_init()
		input_box.next_words()
		while True:
			# redraw the window
			cpm = input_box.cpm()
			wpm = cpm / 1700 * 378
			stdscr.addstr(0, 0, "cpm: %5.0f    wpm : %4.0f" % (cpm,wpm))
			stdscr.noutrefresh()
			input_box.repaint()
			cu.doupdate()

			# wait input
			input_box.type()
	finally:
		cu.echo()
		cu.nocbreak()
		cu.endwin()
		cu.curs_set(1)
		input_box.save_data()
		the_sqc.close()

if __name__=="__main__":
	cu.wrapper(main())

