/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 *
 *                         All Rights Reserved
 */

/*
 *	screen.c
 *		This module handles interpretation of the 3270 data stream
 *		and other screen management actions.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>
#include <suntool/menu.h>
#include <stdio.h>
#include "3270.h"
#include "3270_enc.h"

/* ebcdic to 3270 character generator xlate table */

u_char	ebc2cg[256] = {
	CG_NULLBLANK,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_OVERBAR2,	CG_OVERBAR6,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_OVERBAR3,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_OVERBAR1,	CG_PERIOD,	CG_PERIOD,
	CG_DUP,		CG_PERIOD,	CG_FM,		CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,	CG_OVERBAR4,
	CG_BLANK,	CG_LBRACKET,	CG_RBRACKET,	CG_POUND,
	CG_YEN,		CG_PT,		CG_CURRENCY,	CG_SHARPS,
	CG_SECTION,	CG_OVERSCORE,	CG_CENT,	CG_PERIOD,
	CG_LESS,	CG_LPAREN,	CG_PLUS,	CG_SOLIDBAR,
	CG_AMPERSAND,	CG_DEGREE,	CG_BREVE,	CG_CIRCUMFLEX,
	CG_DIAERESIS,	CG_ACUTE,	CG_CEDILLA,	CG_LAACUTE1,
	CG_LEACUTE1,	CG_LIACUTE1,	CG_EXCLAMATION,	CG_DOLLAR,
	CG_ASTERISK,	CG_RPAREN,	CG_SEMICOLON,	CG_NOT,
	CG_MINUS,	CG_FSLASH,	CG_LOACUTE1,	CG_LUACUTE1,
	CG_LATILDE,	CG_LOTILDE,	CG_LYDIAERESIS,	CG_LAACUTE2,
	CG_LEACUTE2,	CG_LEGRAVE1,	CG_BROKENBAR,	CG_COMMA,
	CG_PERCENT,	CG_UNDERSCORE,	CG_GREATER,	CG_QUESTION,
	CG_LIACUTE2,	CG_LOACUTE2,	CG_LUACUTE2,	CG_LUDIAERESIS1,
	CG_LCCEDILLA1,	CG_LADIAERESIS,	CG_LEDIAERESIS,	CG_LIDIAERESIS,
	CG_LODIAERESIS,	CG_GRAVE,	CG_COLON,	CG_NUMBER,
	CG_AT,		CG_SQUOTE,	CG_EQUAL,	CG_DQUOTE,
	CG_LUDIAERESIS2,CG_LA,		CG_LB,		CG_LC,
	CG_LD,		CG_LE,		CG_LF,		CG_LG,
	CG_LH,		CG_LI,		CG_LACIRCUMFLEX,CG_LECIRCUMFLEX,
	CG_LICIRCUMFLEX,CG_LOCIRCUMFLEX,CG_LUCIRCUMFLEX,CG_LAGRAVE,
	CG_LEGRAVE2,	CG_LJ,		CG_LK,		CG_LL,
	CG_LM,		CG_LN,		CG_LO,		CG_LP,
	CG_LQ,		CG_LR,		CG_LIGRAVE,	CG_LOGRAVE,
	CG_LUGRAVE,	CG_LNTILDE,	CG_CAACUTE,	CG_CEACUTE,
	CG_CIACUTE,	CG_TILDE,	CG_LS,		CG_LT,
	CG_LU,		CG_LV,		CG_LW,		CG_LX,
	CG_LY,		CG_LZ,		CG_COACUTE,	CG_CUACUTE,
	CG_CATILDE,	CG_COTILDE,	CG_CY1,		CG_CA1,
	CG_CE1,		CG_CE2,		CG_CI1,		CG_CO1,
	CG_CU1,		CG_CY2,		CG_CC1,		CG_CADIAERESIS,
	CG_CEDIAERESIS,	CG_CIDIAERESIS,	CG_CODIAERESIS,	CG_CUDIAERESIS,
	CG_CACIRCUMFLEX,CG_CECIRCUMFLEX,CG_CICIRCUMFLEX,CG_COCIRCUMFLEX,
	CG_LBRACE,	CG_CA,		CG_CB,		CG_CC,
	CG_CD,		CG_CE,		CG_CF,		CG_CG,
	CG_CH,		CG_CI,		CG_CUCIRCUMFLEX,CG_CAGRAVE,
	CG_CEGRAVE,	CG_CIGRAVE,	CG_PERIOD,	CG_PERIOD,
	CG_RBRACE,	CG_CJ,		CG_CK,		CG_CL,
	CG_CM,		CG_CN,		CG_CO,		CG_CP,
	CG_CQ,		CG_CR,		CG_COGRAVE,	CG_CUGRAVE,
	CG_CNTILDE,	CG_PERIOD,	CG_PERIOD,	CG_PERIOD,
	CG_BSLASH,	CG_LAE,		CG_CS,		CG_CT,
	CG_CU,		CG_CV,		CG_CW,		CG_CX,
	CG_CY,		CG_CZ,		CG_SSLASH0,	CG_LADOT,
	CG_LCCEDILLA2,	CG_PERIOD,	CG_PERIOD,	CG_MINUS,
	CG_ZERO,	CG_ONE,		CG_TWO,		CG_THREE,
	CG_FOUR,	CG_FIVE,	CG_SIX,		CG_SEVEN,
	CG_EIGHT,	CG_NINE,	CG_CAE,		CG_BSLASH0,
	CG_CADOT,	CG_CCCEDILLA,	CG_MINUS,	CG_OVERBAR7
};

