/*
 * Copyright 2000, 2001, 2002 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * c3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	screen.c
 *		A curses-based 3270 Terminal Emulator
 *		Screen drawing
 */

#include "globals.h"
#include <signal.h>
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
#include "xioc.h"

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

static int cp[8][8];
static int cmap[16] = {
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
static unsigned long input_id;

Boolean escaped = True;

enum ts { TS_AUTO, TS_ON, TS_OFF };
enum ts me_mode = TS_AUTO;
enum ts ab_mode = TS_AUTO;

#if defined(C3270_80_132) /*[*/
struct screen_spec {
	int rows, cols;
	char *mode_switch;
} screen_spec;
struct screen_spec altscreen_spec, defscreen_spec;
static SCREEN *def_screen = NULL, *alt_screen = NULL;
static SCREEN *cur_screen = NULL;
static void parse_screen_spec(const char *str, struct screen_spec *spec);
#endif /*]*/

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */

static Boolean curses_alt = False;

static void kybd_input(void);
static void kybd_input2(int k, Boolean derived);
static void draw_oia(void);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static void status_printer(Boolean on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void screen_init2(void);
static Boolean ts_value(const char *s, enum ts *tsp);

/* Initialize the screen. */
void
screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;

#if !defined(C3270_80_132) /*[*/
	/* Disallow altscreen/defscreen. */
	if ((appres.altscreen != CN) || (appres.defscreen != CN)) {
		(void) fprintf(stderr, "altscreen/defscreen not supported\n");
		exit(1);
	}
	/* Initialize curses. */
	if (initscr() == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}
#else /*][*/
	/* Parse altscreen/defscreen. */
	if ((appres.altscreen != CN) ^ (appres.defscreen != CN)) {
		(void) fprintf(stderr,
		    "Must specify both altscreen and defscreen\n");
		exit(1);
	}
	if (appres.altscreen != CN) {
		parse_screen_spec(appres.altscreen, &altscreen_spec);
		if (altscreen_spec.rows < 27 || altscreen_spec.cols < 132) {
		    (void) fprintf(stderr, "Rows and/or cols too small on "
			"alternate screen (mininum 27x132)\n");
		    exit(1);
		}
		parse_screen_spec(appres.defscreen, &defscreen_spec);
		if (defscreen_spec.rows < 24 || defscreen_spec.cols < 80) {
		    (void) fprintf(stderr, "Rows and/or cols too small on "
			"default screen (mininum 24x80)\n");
		    exit(1);
		}
	}

	/* Set up ncurses, and see if it's within bounds. */
	if (appres.defscreen != CN) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", defscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", defscreen_spec.rows);
		putenv(NewString(nbuf));
		def_screen = newterm(NULL, stdout, stdin);
		if (def_screen == NULL) {
			(void) fprintf(stderr,
			    "Can't initialize %dx%d defscreen terminal.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			exit(1);
		}
		(void) write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch));
	}
	if (appres.altscreen) {
		char nbuf[64];

		(void) sprintf(nbuf, "COLUMNS=%d", altscreen_spec.cols);
		putenv(NewString(nbuf));
		(void) sprintf(nbuf, "LINES=%d", altscreen_spec.rows);
		putenv(NewString(nbuf));
	}
	alt_screen = newterm(NULL, stdout, stdin);
	if (alt_screen == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		exit(1);
	}
	if (appres.altscreen) {
		set_term(alt_screen);
		cur_screen = alt_screen;
	}

	/* If they want 80/132 switching, then they want a model 5. */
	if (def_screen != NULL && model_num != 5) {
		appres.model = NewString("5");
		set_rows_cols(5, 0, 0);
	}
