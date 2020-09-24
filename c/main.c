#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <curses.h>
#include "prog_util.h"
#include "wordlist.h"
#include "win.h"

#define EXE_NAME "typing_c"

Wordlist *the_wordlist = NULL;
char *wordlist_path = "./wordlist";
char *database_path = "./keystrokes.db";

void parse_args(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "hd:w:")) != -1) {
		switch(c) {
			case 'd': database_path = optarg; break;
			case 'w': wordlist_path = optarg; break;
			case 'h':
				puts(
"Usage: " EXE_NAME " [-d FILE] [-w FILE]\n\n"
"Arguments\n"
"  -d set sqlite3 database file for keystrokes\n"
"  -w set wordlist file (utf8, one word per line\n");
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

	printf("Initialized\n");
	cu_setup();
	get_more_words();

	do {
		my_repaint();
	} while(check_input());

	quit();
	return 0;
}

