#ifndef _CPM_COUNTER_H
#define _CPM_COUNTER_H

// called when one character has been typed
void calc_cpm(int ms, int is_correct);

#define CPM_STR_MAX 32
extern char cpm_str[CPM_STR_MAX];
extern char wpm_str[CPM_STR_MAX];

#endif

