/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 *
 *                         All Rights Reserved
 */

/*
 *	kybd.c
 *		This module handles the keyboard for the 3270 emulator.
 */
#include <sys/types.h>
#include <suntool/sunview.h>
#include <suntool/menu.h>
#include <suntool/canvas.h>
#include <suntool/seln.h>
#include <suntool/panel.h>
#include <sundev/kbio.h>
#include <sundev/kbd.h>
#include <fcntl.h>
#include <stdio.h>
#include "3270.h"
#include "3270_enc.h"

/*
 * The following table is used to translate ascii key codes to 3270
 * character generator symbol.  Note that this is not an ebcdic
 * translation.  See xlate.h for details.
 */
u_char	asc2cg[128] = {
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,	CG_CENT,
	CG_SOLIDBAR,	CG_NULLBLANK,	CG_NULLBLANK,	CG_NULLBLANK,
	CG_BLANK,	CG_EXCLAMATION,	CG_DQUOTE,	CG_NUMBER,
	CG_DOLLAR,	CG_PERCENT,	CG_AMPERSAND,	CG_SQUOTE,
	CG_LPAREN,	CG_RPAREN,	CG_ASTERISK,	CG_PLUS,
	CG_COMMA,	CG_MINUS,	CG_PERIOD,	CG_FSLASH,
	CG_ZERO,	CG_ONE,		CG_TWO,		CG_THREE,
	CG_FOUR,	CG_FIVE,	CG_SIX,		CG_SEVEN,
	CG_EIGHT,	CG_NINE,	CG_COLON,	CG_SEMICOLON,
	CG_LESS,	CG_EQUAL,	CG_GREATER,	CG_QUESTION,
	CG_AT,		CG_CA,		CG_CB,		CG_CC,
	CG_CD,		CG_CE,		CG_CF,		CG_CG,
	CG_CH,		CG_CI,		CG_CJ,		CG_CK,
	CG_CL,		CG_CM,		CG_CN,		CG_CO,
	CG_CP,		CG_CQ,		CG_CR,		CG_CS,
	CG_CT,		CG_CU,		CG_CV,		CG_CW,
	CG_CX,		CG_CY,		CG_CZ,		CG_LBRACKET,
	CG_BSLASH,	CG_RBRACKET,	CG_NOT,		CG_UNDERSCORE,
	CG_GRAVE,	CG_LA,		CG_LB,		CG_LC,
	CG_LD,		CG_LE,		CG_LF,		CG_LG,
	CG_LH,		CG_LI,		CG_LJ,		CG_LK,
	CG_LL,		CG_LM,		CG_LN,		CG_LO,
	CG_LP,		CG_LQ,		CG_LR,		CG_LS,
	CG_LT,		CG_LU,		CG_LV,		CG_LW,
	CG_LX,		CG_LY,		CG_LZ,		CG_LBRACE,
	CG_BROKENBAR,	CG_RBRACE,	CG_TILDE,	CG_NULLBLANK
};

u_char	cg2asc[256] = {
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	'>',	'<',	'[',	']',	')',	'(',	'}',	'{',
	' ',	'=',	'\'',	'"',	'/',	'\\',	'|',	'|',
	'?',	'!',	'$',	'c',	'L',	'Y',	'P',	'o',
	'0',	'1',	'2',	'3',	'4',	'5',	'6',	'7',
	'8',	'9',	'B',	'S',	'#',	'@',	'%',	'_',
	'&',	'-',	'.',	',',	':',	'+',	'^',	'~',
	'*',	'^',	'^',	'~',	'~',	'`',	'\'',	',',
	'a',	'e',	'i',	'o',	'u',	'a',	'o',	'y',
	'a',	'e',	'e',	'i',	'o',	'u',	'u',	'c',
	'a',	'e',	'i',	'o',	'u',	'a',	'e',	'i',
	'o',	'u',	'a',	'e',	'i',	'o',	'u',	'n',
	'A',	'E',	'I',	'O',	'U',	'A',	'O',	'Y',
	'A',	'E',	'E',	'I',	'O',	'U',	'Y',	'C',
	'A',	'E',	'I',	'O',	'U',	'A',	'E',	'I',
	'O',	'U',	'A',	'E',	'I',	'O',	'U',	'N',
	'a',	'b',	'c',	'd',	'e',	'f',	'g',	'h',
	'i',	'j',	'k',	'l',	'm',	'n',	'o',	'p',
	'q',	'r',	's',	't',	'u',	'v',	'w',	'x',
	'y',	'z',	'a',	'0',	'a',	'c',	';',	'*',
	'A',	'B',	'C',	'D',	'E',	'F',	'G',	'H',
	'I',	'J',	'K',	'L',	'M',	'N',	'O',	'P',
	'Q',	'R',	'S',	'T',	'U',	'V',	'W',	'X',
	'Y',	'Z',	'A',	'0',	'A',	'C',	';',	'*',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
	' ',	' ',	' ',	' ',	' ',	' ',	' ',	' ',
};

