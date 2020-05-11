#!/usr/bin/env python3
import pandas as pd
import numpy as np
import sqlite3
import argparse
import os

afk_timeout = 10000

def read_wordlist(path):
	pq=path+'.pq'
	if os.path.exists(pq) and os.path.getctime(path) <= os.path.getctime(pq):
		return pd.read_parquet(pq, columns=['word'])
	df = pd.read_table(path,
		skipinitialspace = True,
		sep = '\0',
		names = ['word'],
		dtype = str,
		engine = 'c',
		memory_map = True,
		encoding = 'utf-8')
	df.word = df.word.str.strip()
	if not os.path.exists(pq):
		df.to_parquet(pq, index=False, compression='gzip')
	return df

# word: a string
# n: all sequence lengths
def get_all_sequences(word, n):
	w = word.rjust(n)
	for i in range(len(word)-n+1):
		yield w[i:i+n]

def drop_spaced(df):
	for c in ' \t\n!@#$%^*.,:;()[]{}<>=':
		df = df[df.index.to_series().str.contains(c, regex=False) ^ True]
	return df

def fix_delay(df):
	max_val = max(df['delay'])
	max_std = max(df['delay_std'])
	df['delay'].fillna(value=max_val, inplace=True)
	df['delay'][df['delay']<0] = max_val
	df['delay_std'].fillna(value=max_std, inplace=True)
	df['delay_std'][df['delay_std']<0] = max_std
	return df

class Analysis:
	def __init__(self, limit=2000, path="keystrokes.db", sqc=None):

		sqc_tmp = None
		if sqc is None:
			sqc = sqlite3.connect(path)
			sqc_tmp = sqc

		# ignore mistakes (unusually long delay reveals them anyway)
		# most recent first
		df = pd.read_sql_query(
			"SELECT * FROM keystrokes" +
			" WHERE (pressed == expected and delay_ms < %d)" % afk_timeout +
			" ORDER BY sequence DESC LIMIT %d" % limit,
			sqc)

		if sqc_tmp is not None:
			sqc_tmp.close()

		# rename the fields
		df['delay'] = df['delay_ms']
		df['ch'] = df['expected'].apply(lambda x: chr(x))
		df = df.drop(['delay_ms', 'pressed', 'expected'], axis=1)

		# ignore afk inputs
		df = df[df['delay'] < afk_timeout]

		self.df = df

		g = df.groupby(by='ch')
		mean = g.mean()
		mean = mean.drop(['sequence', 'timestamp'], axis=1)
		mean['delay_std'] = g.std()['delay']
		mean['samples'] = g.count()['delay']
		mean = fix_delay(mean)

		# drop undersampled keys
		mean = mean[mean['samples'] >= 10]

		mean = mean.sort_values(by='delay', ascending=False)
		self.mean = mean

		self.seq = {} # sequence samples (by sequence length)
		self.mseq = {} # time per sequence (by sequence length)
		self.mseqd = {} # time per sequence as python dict

	def gen_seq(self, n=2):
		if n in self.seq:
			return

		seq = pd.DataFrame()
		seq['word'] = self.df['ch']
		seq['delay'] = self.df['delay']
		seq['ok'] = [True] * self.df.shape[0]

		for i in range(1,n):
			next_row = self.df.shift(i)
			seq['word'] = seq['word'].str.cat(next_row['ch'])
			seq['delay'] += next_row['delay']

			# Only consider key presses with adjacent sequence counter
			seq['ok'] &= (next_row['sequence'] - self.df['sequence']) == i

		# drop invalid sequences
		seq = seq[seq['ok']]
		seq = seq.drop(['ok'], axis=1)
		seq = seq.dropna()

		g = seq.groupby(by='word')
		# delay per character (so sequences of different length are comparable)
		delay = g.mean()['delay'] / n
		std = g.std()['delay'] / n

		mseq = pd.DataFrame({
			'delay': delay,
			'delay_std' : std,
			'samples' : g.count()['delay']},
			index=delay.index.to_series())

		mseq = fix_delay(mseq)
		mseq.dropna()

		mseq['word'] = mseq.index.to_series()

		self.seq[n] = seq
		self.mseq[n] = mseq.sort_values(by='delay', ascending=False)
		self.mseqd[n] = dict(zip(mseq.index, zip(mseq.delay, mseq.delay_std, mseq.samples)))
	
	def get_seq(self, n):
		self.gen_seq(n)
		return self.mseq[n]
	
	def slow_keys(self):
		k = self.mean
		k = drop_spaced(k)
		return k
	
	def slow_seq(self, n):
		s = self.get_seq(n)
		s = drop_spaced(s)
		s = s[s['samples'] > 1]
		return s
	
	def print_info(self):
		print("Keystroke rows:", self.df.index.size)
		print("\nSlow keys")
		print(self.slow_keys().head(10))
		for n in self.mseq.keys():
			print("\nSlow sequences of length", n)
			print(self.slow_seq(n).head(5))
			print()
	
	def predict_word_delay(self, word, seq_max=11):
		bad = (np.nan, np.nan, 0)

		if word is None:
			return bad

		seq_len = list(range(3,min(len(word),seq_max)))
		mean = []
		std = []
		samples = 0

		for n in seq_len:
			self.gen_seq(n)
			d = self.mseqd[n]
			for sq in get_all_sequences(word, n):
				if sq in d:
					row = d[sq]
					mean += [row[0]]
					std += [row[1]]
					samples += row[2]

		if len(mean) == 0:
			return bad

		mean = np.array(mean)
		std = np.array(std)
		var = std * std
		var_rcp = np.reciprocal(var)
		delay = np.average(mean, weights=var_rcp)
		delay_std = np.sqrt(1/sum(var_rcp))
		return delay, delay_std, samples
	
	def predict_wordlist_delay(self, wordlist):
		assert(type(wordlist) is pd.Series)
		data=[]
		for index, word in wordlist.items():
			d,s,n = self.predict_word_delay(word)
			data += [[word,d,s,n]]
		df = pd.DataFrame(data, columns=['word','delay','delay_std', 'nseq'])
		df = fix_delay(df)
		df = df.dropna()
		df['total'] = df['delay'].to_numpy() * df['word'].str.len()
		df['total_std'] = df['delay_std'].to_numpy() * df['word'].str.len()
		return df
	
	def training_words(self, wordlist_df, limit=50):
		assert(type(wordlist_df) is pd.DataFrame)
		max_candidates = limit*10
		wl = wordlist_df
		if len(wl) > max_candidates:
			wl = wl.sample(max_candidates)
		df = self.predict_wordlist_delay(wl['word'])
		if len(df) > limit:
			df = df.sample(limit, weights='delay')
		return df
	
	def training_sequences(self, limit=50):
		df = self.slow_seq(2)
		df['word'] = df['word'].str.cat(df['word'])
		df = df.append(self.slow_seq(3))
		df = df.append(self.slow_seq(4))
		df = df.append(self.slow_seq(5))
		# drop sequences consiting of just one character
		df = df[df['word'] != df['word'].str.len() * df['word'].str.slice(0,1)]
		if len(df) > limit:
			df = df.sample(limit, weights=df['delay'])
		return df

