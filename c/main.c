#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "prog_util.h"
#include "database.h"
#include "wordlist.h"
#include "dpy.h"
#include "mainloop.h"

static int quit_flag = 0;

static void test_pick_words(const char *seq)
{
	quit_flag = 1;
	db_open();

	const int limit = 10;
	Word w[limit];
	char buf[512];

	int seq_b = strlen(seq);
	printf("Sequence bytes: %d\n", seq_b);
	printf("Sequence: %*s\n", seq_b, seq);

	const int n = db_get_words(seq, seq_b, w, limit);

	printf("Words that contain it\n");
	for(int i=0; i<n; ++i) {
		int k = word_to_utf8(w+i, buf, sizeof buf);
		buf[sizeof(buf)-1] = '\0';
		printf("> %*s\n", k, buf);
	}
}

void parse_args(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "qhd:w:p:")) != -1) {
		switch(c) {
			case 'd': database_path = optarg; break;
			case 'w':
				db_open();
				printf("Merging wordlist.. %s\n", optarg);
				read_wordlist(optarg);
				break;
			case 'q': quit_flag = 1; break;
			case 'p': test_pick_words(optarg); break;
			case 'h':
				puts(
"\nTyping coach\n"
"git hash: " GIT_REF_STR "\n\n"
"Usage: " EXE_NAME " [-d FILE] [-w FILE]\n\n"
"Arguments\n"
"  -h show this help text\n"
"  -d FILENAME\n"
"     set sqlite3 database file for keystrokes\n"
"     [~/.local/share/typingc/keystrokes.db]\n"
"  -w FILENAME\n"
"     merge wordlist to database from a file (utf8, one word per line)\n"
"  -q quit (after the other options)\n"
"  -p STRING\n"
"     pick words that contain this substring\n"
);
				exit(1);
			default: break;
		}
	}
}

int main(int argc, char **argv)
{
	static char data_dir[1024], db_path[1024];
	struct stat s;
	char *home = getenv("HOME");

	if (home) {
		snprintf(data_dir, sizeof data_dir, "%s/.local/share/typingc", home);
		if (stat(data_dir, &s)) {
			mkdir(data_dir, 0755);
		}
		snprintf(db_path, sizeof db_path, "%s/typing.db", data_dir);
		database_path = db_path;
	}

	setlocale(LC_ALL, "en_US.UTF-8");
	parse_args(argc, argv);
	db_open();

	if (quit_flag)
		quit();

	printf("Initialized\n");

	cu_setup();
	signal(SIGINT, quit_int);
	main_menu();
	cleanup();
	return 0;
}