bool		kybdlock = FALSE,	/* kybd locked */
		insert = FALSE;		/* insert mode */
u_char		aid = AID_NO;		/* current attention ID */
Menu		Key_menu;

extern u_char		screen_buf[ROWS * COLS];
extern int		cursor_addr, buffer_addr;
extern Pixwin		*pixwin;
extern Canvas		canvas;
extern Pixfont		*ibmfont;
extern Frame		frame;
extern int		net_sock;
extern bool		formatted, cursor_alt, cursor_blink, mono_case;
extern Seln_client	s_client;
extern int		char_width, char_height, char_base;

extern int	key_panel_toggle ();
extern u_char	*get_field_attribute ();
extern int	stuff_seln ();


/*
 * Toggle insert mode.
 */
insert_mode (on)
bool	on;
{
    if (on) {
	insert = TRUE;
	status_disp (51, CG_INSERT);
    }
    else {
	insert = FALSE;
	status_disp (51, CG_BLANK);
    }
}


/*
 * Update shift mode indicator.  Not used currently, until I can figure out
 * how to reliably read the state of the CAPS lock key.
 */
update_shift ()
{
    int		on, v;

    v = (int) window_get (canvas, WIN_EVENT_STATE, SHIFT_LEFT);
    printf ("SHIFT_LEFT=%d ", v);
    on = v;

    v = (int) window_get (canvas, WIN_EVENT_STATE, SHIFT_RIGHT);
    printf ("SHIFT_RIGHT=%d ", v);
    on |= v;

    v = (int) window_get (canvas, WIN_EVENT_STATE, SHIFT_LOCK);
    printf ("SHIFT_LOCK=%d ", v);
    on |= v;

    v = (int) window_get (canvas, WIN_EVENT_STATE, SHIFT_CAPSLOCK);
    printf ("SHIFT_CAPSLOCK=%d\n", v);
    on |= v;

    status_disp (41, on ? CG_UPSHIFT : CG_BLANK);
}


/*
 * Handle an AID (Attention IDentifier) key.  This is the common stuff that
 * gets executed for all AID keys (PFs, PAs, Clear and etc).
 */
key_AID (aid_code)
int	aid_code;
{
    status_disp (1, CG_BLANK);
    status_disp (8, CG_LOCK);
    status_disp (9, CG_BLANK);
    status_disp (10, CG_CLOCKLEFT);
    status_disp (11, CG_CLOCKRIGHT);
    insert_mode (FALSE);
    kybdlock = TRUE;
    aid = aid_code;
    do_read_modified ();
    status_disp (1, CG_UNDERA);
}


