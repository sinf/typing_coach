#pragma once

#define INI_STR_MAX 256

#define INI_TYPE_I 1
#define INI_TYPE_D 2
#define INI_TYPE_S 3

union IniVarDataP {
	int *i;
	double *d;
	const char **s;
};

struct IniVar_t {
	const char *key;
	int key_len;
	int type;
	union IniVarDataP p;
	char s_buf[INI_STR_MAX];
	int ok;
};

typedef struct IniVar_t IniVar;

#define _INI_KEY(k) .key=(k),.key_len=sizeof(k)-1
#define INI_INT(key, ptr) { _INI_KEY(key), .type=INI_TYPE_I, .p.i=(ptr) } 
#define INI_DOUBLE(key, ptr) { _INI_KEY(key), .type=INI_TYPE_D, .p.d=(ptr) } 
#define INI_STR(key, pptr) { _INI_KEY(key), .type=INI_TYPE_S, .p.s=(pptr) } 
#define INI_END { .key=NULL }

void ini_settings_read(IniVar v[], const char *path);
void ini_settings_write(IniVar v[], const char *path);