/* 3270 character generator to ebcdic xlate table */
/* generated programmatically from ebc2cg */

u_char	cg2ebc[256] = {
	0x00, 0x19, 0x0c, 0x15, 0x3f, 0x00, 0x0d, 0xff,	/* 0x00 */
	0x6e, 0x4c, 0x41, 0x42, 0x5d, 0x4d, 0xd0, 0xc0,
	0x40, 0x7e, 0x7d, 0x7f, 0x61, 0xe0, 0x4f, 0x6a,	/* 0x10 */
	0x6f, 0x5a, 0x5b, 0x4a, 0x43, 0x44, 0x45, 0x46,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0x20 */
	0xf8, 0xf9, 0x47, 0x48, 0x7b, 0x7c, 0x6c, 0x6d,
	0x50, 0x60, 0x4b, 0x6b, 0x7a, 0x4e, 0x5f, 0x49,	/* 0x30 */
	0x51, 0x52, 0x53, 0xa1, 0x54, 0x79, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x62, 0x63, 0x64, 0x65, 0x66,	/* 0x40 */
	0x67, 0x68, 0x69, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x80, 0x8a, 0x8b, 0x8c,	/* 0x50 */
	0x8d, 0x8e, 0x8f, 0x90, 0x9a, 0x9b, 0x9c, 0x9d,
	0x9e, 0x9f, 0xa0, 0xaa, 0xab, 0xac, 0xad, 0xae,	/* 0x60 */
	0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,	/* 0x70 */
	0xbf, 0xca, 0xcb, 0xcc, 0xcd, 0xda, 0xdb, 0xdc,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,	/* 0x80 */
	0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0x90 */
	0xa8, 0xa9, 0xe1, 0xea, 0xeb, 0xec, 0x1e, 0x1c,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,	/* 0xA0 */
	0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xB0 */
	0xe8, 0xe9, 0xfa, 0xfb, 0xfc, 0xfd, 0x5e, 0x5c,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0xC0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0xD0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0xE0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0xF0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define ITIMER_NULL	((struct itimerval *) 0)
#define TIMER_IVAL	250000L	/* usec's */

/* code_table is used to translate buffer addresses to the 3270
 * datastream representation
 */
u_char	code_table[64] = {
    0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
};

extern Frame		frame;
Canvas			canvas;
Pixwin			*pixwin;
Cursor			cursor;
Pixfont			*ibmfont;
int			cursor_addr, buffer_addr;
bool			cursor_displayed = FALSE;
bool			cursor_blink_on = TRUE;
bool			cursor_alt = FALSE, cursor_blink = FALSE;
bool			mono_case = FALSE;

/* the following are set from values in the 3270 font */
int			char_width, char_height, char_base;

u_char			screen_buf[ROWS * COLS];
bool			formatted = FALSE;	/* set in screen_disp */
struct itimerval	blink_timer;

extern u_char	obuf[], *obptr;
extern u_char	aid;

Notify_value		timer ();
extern Notify_value	canvas_event_proc ();


/*
 * Initialize the screen canvas.  Should only be called once.
 */
screen_init ()
{
    if ((ibmfont = pf_open (FONT3270)) == (Pixfont *) 0) {
	perror ("3270tool: can't open 3270.font");
	exit (1);
    }
    char_width = CHAR_WIDTH;
    char_height = CHAR_HEIGHT;
    char_base = CHAR_BASE;
    canvas = window_create (
	frame, CANVAS,
	CANVAS_AUTO_EXPAND,	FALSE,
	CANVAS_AUTO_SHRINK,	FALSE,
	CANVAS_HEIGHT,		ROW_TO_Y (ROWS + 1) + 4,
	CANVAS_WIDTH,		COL_TO_X (COLS),
	CANVAS_MARGIN,		0,
	CANVAS_RETAINED,	TRUE,
	WIN_HEIGHT,		ROW_TO_Y (ROWS + 1) + 4 + 0,
	WIN_WIDTH,		COL_TO_X (COLS) + 0,
	WIN_EVENT_PROC,		canvas_event_proc,
	WIN_CONSUME_PICK_EVENTS,WIN_NO_EVENTS,
				LOC_WINENTER,
				LOC_WINEXIT,
				WIN_MOUSE_BUTTONS,
				WIN_UP_EVENTS,
				0,
	WIN_CONSUME_KBD_EVENTS,	WIN_NO_EVENTS,
				KBD_USE,
				KBD_DONE,
				WIN_ASCII_EVENTS,
				WIN_LEFT_KEYS,
				WIN_RIGHT_KEYS,
				WIN_TOP_KEYS,
				0,
	0
    );
    cursor = window_get (canvas, WIN_CURSOR);
    cursor_set (cursor, CURSOR_OP, PIX_SRC^PIX_DST, 0);
    window_set (canvas, WIN_CURSOR, cursor, 0);
    window_fit (canvas);
    window_fit (frame);
    pixwin = canvas_pixwin (canvas);
    pw_vector (
	pixwin,
	0, ROW_TO_Y (ROWS) + 2,
	COL_TO_X (COLS), ROW_TO_Y (ROWS) + 2,
	PIX_SET, 1
    );
    bzero ((char *) screen_buf, sizeof (screen_buf));
    cursor_addr = 0;
    buffer_addr = 0;
    pw_batch_on (pixwin);
    screen_disp ();
    cursor_on ();
    status_disp (0, CG_BOX4);
    status_disp (1, CG_UNDERA);
    status_disp (2, CG_BOXSOLID);
    blink_timer.it_interval.tv_sec = 0;
    blink_timer.it_interval.tv_usec = TIMER_IVAL;
    blink_timer.it_value.tv_sec = 0;
    blink_timer.it_value.tv_usec = TIMER_IVAL;
    (void) notify_set_itimer_func (&blink_timer, timer, ITIMER_REAL, &blink_timer, ITIMER_NULL);
}


/*
 * Update the status line by displaying "symbol" at column "col".
 */
status_disp (col, symbol)
int	col, symbol;
{
    PW_CHAR (pixwin, COL_TO_X (col), ROW_TO_Y (ROWS) + 4, PIX_SRC, symbol);
}


/*
 * Timer function for implementing cursor blink.
 */
/*ARGSUSED*/
Notify_value
timer (client, which)
Notify_client	client;
int		which;
{
    if (cursor_blink && cursor_displayed) {	/* blink the cursor */
	cursor_blink_on = !cursor_blink_on;
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
    }
    return (NOTIFY_DONE);
}


/*
 * Turn off the timer/cursor-blink so we can safely update the screen.
 */
timer_hold ()
{
    (void) notify_set_itimer_func (&blink_timer, timer, ITIMER_REAL, ITIMER_NULL, ITIMER_NULL);
    if (cursor_blink && cursor_displayed && !cursor_blink_on) {
	cursor_blink_on = TRUE;
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
    }
}


/*
 * Turn on the timer/cursor-blink after we have safely updated the screen.
 */
timer_release ()
{
    cursor_blink_on = TRUE;
    blink_timer.it_interval.tv_sec = 0;
    blink_timer.it_interval.tv_usec = TIMER_IVAL;
    blink_timer.it_value.tv_sec = 0;
    blink_timer.it_value.tv_usec = TIMER_IVAL;
    (void) notify_set_itimer_func (&blink_timer, timer, ITIMER_REAL, &blink_timer, ITIMER_NULL);
}


/*
 * Toggle cursor blink
 */
blink_cursor (on)
bool	on;
{
    timer_hold ();
    cursor_blink = on;
    timer_release ();
}


/*
 * Make the cursor disappear.
 */
cursor_off ()
{
    timer_hold ();
    if (cursor_displayed) {
	cursor_displayed = FALSE;
	pw_batch_on (pixwin);
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
    }
    timer_release ();
    clear_seln ();
}


/*
 * Make the cursor visible.
 */
cursor_on ()
{
    timer_hold ();
    if (!cursor_displayed) {
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
	cursor_displayed = TRUE;
	pw_batch_off (pixwin);
    }
    timer_release ();
}


/*
 * Toggle the cursor (block/underline).
 */
alt_cursor (alt)
bool	alt;
{
    if (alt != cursor_alt) {
	cursor_off ();
	cursor_alt = alt;
	cursor_on ();
    }
}


/*
 * Move the cursor to the specified buffer address.
 */
cursor_move (baddr)
int	baddr;
{
    timer_hold ();
    if (cursor_displayed) {
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
    }
    cursor_addr = baddr;
    if (cursor_displayed) {
	if (cursor_alt)
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr)),
		COL_TO_X (1),
		ROW_TO_Y (1),
		PIX_SET^PIX_DST
	    );
	else
	    pw_writebackground (
		pixwin,
		COL_TO_X (BA_TO_COL (cursor_addr)),
		ROW_TO_Y (BA_TO_ROW (cursor_addr) + 1) - 1,
		COL_TO_X (1),
		1,
		PIX_SET^PIX_DST
	    );
    }
    timer_release ();
}


