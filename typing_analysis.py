#!/usr/bin/env python3
import pandas as pd
import numpy as np
import sqlite3
import argparse
import os

afk_timeout = 10000

def weighted_average(df, by, data, weight, epsilon=1e-6):
	df.sort_values(by, inplace=True)
	df['_data_x_weight'] = df[data] * df[weight]
	df['_weight_nonzero'] = abs(df[weight]) > epsilon
	g = df.groupby(by)
	W = g[weight].sum()
	avg_out = g['_data_x_weight'].sum() / W
	# then figure out the variance
	n = g['_data_x_weight'].count()
	m = g['_weight_nonzero'].sum()
	# get mean for each input row
	avg = g['_data_x_weight'].transform('sum') / g[weight].transform('sum')
	df['_weight_x_var'] = df[weight] * (df[data] - avg)**2
	g = df.groupby(by)
	var = g['_weight_x_var'].sum() / ((m-1)/m * W)
	idx = g['_weight_x_var'].count().index
	del df['_data_x_weight'], df['_weight_x_var']
	return idx, avg_out, var, n

def result_cleanup(df, min_samples=10, noise_reject=0.1):
	word_len = min(df.index.str.len()) if len(df)>0 else 0
	# drop undersampled sequences
	df = df[df['samples'] >= min_samples]
	# drop all uppercase characters (no shit: modifier key always adds time)
	df = df[~df.index.str.isupper()]
	if word_len > 1:
		# drop sequences consiting of just one character
		df = df[df.index != df.index.str.len() * df.index.str.slice(0,1)]
	# drop the most highly varying rows
	df = df.nsmallest(max(10, int(len(df)*(1-noise_reject))), ['delay_std'])
	# slowest first
	df = df.sort_values('delay', ascending=False)
	return df

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
		df = df[~df.index.to_series().str.contains(c, regex=False)]
	return df

