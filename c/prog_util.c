#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "prog_util.h"

void fail(const char *msg1, ...)
{
	va_list ap;
	va_start(ap, msg1);
	endwin();
	int e = errno;
	const char *es = strerror(e);
	fprintf(stderr, "Error: %d, %s\n", e, es);
	vfprintf(stderr, msg1, ap);
	va_end(ap);
	exit(1);
}

void quit()
{
	endwin();
	exit(1);
}