/*
 * Redraw the entire screen.
 */
screen_disp ()
{
    register int	baddr, sbaddr;
    register u_char	ch;
    bool		is_zero, is_high;

    cursor_off ();
    formatted = FALSE;
    baddr = 0;
    do {
	if (IS_FA (screen_buf[baddr])) {
	    formatted = TRUE;
	    break;
	}
	INC_BA (baddr);
    } while (baddr != 0);
    if (formatted) {	/* formatted display */
	sbaddr = baddr;
	do {
	    is_zero = FA_IS_ZERO (screen_buf[baddr]);
	    is_high = FA_IS_HIGH (screen_buf[baddr]);
	    do {	/* display the field */
		INC_BA (baddr);
		if (is_zero)
		    ch = CG_BLANK;
		else {
		    ch = screen_buf[baddr];
		    /* this if xlates lowercase to uppercase */
		    if (mono_case && ((ch & 0xE0) == 0x40 || (ch & 0xE0) == 0x80))
			ch |= 0x20;
		}
		PW_CHAR (
		    pixwin,
		    COL_TO_X (BA_TO_COL (baddr)),
		    ROW_TO_Y (BA_TO_ROW (baddr)),
		    PIX_SRC, ch
		);
		if (is_high)
		    PW_CHAR (
			pixwin,
			COL_TO_X (BA_TO_COL (baddr)) + 1,
			ROW_TO_Y (BA_TO_ROW (baddr)),
			PIX_SRC|PIX_DST, ch
		    );
	    } while (!IS_FA (screen_buf[baddr]));
	} while (baddr != sbaddr);
    }
    else {		/* unformatted display */
	baddr = 0;
	do {
	    ch = screen_buf[baddr];
	    /* this if xlates lowercase to uppercase */
	    if (mono_case && ((ch & 0xE0) == 0x40 || (ch & 0xE0) == 0x80))
		ch |= 0x20;
	    PW_CHAR (
		pixwin,
		COL_TO_X (BA_TO_COL (baddr)),
		ROW_TO_Y (BA_TO_ROW (baddr)),
		PIX_SRC, ch
	    );
	    INC_BA (baddr);
	} while (baddr != 0);
    }
    cursor_on ();
}


