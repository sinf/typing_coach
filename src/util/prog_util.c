#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "prog_util.h"
#include "database.h"
#include "sz_mult.h"
#include "persist.h"

const char *iso639_lang = "en";

void *Realloc(void *p, size_t n, size_t s, size_t z)
{
	size_t x = sz_mult(n, s, z);
	p = realloc(p, x);
	if (!p) fail_oom();
	return p;
}

void endwin_if_needed()
{
	if (need_endwin) endwin();
	need_endwin=0;
}

void cleanup()
{
	endwin_if_needed();
	db_close();
	save_settings();
}

void fail(const char *msg1, ...)
{
	va_list ap;
	va_start(ap, msg1);
	int e = errno;
	cleanup();
	const char *es = strerror(e);
	fprintf(stderr, "Error: %d, %s\n", e, es);
	vfprintf(stderr, msg1, ap);
	va_end(ap);
	abort();
	exit(1);
}

void quit_msg(int e, const char *msg1, ...)
{
	va_list ap;
	va_start(ap, msg1);
	cleanup();
	if (e) {
		const char *es = strerror(e);
		fprintf(stderr, "errno=%d: %s\n", e, es);
	}
	vfprintf(stderr, msg1, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}

void quit()
{
	cleanup();
	exit(1);
}

void quit_int(int sig)
{
	cleanup();
	printf("Caught signal %d\n", sig);
	exit(1);
}

int ask_yesno(const char *fmt, ...)
{
	int c, ret=0;
	for(;;) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
		printf(" y/N: ");
		fflush(stdout);
		c = fgetc(stdin);
		if (c == 'Y' || c == 'y') {
			ret = 1;
			break;
		}
		if (c == 'N' || c == 'n')
			break;
	}
	do { c = fgetc(stdin); } while (c != '\n' && c != EOF);
	return ret;
}

