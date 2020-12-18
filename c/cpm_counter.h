#ifndef _CPM_COUNTER_H
#define _CPM_COUNTER_H
#include <wchar.h>

// called when one character has been typed
void calc_cpm(int ms, int is_correct);

#define CPM_STR_MAX 32
extern wchar_t cpm_str[CPM_STR_MAX];
extern wchar_t wpm_str[CPM_STR_MAX];

#endif

