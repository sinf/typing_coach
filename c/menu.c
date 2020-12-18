#include <curses.h>
#include "dpy.h"
#include "menu.h"

int show_menu(const char *title, const MenuEntry options[], int n, int sel[1])
{
	for(;;) {
		dpy_begin();
		dpy_print(0, C_STATUS, "%s", title);
		for(int i=0; i<n; ++i) {
			if (options[i].toggle) {
				// on/off toggle
				const char *chk = *(options[i].toggle) ? "on " : "off";
				dpy_print(1+i, C_NORMAL, " %-34.34s [%s]",
					options[i].label, chk);
			} else {
				// button
				dpy_print(1+i, C_NORMAL, " %-40.40s",
					options[i].label);
			}
			if (i==*sel) {
				attron(COLOR_PAIR(C_STATUS));
				addstr(" <===");
			}
		}
		dpy_refresh();

		const MenuEntry *cur = options + *sel;
		int k = read_key();
		switch(k) {
			case KEY_UP:
			case KEY_PPAGE:
			case KEY_PREVIOUS:
			case 'k':
			case 'h':
			case 'p':
				if (--*sel<0) *sel=n-1;
				break;

			case KEY_DOWN:
			case KEY_NPAGE:
			case KEY_NEXT:
			case 'j':
			case 'l':
			case 'n':
			case KEY_STAB:
			case KEY_CTAB:
			case KEY_CATAB:
				if (++*sel>=n) *sel=0;
				break;

			case KEY_SELECT:
			case KEY_ENTER:
			case ' ':
			case 'x':
			case '\n':
			case '\r':
			case 's':
				if (cur->toggle) {
					*(cur->toggle) = !*(cur->toggle);
				} else {
					return cur->id;
				}
				break;

			case KEY_CLOSE:
			case KEY_EXIT:
			case KEY_BACKSPACE:
			case 'q':
				return -1;
		}
	}
	return -1;
}

void loading_screen()
{
	dpy_begin();
	dpy_print(0, C_NORMAL, "Loading...");
	dpy_refresh();
}

