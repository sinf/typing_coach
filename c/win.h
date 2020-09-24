#pragma once
#ifndef _WIN_H
#define _WIN_H
#include <wchar.h>

struct Word;

#define MAX_WORDS 30

void buf_clear();
void buf_write(int len, const wchar_t *s, struct Word *w);
int add_word(struct Word *w);
void get_more_words();

void cu_setup();
void my_repaint();
int check_input();

void main_loop();

#endif


