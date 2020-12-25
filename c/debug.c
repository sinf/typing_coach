#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

static FILE *debug_file=NULL;

void debug_output_init(void)
{
	const char *path = "/tmp/typingc/debug";
	struct stat s;
	if (!stat(path, &s)) {
		debug_file = fopen(path, "w");
	}
}

void debug_msg_x(const char *file, int line, const char *func, const char *fmt, ...)
{
	if (debug_file) {
		va_list a;
		va_start(a, fmt);
		fprintf(debug_file, "%s:%d:%s: ", file, line, func);
		vfprintf(debug_file, fmt, a);
		va_end(a);
	}
}