/*
 * Set the formatted screen flag.  A formatted screen is a screen that
 * has at least one field somewhere on it.
 */
set_formatted ()
{
    register int	baddr;

    formatted = FALSE;
    baddr = 0;
    do {
	if (IS_FA (screen_buf[baddr])) {
	    formatted = TRUE;
	    break;
	}
	INC_BA (baddr);
    } while (baddr != 0);
}


/*
 * Find the field attribute for the given buffer address.
 */
u_char	*
get_field_attribute (baddr)
register int	baddr;
{
    static u_char	fake_fa;
    int			sbaddr;

    sbaddr = baddr;
    do {
	if (IS_FA (screen_buf[baddr]))
	    return (&(screen_buf[baddr]));
	DEC_BA (baddr);
    } while (baddr != sbaddr);
    fake_fa = 0xE0;
    return (&fake_fa);
}


/*
 * Update the character on the screen addressed by the given buffer address
 * from the off-screen buffer.
 */
screen_update (baddr, fa)
register int	baddr;
u_char		fa;
{
    register u_char	ch;
    bool		is_zero, is_high;

    is_zero = FA_IS_ZERO (fa);
    is_high = FA_IS_HIGH (fa);
    if (is_zero)
	ch = CG_BLANK;
    else {
	ch = screen_buf[baddr];
	/* this if xlates lowercase to uppercase */
	if (mono_case && ((ch & 0xE0) == 0x40 || (ch & 0xE0) == 0x80))
	    ch |= 0x20;
    }
    PW_CHAR (
	pixwin,
	COL_TO_X (BA_TO_COL (baddr)),
	ROW_TO_Y (BA_TO_ROW (baddr)),
	PIX_SRC, ch
    );
    if (is_high)
	PW_CHAR (
	    pixwin,
	    COL_TO_X (BA_TO_COL (baddr)) + 1,
	    ROW_TO_Y (BA_TO_ROW (baddr)),
	    PIX_SRC|PIX_DST, ch
	);
}


