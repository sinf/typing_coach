#ifndef _SPAMBOX_H
#define _SPAMBOX_H
#include <stdint.h>
#include "kseq.h"

struct Word;
extern int sb_continue_on_typo;

int sb_add_word(struct Word *w);
void sb_clear();
void sb_write(int len, const uint32_t *s, struct Word *w);

KeyCode sb_expected();
int sb_end_reached();
void sb_putc(KeyCode c, int delay);
void sb_skip_spaces(void);

// return new y
int sb_paint(int y);

void sb_submit_sequences();

#endif