#endif /*]*/

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

	/* Figure out where the status line goes, if it fits. */
	if (LINES < maxROWS + 1) {
		status_row = status_skip = 0;
	} else if (LINES == maxROWS + 1) {
		status_skip = 0;
		status_row = LINES - 1;
	} else {
		status_skip = LINES - 2;
		status_row = LINES - 1;
	}

	/* Set up callbacks for state changes. */
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
	register_schange(ST_PRINTER, status_printer);

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

	/* See about keyboard Meta-key behavior. */
	if (!ts_value(appres.meta_escape, &me_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResMetaEscape, appres.meta_escape);
	if (me_mode == TS_AUTO)
		me_mode = tigetflag("km")? TS_OFF: TS_ON;

	/* See about all-bold behavior. */
	if (appres.all_bold_on)
		ab_mode = TS_ON;
	else if (!ts_value(appres.all_bold, &ab_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResAllBold, appres.all_bold);
	if (ab_mode == TS_AUTO)
		ab_mode = appres.m3279? TS_ON: TS_OFF;
	if (ab_mode == TS_ON)
		defattr |= A_BOLD;

	/* Set up the controller. */
	ctlr_init(-1);
	ctlr_reinit(-1);

	/* Finish screen initialization. */
	screen_init2();
	screen_suspend();
}

/* Configure the TTY settings for a curses screen. */
static void
setup_tty(void)
{
	if (appres.cbreak_mode)
		cbreak();
	else
		raw();
	noecho();
	nonl();
	intrflush(stdscr,FALSE);
	if (appres.curses_keypad)
		keypad(stdscr, TRUE);
	meta(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	refresh();
}

static void
swap_screens(SCREEN *new_screen)
{
	set_term(new_screen);
	cur_screen = new_screen;
}

/* Secondary screen initialization. */
static void
screen_init2(void)
{
	/*
	 * Finish initializing ncurses.  This should be the first time that it
	 * will send anything to the terminal.
	 */
	escaped = False;

	/* Set up the keyboard. */
	setup_tty();
#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL) {
		/*
		 * The first setup_tty() set up altscreen.
		 * Set up defscreen now, and leave it as the
		 * current curses screen.
		 */
		swap_screens(def_screen);
		setup_tty();
	}
#endif /*]*/

	/* Subscribe to input events. */
	input_id = AddInput(0, kybd_input);

	/* Ignore SIGINT and SIGTSTP. */
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

#if defined(C3270_80_132) /*[*/
	/* Ignore SIGWINCH -- it might happen when we do 80/132 changes. */
	if (def_screen != NULL)
		signal(SIGWINCH, SIG_IGN);
#endif /*]*/
}

/*
 * Parse a tri-state resource value.
 * Returns True for success, False for failure.
 */
static Boolean
ts_value(const char *s, enum ts *tsp)
{
	*tsp = TS_AUTO;

	if (s != CN && s[0]) {
		int sl = strlen(s);

		if (!strncasecmp(s, "true", sl))
			*tsp = TS_ON;
		else if (!strncasecmp(s, "false", sl))
			*tsp = TS_OFF;
		else if (strncasecmp(s, "auto", sl))
			return False;
	}
	return True;
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
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL);
	} else
		return ((ab_mode == TS_ON) || FA_IS_HIGH(fa))? A_BOLD: A_NORMAL;
}

/* Display what's in the buffer. */
void
screen_disp(Boolean erasing unused)
{
	int row, col;
	int a;
	unsigned char fa;
	extern Boolean screen_alt;
	struct screen_spec *cur_spec;

	/* This may be called when it isn't time. */
	if (escaped)
		return;

#if defined(C3270_80_132) /*[*/
	/* See if they've switched screens on us. */
	if (def_screen != NULL && screen_alt != curses_alt) {
		if (screen_alt) {
			(void) write(1, altscreen_spec.mode_switch,
			    strlen(altscreen_spec.mode_switch));
			trace_event("Switching to alt (%dx%d) screen.\n",
			    altscreen_spec.rows, altscreen_spec.cols);
			swap_screens(alt_screen);
			cur_spec = &altscreen_spec;
		} else {
			(void) write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch));
			trace_event("Switching to default (%dx%d) screen.\n",
			    defscreen_spec.rows, defscreen_spec.cols);
			swap_screens(def_screen);
			cur_spec = &defscreen_spec;
		}

		/* Figure out where the status line goes now, if it fits. */
		if (cur_spec->rows < ROWS + 1) {
			status_row = status_skip = 0;
		} else if (cur_spec->rows == ROWS + 1) {
			status_skip = 0;
			status_row = ROWS;
		} else {
			status_skip = cur_spec->rows - 2;
			status_row = cur_spec->rows - 1;
		}

		curses_alt = screen_alt;

		/* Tell curses to forget what may be on the screen already. */
		endwin();
		erase();
	}
#endif /*]*/

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
				if (appres.m3279) {
					if (ea_buf[baddr].fg ||
					    ea_buf[baddr].bg) {
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
						a = get_color_pair(fg, bg);
					} else {
						a = color_from_fa(fa);
					}
				} else {
					a = FA_IS_HIGH(fa)? A_BOLD: A_NORMAL;
				}
				if (ea_buf[baddr].gr & GR_BLINK)
					a |= A_BLINK;
				if (ea_buf[baddr].gr & GR_REVERSE)
					a |= A_REVERSE;
				if (ea_buf[baddr].gr & GR_UNDERLINE)
					a |= A_UNDERLINE;
				if (ea_buf[baddr].gr & GR_INTENSIFY)
					a |= A_BOLD;
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
	if (status_row)
		draw_oia();
	(void) attrset(defattr);
	if (flipped)
		move(cursor_addr / cCOLS, cCOLS-1 - (cursor_addr % cCOLS));
	else
		move(cursor_addr / cCOLS, cursor_addr % cCOLS);
	refresh();
}

/* ESC processing. */
static unsigned long eto = 0L;
static Boolean meta_escape = False;

