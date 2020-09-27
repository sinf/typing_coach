#include <time.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#include <sqlite3.h>
#include "timing.h"
#include "prog_util.h"

static sqlite3 *db = 0;
char *database_path = "./keystrokes.db";
static sqlite3_stmt *st_put_key = 0;
static sqlite3_stmt *st_get_recent = 0;
long the_typing_counter = 0;

static const char
sql_create[] = 
"CREATE TABLE IF NOT EXISTS keystrokes"
"( sequence integer primary key,"
" pressed integer," // keycode
" expected integer," // keycode
" delay_ms integer,"
" timestamp integer );",
sql_put_key[] = "INSERT INTO keystrokes (pressed,expected,delay_ms,timestamp) VALUES (?,?,?,?);",
sql_count_rows[] = "SELECT COUNT(*) FROM keystrokes;",
sql_get_recent[] =
"SELECT sequence, pressed, expected, delay_ms, timestamp"
" FROM keystrokes"
" WHERE (delay_ms < 10000)"
" ORDER BY sequence ASC LIMIT ?";

void db_fail()
{
	fail("Database error!\nFile: %s\nError: %s\n", database_path, sqlite3_errmsg(db));
}

static int save_typing_counter(void *p, int num_cols, char *data[], char *colnames[])
{
	the_typing_counter = strtol(data[0], NULL, 10);
	return 0;
}

void db_open()
{
	int e, ok=SQLITE_OK;
	e=sqlite3_open(database_path, &db);
	if (e != ok) db_fail();
	e=sqlite3_exec(db, sql_create, NULL, NULL, NULL);
	if (e != ok) db_fail();
	e=sqlite3_prepare_v2(db, sql_put_key, sizeof sql_put_key, &st_put_key, NULL);
	if (e != ok) db_fail();
	e=sqlite3_prepare_v2(db, sql_get_recent, sizeof sql_get_recent, &st_get_recent, NULL);
	if (e != ok) db_fail();
	sqlite3_exec(db, sql_count_rows,
		save_typing_counter, NULL, NULL);
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

void db_put(wchar_t pressed, wchar_t expected, int delay_ms)
{
	sqlite3_stmt *s = st_put_key;
	int e, ok=SQLITE_OK;
	e = sqlite3_bind_int(s, 1, pressed);
	if (e != ok) db_fail();
	e = sqlite3_bind_int(s, 2, expected);
	if (e != ok) db_fail();
	e = sqlite3_bind_int(s, 3, delay_ms);
	if (e != ok) db_fail();
	e = sqlite3_bind_int64(s, 4, time(0));
	if (e != ok) db_fail();
	e = sqlite3_step(s);
	if (e != SQLITE_DONE) db_fail();
	sqlite3_reset(s);
	the_typing_counter += 1;
}

double calc_cost(int len, int delay[], int mist[], int age[])
{
	double cost=0;
	for(int i=0; i<len; ++i)
	{
		// decay relevance with age
		double a=(.1 + .9*pow(.999, age[i]+1));
		cost = cost + a * (
			+ delay[i]*0.01
			+ mist[i]*2.0
		);
	}
	return cost / len;
}

int cmp_seq_cost(const KSeq *a, const KSeq *b)
{
	if (a->len == b->len) {
		if (a->cost == b->cost) {
			// less samples first
			if (a->samples != b->samples)
				return a->samples < b->samples ? -1 : 1;
			return 0;
		}
		// more expensive first
		return a->cost < b->cost ? 1 : -1;
	}
	// longer first
	return b->len - a->len;
}

size_t db_get_sequences(int num_ch, int sl_min, int sl_max, KSeq *out[1])
{
	if (sl_min > MAX_SEQ || sl_max > MAX_SEQ) {
		fail("%s:%s: too long sequence length (%d, %d)\n",
			__FILE__, __func__, sl_min, sl_max);
	}
	sqlite3_stmt *s = st_get_recent;
	int e, len=0, prev_seq=-2;
	wchar_t keys[MAX_SEQ+1] = {0};
	int k_age[MAX_SEQ+1];
	int delay[MAX_SEQ+1];
	int mist[MAX_SEQ+1];
	int mist_acc = 0;
	int64_t prev_age=0, age=0, now=time(0);
	size_t n_seqs = 0;
	size_t out_alloc = 20000, grow = 20000;
	*out = Realloc(NULL,out_alloc,sizeof *out,0);

	e = sqlite3_bind_int(s, 1, num_ch);
	if (e != SQLITE_OK) db_fail();

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

		if (dif != 1 || iswspace(k0) || age_dif>60) {
			// sequence break
			len = 0;
			mist_acc = 0;
		}

		if (iswspace(k0)) {
			continue;
		}

		if (k0 == k1) {
			// correct keypress
			int i;
			const int N=MAX_SEQ;
			if (len == N) {
				memmove(keys, keys+1, N*sizeof *keys);
				memmove(delay, delay+1, N*sizeof *delay);
				memmove(mist, mist+1, N*sizeof *mist);
				memmove(k_age, k_age+1, N*sizeof *k_age);
				i = N - 1;
			} else {
				i = len;
				len += 1;
			}
			keys[i] = k0;
			delay[i] = kd;
			mist[i] = mist_acc;
			k_age[i] = age > INT_MAX ? INT_MAX : age;
			mist_acc = 0;

			// process sequences
			for(int j=0; j<len; j++) {
				int off = len-1-j;
				int sl = j+1;
				if (sl>=sl_min && sl<=sl_max) {
					if (n_seqs == out_alloc) {
						out_alloc += grow;
						*out=Realloc(*out,out_alloc,sizeof *out,0);
					}

					KSeq *se = *out + n_seqs;
					memcpy(se->s, keys+off, sl*sizeof *keys);
					se->s[sl] = 0;
					se->len = sl;
					se->samples = 1;
					se->cost = calc_cost(sl,
						delay+off,
						mist+off,
						k_age+off
						);
					n_seqs += 1;
				}
			}
		} else {
			// incorrect keypress
			mist_acc += 1;
		}
		prev_seq = seq;
	}
	if (e != SQLITE_DONE) db_fail();
	sqlite3_reset(s);

	n_seqs = remove_duplicate_sequences(*out, n_seqs);
	qsort(*out, n_seqs, sizeof **out, (CmpFunc) cmp_seq_cost);

	return n_seqs;
}

size_t remove_duplicate_sequences(KSeq *s, size_t count)
{
	size_t dst=0, src=1, i;
	if (count<2) return count;
	qsort(s, count, sizeof *s, (CmpFunc) wcscmp);
	while (src < count) {
		if (wcscasecmp(s[src].s, s[dst].s)) {
			dst += 1;
			if (dst != src)
				s[dst] = s[src];
		} else {
			s[dst].samples += 1;
			s[dst].cost += s[src].cost;
		}
		src += 1;
	}
	count = dst + 1;
	for(i=0; i<count; ++i) {
		s[i].cost /= s[i].samples;
	}
	return count;
}

