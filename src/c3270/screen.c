/*
 * Copyright 2000 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	screen.c
 *		A curses-based 3270 Terminal Emulator
 *		Screen drawing
 */

#include "globals.h"
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"

#undef COLS
extern int cCOLS;

#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_WHITE
#if defined(HAVE_NCURSES_H) /*[*/
#include <ncurses.h>
#else /*][*/ 
#include <curses.h>
#endif /*]*/

static SCREEN *screen;
static Boolean fully_initted = False;
int cp[8][8];
int cmap[16] = {
	COLOR_BLACK,	/* neutral black */
	COLOR_BLUE,	/* blue */
	COLOR_RED,	/* red */
	COLOR_MAGENTA,	/* pink */
	COLOR_GREEN,	/* green */
	COLOR_CYAN,	/* turquoise */
	COLOR_YELLOW,	/* yellow */
	COLOR_WHITE,	/* neutral white */

	COLOR_BLACK,	/* black */
	COLOR_BLUE,	/* deep blue */
	COLOR_YELLOW,	/* orange */
	COLOR_BLUE,	/* deep blue */
	COLOR_GREEN,	/* pale green */
	COLOR_CYAN,	/* pale turquoise */
	COLOR_BLACK,	/* grey */
	COLOR_WHITE	/* white */
};
static int defattr = A_NORMAL;

Boolean escaped = True;
unsigned long input_id;

static void kybd_input(void);
static void draw_oia(void);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void screen_init2(void);

/* Initialize the screen. */
void
screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;

	/* Set up ncurses, and see if it's within bounds. */
	screen = newterm(CN, stdout, stdin);
	if (screen == (SCREEN *)NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}

	while (LINES < ROWS || COLS < cCOLS) {
		char buf[2];

		/*
		 * First, cancel any oversize.  This will get us to the correct
		 * model number, if there is any.
		 */
		if ((ov_cols && ov_cols > COLS) ||
		    (ov_rows && ov_rows > LINES)) {
			ov_cols = 0;
			ov_rows = 0;
			oversize = True;
			continue;
		}

		/* If we're at the smallest screen now, give up. */
		if (model_num == 2) {
			(void) fprintf(stderr, "Emulator won't fit on a %dx%d "
			    "display.\n", LINES, COLS);
			exit(1);
		}

		/* Try a smaller model. */
		(void) sprintf(buf, "%d", model_num - 1);
		appres.model = NewString(buf);
		set_rows_cols(model_num - 1, 0, 0);
	}

	/*
	 * Now, if they wanted an oversize, but didn't get it, try applying it
	 * again.
	 */
	if (oversize) {
		if (want_ov_rows > LINES - 2)
			want_ov_rows = LINES - 2;
		if (want_ov_rows < ROWS)
			want_ov_rows = ROWS;
		if (want_ov_cols > COLS)
			want_ov_cols = COLS;
		set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}

	/* Set up callbacks for state changes. */
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);

	/* Play with curses color. */
	if (appres.m3279) {
		start_color();
		if (has_colors() && COLORS >= 8)
			defattr = get_color_pair(COLOR_BLUE, COLOR_BLACK);
		else {
			appres.m3279 = False;
			/* Get the terminal name right. */
			set_rows_cols(model_num, want_ov_cols, want_ov_rows);
		}
	}

	/* Set up the controller. */
	ctlr_init(-1);
	ctlr_reinit(-1);

	/* Finish screen initialization. */
	screen_init2();
	screen_suspend();
}