/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 */
bool
key_Character (cgcode)
int	cgcode;
{
    register int	baddr, end_baddr, t_baddr;
    register u_char	*fa;

    if (kybdlock)
	return (FALSE);
    baddr = cursor_addr;
    fa = get_field_attribute (baddr);
    if (IS_FA (screen_buf[baddr]) || FA_IS_PROTECTED (*fa)) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	kybdlock = TRUE;
	return (FALSE);
    }
    else {
	if (FA_IS_NUMERIC (*fa)
	&&  !((cgcode >= CG_ZERO && cgcode <= CG_NINE) || cgcode == CG_MINUS || cgcode == CG_PERIOD)) {
	    status_disp (8, CG_LOCK);
	    status_disp (9, CG_BLANK);
	    status_disp (10, CG_HUMAN);
	    status_disp (11, CG_CN);
	    status_disp (12, CG_CU);
	    status_disp (13, CG_CM);
	    kybdlock = TRUE;
	    return (FALSE);
	}
	else {
	    if (insert && screen_buf[baddr]) {
		/* find next null or next fa */
		end_baddr = baddr;
		do {
		    INC_BA (end_baddr);
		    if (screen_buf[end_baddr] == CG_NULLBLANK
		    ||  IS_FA (screen_buf[end_baddr]))
			break;
		} while (end_baddr != baddr);
		if (screen_buf[end_baddr] != CG_NULLBLANK) {
		    status_disp (8, CG_LOCK);
		    status_disp (9, CG_BLANK);
		    status_disp (10, CG_HUMAN);
		    status_disp (11, CG_GREATER);
		    kybdlock = TRUE;
		    return (FALSE);
		}
		else {
		    if (end_baddr > baddr)
			bcopy ((char *) &screen_buf[baddr], (char *) &screen_buf[baddr+1], end_baddr - baddr);
		    else {
			bcopy ((char *) &screen_buf[0], (char *) &screen_buf[1], end_baddr);
			screen_buf[0] = screen_buf[(ROWS * COLS) - 1];
			bcopy ((char *) &screen_buf[baddr], (char *) &screen_buf[baddr+1], ((ROWS * COLS) - 1) - baddr);
		    }
		    screen_buf[baddr] = cgcode;
		    cursor_off ();
		    t_baddr = baddr;
		    while (t_baddr != end_baddr) {
			screen_update (t_baddr, *fa);
			INC_BA (t_baddr);
		    }
		    screen_update (t_baddr, *fa);
		}
	    }
	    else {
		screen_buf[baddr] = cgcode;
		cursor_off ();
		screen_update (baddr, *fa);
	    }
	    INC_BA (baddr);
	    if (IS_FA (screen_buf[baddr])) {
		if (FA_IS_NUMERIC (screen_buf[baddr])) {
		    if (FA_IS_PROTECTED (screen_buf[baddr])) {
			/* skip to next unprotected */
			while (TRUE) {
			    INC_BA (baddr);
			    if (IS_FA (screen_buf[baddr])
			    &&  !FA_IS_PROTECTED (screen_buf[baddr]))
				break;
			}
			INC_BA (baddr);
		    }
		    else
			INC_BA (baddr);
		}
		else
		    INC_BA (baddr);
	    }
	    cursor_move (baddr);
	    cursor_on ();
	    *fa |= FA_MODIFY;
	}
    }
    return (TRUE);
}


/*
 * Toggle underline/block cursor.
 */
key_AltCr ()
{
    alt_cursor ((bool) (!cursor_alt));
}


/*
 * Toggle blink/no-blink cursor.
 */
key_CursorBlink ()
{
    blink_cursor ((bool) (!cursor_blink));
}


/*
 * Toggle mono-/dual-case operation.
 */
key_MonoCase ()
{
    change_case ((bool) (!mono_case));
}


/*
 * Tab forward to next field.
 */
