#pragma once

extern int need_endwin;
extern const char *iso639_lang;

#define fail_oom() fail("allocation failed")

/* realloc(ptr, n*s+z) */
void *Realloc(void *, size_t n, size_t s, size_t z);

__attribute__((format(printf,1,2)))
void fail(const char *msg1, ...);

__attribute__((format(printf,2,3)))
void quit_msg(int errNo, const char *msg1, ...);

void endwin_if_needed();
void cleanup();
void quit();
void quit_int(int i);

struct Wordlist;
extern struct Wordlist *the_wordlist;

#define MIN(a,b) ((a)>(b) ? (b) : (a))
#define MAX(a,b) ((a)<(b) ? (b) : (a))
#define CLIP(x,lo,hi) MIN(MAX((x),(lo)),(hi))

#define STR(x) #x
#define STRTOK(x) STR(x)

typedef int (*QSortCmp)(const void *a, const void *b);

int ask_yesno(const char *fmt, ...);