static void
escape_timeout(void)
{
	trace_event("Timeout waiting for key following Escape, processing "
	    "separately\n");
	eto = 0L;
	meta_escape = False;
	kybd_input2(0x1b, False);
}

/* Keyboard input. */
static void
kybd_input(void)
{
	int k;
	Boolean first = True;
	static Boolean failed_first = False;

	for (;;) {
		Boolean derived = False;
		char dbuf[128];

		if (isendwin())
			return;
		k = wgetch(stdscr);
		if (k == ERR) {
			if (first) {
				if (failed_first) {
					trace_event("End of File, exiting.\n");
					x3270_exit(1);
				}
				failed_first = True;
			}
			return;
		} else {
			failed_first = False;
		}
		trace_event("Key %s (0x%x)\n", decode_key(k, 0, dbuf), k);

		/* Handle Meta-Escapes. */
		if (meta_escape) {
			if (eto != 0L) {
				RemoveTimeOut(eto);
				eto = 0L;
			}
			meta_escape = False;
			k |= 0x80;
			derived = True;
		} else if (me_mode == TS_ON && k == 0x1b) {
			eto = AddTimeOut(100L, escape_timeout);
			trace_event(" waiting to see if Escape is followed by"
			    " another key\n");
			meta_escape = True;
			continue;
		}
		kybd_input2(k, derived);
		first = False;
	}
}

