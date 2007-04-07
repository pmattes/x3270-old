/*
 * Copyright 2000, 2001, 2002, 2004, 2005, 2006, 2007 by Paul Mattes.
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
 *		A Windows console-based 3270 Terminal Emulator
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
#include "widec.h"
#include "xioc.h"

#include <windows.h>
#include <wincon.h>

static int cmap[16] = {
	0,					/* neutral black */
	FOREGROUND_BLUE,			/* blue */
	FOREGROUND_RED,				/* red */
	FOREGROUND_RED | FOREGROUND_BLUE,	/* pink */
	FOREGROUND_GREEN,			/* green */
	FOREGROUND_GREEN | FOREGROUND_BLUE,	/* turquoise */
	FOREGROUND_GREEN | FOREGROUND_RED,	/* yellow */
	FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE, /* neutral white */

	0,					/* black */
	FOREGROUND_BLUE,			/* deep blue */
	FOREGROUND_RED | FOREGROUND_BLUE,	/* orange */
	FOREGROUND_BLUE,			/* deep blue */
	FOREGROUND_GREEN,			/* pale green */
	FOREGROUND_GREEN | FOREGROUND_BLUE,	/* pale turquoise */
	0,					/* grey */
	FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE /* white */
};
static int defattr = 0;
static unsigned long input_id;

Boolean escaped = True;

enum ts { TS_AUTO, TS_ON, TS_OFF };
enum ts ab_mode = TS_AUTO;

static int status_row = 0;	/* Row to display the status line on */
static int status_skip = 0;	/* Row to blank above the status line */

static void kybd_input(void);
static void kybd_input2(INPUT_RECORD *ir);
static void draw_oia(void);
static void status_connect(Boolean ignored);
static void status_3270_mode(Boolean ignored);
static void status_printer(Boolean on);
static int get_color_pair(int fg, int bg);
static int color_from_fa(unsigned char);
static void screen_init2(void);
static void set_status_row(int screen_rows, int emulator_rows);
static Boolean ts_value(const char *s, enum ts *tsp);
static int linedraw_to_acs(unsigned char c);
static int apl_to_acs(unsigned char c);

static HANDLE chandle;	/* console input handle */
static HANDLE cohandle;	/* console screen buffer handle */

static HANDLE sbuf[2];	/* dynamically-allocated screen buffers */
static int sbuf_ix = 0;	/* current buffer */
static int dirty = 0;	/* is the current buffer dirty? */

static int console_rows;
static int console_cols;

/*
 * Console event handler.
 */
static BOOL
cc_handler(DWORD type)
{
	if (type == CTRL_C_EVENT) {
		char *action;

		/* Process it as a Ctrl-C. */
		action = lookup_key(0x03, LEFT_CTRL_PRESSED);
		if (action != CN) {
			if (strcmp(action, "[ignore]"))
				push_keymap_action(action);
		} else {
			String params[2];
			Cardinal one;

			params[0] = "0x03";
			params[1] = CN;
			one = 1;
			Key_action(NULL, NULL, params, &one);
		}

		return TRUE;
	} else {
		/* Let Windows have its way with it. */
		return FALSE;
	}
}

/*
 * Get a handle for the console.
 */
static HANDLE
initscr(void)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	int i;

	/* Get a handle to the console. */
	chandle = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	if (chandle == NULL) {
		fprintf(stderr, "CreateFile(CONIN$) failed: %ld\n",
			GetLastError());
		return NULL;
	}
	if (SetConsoleMode(chandle, ENABLE_PROCESSED_INPUT) == 0) {
		fprintf(stderr, "SetConsoleMode failed: %ld\n",
			GetLastError());
		return NULL;
	}

	cohandle = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (cohandle == NULL) {
		fprintf(stderr, "CreateFile(CONOUT$) failed: %ld\n",
			GetLastError());
		return NULL;
	}

	/* Get its dimensions. */
	if (GetConsoleScreenBufferInfo(cohandle, &info) == 0) {
		fprintf(stderr, "GetConsoleScreenBufferInfo failed: %ld\n",
			GetLastError());
		return NULL;
	}
	console_rows = info.srWindow.Bottom - info.srWindow.Top + 1;
	console_cols = info.srWindow.Right - info.srWindow.Left + 1;

	/* Create two screen buffers. */
	for (i = 0; i < 2; i++) {
		sbuf[i] = CreateConsoleScreenBuffer(
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CONSOLE_TEXTMODE_BUFFER,
			NULL);
		if (sbuf[i] == NULL) {
			fprintf(stderr,
				"CreateConsoleScreenBuffer failed: %ld\n",
				GetLastError());
			return NULL;
		}
	}

	/* Define a console handler. */
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)cc_handler, TRUE)) {
		fprintf(stderr, "SetConsoleCtrlHandler failed: %ld\n",
				GetLastError());
		return NULL;
	}


	/* More will no doubt follow. */
	return chandle;
}