/*
 * Toggle mono-/dual-case mode.
 */
change_case (mono)
bool	mono;
{
    if (mono_case != mono) {
	mono_case = mono;
	screen_disp ();
    }
}


/*
 * Interpret an incoming 3270 datastream.
 */
net_process (buf, buflen)
u_char	*buf;
int	buflen;
{
    switch (buf[0]) {	/* 3270 command */
	case CMD_EAU:	/* erase all unprotected */
	    do_erase_all_unprotected ();
	    break;
	case CMD_EWA:	/* erase/write alternate */
	    /* on 3278-2, same as erase/write.  fallthrough */
	case CMD_EW:	/* erase/write */
	    bzero ((char *) screen_buf, sizeof (screen_buf));
	    buffer_addr = 0;
	    cursor_off ();
	    pw_writebackground (pixwin, 0, 0, COL_TO_X (COLS), ROW_TO_Y (ROWS), PIX_CLR);
	    cursor_move (0);
	    /* fallthrough into write */
	case CMD_W:	/* write */
	    do_write (buf, buflen);
	    break;
	case CMD_RB:	/* read buffer */
	    do_read_buffer ();
	    break;
	case CMD_RM:	/* read modifed */
	    do_read_modified ();
	    break;
	case CMD_NOP:	/* no-op */
	    break;
	default:
	    /* unknown 3270 command */
	    exit (1);
    }
}


/*
 * Process a 3270 Read-Modified command and transmit the data back to the
 * host.
 */
do_read_modified ()
{
    register int	baddr, sbaddr;

    obptr = &obuf[0];
    if (aid != AID_PA1 && aid != AID_PA2
    &&  aid != AID_PA3 && aid != AID_CLEAR) {
	if (aid == AID_SYSREQ) {
	    *obptr++ = 0x01;	/* soh */
	    *obptr++ = 0x5B;	/*  %  */
	    *obptr++ = 0x61;	/*  /  */
	    *obptr++ = 0x02;	/* stx */
	}
	else {
	    *obptr++ = aid;
	    *obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
	    *obptr++ = code_table[cursor_addr & 0x3F];
	}
	baddr = 0;
	if (formatted) {
	    /* find first field attribute */
	    do {
		if (IS_FA (screen_buf[baddr]))
		    break;
		INC_BA (baddr);
	    } while (baddr != 0);
	    sbaddr = baddr;
	    do {
		if (FA_IS_MODIFIED (screen_buf[baddr])) {
		    INC_BA (baddr);
		    *obptr++ = ORDER_SBA;
		    *obptr++ = code_table[(baddr >> 6) & 0x3F];
		    *obptr++ = code_table[baddr & 0x3F];
		    do {
			if (screen_buf[baddr])
			    *obptr++ = cg2ebc[screen_buf[baddr]];
			INC_BA (baddr);
		    } while (!IS_FA (screen_buf[baddr]));
		}
		else {	/* not modified - skip */
		    do {
			INC_BA (baddr);
		    } while (!IS_FA (screen_buf[baddr]));
		}
	    } while (baddr != sbaddr);
	}
	else {
	    do {
		if (screen_buf[baddr])
		    *obptr++ = cg2ebc[screen_buf[baddr]];
		INC_BA (baddr);
	    } while (baddr != 0);
	}
    }
    else
	*obptr++ = aid;
    net_output (obuf, obptr - obuf);
}


