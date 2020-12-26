#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "prog_util.h"
#include "ini.h"

#define LINE_BUF 512
#define MAX_LINES 1024
/* save the config files' lines here between parsing and saving
to preserve comments, empty lines and unused variables as-is */
static char the_line_buf[MAX_LINES][LINE_BUF];
static int the_line_count=0;

static FILE *ini_file(const char *path, const char *mode) {
	FILE *fp = fopen(path, mode);
	if (!fp) {
		debug_msg("Failed to open ini file \"%s\" (mode=%s): %s\n",
			path, mode, strerror(errno));
	}
	return fp;
}

static char* rtrim(char *buf) {
	if (buf[0] != '\0') {
		char *end = buf + strlen(buf) - 1;
		while (isspace(*end) && end >= buf) end--;
		end[1]='\0';
	}
	return buf;
}
static char *ltrim(char *buf) {
	while (isspace(*buf)) ++buf;
	return buf;
}
static char *trim(char *buf) {
	return rtrim(ltrim(buf));
}

static int read_i(char buf[LINE_BUF], int *i) {
	return sscanf(buf, "%d", i) == 1;
}
static int read_d(char buf[LINE_BUF], double *d) {
	return sscanf(buf, "%lf", d) == 1;
}
static int read_s(char buf[LINE_BUF], const char **s, char sb[INI_STR_MAX]) {
	strncpy(sb, buf, INI_STR_MAX);
	sb[INI_STR_MAX-1] = '\0';
	*s = trim(sb);
	return 1;
}

static void write_i(char val[], int n, int i) {
	snprintf(val, n, "%d", i);
}
static void write_d(char val[], int n, double d) {
	snprintf(val, n, "%f", d);
}
static void write_s(char val[], int n, const char *s) {
	strncpy(val, s ? s : "", n);
}

static void write_var(char buf[LINE_BUF], IniVar *v)
{
	strncpy(buf, v->key, v->key_len);
	buf[v->key_len]='=';
	char *val = buf + v->key_len + 1;
	const int n = LINE_BUF - (val - buf);
	switch(v->type) {
		case INI_TYPE_I: write_i(val, n, *v->p.i); break;
		case INI_TYPE_D: write_d(val, n, *v->p.d); break;
		case INI_TYPE_S: write_s(val, n, *v->p.s); break;
		default: break;
	}
	buf[LINE_BUF-1]='\0';
	v->ok = 1;
}

static int read_var(char buf[LINE_BUF], char *val, IniVar *v)
{
	int ok;
	switch(v->type) {
		case INI_TYPE_I: ok=read_i(val, v->p.i); break;
		case INI_TYPE_D: ok=read_d(val, v->p.d); break;
		case INI_TYPE_S: ok=read_s(val, v->p.s, v->s_buf); break;
		default: ok=0; break;
	}
	v->ok = ok;
	return ok;
}

static void process_ini_lines(IniVar vars[], int write)
{
	for(IniVar *v=vars; v->key; ++v) {
		v->ok = 0;
	}
	for(int ln=0; ln<the_line_count; ++ln) {
		char *buf = the_line_buf[ln];
		for(IniVar *v=vars; v->key; ++v) {
			if (strncmp(v->key, buf, v->key_len))
				continue;
			if (buf[v->key_len] != '=' && !isspace(buf[v->key_len]))
				continue;
			char *val = strchr(buf, '=');
			if (!val) continue;
			val = val + 1;
			if (write) {
				write_var(buf, v);
			} else {
				int ok = read_var(buf, val, v);
				if (!ok) debug_msg("parse error on line %d", ln);
			}
			// stop processing this line, move to the next
			break;
		}
	}
	if (write) {
		for(IniVar *v=vars; v->key && the_line_count<MAX_LINES; ++v) {
			if (!v->ok) write_var(the_line_buf[the_line_count++], v);
		}
	}
}

void ini_settings_read(IniVar vars[], const char *path)
{
	FILE *fp = ini_file(path, "r");
	if (!fp) return;
	the_line_count=0;
	while(fgets(the_line_buf[the_line_count], LINE_BUF, fp)) {
		if (++the_line_count >= MAX_LINES) {
			debug_msg("WARNING: ini file has too many lines ("STRTOK(MAX_LINES)")");
			break;
		}
	}
	fclose(fp);
	process_ini_lines(vars, 0);
	debug_msg("read settings");
}

void ini_settings_write(IniVar vars[], const char *path)
{
	FILE *fp = ini_file(path, "w");
	if (!fp) return;
	process_ini_lines(vars, 1);
	for(int i=0; i<the_line_count; ++i) {
		fprintf(fp, "%.*s\n", LINE_BUF, the_line_buf[i]);
	}
	fclose(fp);
	debug_msg("write settings");
}

