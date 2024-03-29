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
#include <dirent.h>
#include "debug.h"
#include "prog_util.h"
#include "database.h"
#include "wordlist.h"
#include "dpy.h"
#include "mainloop.h"
#include "persist.h"

#define EXE_NAME "typing_c"
static int quit_flag = 0;
static const char *explicit_db_name = NULL;

static void ask_create()
{
	if (!ask_yesno("Database named %s does not exist, create?", explicit_db_name))
		quit_msg(0, "Canceled");
}

static void set_db_path(const char *name)
{
	static Filepath fp="";
	get_path(fp, "%s.db", name);
	database_path = fp;
}

static void delete_db(const char *name)
{
	Filepath path;
	get_path(path, "%s.db", name);
	if (ask_yesno("Delete %s (%s)?", name, path)) {
		if (remove(path)) {
			printf("Error: %s\n", strerror(errno));
		} else {
			if (!strcmp(database_path, path)) {
				set_db_path("default");
			}
			get_path(path, "%s.db-journal", name);
			remove(path);
		}
	}
}

void list_dbs()
{
	Filepath path;
	int count=0;
	get_path(path, "%c", '\0');
	DIR *dp = opendir(path);
	if (dp) {
		struct dirent *de;
		while((de=readdir(dp))) {
			const char *fn = de->d_name;
			int l = strlen(de->d_name);
			if (l > 3 && !strcmp(fn+l-3, ".db")) {
				get_path(path, "%s", de->d_name);
				printf("%-20.*s %s\n", l-3, de->d_name, path);
				count+=1;
			}
		}
	}
	if (!count) {
		printf("(none)\n");
	}
}

void parse_args(int argc, char **argv)
{
	int d_flag=0;
	struct stat s;
	int c;
	errno=0;
	while((c = getopt(argc, argv, "hlc:d:w:x:")) != -1) {
		switch(c) {
			case 'l':
				list_dbs();
				exit(0);
				break;

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
				quit_flag = 1;
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

			case 'x':
				delete_db(optarg);
				quit_flag = 1;
				break;

			case 'h':
				puts(
"\nTyping coach\n"
"git hash: " GIT_REF_STR "\n\n"
"Usage: " EXE_NAME " [-d/-c NAME] [-w FILENAME]\n\n"
"Optional arguments\n"
"  -h show this help text\n"
"  -l list databases you have\n"
"  -c NAME\n"
"     create sqlite3 database (and optionally import wordlists) and quit\n"
"  -d NAME\n"
"     specify the main sqlite3 database name (must exist)\n"
"  -w FILENAME\n"
"     merge wordlist to database from a file (utf8, one word per line)\n"
"\n"
);
				exit(1);
			default: break;
		}
	}
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "en_US.UTF-8");

	// find files
	find_config_dir();
	set_db_path("default");
	get_path(the_settings_path, "settings.ini");

	load_settings();

	// first need settings for the log filename
	debug_output_init();

	// maybe override settings
	parse_args(argc, argv);
	save_settings();
	db_open();

	if (quit_flag) {
		printf("Defragmenting database...\n");
		db_defrag();
		quit();
	}

	if (db_total_word_count() == 0) {
		printf("No words in database: %s\n", database_path);
		printf("Import some first ([-c/-d ...] -w ...) or select a different database (-d ...)\n");
		quit();
	}

	printf("Initialized\n");
	cu_setup();
	signal(SIGINT, quit_int);
	main_menu();
	cleanup();
	return 0;
}

