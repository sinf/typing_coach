#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

static FILE *debug_file=NULL;
const char *debug_file_path=NULL;

void debug_output_init(void)
{
	if (debug_file_path && debug_file_path[0]) {
		debug_file = fopen(debug_file_path, "w");
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

