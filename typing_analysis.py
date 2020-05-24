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

def filter_out_aaaa(col):
	# Return boolean column
	# that is useful for filtering out strings
	# that repeat just 1 character
	return (col != col.str.len() * col.str.slice(0,1)) | (col.str.len() <= 1)

def denoise(df, noise_reject):
	return df.nsmallest(max(10, int(len(df)*(1-noise_reject))), ['delay_std'])

def result_cleanup(df, min_samples=10, noise_reject=0.1):
	df = df[df['samples'] >= min_samples]
	df = df[filter_out_aaaa(df.index)]
	df = denoise(df, noise_reject)
	df = df.sort_values('delay', ascending=False)
	return df

def training_filter(df, limit=50, min_samples=10):
	df = df[df['samples'] >= min_samples]
	df = df[filter_out_aaaa(df['word'])]
	df = denoise(df, 0.1)
	if len(df) > limit:
		w = df['delay']
		w = w*w
		df = df.sample(limit, weights=w)
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

def drop_spaced(df):
	for c in ' \t\n!@#$%^*.,:;()[]{}<>=':
		df = df[~df.index.to_series().str.contains(c, regex=False)]
	return df

def concat_std(a, b):
	c = pd.DataFrame({
		'word':a['word'].str.cat(b['word']),
		'delay':a['delay'] + b['delay'],
		'delay_var':a['delay_var'] + b['delay_var'],
		'samples':np.fmin(a['samples'].to_numpy(), b['samples'].to_numpy())
	})
	c['delay_std'] = c['delay_var'].pow(0.5)
	return c

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
	def __init__(self, limit=2000, path="keystrokes.db", sqc=None, df=None):

		if df is None:
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

		# upper and lower case should have the same frequency
		# thus inspecting them separately is meaningless
		df['ch'] = df['ch'].str.lower()

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
		# age unit is hours

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
		age_hl = 1 # "half life" of data reliability. data from age_hl hours ago weighs half as much as present
		seq['delay'] /= n
		seq['weight'] = 1/(age*age + age_hl*age_hl)

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
		# mseq has nans dropped already so using mseq's fields here
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
	
	def wpm(self):
		cps = 1000 / self.df['delay'].mean()
		cpm = cps * 60
		wpm = cpm / 5
		return wpm
	
	def print_info(self):
		print("Keystroke rows:", self.df.index.size)
		print("Dataset WPM:", self.wpm())
		print("\nSlow keys")
		print(self.slow_keys().head(10))
		for n in self.mseq.keys():
			print("\nSlow sequences of length", n)
			print(self.slow_seq(n).drop(['delay_var'],axis=1).head(15))
			print()
	
	def predict_word_delay(self, word, seq_max=10):
		bad = (np.nan, np.nan, 0)
		if word is None:
			return bad
		# make sure all the sequence data exists
		for n in range(1,1+seq_max):
			self.gen_seq(n)
		word = word.lower()
		ch_delay = []
		ch_delay_var = []
		samples = 0
		# for each character in the word..
		for b in range(len(word)):
			# try to get the longest pattern for which data exist
			for a in range(max(0,b-seq_max), b+1):
				n = b - a + 1
				s = word[a:b+1]
				row = None
				if n in self.mseqd and s in self.mseqd[n]:
					d,v,ns = self.mseqd[n][s]
					assert(d == d)
					ch_delay += [d]
					ch_delay_var += [v]
					samples += ns
					break
		if len(ch_delay) == 0:
			# no data for relevant sequences exist
			return bad
		# try real hard to clean up the values
		ch_delay = np.array(ch_delay)
		ch_delay = np.nan_to_num(ch_delay, copy=False, nan=np.median(ch_delay), neginf=0, posinf=afk_timeout)
		ch_delay_var = np.array(ch_delay_var)
		ch_delay_var = np.nan_to_num(ch_delay_var, copy=False, nan=np.median(ch_delay_var), neginf=0, posinf=afk_timeout)
		if not all(np.isfinite(ch_delay_var)):
			ch_delay_var = np.full(ch_delay_var.shape, 5000 ** 2)
		var_rcp = np.reciprocal(ch_delay_var + 0.001)
		delay = np.average(ch_delay, weights=var_rcp)
		delay_var = 1/np.sum(var_rcp)
		return delay, delay_var, samples
	
	def predict_wordlist_delay(self, wordlist):
		assert(type(wordlist) is not pd.DataFrame)
		data=[]
		for word in wordlist:
			d,v,n = self.predict_word_delay(word)
			if d == d and n != 0:
				s = np.sqrt(v)
				data += [[word,d,s,n]]
		df = pd.DataFrame(data=data, columns=['word','delay','delay_std', 'samples'])
		df = fix_delay(df)
		df = df.dropna()
		df['total'] = df['delay'].to_numpy() * df['word'].str.len()
		df['total_std'] = df['delay_std'].to_numpy() * df['word'].str.len()
		return df
	
	def training_words(self, wordlist_df, limit=50):
		assert(type(wordlist_df) is pd.DataFrame)
		max_candidates = limit*20
		wl = wordlist_df
		if len(wl) > max_candidates:
			wl = wl.sample(max_candidates)
		df = self.predict_wordlist_delay(wl['word'])
		# here samples column means:
		# number of different sequences
		# that contributed to predicted typing time.
		# 3 letter words can consist of 3 sequences at most
		df = training_filter(df, limit, min_samples=3)
		return df
	
	def training_sequences(self, limit=50):
		s2 = self.slow_seq(2)
		df = concat_std(s2, s2)[:max(5,limit//4)]
		df = df.append(self.slow_seq(3))
		df = df.append(self.slow_seq(4))
		df = df.append(self.slow_seq(5))
		df = df.append(self.slow_seq(6))
		# here samples column means:
		# number of times each sequence was typed by the user
		df = training_filter(df, limit, min_samples=5)
		return df

def main():
	p = argparse.ArgumentParser()
	p.add_argument("-d", "--db", nargs=1, help='sqlite3 database', default=['keystrokes.db'])
	p.add_argument("-n", "--limit", nargs=1, help='characters to analyze', type=int, default=[5000])
	p.add_argument("-w", "--wordlist", nargs='+', help='wordlist file(s)', default=['words/ascii_english'])
	p.add_argument("-k", "--slow-keys", help='show which key(sequence)s are slow', action='store_true')
	p.add_argument('-e', '--estimate-word-time', nargs='+', help='estimate typing time for word(s)', default=[])
	p.add_argument('-E', '--estimate-file-time', nargs='+', help='estimate typing time for file(s)', default=[])
	p.add_argument("-G", "--gen-words", nargs='*', help='generate training words', default=False)
	p.add_argument("-g", "--gen-seqs", nargs='*', help='generate training sequences', default=False)
	args = p.parse_args()

	print("Pandas:", pd.__version__)
	print("sqlite3:", sqlite3.version)

	pd.set_option('display.max_rows', None)
	pd.set_option('display.max_columns', None)
	pd.set_option('display.width', None)
	pd.set_option('display.max_colwidth', None)

	if not os.path.isfile(args.db[0]):
		print("File", args.db[0], "doesn't exist. use typing_coach.py to create it")
		return
	
	print("Analyzing...")

	a = Analysis(limit=args.limit[0], path=args.db[0])
	seqs = [2, 3]
	for n in seqs:
		a.gen_seq(n)

	if args.slow_keys is True:
		a.print_info()
	
	if len(args.estimate_word_time) > 0:
		wd = a.predict_wordlist_delay(args.estimate_word_time)
		print(wd)
	
	if len(args.estimate_file_time) > 0:
		delay=0
		var=0
		samples=0
		n_ch=0
		for f in args.estimate_file_time:
			with open(f,"r") as f:
				txt = f.read().strip()
				n_ch += len(txt)
				d,v,n = a.predict_word_delay(txt)
				if d == d and n > 0:
					delay += d * len(txt)
					var += v * len(txt) * len(txt)
					samples += n
		std = np.sqrt(var)
		print("Keystroke history samples: %d" % samples)
		print("Characters in file(s)    : %d" % n_ch)
		print("Time/character (ms)      : %-8.0f std.dev %g" % (delay/n_ch, std/n_ch))
		print("Time total (seconds)     : %-8.0f std.dev %g" % (delay/1000, std/1000))
		print("Time (min):              : %-8.0f std.dev %g" % (delay/1000/60, std/1000/60))
	
	if args.gen_words is not False or args.gen_seqs is not False:

		wordlist=pd.DataFrame()
		for path in args.wordlist:
			wordlist = wordlist.append(read_wordlist(path))
			print("{}: {} words loaded".format(path, len(wordlist)))

		if args.gen_words is not False:
			n=100
			paths=args.gen_words
			if len(paths) > 0:
				for path in paths:
					df = a.training_words(wordlist, n)
					df.to_csv(path, columns=['word'], header=False, index=False)
			else:
				print(a.training_words(wordlist, n).sort_values('delay'))
		
		if args.gen_seqs is not False:
			n=100
			paths=args.gen_seqs
			if len(paths) > 0:
				for path in paths:
					df = a.training_sequences(n)
					df.to_csv(path, columns=[], header=False, index=True)
			else:
				print(a.training_sequences(n).sort_values('delay'))
	
if __name__=="__main__":
	main()