key_FTab ()
{
    register int	baddr, nbaddr;

    if (kybdlock)
	return;
    nbaddr = cursor_addr;
    INC_BA (nbaddr);
    while (TRUE) {
	baddr = nbaddr;
	INC_BA (nbaddr);
	if (IS_FA (screen_buf[baddr])
	&&  !FA_IS_PROTECTED (screen_buf[baddr])
	&&  !IS_FA (screen_buf[nbaddr]))
	    break;
	if (baddr == cursor_addr) {
	    cursor_move (0);
	    return;
	}
    }
    INC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Tab backward to previous field.
 */
key_BTab ()
{
    register int	baddr, nbaddr;
    int			sbaddr;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    DEC_BA (baddr);
    if (IS_FA (screen_buf[baddr]))	/* at bof */
	DEC_BA (baddr);
    sbaddr = baddr;
    while (TRUE) {
	nbaddr = baddr;
	INC_BA (nbaddr);
	if (IS_FA (screen_buf[baddr])
	&&  !FA_IS_PROTECTED (screen_buf[baddr])
	&&  !IS_FA (screen_buf[nbaddr]))
	    break;
	DEC_BA (baddr);
	if (baddr == sbaddr) {
	    cursor_move (0);
	    return;
	}
    }
    INC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Reset keyboard lock.
 */
key_Reset ()
{
    register int	i;

    kybdlock = FALSE;
    insert_mode (FALSE);
    for (i = 0; i < 9; i++)
	status_disp (i + 8, CG_BLANK);
}


/*
 * Move to first unprotected field on screen.
 */
key_Home ()
{
    register int	baddr, nbaddr;
    register u_char	*fa;

    if (kybdlock)
	return;
    fa = get_field_attribute (0);
    if (!FA_IS_PROTECTED (*fa))
	cursor_move (0);
    else {
	nbaddr = 1;		/* start at 2nd col, 1st col is fa */
	while (TRUE) {
	    baddr = nbaddr;
	    INC_BA (nbaddr);
	    if (IS_FA (screen_buf[baddr])
	    &&  !FA_IS_PROTECTED (screen_buf[baddr])
	    &&  !IS_FA (screen_buf[nbaddr]))
		break;
	    if (baddr == 0) {
		cursor_move (0);
		return;
	    }
	}
	INC_BA (baddr);
	cursor_move (baddr);
    }
}


/*
 * Cursor left 1 position.
 */
key_Left ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    DEC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Cursor right 1 position.
 */
key_Right ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    INC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Cursor left 2 positions.
 */
key_Left2 ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    DEC_BA (baddr);
    DEC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Cursor right 2 positions.
 */
key_Right2 ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    INC_BA (baddr);
    INC_BA (baddr);
    cursor_move (baddr);
}


/*
 * Cursor up 1 position.
 */
key_Up ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = cursor_addr - COLS;
    if (baddr < 0)
	baddr = (cursor_addr + (ROWS * COLS)) - COLS;
    cursor_move (baddr);
}


/*
 * Cursor down 1 position.
 */
key_Down ()
{
    register int	baddr;

    if (kybdlock)
	return;
    baddr = (cursor_addr + COLS) % (COLS * ROWS);
    cursor_move (baddr);
}


/*
 * Cursor to first field on next line or any lines after that.
 */
key_Newline ()
{
    register int	baddr;
    register u_char	*fa;

    if (kybdlock)
	return;
    baddr = (cursor_addr + COLS) % (COLS * ROWS);	/* down */
    baddr = (baddr / COLS) * COLS;			/* 1st col */
    fa = get_field_attribute (baddr);
    if (fa != (&screen_buf[baddr]) && !FA_IS_PROTECTED (*fa))
	cursor_move (baddr);
    else {	/* find next unprotected */
	if (fa == (&screen_buf[baddr]) && !FA_IS_PROTECTED (*fa)) {
	    INC_BA (baddr);
	}
	else {
	    while (TRUE) {
		INC_BA (baddr);
		if (IS_FA (screen_buf[baddr])
		&&  !FA_IS_PROTECTED (screen_buf[baddr]))
		    break;
		if (baddr == cursor_addr) {
		    cursor_move (0);
		    return;
		}
	    }
	    INC_BA (baddr);
	}
	cursor_move (baddr);
    }
}


/*
 * DUP key
 */
key_Dup ()
{
    register int	baddr, nbaddr;

    if (key_Character (CG_DUP)) {
	nbaddr = cursor_addr;
	INC_BA (nbaddr);
	while (TRUE) {
	    baddr = nbaddr;
	    INC_BA (nbaddr);
	    if (IS_FA (screen_buf[baddr])
	    &&  !FA_IS_PROTECTED (screen_buf[baddr])
	    &&  !IS_FA (screen_buf[nbaddr]))
		break;
	    if (baddr == cursor_addr) {
		cursor_move (0);
		return;
	    }
	}
	INC_BA (baddr);
	cursor_move (baddr);
    }
}


/*
 * FM key
 */
key_FieldMark ()
{
    (void) key_Character (CG_FM);
}


/*
 * Enter AID key.
 */
key_Enter ()
{
    if (kybdlock)
	return;
    key_AID (AID_ENTER);
}


/*
 * PA1 AID key
 */
key_PA1 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PA1);
}


/*
 * PA2 AID key
 */
key_PA2 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PA2);
}


/*
 * PA3 AID key
 */
