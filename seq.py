#!/usr/bin/env python3

ch=list('abcdefghijklmnopqrstuvwxyz') #[]{}()=;:.,\'"<>=_-+')
pairs=[a+b for a in ch for b in ch]

def out(path,x):
	fname='words/{}.txt'.format(path)
	print(fname)
	with open(fname, 'w') as f:
		f.write('\n'.join(x) + '\n')
	"""
	with open('words/{}b.txt'.format(path), 'w') as f:
		f.write('\n'.join(c+c for c in x) + '\n')
		f.write('\n'.join(c+c+c for c in x) + '\n')
		f.write('\n'.join( \
			''.join(list(c)+list(reversed(c))) \
			for c in x) + '\n')
	with open('words/{}c.txt'.format(path), 'w') as f:
		f.write('\n'.join(5*c for c in x) + '\n')
	"""

#out('train', pairs)
#out('train', [a+b for a in pairs for b in pairs])

i=1
for ch_subset in \
		ch[:len(ch)//3], \
		ch[len(ch)//3:2*len(ch)//3], \
		ch[2*len(ch)//3:]:
	seq=[3*c for c in ch_subset]
	seq2=[3*(a+b) for a in ch_subset for b in ch_subset]
	out('ez%d' % i, seq+seq2)
	i += 1

