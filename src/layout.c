#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PIPE_MAX 4096
#define LAYOUT_MAX 256

static void str_concat(char buf[], int buf_len, char src[], int src_len)
{
	int i = strlen(buf);
	int j = 0;
	while( i<buf_len && j<src_len )
		buf[i++] = src[j++];
	buf[i<buf_len ? i : buf_len-1] = '\0';
}

static void pick_feature(char in[PIPE_MAX], char out[LAYOUT_MAX], char *key)
{
	char *begin = strstr(in, key);
	if (begin) {
		begin += strlen(key);
		char *end = strchr(begin, '\n');
		int n = end - begin;
		if (n > 0 && n < LAYOUT_MAX) {
			if (out[0] != '\0')
				str_concat(out, LAYOUT_MAX, "_", 1);
			str_concat(out, LAYOUT_MAX, begin, n);
		}
	}
}

const char* detect_layout(void)
{
	static char layout[LAYOUT_MAX]="pc101_us";
	static time_t t0 = 0;
	time_t t = time(0);
	if (t0 && t0 < t && layout[0] != '\0' && t-t0 < 2) {
		// limit layout recheck to every 1 second
		return layout;
	}
	FILE *fp = popen("/usr/bin/setxkbmap -query", "r");
	if (!fp) return layout;
	char buf[PIPE_MAX];
	int len = fread(buf, 1, PIPE_MAX, fp);
	if (len < 40) return layout;
	layout[0] = '\0';
	pick_feature(buf, layout, "\nmodel:      ");
	pick_feature(buf, layout, "\nlayout:     ");
	pick_feature(buf, layout, "\nvariant:    ");
	return layout;
}

#ifdef _TEST_XKB
int main() {
	printf("Layout:\n");
	printf("%s\n", detect_layout());
	return 0;
}
#endif

