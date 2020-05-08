#!/usr/bin/python3
import pandas as pd
import numpy as np
import sqlite3
import argparse

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
		mean = mean[mean['samples'] >= 10]

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
		mseq = mseq[mseq['samples'] >= 15]

		self.mseq[n] = mseq
		self.mseq_by_delay[n] = mseq.sort_values(by='delay', ascending=False)
		self.mseq_by_error[n] = mseq.sort_values(by='typo', ascending=False)
	
	def print_misc(self):
		print("Input rows:", self.df.index.size)
		print("\nKeys most often typo:")
		print(self.by_error.head(5))
		print("\nKeys that are slow:")
		print(self.by_delay.head(5))
	
	def print_seq(self,i):
		print("\nSequences (%d) most often typo:" % i)
		print(self.mseq_by_error[i].head(5))
		print("\nSequences (%d) that are slow:" % i)
		print(self.mseq_by_delay[i].head(5))

if __name__=="__main__":
	p = argparse.ArgumentParser()
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database', default=['keystrokes.db'])
	p.add_argument("-n", "--limit", nargs=1, help='characters to analyze', type=int, default=[50000])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file(s)', default=['words/ascii_english'])
	p.add_argument("-g", "--gen", help='enable sequence generator', action="store_true")
	args = p.parse_args()

	if args.db[0] != "none":
		global the_sqc
		the_sqc = sqlite3.connect(args.db[0])

	print("Pandas:", pd.__version__)
	print("Analyzing...")

	seqs = [2, 3]
	a = Analysis(args.limit[0])
	for n in seqs:
		a.gen_seq(n)
	a.print_misc()
	for n in seqs:
		a.print_seq(n)

