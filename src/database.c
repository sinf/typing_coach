#include <time.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <unistr.h>
#include <unictype.h>
#include <unicase.h>
#include <assert.h>
#include "sz_mult.h"
#include "database.h"
#include "prog_util.h"
#include "debug.h"

#define sq3_bind_int(s,i,x) \
	if (sqlite3_bind_int((s),(i),(x)) != SQLITE_OK) \
		db_fail("bind_int(column=%d,value=%d)",(i),(x))
#define sq3_bind_int64(s,i,x) \
	if (sqlite3_bind_int64((s),(i),(x)) != SQLITE_OK) \
		db_fail("bind_int64(column=%d,value=%ld)",(i),(long)(x))
#define sq3_bind_double(s,i,x) \
	if (sqlite3_bind_double((s),(i),(x)) != SQLITE_OK) \
		db_fail("bind_double(column=%d,value=%g)",(i),(double)(x))
#define sq3_bind_text(s,i,ptr,size) \
	if (sqlite3_bind_text((s),(i),(ptr),(size),NULL) != SQLITE_OK) \
		db_fail("bind_text(column=%d,value=%.*s)",(i),(int)(size),(ptr))
#define sq3_bind_blob(s,i,ptr,size) \
	if (sqlite3_bind_blob((s),(i),(ptr),(size),SQLITE_STATIC) != SQLITE_OK) \
		db_fail("bind_blob(column=%d,size=%lu)",(i),(unsigned long)(size))
#define sq3_bind_zeroblob(s,i,size) \
	if (sqlite3_bind_zeroblob((s),(i),(size)) != SQLITE_OK) \
		db_fail("bind_zeroblob(column=%d,size=%lu)",(i),(unsigned long)(size))
#define sq3_step(s,code) \
	if (sqlite3_step(s) != (code)) db_fail("step")

// sqlite3_bind_.. use 0-based indexing
// sqlite3_column_.. use 1-based indexing. WTF!?! beware

const char *database_path = "";
long the_typing_counter = 0;
static long words_table_rows=0;

static sqlite3 *db = 0;

// prepared statements. remember to add PREP() statement in db_open:
static sqlite3_stmt
	*st_put_key = 0,
	*st_get_recent = 0,
	*st_assoc_seq_word = 0,
	*st_get_words = 0,
	*st_get_words_r = 0,
	*st_put_word = 0,
	*st_get_hist = 0,
	*st_insert_hist = 0,
	*st_update_hist = 0;

static const char sql_create[] = 
"CREATE TABLE IF NOT EXISTS keystrokes"
"( sequence INTEGER PRIMARY KEY,"
" pressed INTEGER," // keycode
" expected INTEGER," // keycode
" delay_ms INTEGER,"
" timestamp INTEGER );\n"

"CREATE TABLE IF NOT EXISTS words"
"( word TEXT UNIQUE NOT NULL );\n"

/* short typing history for each sequence
could be reconstructed from 'keystrokes' table */
"CREATE TABLE IF NOT EXISTS seq_hist"
"( seq TEXT UNIQUE NOT NULL,"
" samples INTEGER(0),"
" delay_mean REAL(0),"
" delay_stdev REAL(0),"
" typo_mean REAL(0),"
" cost_func REAL(0),"
" hist BLOB DEFAULT NULL);\n"

/* useful for getting all alphabet in the dictionary */
"CREATE VIEW IF NOT EXISTS alpha(ch)"
" AS SELECT seq FROM seq_hist WHERE LENGTH(seq)==1;\n"

"CREATE TABLE IF NOT EXISTS alpha_deck"
"( ch TEXT UNIQUE NOT NULL, typed INTEGER(0) );\n"

/* map sequence-->word
for fast training words retrieval based on sequence
so big and fat table */
"CREATE TABLE IF NOT EXISTS seq_words"
"( seq TEXT NOT NULL,"
" word_id INTEGER REFERENCES words(word),"
" UNIQUE(seq, word_id) );\n"
;

