#include <stdlib.h>
#include <stdint.h>
#include "prog_util.h"
#include "dpy.h"
#include "database.h"
#include "tm.h"
#include "menu.h"
#include "mainloop.h"

void show_slow_seq()
{
	KSeq *seqs;
	size_t i,
		n = db_get_sequences(10000, 1, MAX_SEQ, &seqs),
		n1 = n<20 ? n : 20;

	dpy_begin();
	dpy_print(0, C_STATUS, "Press any key to close");
	dpy_print(1, C_STATUS, "Top %d slowest sequences", (int) n1);

	for(i=0; i<n1; ++i) {
		KSeq s = seqs[i];
		dpy_print(2+i, C_NORMAL, "%8.*ls  cost=%.3g samples=%d\n", s.len, s.s, s.cost, s.samples);
	}

	dpy_refresh();

	read_key();
}

void main_menu()
{
	enum {
		M_TM1=100,
		M_TM2,
		M_SLOW_SEQ,
		M_TODO,
		M_EXIT,
	};
	static const MenuEntry m[] = {
		M_BUTTON("Training mode 1: slow sequences", M_TM1),
		M_BUTTON("...", M_TODO),
		M_BUTTON("...", M_TODO),
		M_TOGGLE("Automatic spacebar", &opt_auto_space),
		M_BUTTON("Show slowest sequences", M_SLOW_SEQ),
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
			default:
				break;
		}
	} while(!done);
}

