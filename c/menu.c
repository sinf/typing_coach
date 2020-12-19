#include <curses.h>
#include "dpy.h"
#include "menu.h"

int show_menu(const char *title, const MenuEntry options[], int n, int sel[1])
{
	for(;;) {
		dpy_begin();
		dpy_print(0, C_STATUS, "%s", title);
		for(int i=0; i<n; ++i) {
			char mnemonic = "123456789 "[i<9 ? i : 9];
			const char *xl="   ", *xr="   ";
			int c = C_UNTYPED;
			if (i==*sel) {
				c = C_TYPED;
				xl = "-- ";
				xr = " --";
			}
			if (options[i].toggle) {
				// on/off toggle
				const char *chk = *(options[i].toggle) ? "on " : "off";

				dpy_print(1+i, c, "%c%s%-34.34s [%s]%s",
					mnemonic, xl, options[i].label, chk, xr);
			} else {
				// button
				dpy_print(1+i, c, "%c%s%-40.40s%s",
					mnemonic, xl, options[i].label, xr);
			}
		}
		dpy_refresh();

		const MenuEntry *cur = options + *sel;
		int k = read_key();
		int temp;

		switch(k) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				temp = k - '1';
				if (temp < n)
					*sel = temp;
				break;

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

int read_input(const char *title, char buf[], int bufsize)
{
	int len=0;

	for(;;) {
		buf[len] = '\0';

		dpy_begin();
		dpy_print(0, C_STATUS, "Press enter to submit");
		dpy_print(1, C_STATUS, "%s", title);
		dpy_print(2, C_NORMAL, "%*s_", len, buf);
		dpy_refresh();

		int k = read_key();
		if (k == KEY_BACKSPACE) {
			if (len > 0)
				len -= 1;
		} else if (k == KEY_ENTER || k == '\r' || k == '\n') {
			break;
		} else {
			if (len < bufsize-1) {
				buf[len++] = k;
			}
		}
	}
	buf[len] = '\0';

	return len;
}