static void
kybd_input2(int k, Boolean derived)
{
	char buf[16];
	char *action;
	char dbuf1[128], dbuf2[128];

	if (derived)
		trace_event(" combining <Key>Escape and %s into %s (0x%x)\n",
		    decode_key(k & 0x7f, 0, dbuf1),
		    decode_key(k, KM_META, dbuf2), k);
	action = lookup_key(k);
	if (action != CN) {
		if (strcmp(action, "[ignore]"))
			push_keymap_action(action);
		return;
	}
	ia_cause = IA_DEFAULT;

	/* These first cases apply to both 3270 and NVT modes. */
	switch (k) {
	case 0x1d:
		action_internal(Escape_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_UP:
		action_internal(Up_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_DOWN:
		action_internal(Down_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_LEFT:
		action_internal(Left_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_RIGHT:
		action_internal(Right_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Then look for 3270-only cases. */
	if (IN_3270) switch(k) {
	/* These cases apply only to 3270 mode. */
	case 0x03:
		action_internal(Clear_action, IA_DEFAULT, CN, CN);
		return;
	case 0x12:
		action_internal(Reset_action, IA_DEFAULT, CN, CN);
		return;
	case 'L' & 0x1f:
		action_internal(Redraw_action, IA_DEFAULT, CN, CN);
		return;
	case '\t':
		action_internal(Tab_action, IA_DEFAULT, CN, CN);
		return;
	case 0177:
	case KEY_DC:
		action_internal(Delete_action, IA_DEFAULT, CN, CN);
		return;
	case '\b':
	case KEY_BACKSPACE:
		action_internal(BackSpace_action, IA_DEFAULT, CN, CN);
		return;
	case '\r':
		action_internal(Enter_action, IA_DEFAULT, CN, CN);
		return;
	case '\n':
		action_internal(Newline_action, IA_DEFAULT, CN, CN);
		return;
	case KEY_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Do some NVT-only translations. */
	if (IN_ANSI) switch(k) {
	case KEY_DC:
		k = 0x7f;
		break;
	case KEY_BACKSPACE:
		k = '\b';
		break;
	}

	/* Catch PF keys. */
	if (k >= KEY_F(1) && k <= KEY_F(24)) {
		(void) sprintf(buf, "%d", k - KEY_F0);
		action_internal(PF_action, IA_DEFAULT, buf, CN);
		return;
	}

	/* Then any other 8-bit ASCII character. */
	if (!(k & ~0xff)) {
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
		return;
	}
	trace_event(" dropped (no default)\n");
}

void
screen_suspend(void)
{
	static Boolean need_to_scroll = False;

	if (!escaped) {
		escaped = True;
#if defined(C3270_80_132) /*[*/
		if (def_screen != NULL) {
			/*
			 * Call endwin() for the last-defined screen
			 * (altscreen) first.  Note that this will leave
			 * the curses screen set to defscreen when this
			 * function exits; if the 3270 is really in altscreen
			 * mode, we will have to switch it back when we resume
			 * the screen, below.
			 */
			if (!curses_alt)
				swap_screens(alt_screen);
			endwin();
			swap_screens(def_screen);
			endwin();
		} else {
			endwin();
		}
#else /*][*/
		endwin();
#endif /*]*/

		if (need_to_scroll)
			printf("\n");
		else
			need_to_scroll = True;
#if defined(C3270_80_132) /*[*/
		if (curses_alt && def_screen != NULL) {
			(void) write(1, defscreen_spec.mode_switch,
			    strlen(defscreen_spec.mode_switch));
		}
#endif /*]*/
		RemoveInput(input_id);
	}
}

void
screen_resume(void)
{
	escaped = False;

#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL && curses_alt) {
		/*
		 * When we suspended the screen, we switched to defscreen so
		 * that endwin() got called in the right order.  Switch back.
		 */
		swap_screens(alt_screen);
		(void) write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch));
	}
#endif /*]*/
	screen_disp(False);
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
	screen_disp(False);
}

/* Status line stuff. */

static Boolean status_ta = False;
static Boolean status_rm = False;
static Boolean status_im = False;
static Boolean oia_boxsolid = False;
static Boolean oia_undera = True;
static Boolean oia_compose = False;
static Boolean oia_printer = False;
static unsigned char oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
#define LUCNT	8
static char oia_lu[LUCNT+1];

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

void
status_lu(const char *lu)
{
	if (lu != NULL) {
		(void) strncpy(oia_lu, lu, LUCNT);
		oia_lu[LUCNT] = '\0';
	} else
		(void) memset(oia_lu, '\0', sizeof(oia_lu));
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
status_printer(Boolean on)
{
	oia_printer = on;
}

static void
draw_oia(void)
{
	int rmargin;

#if defined(C3270_80_132) /*[*/
	if (def_screen != NULL) {
		if (curses_alt)
			rmargin = altscreen_spec.cols - 1;
		else
			rmargin = defscreen_spec.cols - 1;
	} else
#endif /*]*/
	{
		rmargin = maxCOLS - 1;
	}

	/* Make sure the status line region is filled in properly. */
	if (appres.m3279) {
		int i;

		attrset(defattr);
		if (status_skip) {
			move(status_skip, 0);
			for (i = 0; i < rmargin; i++) {
				printw(" ");
			}
		}
		move(status_row, 0);
		for (i = 0; i < rmargin; i++) {
			printw(" ");
		}
	}

	(void) attrset(A_REVERSE | defattr);
	mvprintw(status_row, 0, "4");
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
	mvprintw(status_row, 8, "%-11s", status_msg);
	mvprintw(status_row, rmargin-36,
	    "%c%c %c  %c%c%c",
	    oia_compose? 'C': ' ',
	    oia_compose? oia_compose_char: ' ',
	    status_ta? 'T': ' ',
	    status_rm? 'R': ' ',
	    status_im? 'I': ' ',
	    oia_printer? 'P': ' ');

	mvprintw(status_row, rmargin-25, "%s", oia_lu);
	mvprintw(status_row, rmargin-7,
	    "%03d/%03d", cursor_addr/cCOLS + 1, cursor_addr%cCOLS + 1);
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
	screen_disp(False);
}

#if defined(C3270_80_132) /*[*/
/* Alt/default screen spec parsing. */
static void
parse_screen_spec(const char *str, struct screen_spec *spec)
{
	char msbuf[3];
	char *s, *t, c;
	Boolean escaped = False;

	if (sscanf(str, "%dx%d=%2s", &spec->rows, &spec->cols, msbuf) != 3) {
		(void) fprintf(stderr, "Invalid screen screen spec '%s', must "
		    "be '<rows>x<cols>=<init_string>'\n", str);
		exit(1);
	}
	s = strchr(str, '=') + 1;
	spec->mode_switch = Malloc(strlen(s) + 1);
	t = spec->mode_switch;
	while ((c = *s++)) {
		if (escaped) {
			switch (c) {
			case 'E':
			    *t++ = 0x1b;
			    break;
			case 'n':
			    *t++ = '\n';
			    break;
			case 'r':
			    *t++ = '\r';
			    break;
			case 'b':
			    *t++ = '\b';
			    break;
			case 't':
			    *t++ = '\t';
			    break;
			case '\\':
			    *t++ = '\\';
			    break;
			default:
			    *t++ = c;
			    break;
			}
			escaped = False;
		} else if (c == '\\')
			escaped = True;
		else
			*t++ = c;
	}
	*t = '\0';
}
#endif /*]*/

void
screen_132(void)
{
#if defined(C3270_80_132) /*[*/
	if (cur_screen != alt_screen) {
		swap_screens(alt_screen);
		(void) write(1, altscreen_spec.mode_switch,
		    strlen(altscreen_spec.mode_switch));
		ctlr_erase(True);
		screen_disp(True);
	}
#endif /*]*/
}

void
screen_80(void)
{
#if defined(C3270_80_132) /*[*/
	if (cur_screen != def_screen) {
		swap_screens(def_screen);
		(void) write(1, defscreen_spec.mode_switch,
		    strlen(defscreen_spec.mode_switch));
		ctlr_erase(False);
		screen_disp(True);
	}
#endif /*]*/
}