key_PA3 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PA3);
}


/*
 * PF1 AID key
 */
key_PF1 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF1);
}


/*
 * PF2 AID key
 */
key_PF2 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF2);
}


/*
 * PF3 AID key
 */
key_PF3 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF3);
}


/*
 * PF4 AID key
 */
key_PF4 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF4);
}


/*
 * PF5 AID key
 */
key_PF5 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF5);
}


/*
 * PF6 AID key
 */
key_PF6 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF6);
}


/*
 * PF7 AID key
 */
key_PF7 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF7);
}


/*
 * PF8 AID key
 */
key_PF8 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF8);
}


/*
 * PF9 AID key
 */
key_PF9 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF9);
}


/*
 * PF10 AID key
 */
key_PF10 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF10);
}


/*
 * PF11 AID key
 */
key_PF11 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF11);
}


/*
 * PF12 AID key
 */
key_PF12 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF12);
}


/*
 * PF13 AID key
 */
key_PF13 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF13);
}


/*
 * PF14 AID key
 */
key_PF14 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF14);
}


/*
 * PF15 AID key
 */
key_PF15 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF15);
}


/*
 * PF16 AID key
 */
key_PF16 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF16);
}


/*
 * PF17 AID key
 */
key_PF17 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF17);
}


/*
 * PF18 AID key
 */
key_PF18 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF18);
}


/*
 * PF19 AID key
 */
key_PF19 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF19);
}


/*
 * PF20 AID key
 */
key_PF20 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF20);
}


/*
 * PF21 AID key
 */
key_PF21 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF21);
}


/*
 * PF22 AID key
 */
key_PF22 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF22);
}


/*
 * PF23 AID key
 */
key_PF23 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF23);
}


/*
 * PF24 AID key
 */
key_PF24 ()
{
    if (kybdlock)
	return;
    key_AID (AID_PF24);
}


/*
 * System Request AID key
 */
key_SysReq ()
{
    if (kybdlock)
	return;
    key_AID (AID_SYSREQ);
}


/*
 * Clear AID key
 */
key_Clear ()
{
    if (kybdlock)
	return;
    bzero ((char *) screen_buf, sizeof (screen_buf));
    buffer_addr = 0;
    cursor_off ();
    pw_writebackground (pixwin, 0, 0, COL_TO_X (COLS), ROW_TO_Y (ROWS), PIX_CLR);
    cursor_move (0);
    cursor_on ();
    key_AID (AID_CLEAR);
}


/*
 * Cursor Select key (light pen simulator).
 */
key_CursorSelect ()
{
    register u_char	*fa, *sel;

    if (kybdlock)
	return;
    fa = get_field_attribute (cursor_addr);
    if (!FA_IS_SELECTABLE (*fa)) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	kybdlock = TRUE;
    }
    else {
	sel = fa + 1;
	switch (*sel) {
	    case CG_GREATER:		/* > */
		*sel = CG_QUESTION;	/* change to ? */
		screen_update (sel - screen_buf, *fa);
		*fa &= ~FA_MODIFY;	/* clear mdt */
		break;
	    case CG_QUESTION:		/* ? */
		*sel = CG_GREATER;	/* change to > */
		screen_update (sel - screen_buf, *fa);
		*fa |= FA_MODIFY;	/* set mdt */
		break;
	    case CG_BLANK:		/* space */
	    case CG_NULLBLANK:		/* null */
		key_AID (AID_SELECT);
		break;
	    case CG_AMPERSAND:		/* & */
		key_AID (AID_ENTER);
		break;
	    default:
		status_disp (8, CG_LOCK);
		status_disp (9, CG_BLANK);
		status_disp (10, CG_LEFTARROW);
		status_disp (11, CG_HUMAN);
		status_disp (12, CG_RIGHTARROW);
		kybdlock = TRUE;
	}
    }
}


/*
 * Erase End Of Field Key.
 */
