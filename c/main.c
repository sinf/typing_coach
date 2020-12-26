#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "debug.h"
#include "prog_util.h"
#include "database.h"
#include "wordlist.h"
#include "dpy.h"
#include "mainloop.h"
#include "persist.h"

static int quit_flag = 0;
static const char *explicit_db_name = NULL;

static void ask_create()
{
	for(;;) {
		printf("Database named %s does not exist, create? Y/n: ",
			explicit_db_name);
		fflush(stdout);
		char c = fgetc(stdin);
		if (c == 'Y' || c == 'y')
			break;
		quit_msg(0, "Canceled");
	}
}

static void set_db_path(const char *name)
{
	static Filepath fp="";
	get_path(fp, "%s.db", name);
	database_path = fp;
}

void parse_args(int argc, char **argv)
{
	int d_flag=0;
	struct stat s;
	int c;
	errno=0;
	while((c = getopt(argc, argv, "hc:d:w:")) != -1) {
		switch(c) {
			case 'c':
				if (d_flag) {
					quit_msg(0, "Can only specify once -d or -c");
				}
				explicit_db_name = optarg;
				d_flag=1;
				set_db_path(optarg);
				if (stat(database_path, &s) && errno == ENOENT) {
					ask_create();
					printf("Creating database: %s\n", database_path);
				} else {
					printf("Using database: %s\n", database_path);
				}
				db_open();
				break;

			case 'd':
				if (d_flag) {
					quit_msg(0, "Can only specify once -d or -c");
				}
				set_db_path(optarg);
				if (stat(database_path, &s)) {
					quit_msg(errno, "Failed to access database file");
				}
				d_flag=1;
				db_open();
				break;

			case 'w':
				if (!d_flag) {
					quit_msg(0, "Error: database not specified (-d NAME)");
				}
				printf("Merging wordlist.. %s\n", optarg);
				read_wordlist(optarg);
				quit_flag = 1;
				break;

			case 'h':
				puts(
"\nTyping coach\n"
"git hash: " GIT_REF_STR "\n\n"
"Usage: " EXE_NAME " [-d/-c NAME] [-w FILENAME]\n\n"
"Arguments\n"
"  -h show this help text\n"
"  -c NAME\n"
"     create the main sqlite3 database name\n"
"  -d NAME\n"
"     specify the main sqlite3 database name (must exist)\n"
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
	setlocale(LC_ALL, "en_US.UTF-8");
	find_config_dir();
	debug_output_init();
	set_db_path("default");
	get_path(the_settings_path, "settings.ini");
	load_settings();

	parse_args(argc, argv);
	save_settings();
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