static const char sql_alpha_update[] =
"INSERT INTO alpha_deck\n"
// any character that is in alphabet but not in the deck
"(SELECT ch FROM alpha EXCEPT SELECT ch FROM alpha_deck LIMIT 2)\n"
"WHERE\n"
// only if already typed all characters in deck at least 10 times
"(NOT EXISTS (SELECT 1 FROM alpha_deck WHERE typed < 10))\n"
// only if user is competent enough at typing all characters in the deck
"AND EXISTS (SELECT 1 FROM seq_hist WHERE samples>=10\n"
" AND seq IN (SELECT ch FROM alpha_deck)\n"
" AND delay_mean<500 AND delay_stdev<500 AND typo_mean<0.1);\n";

static const char sql_alpha_type[] =
"UPDATE alpha_deck SET typed=typed+1 WHERE ch=?";

static const char sql_alpha_get[] =
"SELECT word FROM words WHERE rowid IN\n"
" (SELECT word_id FROM seq_words WHERE\n"
"  (seq IN SELECT ch FROM alpha_deck)\n"
" )\n"
" ORDER BY RANDOM() LIMIT ?";

static const char sql_insert_hist[] =
"INSERT OR IGNORE INTO seq_hist\n"
" (hist,samples,delay_mean,delay_stdev,typo_mean,cost_func,seq)\n"
" VALUES (?,?,?,?,?,?,?)";

static const char sql_update_hist[] =
"UPDATE seq_hist SET hist=?, samples=?, delay_mean=?, delay_stdev=?, typo_mean=?, cost_func=? WHERE rowid=?";

static const char sql_get_hist[] =
"SELECT rowid,hist FROM seq_hist WHERE seq=? LIMIT 1";

static const char sql_put_key[] =
"INSERT INTO keystrokes (pressed,expected,delay_ms,timestamp) VALUES (?,?,?,?)";

static const char sql_get_recent[] =
"SELECT sequence, pressed, expected, delay_ms, timestamp"
" FROM keystrokes"
//" WHERE (delay_ms < 10000)"
" ORDER BY sequence DESC LIMIT ?";

static const char sql_assoc_seq_word[] =
"INSERT OR IGNORE INTO seq_words (seq,word_id) VALUES (?,?);";

static const char sql_get_words[] =
"SELECT word FROM words WHERE rowid IN\n"
" (SELECT word_id FROM seq_words WHERE (seq = ?))\n"
" ORDER BY RANDOM() LIMIT ?";

static const char sql_get_words_r[] =
"SELECT word FROM words ORDER BY RANDOM() LIMIT ?";

static const char sql_put_word[] =
"INSERT INTO words (word) VALUES (?)";

static const char sql_cleanup_code[] =
/*
lengthy code to trim seq_words to have at most ~1000 words per sequence
because short sequences like "e" are included in too many words
*/
#define TRIM_SEQ_WORDS 1
#if TRIM_SEQ_WORDS
/* temp1: aggregate (seq, word count) */
"CREATE TEMP TABLE temp1 (seq TEXT UNIQUE NOT NULL, n INTEGER);\n"
"INSERT INTO temp1 SELECT seq,COUNT(seq) FROM seq_words GROUP BY seq;\n"

/* only keep sequences that have way too many words */
"DELETE FROM temp1 WHERE n<1100;\n"

/* temp2: r=rowid in seq_words to drop, p=fraction of rows to drop */
"CREATE TEMP TABLE temp2 (r INTEGER PRIMARY KEY,p REAL);\n"
"INSERT INTO temp2 (r,p)\n"
" SELECT ll.rowid, (rr.n-1000.0)/rr.n\n"
" FROM seq_words ll LEFT JOIN temp1 rr WHERE ll.seq=rr.seq;\n"

/* keep rows with probability proportional to p */
"DELETE FROM temp2 WHERE\n"
/* random float in range [0.0, 1.0] */
" (RANDOM()/65536+140737488355328)/281474976710655.0 > p;\n"

"DELETE FROM seq_words WHERE rowid IN (SELECT r FROM temp2);\n"
"DROP TABLE temp1;\n"
"DROP TABLE temp2;\n"
#endif
"VACUUM;\n";

