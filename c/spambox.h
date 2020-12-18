#ifndef _SPAMBOX_H
#define _SPAMBOX_H
#include <stdint.h>
#include "kseq.h"

struct Word;

int sb_add_word(struct Word *w);
void sb_clear();
void sb_write(int len, const uint32_t *s, struct Word *w);

KeyCode sb_expected();
int sb_end_reached();
void sb_putc(KeyCode c);

// return new y
int sb_paint(int y);

#endif

