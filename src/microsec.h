#ifndef _MICROSEC_H
#define _MICROSEC_H
#include <stdint.h>
/* Returns current time in microseconds.
The returned time might not have a meaningful base (such as POSIX Epoch, start of program, etc).
Only useful for measuring elapsed time */
uint64_t get_microsec( void );
#endif