def fix_delay(df):
	for c in ('delay', 'delay_std', 'delay_var'):
		if c in df:
			# any key or sequence that only has 1 sample will have 0/nan/inf std
			val = df[c].median()
			if val <= 0 or val == np.inf or not (val == val):
				# median is bad too (just 1 sample of 1 unique sequence?)
				val = 5555
			df[c].fillna(value=val, inplace=True)
			df[c].mask(df[c]<=0, val, inplace=True)
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

		if len(df) == 0:
			raise Exception("database has no data")

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

		t0 = max(self.df['timestamp']) + 0.001
		age = ((t0 - self.df['timestamp']) * (1.0/3600)).shift(n)

		seq = pd.DataFrame()
		seq['word'] = self.df['ch']
		seq['delay'] = self.df['delay']
		seq['ok'] = [True] * self.df.shape[0]

		for i in range(1,n):
			next_row = self.df.shift(i)
			seq['word'] = seq['word'].str.cat(next_row['ch'])
			seq['delay'] += next_row['delay']

			# Only consider key presses with adjacent sequence counters
			seq['ok'] &= (next_row['sequence'] - self.df['sequence']) == i

		# drop invalid sequences
		seq = seq[seq['ok']]
		seq = seq.drop(['ok'], axis=1)
		seq = seq.dropna()

		# delay per character (so sequences of different length are comparable)
		seq['delay'] /= n
		seq['weight'] = 1/(age*age)

		idx, delay, delay_var, samples = \
			weighted_average(seq, 'word', 'delay', 'weight')

		delay_std = delay_var.pow(0.5)

		mseq = pd.DataFrame({
			'delay': delay,
			'delay_var' : delay_var,
			'delay_std' : delay_std,
			'samples' : samples,
			'word' : idx},
			index=idx)

		mseq = fix_delay(mseq)
		mseq.dropna()

		self.seq[n] = seq
		self.mseq[n] = mseq.sort_values(by='delay', ascending=False)
		self.mseqd[n] = dict(zip(mseq.index, zip(mseq.delay, mseq.delay_var, mseq.samples)))
	
	def get_seq(self, n):
		self.gen_seq(n)
		return self.mseq[n]
	
	def slow_keys(self):
		k = self.mean
		k = drop_spaced(k)
		k = result_cleanup(k, min_samples=10, noise_reject=0.01)
		return k
	
	def slow_seq(self, n):
		s = self.get_seq(n)
		s = drop_spaced(s)
		s = result_cleanup(s, min_samples=10, noise_reject=0.05)
		return s
	
	def wpm(self, n=2000):
		rows = self.df.nlargest(n, 'sequence')
		cps = 1000 * len(rows) / sum(rows['delay'])
		cpm = cps * 60
		wpm = cpm / 5
		return wpm
	
	def print_info(self):
		print("Keystroke rows:", self.df.index.size)
		print("WPM of last 2000 keys:", self.wpm(2000))
		print("\nSlow keys")
		print(self.slow_keys().head(10))
		for n in self.mseq.keys():
			print("\nSlow sequences of length", n)
			print(self.slow_seq(n).drop(['delay_var'],axis=1).head(15))
			print()
	
	def predict_word_delay(self, word, seq_min=3, seq_max=10):
		bad = (np.nan, np.nan, 0)
		if word is None:
			return bad
		seq_len = list(range(seq_min,min(len(word),seq_max+1)))
		mean = []
		var = []
		samples = 0
		for n in seq_len:
			self.gen_seq(n)
			d = self.mseqd[n]
			for sq in get_all_sequences(word, n):
				if sq in d:
					row = d[sq]
					mean += [row[0]]
					var += [row[1]]
					samples += row[2]
		if len(mean) == 0:
			if seq_min > 1:
				# no data for relevant sequences exist
				# so try again with single characters
				return self.predict_word_delay(word, 1, 2)
			# absolutely no data at all
			return bad
		mean = np.array(mean)
		mean = np.clip(mean, 0, afk_timeout)
		with np.errstate(divide='ignore'):
			var_rcp = np.reciprocal(var)
		if abs(sum(var_rcp)) > 1e-5:
			delay = np.average(mean, weights=var_rcp)
			delay_std = np.sqrt(1/sum(var_rcp))
		else:
			mean = np.mean(mean)
			delay = mean
			delay_std = np.nanstd(mean)
		return delay, delay_std, samples
	
	def predict_wordlist_delay(self, wordlist):
		assert(type(wordlist) is not pd.DataFrame)
		data=[]
		for word in wordlist:
			d,s,n = self.predict_word_delay(word)
			data += [[word,d,s,n]]
		df = pd.DataFrame(data=data, columns=['word','delay','delay_std', 'nseq'])
		df = fix_delay(df)
		df = df.dropna()
		df['total'] = df['delay'].to_numpy() * df['word'].str.len()
		df['total_std'] = df['delay_std'].to_numpy() * df['word'].str.len()
		return df
	
	def training_words(self, wordlist_df, limit=50):
		assert(type(wordlist_df) is pd.DataFrame)
		max_candidates = limit*50
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

def main():
	p = argparse.ArgumentParser()
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database', default=['keystrokes.db'])
	p.add_argument("-n", "--limit", nargs=1, help='characters to analyze', type=int, default=[2000])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file(s)', default=['words/ascii_english'])
	p.add_argument('-e', '--estimate-word-time', nargs='+', help='estimate typing time for word(s)', default=[])
	p.add_argument("-g", "--gen", nargs=1, help='generate training words', default=[])
	p.add_argument("-s", "--seq", nargs=1, help='generate training sequences', default=[])
	args = p.parse_args()

	print("Pandas:", pd.__version__)
	print("sqlite3:", sqlite3.version)

	if not os.path.isfile(args.db[0]):
		print("File", args.db[0], "doesn't exist. use typing_coach.py to create it")
		return

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
	
if __name__=="__main__":
	main()