__attribute__((format(printf,4,5)))
void db_fail_x(const char *file, const char *fun, int ln, const char *fmt, ...)
{
	endwin_if_needed();

	va_list a;
	va_start(a, fmt);

	fprintf(stderr,
	"Database error!\n"
	"Database: %s\n"
	"Reason: %s\n"
	"Where: %s:%d: %s()\n"
	"What:\n",
	database_path, sqlite3_errmsg(db), file, ln, fun);

	vfprintf(stderr, fmt, a);
	va_end(a);

	fprintf(stderr, "\n");
	cleanup();
	abort();
	exit(1);
}

#define db_fail(msg...) db_fail_x(__FILE__,__func__,__LINE__,msg)

static int save_long(void *p, int num_cols, char *data[], char *colnames[])
{
	*(long*) p = strtol(data[0], NULL, 10);
	return 0;
}

static void count_rows(const char *query, long *p)
{
	int e = sqlite3_exec(db, query, save_long, p, NULL);
	if (e != SQLITE_OK) db_fail("%s", query);
}

void db_open()
{
	if (db) return;

	int e, ok=SQLITE_OK;
	// default encoding should be UTF-8
	e=sqlite3_open(database_path, &db);
	if (e != ok) db_fail("sqlite3_open");
	e=sqlite3_exec(db, sql_create, NULL, NULL, NULL);
	if (e != ok) db_fail("initializing tables");

#define PREP(x) \
e=sqlite3_prepare_v2(db, sql_ ## x, sizeof(sql_ ## x), &(st_ ## x), NULL); \
if (e != ok) db_fail("preparing statement \"%s\"\n", #x)

	PREP(put_key);
	PREP(get_recent);
	PREP(assoc_seq_word);
	PREP(get_words);
	PREP(get_words_r);
	PREP(put_word);
	PREP(insert_hist);
	PREP(update_hist);
	PREP(get_hist);
#undef PREP

	count_rows("SELECT COUNT(*) FROM keystrokes", &the_typing_counter);
	count_rows("SELECT COUNT(*) FROM words", &words_table_rows);
}

static int in_transaction=0;
void db_trans_begin()
{
	if (!in_transaction) {
		sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
		in_transaction=1;
	}
}
void db_trans_end()
{
	if (in_transaction) {
		sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
		in_transaction=0;
	}
}

void db_close()
{
	if (st_put_key) {
		sqlite3_finalize(st_put_key);
		st_put_key=0;
	}
	if (db) {
		db_trans_end();
		sqlite3_close(db);
		db=0;
	}
}

void db_put(KeyCode pressed, KeyCode expected, int delay_ms)
{
	sqlite3_stmt *s = st_put_key;
	const int64_t t=time(0);
	sq3_bind_int(s, 1, pressed);
	sq3_bind_int(s, 2, expected);
	sq3_bind_int(s, 3, delay_ms);
	sq3_bind_int64(s, 4, t);
	sq3_step(s, SQLITE_DONE);
	sqlite3_reset(s);
	the_typing_counter += 1;
}

double calc_cost(int len, int delay[], int mist[])
{
	double cost=0;
	for(int i=0; i<len; ++i) {
		cost = cost + delay[i]*0.01 + mist[i]*10.0;
	}
	return cost / len;
}

double calc_weight(int len, int age[])
{
	double total=0.1;
	for(int i=0; i<len; ++i) total += age[i];
	const double A=0.001; // minimum weight
	const double B = pow(0.2, 1.0 / (24*60*60));
	return A + (1.0-A)*pow(B, total/len);
}

int cmp_seq_cost(const KSeq *a, const KSeq *b)
{
	if (a->cost == b->cost) {
		// less samples first
		if (a->samples != b->samples)
			return a->samples < b->samples ? -1 : 1;
		return 0;
	}
	// more expensive first
	return a->cost < b->cost ? 1 : -1;
}

size_t db_get_sequences(int num_ch, int sl_min, int sl_max, KSeq *out[1])
{
	if (sl_min > MAX_SEQ || sl_max > MAX_SEQ) {
		fail("%s:%s: too long sequence length (%d, %d)\n",
			__FILE__, __func__, sl_min, sl_max);
	}
	sqlite3_stmt *s = st_get_recent;
	int e, len=0, prev_seq=-2;
	KeyCode keys[MAX_SEQ+1] = {0};
	int k_age[MAX_SEQ+1];
	int delay[MAX_SEQ+1];
	int mist[MAX_SEQ+1];
	int mist_acc = 0;
	int64_t prev_age=0, age=0, now=time(0);
	size_t n_seqs = 0;
	size_t out_alloc = 0, grow = 20000;
	*out = NULL;

	sq3_bind_int(s, 1, num_ch);

	while((e=sqlite3_step(s))==SQLITE_ROW) {
		int seq, k0, k1, kd;
		int64_t ts;
		seq = sqlite3_column_int(s, 0);
		k0 = sqlite3_column_int(s, 1);
		k1 = sqlite3_column_int(s, 2);
		kd = sqlite3_column_int(s, 3);
		ts = sqlite3_column_int64(s, 4);
		age = now - ts;

		int dif = seq - prev_seq;
		int64_t age_dif=0;
		if (prev_age != 0)
			age_dif = prev_age - age;

		if (dif != 1 || k_is_space(k0) || age_dif>10) {
			// sequence break
			len = 0;
			mist_acc = 0;
		}

		if (k_is_space(k0)) {
			continue;
		}

		if (k0 == k1) {
			// correct keypress
			const int N=MAX_SEQ;
			memmove(keys+1, keys, N*sizeof *keys);
			memmove(delay+1, delay, N*sizeof *delay);
			memmove(mist+1, mist, N*sizeof *mist);
			memmove(k_age+1, k_age, N*sizeof *k_age);
			keys[0] = k0;
			delay[0] = kd;
			mist[0] = mist_acc;
			k_age[0] = age > INT_MAX ? INT_MAX : age;
			mist_acc = 0;
			
			if (len < MAX_SEQ)
				len += 1;

			// process sequences
			if (len >= sl_min)
			for(int l=sl_min; l<=sl_max; l++) {
				if (n_seqs == out_alloc) {
					out_alloc += grow;
					*out=Realloc(*out,out_alloc,sizeof **out,0);
				}
				KSeq *se = *out + n_seqs;
				memcpy(se->s, keys, l*sizeof *keys);
				se->s[l] = 0;
				se->len = l;
				se->samples = 1;
				se->cost = calc_cost(l, delay, mist);
				se->cost_var = -1;
				se->weight = calc_weight(l, k_age);
				n_seqs += 1;
			}
		} else {
			// incorrect keypress
			mist_acc += 1;
			if (mist_acc > 3) {
				// probably typing really fast
				// and got one character wrong
				mist_acc = 3;
			}
		}
		prev_seq = seq;
	}
	sqlite3_reset(s);

	n_seqs = remove_duplicate_sequences(*out, n_seqs);
	qsort(*out, n_seqs, sizeof **out, (CmpFunc) cmp_seq_cost);

	return n_seqs;
}

// look up history for each supplied sequence
void db_get_sequences_hist(size_t count, const KSeq seqs[], KSeqHist hist_out[])
{
	sqlite3_stmt *s = st_get_hist;

	for(size_t i=0; i<count; ++i) {
		const size_t B = sizeof *hist_out;
		size_t l=0, bytes=0;
		uint8_t *buf = u32_to_u8(seqs[i].s, seqs[i].len, NULL, &l);
		const void *blob;

		sq3_bind_text(s, 1, (char*) buf, l);

		if (sqlite3_step(s) == SQLITE_ROW) {
			blob = sqlite3_column_blob(s, 1);
			if (blob) {
				bytes = sqlite3_column_bytes(s, 1);
				assert(bytes == B);
			}
		}

		if (bytes == B && kseq_hist_validate(blob)) {
			memcpy(hist_out+i, blob, B);
		} else {
			memset(hist_out+i, 0, B);
		}

		sqlite3_reset(s);
		free(buf);
	}
}

size_t remove_duplicate_sequences(KSeq *s, size_t count)
{
	size_t dst=0, src=0, mean_var_n=0, i;
	double mean_var = 0;

	if (count<2) return count;
	qsort(s, count, sizeof *s, (CmpFunc) kseq_cmp);

	while (src < count) {
		size_t run=1, n;
		double mean=s[src].cost, w=0;

		// see how many sequences are equal. compute mean over all samples
		for(i=src+1; i<count && kseq_equal(s+src, s+i); ++i) {
			run += 1;
			mean += s[i].cost * s[i].weight;
			w += s[i].weight;
		}

		if (run > 1) {
			mean /= w;

			// compute weighted variance over all samples
			double var = 0;
			for(i=0; i<run; ++i) {
				double x = s[src+i].cost - mean;
				var += s[src+i].weight * x*x;
			}
			var /= w;

			double std = sqrt(var);
			double max_cost = mean + 3*std;

			mean = 0;
			var = 0;
			n = 0;
			w = 0;

			// compute weighted mean for samples within reasonable range
			for(i=0; i<run; ++i) {
				if (s[src+i].cost < max_cost) {
					n += 1;
					mean += s[src+i].weight * s[src+i].cost;
					w += s[src+i].weight;
				}
			}
			
			mean /= w;
			// compute weighted variance for samples within reasonable range
			for(i=0; i<run; ++i) {
				if (s[src+i].cost < max_cost) {
					double x = s[src+i].cost - mean;
					var += s[src+i].weight * x*x;
				}
			}
			var /= w;

			s[dst] = s[src];
			s[dst].samples = n;
			s[dst].samples_raw = run;
			s[dst].cost = mean;
			s[dst].cost_var = var;
			dst += 1;

			mean_var += var;
			mean_var_n += 1;
		} else {
			// unique sample. can't compute variance
			s[dst] = s[src];
			s[dst].samples = 1;
			s[dst].samples_raw = 1;
			s[dst].cost_var = 0.0001;
			s[dst].weight = 1.0;
			dst += 1;
		}

		src += run;
	}

	if (mean_var_n > 0) {
		// assume variance of unique sequences is mean of all variances
		mean_var /= mean_var_n;
		for(i=0; i<dst; ++i) {
			if (s[i].samples==1)
				s[i].cost_var = mean_var;
		}
	}

	return dst;
}

size_t remove_neg_cost(KSeq *s, size_t count)
{
	size_t dst=0, src=0;
	while (src < count) {
		if (s[src].cost >= 0) {
			if (dst != src)
				s[dst] = s[src];
			dst += 1;
		}
		src += 1;
	}
	return dst;
}

/* put one sequence:word pair into database
for later fetching a list of words that contain that sequence */
static void db_put_word_seq(const char seq[], int seq_bytes, const char word[], int word_bytes, int64_t word_id)
{
	assert(seq_bytes > 0);
	assert(word_bytes > 0);

	sqlite3_stmt *s = st_assoc_seq_word;

	// seq --> word
	sq3_bind_text(s, 1, seq, seq_bytes);
	sq3_bind_int64(s, 2, word_id);
	sq3_step(s, SQLITE_DONE);
	sqlite3_reset(s);
}

static int64_t db_put_word_1(const char word[], int word_bytes)
{
	assert(word_bytes > 0);
	sqlite3_stmt *s = st_put_word;
	sq3_bind_text(s, 1, word, word_bytes);
	const int e = sqlite3_step(s);
	sqlite3_reset(s);

	if (e == SQLITE_DONE) 
		return ++words_table_rows;
	else
		return -1; // word already exists in database
}

static void put_seq_hist(const char word[], int word_bytes)
{
	sqlite3_stmt *s = st_insert_hist;
	sq3_bind_zeroblob(s, 1, sizeof(struct KSeqHist));
	sq3_bind_int(s, 2, 0);//samples
	sq3_bind_double(s, 3, 0);//delay_mean
	sq3_bind_double(s, 4, 0);//delay_stdev
	sq3_bind_double(s, 5, 0);//typo_mean
	sq3_bind_double(s, 6, 0);//cost_func
	sq3_bind_text(s, 7, word, word_bytes);//seq
	sq3_step(s, SQLITE_DONE);
	sqlite3_reset(s);
}

int db_put_word(const char word[], int word_bytes)
{
	assert(word_bytes > 0);

	int64_t word_id = db_put_word_1(word, word_bytes);
	if (word_id < 0) {
		// word already present in database
		return 0;
	}

	put_seq_hist(word, word_bytes);

	const char *mbc_begin[WORD_MAX];
	int mbc_bytes[WORD_MAX];
	int word_len=0;
	int bytes_left=word_bytes;

	const char *it = word;
	while (bytes_left > 0) {
		// n: number of bytes of the first UTF8 character in (it)
		int n = u8_mblen((const uint8_t*) it, bytes_left);
		if (n <= 0) break;
		mbc_begin[word_len] = it;
		mbc_bytes[word_len] = n;
		it += n;
		word_len += 1;
		bytes_left -= n;
	}

	for(int a=0; a<word_len; ++a) {
		const char *seq_begin = mbc_begin[a];
		int stop = a + MAX_SEQ;
		if (stop > word_len) stop = word_len;
		for(int b=a; b<stop; ++b) {
			const char *seq_end = mbc_begin[b] + mbc_bytes[b];
			int seq_bytes = seq_end - seq_begin;
			db_put_word_seq(seq_begin, seq_bytes, word, word_bytes, word_id);
		}
	}

	return 1;
}

int db_get_words(const char seq[], int seq_bytes, Word32 words[], int limit)
{
	assert(seq_bytes>0);
	assert(limit>0);
	assert(seq!=NULL);
	assert(words!=NULL);

	sqlite3_stmt *s = st_get_words;
	int count=0;

	sq3_bind_text(s, 1, seq, seq_bytes);
	sq3_bind_int(s, 2, limit);

	while(count < limit && sqlite3_step(s)==SQLITE_ROW) {
		const char *word = (const char*) sqlite3_column_text(s, 0);
		int len = sqlite3_column_bytes(s, 0);
		assert(word != NULL);
		words[count] = utf8_to_word(word, len);
		count += 1;
	}

	sqlite3_reset(s);
	return count;
}

int db_get_words_random(Word32 words[], int limit)
{
	sqlite3_stmt *s = st_get_words_r;
	int e;
	int count=0;

	sq3_bind_int(s, 1, limit);

	while(count < limit && (e=sqlite3_step(s))==SQLITE_ROW) {
		const char *word = (const char*) sqlite3_column_text(s, 0);
		int len = sqlite3_column_bytes(s, 0);
		words[count] = utf8_to_word(word, len);
		count += 1;
	}

	sqlite3_reset(s);
	return count;
}

void db_defrag()
{
	if (sqlite3_exec(db, sql_cleanup_code, NULL, NULL, NULL) != SQLITE_OK)
		db_fail("db_defrag");
}

static int question_marks(char buf[], int min_count)
{
	int i;
	for(i=0; i<min_count; i+=4) {
		char *p = buf + 2*i;
		p[0] = '?'; p[1] = ',';
		p[2] = '?'; p[3] = ',';
		p[4] = '?'; p[5] = ',';
		p[6] = '?'; p[7] = ',';
	}
	i = 2*min_count - 1;
	return i;
}

static const char* make_query_q1(int query_len[1], size_t num_seqs)
{
	#define MAX_SPAMBOX_SEQ SUBSTR_COUNT(SPAMBOX_BUFLEN,MAX_SEQ)
	#define Q1A "SELECT rowid,hist,seq FROM seq_hist WHERE seq IN ("
	#define Q1A_LEN (sizeof Q1A - 1)
	#define Q1B ") ORDER BY seq"
	#define Q1B_LEN (sizeof Q1B - 1)
	static char query[sizeof Q1A + MAX_SPAMBOX_SEQ*2 + sizeof Q1B + 50];

	// generate query string
	memcpy(query, Q1A, Q1A_LEN);
	int q = question_marks(query + Q1A_LEN, num_seqs);
	memcpy(query + Q1A_LEN + q, Q1B, Q1B_LEN);
	*query_len = Q1A_LEN + Q1B_LEN + q;
	query[*query_len]='\0';

	return query;
}

typedef struct SeqSample {
	uint8_t *s;
	const uint32_t *src;
	int s_len; // how many bytes in s (utf8)
	int src_len; // sequence length
	int16_t delay;
} SeqSample;

static int cmp_seq_sample(const SeqSample *a, const SeqSample *b) {
	return u8_cmp2(a->s, a->s_len, b->s, b->s_len);
}

static int scan_seq(
	int num_ch,
	const uint32_t ch[SPAMBOX_BUFLEN],
	const int16_t delay_ms[SPAMBOX_BUFLEN],
	SeqSample seq[MAX_SPAMBOX_SEQ],
	SeqSample *uniq[MAX_SPAMBOX_SEQ],
	int num_uniq[1])
{
	size_t num_samples=0;

	for(int a=0; a<num_ch; ++a) {
		int32_t delay=0;
		int stop=a+MAX_SEQ;
		if (stop > num_ch) stop = num_ch;

		for(int b=a; b<stop; ++b) {
			if (delay_ms[b] == 0) {
				continue; // ??
			}
			if (uc_is_c_whitespace(ch[b])) {
				// don't include sequences with whitespace in them
				break;
			}

			SeqSample *se = seq + num_samples;
			num_samples += 1;
			assert(num_samples <= MAX_SPAMBOX_SEQ);

			if (delay >= 0 && delay_ms[b] > 0) {
				// accumulate delay
				delay += delay_ms[b];
			} else {
				// if sequence includes one mistake (<0) stop accumulating
				// because all longer sequences will also be invalid
				delay = -1;
			}

			size_t l=0;
			se->delay = delay;
			se->src = ch + a;
			se->s = u32_to_u8(ch + a, b - a + 1, NULL, &l);
			se->s_len = l;
			se->src_len = b - a + 1;
		}
	}

	if (!num_samples) {
		*num_uniq=0;
		return 0;
	}

	qsort(seq, num_samples, sizeof seq[0], (QSortCmp) cmp_seq_sample);
	*num_uniq=1;
	uniq[0] = seq;
	assert(num_samples <= MAX_SPAMBOX_SEQ);
	
	// collect pointers to start of each batch of each sequence
	for(size_t i=1; i<num_samples; ++i) {
		if (cmp_seq_sample(seq+i-1, seq+i)) {
			uniq[(*num_uniq)++] = seq+i;
			assert(*num_uniq <= MAX_SPAMBOX_SEQ);
		}
	}

	return num_samples;
}

static void update_seq_history(KSeqHist hi[], int num_uniq, SeqSample seq[], int num_seq)
{
	FILE *fp;
#ifndef NDEBUG
	fp = fopen("/tmp/kseqhist.txt", "w");
#endif

	KSeqHist *hi_end = hi + num_uniq;
	int se=0;
	for(; hi<hi_end; ++hi) {
		SeqSample *first = seq + se;
		do {
			if (fp) {
				fprintf(fp, "%.*s %d %d\n",
						seq[se].s_len, (char*) seq[se].s,
						seq[se].src_len, (int) seq[se].delay);
			}
			kseq_hist_push(hi, seq[se].delay);
			se += 1;
			if (se >= num_seq)
				return;
		} while(cmp_seq_sample(first, seq+se) == 0);
		if (fp)
			fprintf(fp, "\n");
	}
	if (fp) fclose(fp);
}

void db_put_seq_samples(
	size_t num_ch,
	const uint32_t ch[SPAMBOX_BUFLEN],
	const int16_t delay_ms[SPAMBOX_BUFLEN] )
{
	SeqSample seq[MAX_SPAMBOX_SEQ], *uniq[MAX_SPAMBOX_SEQ];
	KSeqHist hist[MAX_SPAMBOX_SEQ];
	int64_t row_id[MAX_SPAMBOX_SEQ];
	sqlite3_stmt *st;
	int e, num_uniq, num_seq;

	// break text into sequences, sort, count how many unique
	num_seq = scan_seq(num_ch, ch, delay_ms, seq, uniq, &num_uniq);

	if (!num_seq || !num_uniq)
		return;

	// be sure all sequences exist in database (even if all zeros)
	// otherwise SELECT returns unexpected amount of rows
	assert(!in_transaction);
	db_trans_begin();
	for(int i=0; i<num_uniq; ++i) {
		put_seq_hist((char*)(uniq[i]->s), uniq[i]->s_len);
	}
	db_trans_end();

	// build SELECT query
	const char *query;
	int query_len;
	if (num_uniq > 999) {
		fail("sql query can't have >999 host parameters but now we have %d sequences to submit", num_uniq);
	}
	query = make_query_q1(&query_len, num_uniq);

	st = NULL;
	e = sqlite3_prepare_v2(db, query, query_len, &st, NULL);
	if (e != SQLITE_OK) {
		db_fail("line " STRTOK(__LINE__) " prepare: Query:\n%.*s\n",
			query_len, query);
	}

	for(int i=0; i<num_uniq; ++i) {
		sq3_bind_text(st, 1+i, (char*)(uniq[i]->s), uniq[i]->s_len);
	}

	// get rows
	int row_nr=0;
	while((e=sqlite3_step(st))==SQLITE_ROW) {
		row_id[row_nr] = sqlite3_column_int64(st, 0);

		const uint8_t *s = sqlite3_column_text(st, 2);
		size_t s_bytes = sqlite3_column_bytes(st, 2);
		if (u8_cmp2(s, s_bytes, uniq[row_nr]->s, uniq[row_nr]->s_len))
			fail("KSeqHist sequence mismatch: %.*s vs %.*s",
					(int) s_bytes, s,
					uniq[row_nr]->s_len, uniq[row_nr]->s);

		const void *blob = sqlite3_column_blob(st, 1);
		int hist_ok = 0;

		if (blob) {
			size_t blob_sz = sqlite3_column_bytes(st, 1);
			if (blob_sz != sizeof *hist)
				fail("KSeqHist size mismatch");
			if (kseq_hist_validate(blob)) {
				memcpy(hist+row_nr, blob, sizeof *hist);
				hist_ok = 1;
			}
		}

		if (!hist_ok)
			memset(hist+row_nr, 0, sizeof *hist);

		debug_msg("fetch %4.*s: samples=%d\n",
			(int) s_bytes, s, (int) hist[row_nr].samples);

		row_nr += 1;
	}
	if (e != SQLITE_DONE) db_fail("fetch seq_hist not done");
	if (row_nr != num_uniq) db_fail("fetch seq_hist row count mismatch");

	sqlite3_finalize(st);

	// update rows
	update_seq_history(hist, num_uniq, seq, num_seq);

	// insert back to database
	db_trans_begin();
	for(int i=0; i<num_uniq; ++i) {
		KSeqStats stat = kseq_hist_stats(hist+i);

		debug_msg(
			"%4.*s: samples=%d delay=%.2f stdev=%.2f typos=%.3f cost=%.3g\n",
			uniq[i]->s_len, uniq[i]->s, hist[i].samples,
			stat.delay_mean, stat.delay_stdev, stat.typo_mean, stat.cost_func);

		st = st_update_hist;

		sq3_bind_blob(st, 1, hist+i, sizeof *hist);
		sq3_bind_int(st, 2, hist[i].samples);
		sq3_bind_double(st, 3, stat.delay_mean);
		sq3_bind_double(st, 4, stat.delay_stdev);
		sq3_bind_double(st, 5, stat.typo_mean);
		sq3_bind_double(st, 6, stat.cost_func);
		sq3_bind_int64(st, 7, row_id[i]);

		sq3_step(st, SQLITE_DONE);
		sqlite3_reset(st_update_hist);
	}
	db_trans_end();
}

