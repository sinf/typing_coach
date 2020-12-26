#ifndef _SZ_MULT_H
#define _SZ_MULT_H
#include <stddef.h>
extern int overflow_flag;
// compute x*y+z but check overflow
size_t sz_mult(size_t x, size_t y, size_t z);
#endif