key_EraseEOF ()
{
    register int	baddr;
    register u_char	*fa;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    fa = get_field_attribute (baddr);
    if (FA_IS_PROTECTED (*fa) || IS_FA (screen_buf[baddr])) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	kybdlock = TRUE;
    }
    else {
	if (formatted) {	/* erase to next field attribute */
	    cursor_off ();
	    do {
		screen_buf[baddr] = CG_NULLBLANK;
		screen_update (baddr, *fa);
		INC_BA (baddr);
	    } while (!IS_FA (screen_buf[baddr]));
	    *fa |= FA_MODIFY;
	    cursor_on ();
	}
	else {	/* erase to end of screen */
	    cursor_off ();
	    do {
		screen_buf[baddr] = CG_NULLBLANK;
		screen_update (baddr, *fa);
		INC_BA (baddr);
	    } while (baddr != 0);
	    cursor_on ();
	}
    }
}


/*
 * Erase all Input Key.
 */
key_EraseInput ()
{
    register int	baddr, sbaddr;
    u_char		fa;
    bool		f;

    if (kybdlock)
	return;
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
		}
		while (!IS_FA (screen_buf[baddr]));
	    }
	    else {	/* skip protected */
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
	pw_writebackground (
	    pixwin,
	    0, 0, COL_TO_X (COLS), ROW_TO_Y (ROWS),
	    PIX_CLR
	);
	cursor_move (0);
    }
    cursor_on ();
}


/*
 * Delete char key.
 */
key_Delete ()
{
    register int	baddr, end_baddr, t_baddr;
    register u_char	*fa;

    if (kybdlock)
	return;
    baddr = cursor_addr;
    fa = get_field_attribute (baddr);
    if (FA_IS_PROTECTED (*fa) || IS_FA (screen_buf[baddr])) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	kybdlock = TRUE;
    }
    else {
	/* find next fa */
	end_baddr = baddr;
	do {
	    INC_BA (end_baddr);
	    if (IS_FA (screen_buf[end_baddr]))
		break;
	} while (end_baddr != baddr);
	DEC_BA (end_baddr);
	if (end_baddr > baddr)
	    bcopy ((char *) &screen_buf[baddr+1], (char *) &screen_buf[baddr], end_baddr - baddr);
	else {
	    bcopy ((char *) &screen_buf[baddr+1], (char *) &screen_buf[baddr], ((ROWS * COLS) - 1) - baddr);
	    screen_buf[(ROWS * COLS) - 1] = screen_buf[0];
	    bcopy ((char *) &screen_buf[1], (char *) &screen_buf[0], end_baddr);
	}
	screen_buf[end_baddr] = CG_NULLBLANK;
	cursor_off ();
	t_baddr = baddr;
	while (t_baddr != end_baddr) {
	    screen_update (t_baddr, *fa);
	    INC_BA (t_baddr);
	}
	screen_update (t_baddr, *fa);
	cursor_on ();
    }
}


/*
 * Set insert mode key.
 */
key_Insert ()
{
    if (kybdlock)
	return;
    insert_mode (TRUE);
}


/*
 * Catch and dispatch events coming in from suntools.  There are currently
 * two versions of this routine, one for type 3 keyboards (on Sun3's) and
 * one for type 4 keyboards (on Sun 386i's).  This should really be revamped
 * and made more general using a user customizable key definition configuration
 * file.  Mouse actions are also handled in here.
 */
