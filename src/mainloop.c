#include <stdlib.h>
#include <stdint.h>
#include "prog_util.h"
#include "dpy.h"
#include "database.h"
#include "tm.h"
#include "menu.h"
#include "mainloop.h"
#include "spambox.h"

void show_slow_seq()
{
	KSeq *seqs;
	const size_t limit = 20;
	size_t i,
		n = db_get_sequences(10000, 1, MAX_SEQ, &seqs),
		n1 = n<limit ? n : limit;
	
	KSeqHist hist[limit];
	db_get_sequences_hist(n1, seqs, hist);

	dpy_begin();
	dpy_print(0, C_STATUS, "Press any key to close");
	dpy_print(1, C_STATUS, "Top %d slowest sequences", (int) n1);

	dpy_print(2, C_STATUS, "%10.10s %10.10s %8.8s %8.8s %8.8s %7.7s %s",
			"sequence", "cost", "samples",
			"ms", "ms/stdev", "typo%",
			"| buffer");

	for(i=0; i<n1; ++i) {
		KSeq s = seqs[i];
		KSeqHist h = hist[i];
		KSeqStats st = kseq_hist_stats(&h);
		int d[8];

		for(unsigned j=0; j<8; ++j) {
			d[j] = h.delay_ms[(h.start_pos + j) % KSEQ_HIST];
		}

		dpy_print(3+i, C_NORMAL,
				"%10.*ls %10.2g %8d %8.0f %8.3f %7.3f"
				" | %d %d %d %d %d %d %d %d"
				"\n",
				s.len, s.s, s.cost, s.samples,
				st.delay_mean, st.delay_stdev, st.typo_mean*100.0,
				d[0], d[1], d[2], d[3], d[5], d[6], d[7], d[8]
			);
	}

	dpy_refresh();

	read_key();
}

static void test_pick_words(const char *seq, const int seq_b)
{
	const int limit = 20;
	Word w[limit];
	char buf[512];

	dpy_begin();
	dpy_print(0, C_STATUS, "Press any key to close");
	dpy_print(1, C_STATUS, "Sequence bytes: %d\n", seq_b);
	dpy_print(2, C_STATUS, "Sequence: %.*s\n", seq_b, seq);

	const int n = db_get_words(seq, seq_b, w, limit);

	dpy_print(3, C_STATUS, "Words that contain this sequence:\n");
	for(int i=0; i<n; ++i) {
		int k = word_to_utf8(w+i, buf, sizeof buf);
		buf[sizeof(buf)-1] = '\0';
		dpy_print(4+i, C_NORMAL, "%.*s\n", k, buf);
	}

	dpy_refresh();
	read_key();
}
static void query_seq_words()
{
	char buf[256];
	int n = read_input("Sequence to search", buf, sizeof buf);
	if (n > 0)
		test_pick_words(buf, n);
}

void main_menu()
{
	enum {
		M_TM1=100,
		M_TM2,
		M_SLOW_SEQ,
		M_QUERY_SEQ,
		M_TODO,
		M_EXIT,
	};
	static const MenuEntry m[] = {
		M_BUTTON("Training mode 1: slow sequences", M_TM1),
		M_BUTTON("...", M_TODO),
		M_BUTTON("...", M_TODO),
		M_TOGGLE("Automatic spacebar", &opt_auto_space),
		M_TOGGLE("Advance despite typo", &sb_continue_on_typo),
		M_BUTTON("Show slowest sequences", M_SLOW_SEQ),
		M_BUTTON("Find words with sequence", M_QUERY_SEQ),
		M_BUTTON("Exit program", M_EXIT),
	};
	static int sel=0;
	int done = 0;

	do {
		int i = show_menu("Main menu", m, sizeof m / sizeof m[0], &sel);
		switch(i) {
			case M_EXIT:
				done = 1;
				break;
			case M_TM1:
				tm_words = tm1_words;
				tm_info = tm1_info;
				training_session();
				break;
			case M_TM2:
				break;
			case M_SLOW_SEQ:
				show_slow_seq();
				break;
			case M_QUERY_SEQ:
				query_seq_words();
				break;
			default:
				break;
		}
	} while(!done);
}