if __name__=="__main__":
	p = argparse.ArgumentParser()
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database', default=['keystrokes.db'])
	p.add_argument("-n", "--limit", nargs=1, help='characters to analyze', type=int, default=[2000])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file(s)', default=['words/ascii_english'])
	p.add_argument('-e', '--estimate-word-time', nargs='+', help='estimate typing time for word(s)', default=[])
	p.add_argument("-g", "--gen", nargs=1, help='generate training words', default=[])
	p.add_argument("-s", "--seq", nargs=1, help='generate training sequences', default=[])
	args = p.parse_args()

	print("Pandas:", pd.__version__)

	wordlist=pd.DataFrame()
	for path in args.wordlist:
		wordlist = wordlist.append(read_wordlist(path))
		print("{}: {} words loaded".format(path, len(wordlist)))
	
	print("Wordlist:")
	print(wordlist.head(5))
	
	print("Analyzing...")

	a = Analysis(limit=args.limit[0], path=args.db[0])
	seqs = [2, 3]
	for n in seqs:
		a.gen_seq(n)
	a.print_info()
	
	if len(args.estimate_word_time) > 0:
		wd = a.predict_wordlist_delay(args.estimate_word_time)
		print(wd)
	
	for path in args.gen:
		df = a.training_words(wordlist, 100)
		if path == '-':
			print(df)
		else:
			df.to_csv(path, columns=['word'], header=False, index=False)
	
	for path in args.seq:
		df = a.training_sequences(100)
		if path == '-':
			print(df)
		else:
			df.to_csv(path, columns=[], header=False, index=True)