/*ARGSUSED*/
Notify_value
canvas_event_proc (win, event, arg)
Window	win;
Event	*event;
caddr_t	arg;
{
    register int	baddr;
    int			cgcode;

#ifdef DEBUG
	printf (
	    "event: code=%d, flags=0x%x, shiftmask=0x%x, x,y=%d,%d\n",
	    event->ie_code, event->ie_flags, event->ie_shiftmask,
	    event->ie_locx, event->ie_locy
	);
#endif
    switch (event_id (event)) {

	/* function keys on top of keyboard */
#ifdef TYPE4KBD
	case KEY_TOP(1):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
	        key_AltCr ();
	    else
	        key_PF1 ();
	    break;
	case KEY_TOP(2):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_CursorBlink ();
	    else
		key_PF2 ();
	    break;
	case KEY_TOP(3):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_Reset ();
	    else
		key_PF3 ();
	    break;
	case KEY_TOP(4):
	    key_PF4 ();
	    break;
	case KEY_TOP(5):
	    key_PF5 ();
	    break;
	case KEY_TOP(6):
	    key_PF6 ();
	    break;
	case KEY_TOP(7):
	    key_PF7 ();
	    break;
	case KEY_TOP(8):
	    key_PF8 ();
	    break;
	case KEY_TOP(9):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_MonoCase ();
	    else
		key_PF9 ();
	    break;
	case KEY_TOP(10):
	    key_PF10 ();
	    break;
	case KEY_TOP(11):
	    key_PF11 ();
	    break;
	case KEY_TOP(12):
	    key_PF12 ();
	    break;
#else
	case KEY_TOP(1):
	    key_AltCr ();
	    break;
	case KEY_TOP(2):
	    key_CursorBlink ();
	    break;
	case KEY_TOP(3):
	    key_Reset ();
	    break;
	case KEY_TOP(9):
	    key_MonoCase ();
	    break;
#endif

	/* function keys on right of keyboard */
	case KEY_RIGHT(1):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PA1 ();
	    else
		key_Dup ();
	    break;
	case KEY_RIGHT(2):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PA2 ();
	    else
		key_FieldMark ();
	    break;
	case KEY_RIGHT(3):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_Clear ();
	    else
		key_CursorSelect ();
	    break;
	case KEY_RIGHT(4):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF13 ();
	    else
		key_EraseEOF ();
	    break;
	case KEY_RIGHT(5):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF14 ();
	    else
		key_EraseInput ();
	    break;
	case KEY_RIGHT(6):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF15 ();
	    else
		key_SysReq ();
	    break;
	case KEY_RIGHT(7):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF16 ();
	    else
		key_Delete ();
	    break;
	case KEY_RIGHT(8):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF17 ();
	    else
		key_Up ();
	    break;
	case KEY_RIGHT(9):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF18 ();
	    else
		key_Insert ();
	    break;
	case KEY_RIGHT(10):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF19 ();
	    else
		key_Left ();
	    break;
	case KEY_RIGHT(11):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF20 ();
	    else
		key_Home ();
	    break;
	case KEY_RIGHT(12):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF21 ();
	    else
		key_Right ();
	    break;
	case KEY_RIGHT(13):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF22 ();
	    else
		key_Left2 ();
	    break;
	case KEY_RIGHT(14):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF23 ();
	    else
		key_Down ();
	    break;
	case KEY_RIGHT(15):
	    if (event_shiftmask (event) & META_SHIFT_MASK)
		key_PF24 ();
	    else
		key_Right2 ();
	    break;

	/* mouse events */
	case MS_LEFT:	/* left mouse move cursor under pointer */
	    if (kybdlock)
		return (NOTIFY_IGNORED);
	    baddr = ROWCOL_TO_BA(Y_TO_ROW (event_y (event)), 
				 X_TO_COL (event_x (event)));
	    while (baddr >= (COLS * ROWS))
		baddr -= COLS;
	    cursor_move (baddr);
	    break;
	case MS_MIDDLE:	/* middle mouse sets selections */
	    baddr = ROWCOL_TO_BA(Y_TO_ROW (event_y (event)), 
				 X_TO_COL (event_x (event)));
	    while (baddr >= (COLS * ROWS))
		baddr -= COLS;
	    set_seln (cursor_addr, baddr);
	    break;
	case MS_RIGHT:	/* right mouse pops up canvas menu */
	    if (event_is_down (event))
		menu_show (Key_menu, win, event, 0);
	    break;
	case LOC_RGNENTER:	/* mouse entered window */
	    if (seln_acquire (s_client, SELN_CARET) != SELN_CARET)
		fprintf (stderr, "can't acquire SELN_CARET!\n");
	    break;

#ifdef DEBUG
	/* things we need to catch in order for stuff to work
	 * right, but we don't need to do anything explicitely
	 * with them.
	 */
	case LOC_RGNEXIT:
	case KBD_USE:
	case KBD_DONE:
	case WIN_REPAINT:
	case WIN_RESIZE:
	    break;

	/* the system seems to pass these on AFTER it's
	 * finished processing them.  so we gotta ignore them.
	 */
	case KEY_LEFT(1):	/* STOP */
	case KEY_LEFT(2):	/* AGAIN */
	case KEY_LEFT(3):	/* PROPS */
	case KEY_LEFT(4):	/* UNDO */
	case KEY_LEFT(5):	/* EXPOSE */
	case KEY_LEFT(6):	/* PUT */
	case KEY_LEFT(7):	/* OPEN */
	case KEY_LEFT(8):	/* GET */
	case KEY_LEFT(9):	/* FIND */
	case KEY_LEFT(10):	/* DELETE */
	    break;
#endif

	/* miscellany (character keys) */
	default:
	    if ((event_shiftmask (event) & META_SHIFT_MASK)) {
		event_set_id (event, (event_id (event) & ~0x80));
		switch (event_id (event)) {
		    case 0x7F:	/* delete */
		    case 0x08:	/* backspace */
			/* alt-delete or alt-backspace is home */
			key_Home ();
			break;
		    case '1':
			key_PF1 ();
			break;
		    case '2':
			key_PF2 ();
			break;
		    case '3':
			key_PF3 ();
			break;
		    case '4':
			key_PF4 ();
			break;
		    case '5':
			key_PF5 ();
			break;
		    case '6':
			key_PF6 ();
			break;
		    case '7':
			key_PF7 ();
			break;
		    case '8':
			key_PF8 ();
			break;
		    case '9':
			key_PF9 ();
			break;
		    case '0':
			key_PF10 ();
			break;
		    case '-':
			key_PF11 ();
			break;
		    case '=':
			key_PF12 ();
			break;
		    default:
			return (NOTIFY_IGNORED);
			break;
		}
	    }
	    else if ((event_id (event) >= ' ' && event_id (event) <= '~')
	    ||  event_id (event) == 0x1B) {
		/* if a printable key or the escape key */
		if (event_id (event) == 0x1B
		&&  (event_shiftmask (event) & SHIFTMASK) != 0) {
		    /* fake shift-ESC to something unique */
		    event_set_id (event, 0x1C);
		}
		cgcode = asc2cg[event_id (event)];
		if (cgcode == CG_NULLBLANK)
		    return (NOTIFY_IGNORED);
		else
		    (void) key_Character (cgcode);
	    }
	    else if (event_id (event) == 0x08)
		key_Left ();
	    else if (event_id (event) == 0x09)
		key_FTab ();
	    else if (event_id (event) == 0x0A)
		key_Newline ();
	    else if (event_id (event) == 0x0D)
		key_Enter ();
	    else if (event_id (event) == 0x7F)
		key_BTab ();
	    else {
#ifdef DEBUG
		fprintf (stderr, "unknown event: %d\n", event_id (event));
#endif
		return (NOTIFY_IGNORED);
	    }
	    break;
    }
    return (NOTIFY_DONE);
}