/*
 * Process a 3270 Read-Buffer command and transmit the data back to the
 * host.
 */
do_read_buffer ()
{
    register int	baddr;
    u_char		fa;

    obptr = &obuf[0];
    *obptr++ = aid;
    *obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
    *obptr++ = code_table[cursor_addr & 0x3F];
    baddr = 0;
    do {
	if (IS_FA (screen_buf[baddr])) {
	    *obptr++ = ORDER_SF;
	    fa = 0x00;
	    if (FA_IS_PROTECTED (screen_buf[baddr]))
		fa |= 0x20;
	    if (FA_IS_NUMERIC (screen_buf[baddr]))
		fa |= 0x10;
	    if (FA_IS_MODIFIED (screen_buf[baddr]))
		fa |= 0x01;
	    fa |= ((screen_buf[baddr] | FA_INTENSITY) << 2);
	    *obptr++ = fa;
	}
	else
	    *obptr++ = cg2ebc[screen_buf[baddr]];
	INC_BA (baddr);
    } while (baddr != 0);
    net_output (obuf, obptr - obuf);
}


/*
 * Process a 3270 Erase All Unprotected command.
 */
do_erase_all_unprotected ()
{
    register int	baddr, sbaddr;
    u_char		fa;
    bool		f;

    cursor_off ();
    if (formatted) {
	/* find first field attribute */
	baddr = 0;
	do {
	    if (IS_FA (screen_buf[baddr]))
		break;
	    INC_BA (baddr);
	} while (baddr != 0);
	sbaddr = baddr;
	f = FALSE;
	do {
	    fa = screen_buf[baddr];
	    if (!FA_IS_PROTECTED (fa)) {
		screen_buf[baddr] &= ~FA_MODIFY;
		do {
		    INC_BA (baddr);
		    if (!f) {
			cursor_move (baddr);
			f = TRUE;
		    }
		    if (!IS_FA (screen_buf[baddr])) {
			screen_buf[baddr] = CG_NULLBLANK;
			screen_update (baddr, fa);
		    }
		} while (!IS_FA (screen_buf[baddr]));
	    }
	    else {
		do {
		    INC_BA (baddr);
		} while (!IS_FA (screen_buf[baddr]));
	    }
	} while (baddr != sbaddr);
	if (!f)
	    cursor_move (0);
    }
    else {
	bzero ((char *) screen_buf, sizeof (screen_buf));
	pw_writebackground (pixwin, 0, 0, COL_TO_X (COLS), ROW_TO_Y (ROWS), PIX_CLR);
	buffer_addr = 0;
	cursor_move (0);
    }
    cursor_on ();
    aid = AID_NO;
    key_Reset ();
}


/*
 * Process a 3270 Write command.
 */
