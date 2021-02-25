#include <stdio.h>
#define _TEST_SZMULT
#include "sz_mult.c"

int main()
{
	const struct {
		size_t x, y, z, res;
		int o;//overflow flag
	} t[] = {
{0x00ffffffaaaabbbb, 0x0000000faaaabbbb, 0, 0, 1},
{0, 0, 0xDeadBeef, 0xDeadBeef, 0},
{0x00ffffffaaaabbbb, 200, 0,  0xc7ffffbd5562aa18, 0},
{0x00ffffffaaaabbbb, 300, 0,  0, 1},
{0x00ffffffaaaabbbb, 200, 0x38000042aa9d55e6,  0xfffffffffffffffe, 0},
{0x00ffffffaaaabbbb, 200, 0x38000042aa9d55e7,  0xffffffffffffffff, 0},
{0x00ffffffaaaabbbb, 200, 0x38000042aa9d55e8,  0, 1},
{0, 0, 0,  0, 0},
{0, 0, 5,  5, 0},
{22500, 819855292164868, 0, 0xffffffffffffab90, 0},
{22500, 819855292164869, 0, 0, 1},
{22500, 819855292164868, 21614, 0xfffffffffffffffe, 0},
{22500, 819855292164868, 21615, 0xffffffffffffffff, 0},
{22500, 819855292164868, 21616, 0, 1},
{6903143535, 2672223745, 0, 0xfffffffefd5b566f, 0},
{6903143535, 2672223745, 0x102a4a990, 0xffffffffffffffff, 0},
{6903143535, 2672223745, 0x102a4a991, 0, 1},
{100, 5, 32, 532, 0},
	};
	int ret=0;
	for(size_t i=0; i<sizeof(t)/sizeof(t[0]); ++i) {
		overflow_flag=0;
		size_t tmp=sz_mult(t[i].x, t[i].y, t[i].z);
		const char* res_s="OK";
		if (tmp != t[i].res || overflow_flag != t[i].o) {
			res_s="FAIL";
			ret=1;
		}
		printf("%-16lx * %-16lx + %-16lx = %-16lx (%-16lx) OF=%d (%d) [%s]\n", t[i].x, t[i].y, t[i].z, tmp, t[i].res, overflow_flag, t[i].o, res_s);
	}
	return ret;
}

