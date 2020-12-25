#pragma once

void debug_output_init(void);

__attribute__((format(printf,4,5)))
void debug_msg_x(const char *file, int line, const char *func, const char *fmt, ...);

#define debug_msg(fmt...) debug_msg_x(__FILE__,__LINE__,__func__,fmt)

