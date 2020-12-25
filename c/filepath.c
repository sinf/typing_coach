#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "prog_util.h"
#include "filepath.h"

static Filepath data_dir;
static struct stat data_dir_s;

#define DATADIR_BASENAME "typingc"

static void check_d()
{
	if (!(data_dir_s.st_mode & S_IFDIR))
		quit_msg(0, "expected a directory: %s", data_dir);
}

void find_config_dir()
{
	const char *attempts[][2] = {
		{"TYPINGC_DATA_DIR", ""},
		{"XDG_DATA_HOME", "/" DATADIR_BASENAME},
		{"HOME", "/.local/share/" DATADIR_BASENAME},
	};

	for(int i=0; i<sizeof attempts / sizeof attempts[0]; ++i) {
		char *p = "";
		if (attempts[i][0]) {
			p = getenv(attempts[i][0]);
			if (!p) continue;
		}
		snprintf(data_dir, sizeof data_dir, "%s%s", p, attempts[i][1]);
		errno=0;
		if (!stat(data_dir, &data_dir_s)) {
			check_d();
			return;
		}
		if (errno == ENOENT) {
			mkdir(data_dir, 0700);
			int e=errno;
			if (stat(data_dir, &data_dir_s))
				quit_msg(e, "failed to create data directory: %s", data_dir);
			check_d();
			return;
		}
	}

	quit_msg(0, "Program data directory not found. Set any of $XDG_DATA_HOME, $HOME or $TYPINGC_DATA_DIR");
}

void get_path(Filepath p, const char *fmt, ...)
{
	va_list a;
	int end;

	va_start(a, fmt);

	strncpy(p, data_dir, sizeof(Filepath) - 2);
	end = strlen(p);

	p[end] = '/';
	end += 1;

	end += vsnprintf(p+end, sizeof(Filepath) - end - 1, fmt, a);
	p[end] = '\0';

	va_end(a);
}