/* Secondary screen initialization. */
static void
screen_init2(void)
{
	/*
	 * Finish initializing ncurses.  This should be the first time that it
	 * will send anything to the terminal.
	 */
	(void) set_term(screen);
	escaped = False;

	/* Set up the keyboard. */
	raw();
	noecho();
	nonl();
	intrflush(stdscr,FALSE);
	keypad(stdscr, TRUE);
	meta(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	input_id = AddInput(0, kybd_input);
}

/* Allocate a color pair. */
static int
get_color_pair(int fg, int bg)
{
	static int next_pair = 1;
	int pair;

	if ((pair = cp[fg][bg]))
		return COLOR_PAIR(pair);
	if (next_pair >= COLOR_PAIRS)
		return 0;
	if (init_pair(next_pair, fg, bg) != OK)
		return 0;
	pair = cp[fg][bg] = next_pair++;
	return COLOR_PAIR(pair);
}

static int
color_from_fa(unsigned char fa)
{
	static int field_colors[4] = {
	    COLOR_GREEN,	/* default */
	    COLOR_RED,		/* intensified */
	    COLOR_BLUE,		/* protected */
	    COLOR_WHITE		/* protected, intensified */
	};

	if (appres.m3279) {
		int fg;

		fg = field_colors[(fa >> 1) & 0x03];
		return get_color_pair(fg, COLOR_BLACK) |
		    (FA_IS_HIGH(fa)? A_BOLD: A_NORMAL);
	} else
		return FA_IS_HIGH(fa)? A_BOLD: A_NORMAL;
}

/* Display what's in the buffer. */
void
screen_disp(void)
{
	int row, col;
	int a;
	unsigned char fa;

	/* This may be called when it isn't time. */
	if (escaped)
		return;

	fa = *get_field_attribute(0);
	a = color_from_fa(fa);
	for (row = 0; row < ROWS; row++) {
		int baddr;

		if (!flipped)
			move(row, 0);
		for (col = 0; col < cCOLS; col++) {
			if (flipped)
				move(row, cCOLS-1 - col);
			baddr = row*cCOLS+col;
			if (IS_FA(screen_buf[baddr])) {
				fa = screen_buf[baddr];
				a = FA_IS_HIGH(fa)? A_BOLD: A_NORMAL;
				if (ea_buf[baddr].gr & GR_BLINK)
					a |= A_BLINK;
				if (ea_buf[baddr].gr & GR_REVERSE)
					a |= A_REVERSE;
				if (ea_buf[baddr].gr & GR_UNDERLINE)
					a |= A_UNDERLINE;
				if (ea_buf[baddr].gr & GR_INTENSIFY)
					a |= A_BOLD;
				if (appres.m3279 &&
				    (ea_buf[baddr].fg ||
				     ea_buf[baddr].bg)) {
					int fg, bg;

					if (ea_buf[baddr].fg)
						fg = cmap[ea_buf[baddr].fg
						    & 0x0f];
					else
						fg = COLOR_WHITE;
					if (ea_buf[baddr].bg)
						bg = cmap[ea_buf[baddr].bg
						    & 0x0f];
					else
						bg = COLOR_BLACK;
					a |= get_color_pair(fg, bg);
				} else
					a = color_from_fa(fa);
				attrset(defattr);
				addch(' ');
			} else if (FA_IS_ZERO(fa)) {
				attrset(a);
				addch(' ');
			} else {
				if (ea_buf[baddr].gr ||
				    ea_buf[baddr].fg ||
				    ea_buf[baddr].bg) {
					int b = FA_IS_HIGH(fa)? A_BOLD: A_NORMAL;

					if (ea_buf[baddr].gr & GR_BLINK)
						b |= A_BLINK;
					if (ea_buf[baddr].gr & GR_REVERSE)
						b |= A_REVERSE;
					if (ea_buf[baddr].gr & GR_UNDERLINE)
						b |= A_UNDERLINE;
					if (ea_buf[baddr].gr & GR_INTENSIFY)
						b |= A_BOLD;
					if (appres.m3279 &&
					    (ea_buf[baddr].fg ||
					     ea_buf[baddr].bg)) {
						int fg, bg;

						if (ea_buf[baddr].fg)
							fg = cmap[ea_buf[baddr].fg
							    & 0x0f];
						else
							fg = COLOR_WHITE;
						if (ea_buf[baddr].bg)
							bg = cmap[ea_buf[baddr].bg
							    & 0x0f];
						else
							bg = COLOR_BLACK;
						b |= get_color_pair(fg, bg);
					} else
						b |= a;
		
					attrset(b);
				} else {
					(void) attrset(a);
				}
				if (toggled(MONOCASE))
					addch(asc2uc[cg2asc[screen_buf[baddr]]]);
				else
					addch(cg2asc[screen_buf[baddr]]);
			}
		}
	}
	if (LINES > maxROWS+1)
		draw_oia();
	(void) attrset(defattr);
	if (flipped)
		move(cursor_addr / cCOLS, cCOLS-1 - (cursor_addr % cCOLS));
	else
		move(cursor_addr / cCOLS, cursor_addr % cCOLS);
	refresh();
}

/* Keyboard input. */
static void
kybd_input(void)
{
	int k;
	char buf[16];
	char *action;

	k = wgetch(stdscr);
	if (k == ERR)
		return;
	trace_event("Key %s (0x%x)\n", decode_key(k, 0), k);
	action = lookup_key(k);
	if (action != CN) {
		if (strcmp(action, "[ignore]"))
			push_keymap_action(action);
		return;
	}
	ia_cause = IA_DEFAULT;

	switch (k) {
	case 0x1d:
		action_internal(Escape_action, IA_DEFAULT, CN, CN);
		break;
	case 0x03:
	case 0x80 + 'c':
		action_internal(Clear_action, IA_DEFAULT, CN, CN);
		break;
	case 0x12:
	case 0x80 + 'r':
		action_internal(Reset_action, IA_DEFAULT, CN, CN);
		break;
	case 'L' & 0x1f:
		action_internal(Redraw_action, IA_DEFAULT, CN, CN);
		break;
	case 0x80 + 'm':
		action_internal(Compose_action, IA_DEFAULT, CN, CN);
		break;
	case '\t':
		action_internal(Tab_action, IA_DEFAULT, CN, CN);
		break;
	case 0177:
	case KEY_DC:
		action_internal(Delete_action, IA_DEFAULT, CN, CN);
		break;
	case '\b':
	case KEY_BACKSPACE:
		action_internal(BackSpace_action, IA_DEFAULT, CN, CN);
		break;
	case '\r':
		action_internal(Enter_action, IA_DEFAULT, CN, CN);
		break;
	case '\n':
		action_internal(Newline_action, IA_DEFAULT, CN, CN);
		break;
	case KEY_UP:
		action_internal(Up_action, IA_DEFAULT, CN, CN);
		break;
	case KEY_DOWN:
		action_internal(Down_action, IA_DEFAULT, CN, CN);
		break;
	case KEY_LEFT:
		action_internal(Left_action, IA_DEFAULT, CN, CN);
		break;
	case KEY_RIGHT:
		action_internal(Right_action, IA_DEFAULT, CN, CN);
		break;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		break;
	case 0x80 + '1':
	case 0x80 + '2':
	case 0x80 + '3':
		(void) sprintf(buf, "%d", (k & 0x7f) - '1' + 1);
		action_internal(PA_action, IA_DEFAULT, buf, CN);
		break;
	default:
		if (k >= KEY_F(1) && k <= KEY_F(24)) {
			(void) sprintf(buf, "%d", k - KEY_F0);
			action_internal(PF_action, IA_DEFAULT, buf, CN);
			break;
		}
		if (k >= '\001' && k <= '~') {
			char ks[6];
			String params[2];
			Cardinal one;

			if (k >= ' ') {
				ks[0] = k;
				ks[1] = '\0';
			} else {
				(void) sprintf(ks, "0x%x", k);
			}
			params[0] = ks;
			params[1] = CN;
			one = 1;
			Key_action(NULL, NULL, params, &one);
			break;
		}
		trace_event(" dropped (no default)\n");
		break;
	}
}

void
screen_suspend(void)
{
	static Boolean need_to_scroll = False;

	if (!escaped) {
		escaped = True;
		endwin();
		if (need_to_scroll)
			printf("\n");
		else
			need_to_scroll = True;
		RemoveInput(input_id);
	}
}

void
screen_resume(void)
{
	/*
	 * If this is the first time the screen has been resumed,
	 * complete the ncurses initialization.
	 */
	if (!fully_initted) {
		screen_init2();
		fully_initted = True;
	}

	escaped = False;
	screen_disp(); /* in case something changed while we were escaped */
	refresh();
	input_id = AddInput(0, kybd_input);
}

void
cursor_move(int baddr)
{
	cursor_addr = baddr;
}

void
toggle_monocase(struct toggle *t unused, enum toggle_type tt unused)
{
	screen_disp();
}

/* Status line stuff. */

static Boolean status_ta = False;
static Boolean status_rm = False;
static Boolean status_im = False;
static Boolean oia_boxsolid = False;
static Boolean oia_undera = True;
static Boolean oia_compose = False;
static unsigned char oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;

static char *status_msg = "";

void
status_ctlr_done(void)
{
	oia_undera = True;
}

void
status_insert_mode(Boolean on)
{
	status_im = on;
}

void
status_minus(void)
{
	status_msg = "X -f";
}

void
status_oerr(int error_type)
{
	switch (error_type) {
	case KL_OERR_PROTECTED:
		status_msg = "X Protected";
		break;
	case KL_OERR_NUMERIC:
		status_msg = "X Numeric";
		break;
	case KL_OERR_OVERFLOW:
		status_msg = "X Overflow";
		break;
	}
}

void
status_reset(void)
{
	if (kybdlock & KL_ENTER_INHIBIT)
		status_msg = "X Inhibit";
	else if (kybdlock & KL_DEFERRED_UNLOCK)
		status_msg = "X";
	else
		status_msg = "";
}

void
status_reverse_mode(Boolean on)
{
	status_rm = on;
}

void
status_syswait(void)
{
	status_msg = "X SYSTEM";
}

void
status_twait(void)
{
	oia_undera = False;
	status_msg = "X Wait";
}

void
status_typeahead(Boolean on)
{
	status_ta = on;
}

void    
status_compose(Boolean on, unsigned char c, enum keytype keytype)
{
        oia_compose = on;
        oia_compose_char = c;
        oia_compose_keytype = keytype;
}

static void
status_connect(Boolean connected)
{
	if (connected) {
		oia_boxsolid = IN_3270 && !IN_SSCP;
		if (kybdlock & KL_AWAITING_FIRST)
			status_msg = "X";
		else
			status_msg = "";
	} else {
		oia_boxsolid = False;
		status_msg = "X Disconnected";
	}       
}

static void
status_3270_mode(Boolean ignored unused)
{
	oia_boxsolid = IN_3270 && !IN_SSCP;
	if (oia_boxsolid)
		oia_undera = True;
}

static void
draw_oia(void)
{
	/* Make sure the status line region is filled in properly. */
	if (appres.m3279) {
		int i;

		attrset(defattr);
		move(maxROWS, 0);
		for (i = 0; i < maxCOLS; i++) {
			printw(" ");
		}
		move(maxROWS+1, 0);
		for (i = 0; i < maxCOLS; i++) {
			printw(" ");
		}
	}

	(void) attrset(A_REVERSE | defattr);
	mvprintw(maxROWS + 1, 0, "4");
	(void) attrset(A_UNDERLINE | defattr);
	if (oia_undera)
		printw("%c", IN_E? 'B': 'A');
	else
		printw(" ");
	(void) attrset(A_REVERSE | defattr);
	if (IN_ANSI)
		printw("N");
	else if (oia_boxsolid)
		printw(" ");
	else if (IN_SSCP)
		printw("S");
	else
		printw("?");

	(void) attrset(defattr);
	mvprintw(maxROWS + 1, 8, "%-11s", status_msg);
	mvprintw(maxROWS + 1, maxCOLS-36,
	    "%c%c %c  %c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ',
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ');

	mvprintw(maxROWS + 1, maxCOLS-7,
	    "%03d/%03d", cursor_addr/COLS + 1, cursor_addr%COLS + 1);
}

void
Redraw_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	if (!escaped) {
		endwin();
		refresh();
	}
}

void
ring_bell(void)
{
	beep();
}

void
screen_flip(void)
{
	flipped = !flipped;
	screen_disp();
}
