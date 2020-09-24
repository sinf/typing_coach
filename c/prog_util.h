#pragma once
#ifndef _PROG_UTIL_H
#define _PROG_UTIL_H

__attribute__((format(printf,1,2)))
void fail(const char *msg1, ...);

void quit();

struct Wordlist;
extern struct Wordlist *the_wordlist;
extern char *wordlist_path;
extern char *database_path;

#endif

