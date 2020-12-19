#ifndef _MENU_H
#define _MENU_H

typedef struct MenuEntry {
	const char *label;
	int id;
	int *toggle;
} MenuEntry;

#define M_BUTTON(s,i) {.label=(s), .id=(i)}
#define M_TOGGLE(s,p) {.label=(s), .toggle=(p)}

int show_menu(const char *title, const MenuEntry options[], int n, int sel[1]);
void loading_screen();

int read_input(const char *title, char buf[], int bufsize);

#endif


