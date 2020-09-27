#pragma once
#ifndef _PROG_UTIL_H
#define _PROG_UTIL_H

extern int need_endwin;

#define fail_oom() fail("allocation failed")

void *Realloc(void *, size_t n, size_t s, size_t z);

__attribute__((format(printf,1,2)))
void fail(const char *msg1, ...);

void cleanup();
void quit();
void quit_int(int i);

struct Wordlist;
extern struct Wordlist *the_wordlist;
extern char *wordlist_path;
extern char *database_path;

#endif

