#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <curses.h>
#include "prog_util.h"
#include "timing.h"
#include "wordlist.h"
#include "win.h"

#define EXE_NAME "typing_c"

Wordlist *the_wordlist = NULL;
char *wordlist_path = "./wordlist";
static int test_quit = 0;
static int test_seq_ = 0;

void parse_args(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "qshd:w:")) != -1) {
		switch(c) {
			case 'd': database_path = optarg; break;
			case 'w': wordlist_path = optarg; break;
			case 'q': test_quit = 1; break;
			case 's': test_seq_ = 1; break;
			case 'h':
				puts(
"Usage: " EXE_NAME " [-d FILE] [-w FILE]\n\n"
"Arguments\n"
"  -h show this help text\n"
"  -d set sqlite3 database file for keystrokes\n"
"  -w set wordlist file (utf8, one word per line)\n"
"  -q open/create database and quit\n"
"  -s show slowest sequences and quit\n");
				exit(1);
			default: break;
		}
	}
}

int main(int argc, char **argv)
{
	//setlocale(LC_ALL, "en_US.UTF-8");
	parse_args(argc, argv);
	the_wordlist = read_wordlist(the_wordlist, "./wordlist");
	db_open();

	printf("Initialized\n");

	if (test_quit)
		quit();
	
	if (test_seq_) {
		KSeq *seqs;
		size_t n = db_get_sequences(10000, 1, MAX_SEQ, &seqs),
			n1 = n<20 ? n : 20, i;
		printf("Top %d slowest sequences\n", (int) n1);
		for(i=0; i<n1; ++i) {
			KSeq s = seqs[i];
			printf("%8.*ls  cost=%.3g samples=%d\n", s.len, s.s, s.cost, s.samples);
		}
		quit();
	}

	cu_setup();
	signal(SIGINT, quit_int);
	get_more_words();

	do {
		my_repaint();
	} while(check_input());

	cleanup();
	return 0;
}