/* Try to set the console output character set. */
void
set_display_charset(char *dcs)
{
	char *copy;
	char *s;
	char *cs;
	int success = 0;

	copy = strdup(dcs);
	s = copy;
	while ((cs = strtok(s, ",")) != NULL) {
		s = NULL;

		/* Skip 3270 CG proprietary stuff. */
		if (!strncmp(cs, "3270cg", 6))
			continue;

		if (!strncmp(cs, "iso8859-", 8)) {
			/* Try ISO. */
			int v = atoi(cs + 8);
			int cp;

			cp = 28590 + v;
			if (SetConsoleOutputCP(cp)) {
#if defined(DEBUG_WINCS) /*[*/
				fprintf(stderr, "Set CP to %d\n", cp);
#endif /*]*/
				success = 1;
				break;
			}
			if (v == 1) {
				if (SetConsoleOutputCP(1252)) {
#if defined(DEBUG_WINCS) /*[*/
					fprintf(stderr, "Set CP to 1252\n");
#endif /*]*/
					success = 1;
					break;
				}
			}
		} else if (!strcmp(cs, "koi8-r")) {
			if (SetConsoleOutputCP(20866)) {
#if defined(DEBUG_WINCS) /*[*/
				fprintf(stderr, "Set CP to 20866\n");
#endif /*]*/
				success = 1;
				break;
			}
		}
	}
	
	free(copy);

	if (!success) {
		fprintf(stderr, "Unable to set output character set to '%s'.\n",
			dcs);
	}
}

/*
 * Vitrual curses functions.
 */
static int cur_row = 0;
static int cur_col = 0;
static int cur_attr = 0;

static void
move(int row, int col)
{
	cur_row = row;
	cur_col = col;
}

static void
attrset(int a)
{
	cur_attr = a;
}

