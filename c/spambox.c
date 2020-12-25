#include <unictype.h>
#include "dpy.h"
#include "wordlist.h"
#include "spambox.h"
#include "kseq.h"
#include "database.h"

#define BUFLEN SPAMBOX_BUFLEN

struct Buf {
	// characters the user needs to type
	KeyCode ch[BUFLEN];

	// delay for each character (or <0 for typo)
	int16_t delay[BUFLEN];

	// Each character is identified as part of some word. Word separators use NULL
	struct Word *wp[BUFLEN];

	// each character has a color C_...
	int color[BUFLEN];

	int pos; //cursor position
	int len; //how many characters in buffer
};

static struct Buf cbuf = {{0},{0},{0},{0},0,0};
int sb_continue_on_typo=0;

int sb_add_word(struct Word *w)
{
	int l0 = cbuf.len;
	if (cbuf.len > 0) {
		uint32_t space = ' ';
		sb_write(1, &space, NULL);
	}
	Word w2 = w_strip(w);
	sb_write(w2.len, w2.s, &w2);
	return l0 != cbuf.len;
}

void sb_submit_sequences()
{
	db_put_seq_samples(cbuf.len, cbuf.ch, cbuf.delay);
}

void sb_clear()
{
	cbuf.pos = 0;
	cbuf.len = 0;
	for(int i=0; i<BUFLEN; ++i) {
		cbuf.ch[i] = L' ';
		cbuf.wp[i] = NULL;
		cbuf.color[i] = C_UNTYPED;
	}
}

void sb_write(int len, const uint32_t *s, Word *w)
{
	int i0 = cbuf.len;
	int i1 = i0 + len;
	if (i1 > BUFLEN) return;
	cbuf.len = i1;
	for(int i=0; i<len; ++i) {
		int j = i0 + i;
		cbuf.ch[j] = s[i];
		cbuf.color[j] = C_UNTYPED;
		cbuf.wp[j] = w;
	}
}

KeyCode sb_expected()
{
	// what character is expected to be typed right now
	return cbuf.ch[cbuf.pos];
}

int sb_end_reached()
{
	return cbuf.pos >= cbuf.len;
}

void sb_putc(KeyCode c, int d)
{
	if (sb_end_reached())
		return;
	if (c == sb_expected()) {
		// typed ok
		if (cbuf.color[cbuf.pos] != C_MISTAKE) {
			cbuf.color[cbuf.pos] = C_TYPED;
			cbuf.delay[cbuf.pos] = d;
		}
		cbuf.pos += 1;
	} else {
		// typed wrong
		cbuf.color[cbuf.pos] = C_MISTAKE;
		cbuf.delay[cbuf.pos] = -1;
		if (sb_continue_on_typo)
			cbuf.pos += 1;
	}
}

void sb_skip_spaces(void)
{
	int i;
	for(i=cbuf.pos; i<cbuf.len; ++i) {
		if (!is_whitespace(cbuf.ch[i]))
			break;
	}
	cbuf.pos = i;
}

int sb_paint(int y)
{
	return
	dpy_write_color_multiline
	(y, cbuf.ch, cbuf.color, cbuf.len, cbuf.pos);
}


