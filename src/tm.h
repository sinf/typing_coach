#ifndef _TRAINING_MODE_H
#define _TRAINING_MODE_H

extern int opt_auto_space;

// function to fill spam box with more content
extern void (*tm_words)(void);
// function to display some optional extra info
extern int (*tm_info)(int);

void training_session(void);

/* training mode 1:
analyze which sequences are slow
pick relevant words based on what might be slow
*/
void tm1_words(void);
int tm1_info(int y);

/* training mode 2:
   +type the contents of a text file
   +*/
void tm2_words(void);
int tm2_info(int y);

#endif

