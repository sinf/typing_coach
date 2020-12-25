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

void find_config_dir()
{
	const char *attempts[][2] = {
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
		if (stat(data_dir, &data_dir_s)) {
			if (errno == ENOENT) {
				mkdir(data_dir, 0700);
			} else {
				fail("failed to stat data directory: %s", data_dir);
			}
		}
		break;
	}
	if (stat(data_dir, &data_dir_s)) {
		fail("failed to stat data directory: %s", data_dir);
	}
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


