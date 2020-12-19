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

void parse_args(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "hc:d:w:")) != -1) {
		switch(c) {
			case 'd':
				database_path = optarg;
				db_open();
				break;

			case 'w':
				if (!database_path) {
					fprintf(stderr, "Error: database not specified (-d FILENAME)");
					exit(1);
				}
				printf("Merging wordlist.. %s\n", optarg);
				read_wordlist(optarg);
				quit_flag = 1;
				break;

			case 'h':
				puts(
"\nTyping coach\n"
"git hash: " GIT_REF_STR "\n\n"
"Usage: " EXE_NAME " [-d FILE] [-w FILE]\n\n"
"Arguments\n"
"  -h show this help text\n"
"  -d FILENAME\n"
"     specify the main sqlite3 database file\n"
"     [~/.local/share/typingc/keystrokes.db]\n"
"  -w FILENAME\n"
"     merge wordlist to database from a file (utf8, one word per line)\n"
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

	if (quit_flag) {
		printf("Defragmenting database...\n");
		db_defrag();

		quit();
	}

	printf("Initialized\n");

	cu_setup();
	signal(SIGINT, quit_int);
	main_menu();
	cleanup();
	return 0;
}