static void
addch(char c)
{
	CHAR_INFO buffer;
	COORD bufferSize;
	COORD bufferCoord;
	SMALL_RECT writeRegion;

	/* If the buffer wasn't dirty, it is now. */
	if (!dirty) {
		sbuf_ix = (sbuf_ix + 1) % 2;
		dirty = 1;
	}

	/* Write it. */
	if (!(cur_attr & FOREGROUND_INTENSITY))
		abort();
	buffer.Char.AsciiChar = c;
	buffer.Attributes = cur_attr;
	bufferSize.X = 1;
	bufferSize.Y = 1;
	bufferCoord.X = 0;
	bufferCoord.Y = 0;
	writeRegion.Left = cur_col;
	writeRegion.Top = cur_row;
	writeRegion.Right = cur_col;
	writeRegion.Bottom = cur_row;
	if (WriteConsoleOutput(sbuf[sbuf_ix], &buffer, bufferSize, bufferCoord,
		&writeRegion) == 0) {

		fprintf(stderr, "WriteConsoleOutput failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}

	/* Increment and wrap. */
	if (++cur_col >= console_cols) {
		cur_col = 0;
		if (++cur_row >= console_rows)
			cur_row = 0;
	}
}

static void
printw(char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char *s;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	for (s = buf; *s; s++) {
		addch(*s);
	}
	va_end(ap);
}

static void
mvprintw(int row, int col, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char *s;

	va_start(ap, fmt);
	cur_row = row;
	cur_col = col;
	vsprintf(buf, fmt, ap);
	for (s = buf; *s; s++) {
		addch(*s);
	}
	va_end(ap);
}

/* Repaint the screen. */
static void
refresh(void)
{
	COORD coord;

	/* Move the cursor. */
	coord.X = cur_col;
	coord.Y = cur_row;
	if (SetConsoleCursorPosition(sbuf[sbuf_ix], coord) == 0) {
		fprintf(stderr, "\nSetConsoleCursorPosition failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}

	/* Swap in this buffer. */
	if (SetConsoleActiveScreenBuffer(sbuf[sbuf_ix]) == 0) {
		fprintf(stderr, "\nSetConsoleActiveScreenBuffer failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}

	/* This buffer is no longer dirty. */
	dirty = 0;
}

/* Go back to the original screen. */
static void
endwin(void)
{
	if (SetConsoleMode(chandle, ENABLE_ECHO_INPUT |
				    ENABLE_LINE_INPUT |
				    ENABLE_PROCESSED_INPUT) == 0) {
		fprintf(stderr, "\nSetConsoleMode(CONIN$) failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}
	if (SetConsoleMode(cohandle, ENABLE_PROCESSED_OUTPUT |
				     ENABLE_WRAP_AT_EOL_OUTPUT) == 0) {
		fprintf(stderr, "\nSetConsoleMode(CONOUT$) failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}

	/* Swap in the original buffer. */
	if (SetConsoleActiveScreenBuffer(cohandle) == 0) {
		fprintf(stderr, "\nSetConsoleActiveScreenBuffer failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}

	/* Mark the current buffer dirty. */
	dirty = 1;
}

/* Initialize the screen. */
void
screen_init(void)
{
	int want_ov_rows = ov_rows;
	int want_ov_cols = ov_cols;
	Boolean oversize = False;

	/* Disallow altscreen/defscreen. */
	if ((appres.altscreen != CN) || (appres.defscreen != CN)) {
		(void) fprintf(stderr, "altscreen/defscreen not supported\n");
		x3270_exit(1);
	}
	/* Initialize the console. */
	if (initscr() == NULL) {
		(void) fprintf(stderr, "Can't initialize terminal.\n");
		x3270_exit(1);
	}

	while (console_rows < maxROWS || console_cols < maxCOLS) {
		char buf[2];

		/*
		 * First, cancel any oversize.  This will get us to the correct
		 * model number, if there is any.
		 */
		if ((ov_cols && ov_cols > console_cols) ||
		    (ov_rows && ov_rows > console_rows)) {
			ov_cols = 0;
			ov_rows = 0;
			oversize = True;
			continue;
		}

		/* If we're at the smallest screen now, give up. */
		if (model_num == 2) {
			(void) fprintf(stderr, "Emulator won't fit on a %dx%d "
			    "display.\n", console_rows, console_cols);
			x3270_exit(1);
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
		if (want_ov_rows > console_rows - 2)
			want_ov_rows = console_rows - 2;
		if (want_ov_rows < maxROWS)
			want_ov_rows = maxROWS;
		if (want_ov_cols > console_cols)
			want_ov_cols = console_cols;
		set_rows_cols(model_num, want_ov_cols, want_ov_rows);
	}

	/* Figure out where the status line goes, if it fits. */
	/* Start out in altscreen mode. */
	set_status_row(console_rows, maxROWS);

	/* Set up callbacks for state changes. */
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
	register_schange(ST_PRINTER, status_printer);

	/* See about all-bold behavior. */
	if (appres.all_bold_on)
		ab_mode = TS_ON;
	else if (!ts_value(appres.all_bold, &ab_mode))
		(void) fprintf(stderr, "invalid %s value: '%s', "
		    "assuming 'auto'\n", ResAllBold, appres.all_bold);
	if (ab_mode == TS_AUTO)
		ab_mode = appres.m3279? TS_ON: TS_OFF;
	if (ab_mode == TS_ON)
		defattr |= FOREGROUND_INTENSITY;

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
	/* nothing to do... */

	escaped = False;

	/* Subscribe to input events. */
	input_id = AddInput((int)chandle, kybd_input);
}

/* Calculate where the status line goes now. */
static void
set_status_row(int screen_rows, int emulator_rows)
{
	if (screen_rows < emulator_rows + 1) {
		status_row = status_skip = 0;
	} else if (screen_rows == emulator_rows + 1) {
		status_skip = 0;
		status_row = emulator_rows;
	} else {
		status_skip = screen_rows - 2;
		status_row = screen_rows - 1;
	}
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
	return cmap[fg];
}

static int
color_from_fa(unsigned char fa)
{
	static int field_colors[4] = {
	    COLOR_GREEN,	/* default */
	    COLOR_RED,		/* intensified */
	    COLOR_BLUE,		/* protected */
	    COLOR_WHITE		/* protected, intensified */
#	define DEFCOLOR_MAP(f) \
		((((f) & FA_PROTECT) >> 4) | (((f) & FA_INT_HIGH_SEL) >> 3))

	};

	if (appres.m3279) {
		int fg;

		fg = field_colors[DEFCOLOR_MAP(fa)];
		return get_color_pair(fg, COLOR_BLACK) |
		    (((ab_mode == TS_ON) || FA_IS_HIGH(fa))? FOREGROUND_INTENSITY: 0);
	} else
		return ((ab_mode == TS_ON) || FA_IS_HIGH(fa))? FOREGROUND_INTENSITY: 0;
}

/* Display what's in the buffer. */
void
screen_disp(Boolean erasing unused)
{
	int row, col;
	int a;
	int c;
	unsigned char fa;
#if defined(X3270_DBCS) /*[*/
	enum dbcs_state d;
#endif /*]*/

	/* This may be called when it isn't time. */
	if (escaped)
		return;

	fa = get_field_attribute(0);
	a = color_from_fa(fa);
	for (row = 0; row < ROWS; row++) {
		int baddr;

		if (!flipped)
			move(row, 0);
		for (col = 0; col < cCOLS; col++) {
			if (flipped)
				move(row, cCOLS-1 - col);
			baddr = row*cCOLS+col;
			if (ea_buf[baddr].fa) {
				fa = ea_buf[baddr].fa;
				if (appres.m3279) {
					if (ea_buf[baddr].fg ||
					    ea_buf[baddr].bg) {
						int fg, bg;

						if (ea_buf[baddr].fg)
							fg = ea_buf[baddr].fg
							    & 0x0f;
						else
							fg = COLOR_WHITE;
						if (ea_buf[baddr].bg)
							bg = ea_buf[baddr].bg
							    & 0x0f;
						else
							bg = COLOR_BLACK;
						a = get_color_pair(fg, bg) |
							((ab_mode == TS_ON)?
							  FOREGROUND_INTENSITY: 0);
					} else {
						a = color_from_fa(fa);
					}
				} else {
					a = FA_IS_HIGH(fa)? FOREGROUND_INTENSITY: 0;
				}
#if 0
				if (ea_buf[baddr].gr & GR_BLINK)
					a |= A_BLINK;
#endif
#if defined(COMMON_LVB_REVERSE_VIDEO)
				if (ea_buf[baddr].gr & GR_REVERSE)
					a |= COMMON_LVB_REVERSE_VIDEO;
				if (ea_buf[baddr].gr & GR_UNDERLINE)
					a |= COMMON_LVB_UNDERSCORE;
#endif
				if (ea_buf[baddr].gr & GR_INTENSIFY)
					a |= FOREGROUND_INTENSITY;
				attrset(defattr);
				addch(' ');
			} else if (FA_IS_ZERO(fa)) {
				attrset(a);
				addch(' ');
			} else {
				if (ea_buf[baddr].gr ||
				    ea_buf[baddr].fg ||
				    ea_buf[baddr].bg) {
					int b = ((ab_mode == TS_ON) ||
						 FA_IS_HIGH(fa))? FOREGROUND_INTENSITY:
						                  0;

#if 0
					if (ea_buf[baddr].gr & GR_BLINK)
						b |= A_BLINK;
#endif
#if defined(COMMON_LVB_REVERSE_VIDEO)
					if (ea_buf[baddr].gr & GR_REVERSE)
						b |= COMMON_LVB_REVERSE_VIDEO;
					if (ea_buf[baddr].gr & GR_UNDERLINE)
						b |= COMMON_LVB_UNDERSCORE;
#endif
					if (ea_buf[baddr].gr & GR_INTENSIFY)
						b |= FOREGROUND_INTENSITY;
					if (appres.m3279 &&
					    (ea_buf[baddr].fg ||
					     ea_buf[baddr].bg)) {
						int fg, bg;

						if (ea_buf[baddr].fg)
							fg = ea_buf[baddr].fg
							    & 0x0f;
						else
							fg = COLOR_WHITE;
						if (ea_buf[baddr].bg)
							bg = ea_buf[baddr].bg
							    & 0x0f;
						else
							bg = COLOR_BLACK;
						b |= get_color_pair(fg, bg);
					} else
						b |= a;
		
					attrset(b);
				} else {
					(void) attrset(a);
				}
#if defined(X3270_DBCS) /*[*/
				d = ctlr_dbcs_state(baddr);
				if (IS_LEFT(d)) {
					int xaddr = baddr;
					char mb[16];
					int len;
					int i;

					INC_BA(xaddr);
					len = dbcs_to_mb(ea_buf[baddr].cc,
					    ea_buf[xaddr].cc,
					    mb);
					for (i = 0; i < len; i++) {
						addch(mb[i] & 0xff);
					}
				} else if (!IS_RIGHT(d)) {
#endif /*]*/
					if (ea_buf[baddr].cs == CS_LINEDRAW) {
						c = linedraw_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							addch(c);
						else
							addch(' ');
					} else if (ea_buf[baddr].cs == CS_APL ||
						   (ea_buf[baddr].cs & CS_GE)) {
						c = apl_to_acs(ea_buf[baddr].cc);
						if (c != -1)
							addch(c);
						else
							addch(' ');
					} else {
						if (toggled(MONOCASE))
							addch(asc2uc[ebc2asc[ea_buf[baddr].cc]]);
						else
							addch(ebc2asc[ea_buf[baddr].cc]);
					}
#if defined(X3270_DBCS) /*[*/
				}
#endif /*]*/
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

static const char *
decode_state(int state)
{
	static char buf[128];
	char *s = buf;
	char *space = "";

	if (!state)
		return "-";

	*s = '\0';
	if (state & LEFT_CTRL_PRESSED) {
		s += sprintf(s, "%slctrl", space);
		state &= ~LEFT_CTRL_PRESSED;
		space = " ";
	}
	if (state & RIGHT_CTRL_PRESSED) {
		s += sprintf(s, "%srctrl", space);
		state &= ~RIGHT_CTRL_PRESSED;
		space = " ";
	}
	if (state & LEFT_ALT_PRESSED) {
		s += sprintf(s, "%slalt", space);
		state &= ~LEFT_ALT_PRESSED;
		space = " ";
	}
	if (state & RIGHT_ALT_PRESSED) {
		s += sprintf(s, "%sralt", space);
		state &= ~RIGHT_ALT_PRESSED;
		space = " ";
	}
	if (state & SHIFT_PRESSED) {
		s += sprintf(s, "%sshift", space);
		state &= ~SHIFT_PRESSED;
		space = " ";
	}
	if (state & NUMLOCK_ON) {
		s += sprintf(s, "%snlock", space);
		state &= ~NUMLOCK_ON;
		space = " ";
	}
	if (state & SCROLLLOCK_ON) {
		s += sprintf(s, "%sslock", space);
		state &= ~SCROLLLOCK_ON;
		space = " ";
	}
	if (state & ENHANCED_KEY) {
		s += sprintf(s, "%senhanced", space);
		state &= ~ENHANCED_KEY;
		space = " ";
	}
	if (state) {
		s += sprintf(s, "%s?0x%x", space, state);
	}

	return buf;
}

/* Keyboard input. */
static void
kybd_input(void)
{
	INPUT_RECORD ir;
	DWORD nr;
	const char *s;
	extern const char *lookup_cname(unsigned long ccode);

	/* Get the next input event. */
	if (ReadConsoleInput(chandle, &ir, 1, &nr) == 0) {
		fprintf(stderr, "ReadConsoleInput failed: %ld\n",
			GetLastError());
		x3270_exit(1);
	}
	if (nr == 0)
		return;

	switch (ir.EventType) {
	case FOCUS_EVENT:
		trace_event("Focus\n");
		return;
	case KEY_EVENT:
		if (!ir.Event.KeyEvent.bKeyDown) {
			trace_event("KeyUp\n");
			return;
		}
		s = lookup_cname(ir.Event.KeyEvent.wVirtualKeyCode << 16);
		if (s == NULL)
			s = "?";
		trace_event("Key%s vkey 0x%x (%s) scan 0x%x char 0x%x state 0x%x (%s)\n",
			ir.Event.KeyEvent.bKeyDown? "Down": "Up",
			ir.Event.KeyEvent.wVirtualKeyCode, s,
			ir.Event.KeyEvent.wVirtualScanCode,
			ir.Event.KeyEvent.uChar.AsciiChar,
			(int)ir.Event.KeyEvent.dwControlKeyState,
			decode_state(ir.Event.KeyEvent.dwControlKeyState));
		break;
	case MENU_EVENT:
		trace_event("Menu\n");
		return;
	case MOUSE_EVENT:
		trace_event("Mouse\n");
		return;
	case WINDOW_BUFFER_SIZE_EVENT:
		trace_event("WindowBufferSize\n");
		return;
	default:
		trace_event("Unknown input event %d\n", ir.EventType);
		return;
	}

	if (!ir.Event.KeyEvent.bKeyDown) {
		return;
	}

	kybd_input2(&ir);
}

static void
kybd_input2(INPUT_RECORD *ir)
{
	int k;
	char buf[16];
	unsigned long xk;
	char *action;

	/*
	 * Translate the INPUT_RECORD into an integer we can match keymaps
	 * against.
	 * In theory, this is simple -- if it's a VK_xxx, put it in the high
	 * 16 bits and leave the low 16 clear; otherwise if there's an ASCII
	 * value, put it in the low 16; otherwise give up.
	 *
	 * In practice, this is harder, because some of the VK_ codes are
	 * aliases for ASCII characters and you get both.  So the rule becomes
	 * that if you get both VK_xxx and ASCII, and they are equal, ignore
	 * the VK.
	 */
	if (ir->Event.KeyEvent.uChar.AsciiChar != 0)
		xk = ir->Event.KeyEvent.uChar.AsciiChar & 0xffff;
	else if (ir->Event.KeyEvent.wVirtualKeyCode != 0)
		xk = (ir->Event.KeyEvent.wVirtualKeyCode << 16) & 0xffff0000;
	else
		xk = 0;

	if (xk) {
		action = lookup_key(xk, ir->Event.KeyEvent.dwControlKeyState);
		if (action != CN) {
			if (strcmp(action, "[ignore]"))
				push_keymap_action(action);
			return;
		}
	}

	ia_cause = IA_DEFAULT;

	k = ir->Event.KeyEvent.wVirtualKeyCode;

	/* These first cases apply to both 3270 and NVT modes. */
	switch (k) {
	case VK_ESCAPE:
		action_internal(Escape_action, IA_DEFAULT, CN, CN);
		return;
	case VK_UP:
		action_internal(Up_action, IA_DEFAULT, CN, CN);
		return;
	case VK_DOWN:
		action_internal(Down_action, IA_DEFAULT, CN, CN);
		return;
	case VK_LEFT:
		action_internal(Left_action, IA_DEFAULT, CN, CN);
		return;
	case VK_RIGHT:
		action_internal(Right_action, IA_DEFAULT, CN, CN);
		return;
	case VK_HOME:
		action_internal(Home_action, IA_DEFAULT, CN, CN);
		return;
	default:
		break;
	}

	/* Then look for 3270-only cases. */
	if (IN_3270) switch(k) {
	/* These cases apply only to 3270 mode. */
#if 0
	case VK_OEM_CLEAR:
		action_internal(Clear_action, IA_DEFAULT, CN, CN);
		return;
	case 0x12:
		action_internal(Reset_action, IA_DEFAULT, CN, CN);
		return;
	case 'L' & 0x1f:
		action_internal(Redraw_action, IA_DEFAULT, CN, CN);
		return;
#endif
	case VK_TAB:
		action_internal(Tab_action, IA_DEFAULT, CN, CN);
		return;
	case VK_DELETE:
		action_internal(Delete_action, IA_DEFAULT, CN, CN);
		return;
	case VK_BACK:
		action_internal(BackSpace_action, IA_DEFAULT, CN, CN);
		return;
	case VK_RETURN:
		action_internal(Enter_action, IA_DEFAULT, CN, CN);
		return;
#if 0
	case '\n':
		action_internal(Newline_action, IA_DEFAULT, CN, CN);
		return;
#endif
	default:
		break;
	}

	/* Do some NVT-only translations. */
	if (IN_ANSI) switch(k) {
	case VK_DELETE:
		k = 0x7f;
		break;
	case VK_BACK:
		k = '\b';
		break;
	}

	/* Catch PF keys. */
	if (k >= VK_F1 && k <= VK_F24) {
		(void) sprintf(buf, "%d", k - VK_F1 + 1);
		action_internal(PF_action, IA_DEFAULT, buf, CN);
		return;
	}

	/* Then any other 8-bit ASCII character. */
	k = ir->Event.KeyEvent.uChar.AsciiChar;
	if (k && !(k & ~0xff)) {
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
	escaped = False;

	screen_disp(False);
	refresh();
	input_id = AddInput((int)chandle, kybd_input);
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

	rmargin = maxCOLS - 1;

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

#if defined(COMMON_LVB_REVERSE_VIDEO)
	(void) attrset(COMMON_LVB_UNDERSCORE | defattr);
#endif
	mvprintw(status_row, 0, "4");
#if defined(COMMON_LVB_REVERSE_VIDEO)
	(void) attrset(COMMON_LVB_UNDERSCORE | defattr);
#endif
	if (oia_undera)
		printw("%c", IN_E? 'B': 'A');
	else
		printw(" ");
#if defined(COMMON_LVB_REVERSE_VIDEO)
	(void) attrset(COMMON_LVB_REVERSE_VIDEO | defattr);
#endif
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
	/*Beep(750, 300);*/
}

void
screen_flip(void)
{
	flipped = !flipped;
	screen_disp(False);
}

void
screen_132(void)
{
}

void
screen_80(void)
{
}

/*
 * Translate an x3270 font line-drawing character (the first two rows of a
 * standard X11 fixed-width font) to a curses ACS character.
 *
 * Returns -1 if there is no translation.
 */
static int
linedraw_to_acs(unsigned char c)
{
	switch (c) {
#if defined(ACS_BLOCK) /*[*/
	case 0x0:
		return ACS_BLOCK;
#endif /*]*/
#if defined(ACS_DIAMOND) /*[*/
	case 0x1:
		return ACS_DIAMOND;
#endif /*]*/
#if defined(ACS_CKBOARD) /*[*/
	case 0x2:
		return ACS_CKBOARD;
#endif /*]*/
#if defined(ACS_DEGREE) /*[*/
	case 0x7:
		return ACS_DEGREE;
#endif /*]*/
#if defined(ACS_PLMINUS) /*[*/
	case 0x8:
		return ACS_PLMINUS;
#endif /*]*/
#if defined(ACS_BOARD) /*[*/
	case 0x9:
		return ACS_BOARD;
#endif /*]*/
#if defined(ACS_LANTERN) /*[*/
	case 0xa:
		return ACS_LANTERN;
#endif /*]*/
#if defined(ACS_LRCORNER) /*[*/
	case 0xb:
		return ACS_LRCORNER;
#endif /*]*/
#if defined(ACS_URCORNER) /*[*/
	case 0xc:
		return ACS_URCORNER;
#endif /*]*/
#if defined(ACS_ULCORNER) /*[*/
	case 0xd:
		return ACS_ULCORNER;
#endif /*]*/
#if defined(ACS_LLCORNER) /*[*/
	case 0xe:
		return ACS_LLCORNER;
#endif /*]*/
#if defined(ACS_PLUS) /*[*/
	case 0xf:
		return ACS_PLUS;
#endif /*]*/
#if defined(ACS_S1) /*[*/
	case 0x10:
		return ACS_S1;
#endif /*]*/
#if defined(ACS_S3) /*[*/
	case 0x11:
		return ACS_S3;
#endif /*]*/
#if defined(ACS_HLINE) /*[*/
	case 0x12:
		return ACS_HLINE;
#endif /*]*/
#if defined(ACS_S7) /*[*/
	case 0x13:
		return ACS_S7;
#endif /*]*/
#if defined(ACS_S9) /*[*/
	case 0x14:
		return ACS_S9;
#endif /*]*/
#if defined(ACS_LTEE) /*[*/
	case 0x15:
		return ACS_LTEE;
#endif /*]*/
#if defined(ACS_RTEE) /*[*/
	case 0x16:
		return ACS_RTEE;
#endif /*]*/
#if defined(ACS_BTEE) /*[*/
	case 0x17:
		return ACS_BTEE;
#endif /*]*/
#if defined(ACS_TTEE) /*[*/
	case 0x18:
		return ACS_TTEE;
#endif /*]*/
#if defined(ACS_VLINE) /*[*/
	case 0x19:
		return ACS_VLINE;
#endif /*]*/
#if defined(ACS_LEQUAL) /*[*/
	case 0x1a:
		return ACS_LEQUAL;
#endif /*]*/
#if defined(ACS_GEQUAL) /*[*/
	case 0x1b:
		return ACS_GEQUAL;
#endif /*]*/
#if defined(ACS_PI) /*[*/
	case 0x1c:
		return ACS_PI;
#endif /*]*/
#if defined(ACS_NEQUAL) /*[*/
	case 0x1d:
		return ACS_NEQUAL;
#endif /*]*/
#if defined(ACS_STERLING) /*[*/
	case 0x1e:
		return ACS_STERLING;
#endif /*]*/
#if defined(ACS_BULLET) /*[*/
	case 0x1f:
		return ACS_BULLET;
#endif /*]*/
	default:
		return -1;
	}
}

static int
apl_to_acs(unsigned char c)
{
	switch (c) {
#if defined(ACS_DEGREE) /*[*/
	case 0xaf: /* CG 0xd1 */
		return ACS_DEGREE;
#endif /*]*/
#if defined(ACS_LRCORNER) /*[*/
	case 0xd4: /* CG 0xac */
		return ACS_LRCORNER;
#endif /*]*/
#if defined(ACS_URCORNER) /*[*/
	case 0xd5: /* CG 0xad */
		return ACS_URCORNER;
#endif /*]*/
#if defined(ACS_ULCORNER) /*[*/
	case 0xc5: /* CG 0xa4 */
		return ACS_ULCORNER;
#endif /*]*/
#if defined(ACS_LLCORNER) /*[*/
	case 0xc4: /* CG 0xa3 */
		return ACS_LLCORNER;
#endif /*]*/
#if defined(ACS_PLUS) /*[*/
	case 0xd3: /* CG 0xab */
		return ACS_PLUS;
#endif /*]*/
#if defined(ACS_HLINE) /*[*/
	case 0xa2: /* CG 0x92 */
		return ACS_HLINE;
#endif /*]*/
#if defined(ACS_LTEE) /*[*/
	case 0xc6: /* CG 0xa5 */
		return ACS_LTEE;
#endif /*]*/
#if defined(ACS_RTEE) /*[*/
	case 0xd6: /* CG 0xae */
		return ACS_RTEE;
#endif /*]*/
#if defined(ACS_BTEE) /*[*/
	case 0xc7: /* CG 0xa6 */
		return ACS_BTEE;
#endif /*]*/
#if defined(ACS_TTEE) /*[*/
	case 0xd7: /* CG 0xaf */
		return ACS_TTEE;
#endif /*]*/
#if defined(ACS_VLINE) /*[*/
	case 0x85: /* CG 0xa84? */
		return ACS_VLINE;
#endif /*]*/
#if defined(ACS_LEQUAL) /*[*/
	case 0x8c: /* CG 0xf7 */
		return ACS_LEQUAL;
#endif /*]*/
#if defined(ACS_GEQUAL) /*[*/
	case 0xae: /* CG 0xd9 */
		return ACS_GEQUAL;
#endif /*]*/
#if defined(ACS_NEQUAL) /*[*/
	case 0xbe: /* CG 0x3e */
		return ACS_NEQUAL;
#endif /*]*/
#if defined(ACS_BULLET) /*[*/
	case 0xa3: /* CG 0x93 */
		return ACS_BULLET;
#endif /*]*/
	case 0xad:
		return '[';
	case 0xbd:
		return ']';
	default:
		return -1;
	}
}