do_write (buf, buflen)
u_char	buf[];
int	buflen;
{
    register u_char	*cp;
    register int	baddr;
    u_char		*current_fa;
    bool		last_cmd;
    bool		wcc_keyboard_restore, wcc_sound_alarm;

    buffer_addr = cursor_addr;
    wcc_sound_alarm = WCC_SOUND_ALARM (buf[1]);
    wcc_keyboard_restore = WCC_KEYBOARD_RESTORE (buf[1]);
    status_disp (8, CG_LOCK);
    status_disp (9, CG_BLANK);
    status_disp (10, CG_CS);
    status_disp (11, CG_CY);
    status_disp (12, CG_CS);
    status_disp (13, CG_CT);
    status_disp (14, CG_CE);
    status_disp (15, CG_CM);
    if (WCC_RESET_MDT (buf[1])) {
	baddr = 0;
	do {
	    if (IS_FA (screen_buf[baddr]))
		screen_buf[baddr] &= ~FA_MODIFY;
	    INC_BA (baddr);
	} while (baddr != 0);
    }
    last_cmd = TRUE;
    cursor_off ();
    current_fa = get_field_attribute (buffer_addr);
    for (cp = &buf[2]; cp < (buf + buflen); cp++) {
	switch (*cp) {
	    case ORDER_GE:	/* graphic escape - ignore */
		last_cmd = TRUE;
		break;
	    case ORDER_SF:	/* start field */
		cp++;		/* skip field attribute */
		screen_buf[buffer_addr] = FA_BASE;
		if (*cp & 0x20)
		    screen_buf[buffer_addr] |= FA_PROTECT;
		if (*cp & 0x10)
		    screen_buf[buffer_addr] |= FA_NUMERIC;
		if (*cp & 0x01)
		    screen_buf[buffer_addr] |= FA_MODIFY;
		screen_buf[buffer_addr] |= (*cp >> 2) & FA_INTENSITY;
		current_fa = &(screen_buf[buffer_addr]);
		screen_update (buffer_addr, *current_fa);
		formatted = TRUE;
		INC_BA (buffer_addr);
		last_cmd = TRUE;
		break;
	    case ORDER_SBA:	/* set buffer address */
		cp += 2;	/* skip buffer address */
		if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
		    buffer_addr = ((*(cp-1) & 0x3F) << 8) | *cp;
		else	/* 12-bit coded */
		    buffer_addr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
		buffer_addr %= (COLS * ROWS);
		current_fa = get_field_attribute (buffer_addr);
		last_cmd = TRUE;
		break;
	    case ORDER_IC:	/* insert cursor */
		cursor_move (buffer_addr);
		last_cmd = TRUE;
		break;
	    case ORDER_PT:	/* program tab */
		baddr = buffer_addr;
		while (TRUE) {
		    if (IS_FA (screen_buf[baddr])
		    &&  (!FA_IS_PROTECTED (screen_buf[baddr]))) {
			current_fa = &screen_buf[baddr];
			INC_BA (baddr);
			buffer_addr = baddr;
			if (!last_cmd) {
			    while (!IS_FA (screen_buf[baddr])) {
				screen_buf[baddr] = CG_NULLBLANK;
				screen_update (baddr, *current_fa);
				INC_BA (baddr);
			    }
			}
			break;
		    }
		    else {
			INC_BA (baddr);
			if (baddr == 0) {
			    buffer_addr = baddr;
			    current_fa = get_field_attribute (baddr);
			    break;
			}
		    }
		}
		last_cmd = TRUE;
		break;
	    case ORDER_RA:	/* repeat to address */
		cp += 2;	/* skip buffer address */
		if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
		    baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
		else	/* 12-bit coded */
		    baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
		baddr %= (COLS * ROWS);
		cp++;		/* skip char to repeat */
		if (*cp == ORDER_GE)
		    cp++;
		if (buffer_addr == baddr) {
		    screen_buf[buffer_addr] = ebc2cg[*cp];
		    screen_update (buffer_addr, *current_fa);
		    INC_BA (buffer_addr);
		}
		while (buffer_addr != baddr) {
		    screen_buf[buffer_addr] = ebc2cg[*cp];
		    screen_update (buffer_addr, *current_fa);
		    INC_BA (buffer_addr);
		}
		current_fa = get_field_attribute (buffer_addr);
		last_cmd = TRUE;
		break;
	    case ORDER_EUA:	/* erase unprotected to address */
		cp += 2;	/* skip buffer address */
		if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
		    baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
		else	/* 12-bit coded */
		    baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
		baddr %= (COLS * ROWS);
		do {
		    if (IS_FA (screen_buf[buffer_addr]))
			current_fa = &(screen_buf[buffer_addr]);
		    else if (!FA_IS_PROTECTED (*current_fa)) {
			screen_buf[buffer_addr] = CG_NULLBLANK;
			screen_update (buffer_addr, *current_fa);
		    }
		    INC_BA (buffer_addr);
		} while (buffer_addr != baddr);
		current_fa = get_field_attribute (buffer_addr);
		last_cmd = TRUE;
		break;
	    case ORDER_MF:	/* modify field */
	    case ORDER_SFE:	/* start field extended */
	    case ORDER_SA:	/* set attribute */
		/* unsupported 3270 order */
		break;
	    default:	/* enter character */
		screen_buf[buffer_addr] = ebc2cg[*cp];
		screen_update (buffer_addr, *current_fa);
		INC_BA (buffer_addr);
		last_cmd = FALSE;
		break;
	}
    }
    cursor_on ();
    set_formatted ();
    if (wcc_keyboard_restore) {
	key_Reset ();
	aid = AID_NO;
    }
    if (wcc_sound_alarm)
	window_bell (canvas);
}