/*
 * Initialize the canvas menu.
 */
menu_init ()
{

    Key_menu = menu_create (
	MENU_ITEM,
	    MENU_STRING,		"Show/Hide Key Panel",
	    MENU_ACTION_PROC,		key_panel_toggle,
	    0,
	MENU_PULLRIGHT_ITEM, "Other Keys", menu_create (
	    MENU_ITEM,
		MENU_STRING,		"Reset",
		MENU_ACTION_PROC,	key_Reset,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Erase EOF",
		MENU_ACTION_PROC,	key_EraseEOF,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Erase Inp",
		MENU_ACTION_PROC,	key_EraseInput,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Delete",
		MENU_ACTION_PROC,	key_Delete,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Insert",
		MENU_ACTION_PROC,	key_Insert,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Dup",
		MENU_ACTION_PROC,	key_Dup,
		0,
	    MENU_ITEM,
		MENU_STRING,		"FM",
		MENU_ACTION_PROC,	key_FieldMark,
		0,
	    MENU_ITEM,
		MENU_STRING,		"CursorSel",
		MENU_ACTION_PROC,	key_CursorSelect,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Alternate Cursor",
		MENU_ACTION_PROC,	key_AltCr,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Cursor Blink",
		MENU_ACTION_PROC,	key_CursorBlink,
		0,
	    MENU_ITEM,
		MENU_STRING,		"Monocase",
		MENU_ACTION_PROC,	key_MonoCase,
		0,
	    0
	),
	MENU_ITEM,
	    MENU_STRING,		"Stuff",
	    MENU_ACTION_PROC,		stuff_seln,
	    0,
	0
    );

}
