#include "sz_mult.h"

int overflow_flag=0;

size_t sz_mult(size_t x, size_t y, size_t z)
{
	if (!x || !y) return z;
#if defined(__x86_64__) && 0
	// broken
	size_t w;
	int of;
	__asm(
	"xor %0,%0;"
	"xor %1,%1;"
	"movq %2, %%rax;"
	"mulq %3;"
	"jo 10f;"
	"addq %4, %%rax;"
	"jo 10f;"
	"jmp 20f;"
	"10: inc %1;"
	"20: mov %%rax, %0;"
	:"=&g"(w), "=&g"(of)
	:"g"(x), "g"(y), "g"(z)
	:"rax","rdx","flags");
	if (of) goto overflow;
	return w;
#else
	const size_t c=~0;
	if (x > (c-z)/y || y > (c-z)/x)
		goto overflow;
	return x*y + z;
#endif
overflow:;
	overflow_flag=1;
#ifndef _TEST_SZMULT
	extern void fail();
	fail("sz_mult: overflow");
#endif
	return 0;
}

