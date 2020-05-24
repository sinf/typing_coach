#!/usr/bin/env python3
import sqlite3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from contextlib import closing as ctx_closing
from argparse import ArgumentParser

def read_daily_stats(sqc):
	df = pd.read_sql_query(
		"SELECT day, avg, (xx/n - avg*avg) AS var, min, max, n AS count FROM (" + \
		"SELECT strftime('%Y-%m-%d',timestamp,'unixepoch') AS day," + \
		" AVG(delay_ms) AS avg," + \
		" MIN(delay_ms) AS min," + \
		" MAX(delay_ms) AS max," + \
		" SUM(delay_ms*delay_ms) AS xx," + \
		" COUNT(*) AS n" + \
		" FROM keystrokes WHERE (delay_ms <= 5000 AND pressed == expected)" + \
		" GROUP BY day ORDER BY day)", sqc)
	df['day'] = pd.to_datetime(df['day'])
	return df

def main():
	p = ArgumentParser()
	p.add_argument('-d', '--db', nargs='+', help='sqlite database paths', default=['keystrokes.db'])
	args = p.parse_args()
	df = pd.DataFrame()

	# Read each day from each database
	for path in args.db:
		with ctx_closing(sqlite3.connect(path)) as sqc:
			tmp = read_daily_stats(sqc)
			df = df.append(tmp)
	
	# Combine duplicate day rows
	g = df.groupby('day')
	df = pd.DataFrame({
		'avg' : g['avg'].mean(),
		'var' : g['var'].sum() / (g['var'].count()**2),
		'count' : g['count'].sum(),
		'min' : g['min'].min(),
		'max' : g['max'].max(),
		}, index=g.groups)

	df = df.sort_index()
	df['std'] = np.sqrt(df['var'])
	
	print(df)
	print("Total keystrokes sampled:", sum(df['count']))

	x = df.index.strftime('%d/%m')
	y = df['avg']
	ye = df['std']
	y0 = df['min']
	y0 = np.maximum(df['min'], y-ye)
	y1 = np.minimum(df['max'], y+ye)

	with plt.style.context('Solarize_Light2'):
		fig,ax = plt.subplots()
		ax.fill_between(x, y0, y1, alpha=0.2)
		ax.plot(x, y, label='ms/keystroke', lw=5)
		#plt.ylim((max(0, min(y) - 50), max(y) + 50))
		plt.ylim((0, max(y1)*1.1))
		plt.legend(loc='upper left')
		plt.show()

if __name__ == "__main__":
	main()

