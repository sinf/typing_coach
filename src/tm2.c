#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistr.h>
#include <unicase.h>
#include <unictype.h>
#include "prog_util.h"
#include "dpy.h"
#include "kseq.h"
#include "spambox.h"
#include "wordlist.h"
#include "database.h"
#include "tm.h"

static char* fetch_text(const char *cmd)
{
	const size_t bufsize = 1000 * 1024;
	char *buf = calloc(bufsize, 1);
	if (!buf) fail_oom();
	FILE *fp = popen(cmd, "r");
	if (!fp) {
		fail("popen failed. command: %s", cmd);
	}
	size_t n = fread(buf, 1, bufsize-1, fp);
	buf[n] = '\0';
	pclose(fp);
	return buf;
}

char *skip_lines(char *buf, int count)
{
	for(int i=0; i<count; ++i) {
		for(;;) {
			if (*buf == '\n') break;
			if (*buf == '\0') return buf;
			buf += 1;
		}
		buf += 1;
	}
	return buf;
}

#define MAX_WORDS 30
static Word my_words[MAX_WORDS];

void tm2_words(void)
{
	char *buf = fetch_text(
"wget -q https://en.wikipedia.org/wiki/Special:Random -O - 2>/dev/null|pandoc -f html -t plain");

	uint8_t *temp1 = (uint8_t*) skip_lines(buf, 10);
	uint8_t *temp2 = NULL;
	uint8_t *t;
	int n = 0;

	while ((t = u8_strtok(temp1, (uint8_t*) " \t\n\r\v", &temp2))) {
		temp1 = NULL;
		size_t l = u8_strlen(t);
		if (l < 50) {
			my_words[n] = utf8_to_word((char*) t, l);
			sb_add_word(my_words + n);
			n += 1;
			if (n >= MAX_WORDS) {
				break;
			}
		}
	}

	free(buf);
}

int tm2_info(int y)
{
	y += 1;
	dpy_print(y++, C_NORMAL, "Article: %s", "");
	return y;
}

