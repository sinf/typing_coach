#ifndef _DPY_H
#define _DPY_H
#include <stdint.h>

// colors
#define C_NORMAL 1
#define C_UNTYPED 2
#define C_TYPED 3
#define C_MISTAKE 4
#define C_STATUS 5

// can be OR'd to color
#define CURSOR_BIT 16

extern int need_endwin;
void cu_setup();

/*
curs: relative cursor position, 0 means first character in (s)
cursor character has inverted color or something
-1 disables cursor
*/
void dpy_write_color(int y, const uint32_t s[], const int colors[], int len, int curs);

/*
returns new y
used for drawing the spam box
*/
int dpy_write_color_multiline(int y, const uint32_t s[], const int colors[], int len, int cursor);

__attribute__((format(printf,3,4)))
void dpy_print(int y, int color, const char *s, ...);

void dpy_begin(void);
void dpy_refresh(void);

int read_key(void);

#endif


