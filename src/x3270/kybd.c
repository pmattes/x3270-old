/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	kybd.c
 *		This module handles the keyboard for the 3270 emulator.
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "3270ds.h"
#include "cg.h"
#include "globals.h"

/* Externals: tables.c */
extern unsigned char	asc2cg[];

/* Statics */
#define NP	5
static Atom	paste_atom[NP];
static int	n_pasting = 0;
static int	pix = 0;
static Time	paste_time;
static enum	{ NONE, COMPOSE, FIRST } composing = NONE;
static unsigned char pf_xlate[] = { 
	AID_PF1, AID_PF2, AID_PF3, AID_PF4, AID_PF5, AID_PF6,
	AID_PF7, AID_PF8, AID_PF9, AID_PF10, AID_PF11,
	AID_PF12, AID_PF13, AID_PF14, AID_PF15, AID_PF16,
	AID_PF17, AID_PF18, AID_PF19, AID_PF20,
	AID_PF21, AID_PF22, AID_PF23, AID_PF24
};
static unsigned char pa_xlate[] = { 
	AID_PA1, AID_PA2, AID_PA3
};
#define PF_SZ	(sizeof(pf_xlate)/sizeof(pf_xlate[0]))
#define PA_SZ	(sizeof(pa_xlate)/sizeof(pa_xlate[0]))

/* Globals */
Boolean		kybdlock = True,	/* kybd locked */
		insert = False;		/* insert mode */
unsigned char	aid = AID_NO;		/* current attention ID */
unsigned char	aid2 = AID_NO;		/* next attention ID */
Boolean		oia_twait = False, oia_locked = False;

extern unsigned char	*screen_buf;
extern int		cursor_addr, buffer_addr;
extern Boolean		formatted;

/* Composite key mappings. */

enum keytype { LATIN, APL };
struct akeysym {
	KeySym keysym;
	enum keytype keytype;
};
static struct akeysym cc_first;
static struct composite {
	struct akeysym k1, k2;
	KeySym translation;
} *composites = NULL;
static int n_composites = 0;

#define ak_eq(k1, k2)	(((k1).keysym  == (k2).keysym) && \
			 ((k1).keytype == (k2).keytype))


/*
 * Called when a host connects or disconnects.
 */
void
kybd_connect()
{
	kybdlock = !(IN_3270 || IN_ANSI);
}

/*
 * Toggle insert mode.
 */
static void
insert_mode(on)
Boolean on;
{
	insert = on;
	status_insert_mode(on);
}

/*
 * Lock the keyboard because of an operator error.
 */
static void
operator_error(status_fn)
void (*status_fn)();
{
	(*status_fn)();
	mcursor_locked();
	kybdlock = True;
	if (scripting())
		popup_an_error("Keyboard locked");
}


/*
 * Handle an AID (Attention IDentifier) key.  This is the common stuff that
 * gets executed for all AID keys (PFs, PAs, Clear and etc).
 */
void
key_AID(aid_code)
unsigned char	aid_code;
{
	if (IN_ANSI) {
		register int	i;

		if (aid_code == AID_ENTER) {
			net_sendc('\r');
			return;
		}
		for (i = 0; i < PF_SZ; i++)
			if (aid_code == pf_xlate[i]) {
				ansi_send_pf(i+1);
				return;
			}
		for (i = 0; i < PA_SZ; i++)
			if (aid_code == pa_xlate[i]) {
				ansi_send_pa(i+1);
				return;
			}
		return;
	}
	status_twait();
	mcursor_waiting();
	insert_mode(False);
	kybdlock = True;
	oia_twait = True;
	oia_locked = True;
	aid = aid_code;
	ctlr_read_modified();
	ticking_start(False);
	status_ctlr_done();
}

/*ARGSUSED*/
static void
key_PF(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int k;

	if (*num_params != 1) {
		popup_an_error("PF requires 1 argument");
		return;
	}
	k = atoi(params[0]);
	if (k < 0 || k > PF_SZ) {
		popup_an_error("PF: invalid argument");
		return;
	}
	if (!kybdlock)
		key_AID(pf_xlate[k-1]);
}

/*ARGSUSED*/
static void
key_PA(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int k;

	if (*num_params != 1) {
		popup_an_error("PA requires 1 argument");
		return;
	}
	k = atoi(params[0]);
	if (k < 0 || k > PA_SZ) {
		popup_an_error("PA: invalid argument");
		return;
	}
	if (!kybdlock)
		key_AID(pa_xlate[k-1]);
}


/*
 * ATTN key, similar to an AID key but without the read_modified call.
 */
void
key_Attn()
{
	if (!IN_3270 || kybdlock)
		return;
	status_twait();
	mcursor_waiting();
	insert_mode(False);
	kybdlock = True;
	oia_twait = True;
	oia_locked = True;
	net_break();
	ticking_start(False);
	status_ctlr_done();
}



/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 */
static Boolean
key_Character(cgcode)
int	cgcode;
{
	register int	baddr, end_baddr;
	register unsigned char	*fa;

	if (kybdlock)
		return False;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (IS_FA(screen_buf[baddr]) || FA_IS_PROTECTED(*fa)) {
		operator_error(status_protected);
		return False;
	}
	if (FA_IS_NUMERIC(*fa) &&
	    !((cgcode >= CG_0 && cgcode <= CG_9) ||
	    cgcode == CG_minus || cgcode == CG_period)) {
		operator_error(status_numeric);
		return False;
	}
	if (insert && screen_buf[baddr]) {

		/* Find next null or fa. */
		end_baddr = baddr;
		do {
			INC_BA(end_baddr);
			if (screen_buf[end_baddr] == CG_null
			    ||  IS_FA(screen_buf[end_baddr]))
				break;
		} while (end_baddr != baddr);

		/*
		 * If the last character in the field is a space, perhaps
		 * convert it to a null.
		 */
		if (toggled(BLANKFILL) && IS_FA(screen_buf[end_baddr])) {
			int lastf = end_baddr;

			DEC_BA(lastf);
			if (screen_buf[lastf] == CG_space) {
				DEC_BA(end_baddr);

				/* No shift required if only one byte. */
				if (end_baddr == baddr)
					goto shift_done;

				ctlr_add(end_baddr, CG_null);
			}
		}

		/* Check for field overflow. */
		if (screen_buf[end_baddr] != CG_null) {
			operator_error(status_overflow);
			return False;
		}

		/* Shift data over. */
		if (end_baddr > baddr) {
			ctlr_bcopy(baddr, baddr+1, end_baddr - baddr, 0);
		} else {
			ctlr_bcopy(0, 1, end_baddr, 0);
			ctlr_add(0, screen_buf[(ROWS * COLS) - 1]);
			ctlr_bcopy(baddr, baddr+1, ((ROWS * COLS) - 1) - baddr, 0);
		}
	}

    shift_done:
	/* Replace leading nulls with blanks, if desired. */
	if (formatted && toggled(BLANKFILL)) {
		register int	baddr2 = fa - screen_buf;

		INC_BA(baddr2);
		while (baddr2 != baddr) {
			if (screen_buf[baddr2] == CG_null)
				ctlr_add(baddr2, CG_space);
			INC_BA(baddr2);
		}
	}

	/* Add the key (finally!) */
	ctlr_add(baddr, (unsigned char) cgcode);

	/* Implement auto-skip, and don't land on attribute bytes. */
	INC_BA(baddr);
	if (IS_FA(screen_buf[baddr]) &&
	    FA_IS_SKIP(screen_buf[baddr]))
		baddr = next_unprotected(baddr);
	else while (IS_FA(screen_buf[baddr]))
		INC_BA(baddr);

	cursor_move(baddr);
	mdt_set(fa);
	return True;
}

/*
 * Handle an ordinary character key, given an ASCII code.
 */
static void
key_ACharacter(c, keytype)
unsigned char	c;
enum keytype	keytype;
{
	register int i;
	struct akeysym ak;

	ak.keysym = c;
	ak.keytype = keytype;

	switch (composing) {
	    case NONE:
		break;
	    case COMPOSE:
		for (i = 0; i < n_composites; i++)
			if (ak_eq(composites[i].k1, ak) ||
			    ak_eq(composites[i].k2, ak))
				break;
		if (i < n_composites) {
			cc_first.keysym = c;
			cc_first.keytype = keytype;
			composing = FIRST;
			status_compose(True, c);
		} else {
			ring_bell();
			composing = NONE;
			status_compose(False, 0);
		}
		return;
	    case FIRST:
		composing = NONE;
		status_compose(False, 0);
		for (i = 0; i < n_composites; i++)
			if ((ak_eq(composites[i].k1, cc_first) &&
			     ak_eq(composites[i].k2, ak)) ||
			    (ak_eq(composites[i].k1, ak) &&
			     ak_eq(composites[i].k2, cc_first)))
				break;
		if (i < n_composites)
			c = composites[i].translation;
		else {
			ring_bell();
			return;
		}
		break;
	}

	if (IN_3270) {
		if (c == '^' && keytype == LATIN) {
			(void) key_Character(CG_notsign);
		} else {
			(void) key_Character((int) asc2cg[c]);
		}
	} else if (IN_ANSI) {
		net_sendc((char) c);
	}
}


/*
 * Simple toggles.
 */
static void key_AltCr()	{ do_toggle(ALTCURSOR); }
void key_MonoCase()    	{ do_toggle(MONOCASE); }



/*
 * Tab forward to next field.
 */
void
key_FTab()
{
	if (IN_ANSI) {
		net_sendc('\t');
		return;
	}
	if (kybdlock)
		return;
	cursor_move(next_unprotected(cursor_addr));
}


/*
 * Tab backward to previous field.
 */
void
key_BTab()
{
	register int	baddr, nbaddr;
	int		sbaddr;

	if (!IN_3270)
		return;
	if (kybdlock)
		return;
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (IS_FA(screen_buf[baddr]))	/* at bof */
		DEC_BA(baddr);
	sbaddr = baddr;
	while (True) {
		nbaddr = baddr;
		INC_BA(nbaddr);
		if (IS_FA(screen_buf[baddr])
		    &&  !FA_IS_PROTECTED(screen_buf[baddr])
		    &&  !IS_FA(screen_buf[nbaddr]))
			break;
		DEC_BA(baddr);
		if (baddr == sbaddr) {
			cursor_move(0);
			return;
		}
	}
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Reset keyboard lock.
 */
void
key_Reset()
{
	insert_mode(False);
	if (!CONNECTED)
		return;
	kybdlock = False;
	status_reset();
	mcursor_normal();
	oia_twait = False;
	oia_locked = False;
	aid2 = AID_NO;
	composing = NONE;
	status_compose(False, 0);
}


/*
 * Move to first unprotected field on screen.
 */
void
key_Home()
{
	if (IN_ANSI) {
		ansi_send_home();
		return;
	}
	if (kybdlock)
		return;
	if (!formatted) {
		cursor_move(0);
		return;
	}
	cursor_move(next_unprotected(ROWS*COLS-1));
}


/*
 * Cursor left 1 position.
 */
void
key_Left()
{
	register int	baddr;

	if (IN_ANSI) {
		ansi_send_left();
		return;
	}
	if (kybdlock)
		return;
	baddr = cursor_addr;
	DEC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Backspace.
 */
void
key_BackSpace()
{
	if (IN_ANSI) {
		net_send_erase();
	} else {
		key_Left();
	}
}


/*
 * Destructive backspace, like Unix "erase".
 */
void
key_Erase()
{
	int	baddr;
	unsigned char	*fa;

	if (IN_ANSI) {
		net_send_erase();
		return;
	}
	if (kybdlock)
		return;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (fa == &screen_buf[baddr] || FA_IS_PROTECTED(*fa)) {
		operator_error(status_protected);
		return;
	}
	if (baddr && fa == &screen_buf[baddr - 1])
		return;
	key_Left();
	key_Delete();
}


/*
 * Cursor right 1 position.
 */
void
key_Right()
{
	register int	baddr;

	if (IN_ANSI) {
		ansi_send_right();
		return;
	}
	if (kybdlock)
		return;
	baddr = cursor_addr;
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor left 2 positions.
 */
static void
key_Left2()
{
	register int	baddr;

	if (IN_ANSI || kybdlock)
		return;
	baddr = cursor_addr;
	DEC_BA(baddr);
	DEC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor right 2 positions.
 */
static void
key_Right2()
{
	register int	baddr;

	if (IN_ANSI || kybdlock)
		return;
	baddr = cursor_addr;
	INC_BA(baddr);
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor up 1 position.
 */
void
key_Up()
{
	register int	baddr;

	if (IN_ANSI) {
		ansi_send_up();
		return;
	}
	if (kybdlock)
		return;
	baddr = cursor_addr - COLS;
	if (baddr < 0)
		baddr = (cursor_addr + (ROWS * COLS)) - COLS;
	cursor_move(baddr);
}


/*
 * Cursor down 1 position.
 */
void
key_Down()
{
	register int	baddr;

	if (IN_ANSI) {
		ansi_send_down();
		return;
	}
	if (kybdlock)
		return;
	baddr = (cursor_addr + COLS) % (COLS * ROWS);
	cursor_move(baddr);
}


/*
 * Cursor to first field on next line or any lines after that.
 */
void
key_Newline()
{
	register int	baddr;
	register unsigned char	*fa;

	if (IN_ANSI) {
		net_sendc('\n');
		return;
	}
	if (kybdlock)
		return;
	baddr = (cursor_addr + COLS) % (COLS * ROWS);	/* down */
	baddr = (baddr / COLS) * COLS;			/* 1st col */
	fa = get_field_attribute(baddr);
	if (fa != (&screen_buf[baddr]) && !FA_IS_PROTECTED(*fa))
		cursor_move(baddr);
	else
		cursor_move(next_unprotected(baddr));
}


/*
 * DUP key
 */
void
key_Dup()
{
	if (IN_ANSI)
		return;
	if (key_Character(CG_dup))
		cursor_move(next_unprotected(cursor_addr));
}


/*
 * FM key
 */
void
key_FieldMark()
{
	if (IN_ANSI)
		return;
	(void) key_Character(CG_fm);
}


/*
 * Vanilla AID keys.
 */
void key_Enter()  { if (!kybdlock) key_AID(AID_ENTER); }
void key_PA1()    { if (!kybdlock) key_AID(AID_PA1); }
void key_PA2()    { if (!kybdlock) key_AID(AID_PA2); }
void key_PA3()    { if (!kybdlock) key_AID(AID_PA3); }
void key_PF1()    { if (!kybdlock) key_AID(AID_PF1); }
void key_PF2()    { if (!kybdlock) key_AID(AID_PF2); }
void key_PF3()    { if (!kybdlock) key_AID(AID_PF3); }
void key_PF4()    { if (!kybdlock) key_AID(AID_PF4); }
void key_PF5()    { if (!kybdlock) key_AID(AID_PF5); }
void key_PF6()    { if (!kybdlock) key_AID(AID_PF6); }
void key_PF7()    { if (!kybdlock) key_AID(AID_PF7); }
void key_PF8()    { if (!kybdlock) key_AID(AID_PF8); }
void key_PF9()    { if (!kybdlock) key_AID(AID_PF9); }
void key_PF10()   { if (!kybdlock) key_AID(AID_PF10); }
void key_PF11()   { if (!kybdlock) key_AID(AID_PF11); }
void key_PF12()   { if (!kybdlock) key_AID(AID_PF12); }
void key_PF13()   { if (!kybdlock) key_AID(AID_PF13); }
void key_PF14()   { if (!kybdlock) key_AID(AID_PF14); }
void key_PF15()   { if (!kybdlock) key_AID(AID_PF15); }
void key_PF16()   { if (!kybdlock) key_AID(AID_PF16); }
void key_PF17()   { if (!kybdlock) key_AID(AID_PF17); }
void key_PF18()   { if (!kybdlock) key_AID(AID_PF18); }
void key_PF19()   { if (!kybdlock) key_AID(AID_PF19); }
void key_PF20()   { if (!kybdlock) key_AID(AID_PF20); }
void key_PF21()   { if (!kybdlock) key_AID(AID_PF21); }
void key_PF22()   { if (!kybdlock) key_AID(AID_PF22); }
void key_PF23()   { if (!kybdlock) key_AID(AID_PF23); }
void key_PF24()   { if (!kybdlock) key_AID(AID_PF24); }
void key_SysReq() { if (!kybdlock) key_AID(AID_SYSREQ); }


/*
 * Clear AID key
 */
void
key_Clear()
{
	if (IN_ANSI) {
		ansi_send_clear();
		return;
	}
	if (kybdlock && CONNECTED)
		return;
	buffer_addr = 0;
	ctlr_clear();
	cursor_move(0);
	if (CONNECTED)
		key_AID(AID_CLEAR);
}


/*
 * Cursor Select key (light pen simulator).
 */
void
key_CursorSelect()
{
	register unsigned char	*fa, *sel;

	if (IN_ANSI || kybdlock)
		return;
	fa = get_field_attribute(cursor_addr);
	if (!FA_IS_SELECTABLE(*fa)) {
		operator_error(status_protected);
		return;
	}
	sel = fa + 1;
	switch (*sel) {
	    case CG_greater:		/* > */
		ctlr_add(cursor_addr, CG_question); /* change to ? */
		mdt_clear(fa);
		break;
	    case CG_question:		/* ? */
		ctlr_add(cursor_addr, CG_greater);	/* change to > */
		mdt_set(fa);
		break;
	    case CG_space:		/* space */
	    case CG_null:		/* null */
		key_AID(AID_SELECT);
		break;
	    case CG_ampersand:		/* & */
		key_AID(AID_ENTER);
		break;
	    default:
		operator_error(status_protected);
	}
}


/*
 * Erase End Of Field Key.
 */
void
key_EraseEOF()
{
	register int	baddr;
	register unsigned char	*fa;

	if (IN_ANSI || kybdlock)
		return;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(status_protected);
		return;
	}
	if (formatted) {	/* erase to next field attribute */
		do {
			ctlr_add(baddr, CG_null);
			INC_BA(baddr);
		} while (!IS_FA(screen_buf[baddr]));
		mdt_set(fa);
	} else {	/* erase to end of screen */
		do {
			ctlr_add(baddr, CG_null);
			INC_BA(baddr);
		} while (baddr != 0);
	}
}


/*
 * Erase all Input Key.
 */
void
key_EraseInput()
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

	if (IN_ANSI || kybdlock)
		return;
	if (formatted) {
		/* find first field attribute */
		baddr = 0;
		do {
			if (IS_FA(screen_buf[baddr]))
				break;
			INC_BA(baddr);
		} while (baddr != 0);
		sbaddr = baddr;
		f = False;
		do {
			fa = screen_buf[baddr];
			if (!FA_IS_PROTECTED(fa)) {
				mdt_clear(&screen_buf[baddr]);
				do {
					INC_BA(baddr);
					if (!f) {
						cursor_move(baddr);
						f = True;
					}
					if (!IS_FA(screen_buf[baddr])) {
						ctlr_add(baddr, CG_null);
					}
				}		while (!IS_FA(screen_buf[baddr]));
			} else {	/* skip protected */
				do {
					INC_BA(baddr);
				} while (!IS_FA(screen_buf[baddr]));
			}
		} while (baddr != sbaddr);
		if (!f)
			cursor_move(0);
	} else {
		ctlr_clear();
		cursor_move(0);
	}
}


/*
 * Delete char key.
 */
void
key_Delete()
{
	register int	baddr, end_baddr;
	register unsigned char	*fa;

	if (IN_ANSI) {
		net_sendc('\177');
		return;
	}
	if (kybdlock)
		return;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(status_protected);
		return;
	}
	/* find next fa */
	end_baddr = baddr;
	do {
		INC_BA(end_baddr);
		if (IS_FA(screen_buf[end_baddr]))
			break;
	} while (end_baddr != baddr);
	DEC_BA(end_baddr);
	if (end_baddr > baddr) {
		ctlr_bcopy(baddr+1, baddr, end_baddr - baddr, 0);
	} else if (end_baddr != baddr) {
		ctlr_bcopy(baddr+1, baddr, ((ROWS * COLS) - 1) - baddr, 0);
		ctlr_add((ROWS * COLS) - 1, screen_buf[0]);
		ctlr_bcopy(1, 0, end_baddr, 0);
	}
	ctlr_add(end_baddr, CG_null);
	mdt_set(fa);
}


/*
 * Delete word key.  Backspaces the cursor until it hits the front of a word,
 * deletes characters until it hits a blank or null, and deletes all of these
 * but the last.
 *
 * Which is to say, does a ^W.
 */
void
key_DeleteWord()
{
	register int	baddr, baddr2, front_baddr, back_baddr, end_baddr;
	register unsigned char	*fa;

	if (IN_ANSI) {
		net_send_werase();
		return;
	}
	if (kybdlock || !formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);

	/* Make sure we're on a modifiable field. */
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(status_protected);
		return;
	}

	/* Search backwards for a non-blank character. */
	front_baddr = baddr;
	while (screen_buf[front_baddr] == CG_space ||
	       screen_buf[front_baddr] == CG_null)
		DEC_BA(front_baddr);

	/* If we ran into the edge of the field without seeing any non-blanks,
	   there isn't any word to delete; just move the cursor. */
	if (IS_FA(screen_buf[front_baddr])) {
		cursor_move(front_baddr+1);
		return;
	}

	/* front_baddr is now pointing at a non-blank character.  Now search
	   for the first blank to the left of that (or the edge of the field),
	   leaving front_baddr pointing at the the beginning of the word. */
	while (!IS_FA(screen_buf[front_baddr]) &&
	       screen_buf[front_baddr] != CG_space &&
	       screen_buf[front_baddr] != CG_null)
		DEC_BA(front_baddr);
	INC_BA(front_baddr);

	/* Find the end of the word, searching forward for the edge of the
	   field or a non-blank. */
	back_baddr = front_baddr;
	while (!IS_FA(screen_buf[back_baddr]) &&
	       screen_buf[back_baddr] != CG_space &&
	       screen_buf[back_baddr] != CG_null)
		INC_BA(back_baddr);

	/* Find the start of the next word, leaving back_baddr pointing at it
	   or at the end of the field. */
	while (screen_buf[back_baddr] == CG_space ||
	       screen_buf[back_baddr] == CG_null)
		INC_BA(back_baddr);

	/* Find the end of the field, leaving end_baddr pointing at the field
	   attribute of the start of the next field. */
	end_baddr = back_baddr;
	while (!IS_FA(screen_buf[end_baddr]))
		INC_BA(end_baddr);

	/* Copy any text to the right of the word we are deleting. */
	baddr = front_baddr;
	baddr2 = back_baddr;
	while (baddr2 != end_baddr) {
		ctlr_add(baddr, screen_buf[baddr2]);
		INC_BA(baddr);
		INC_BA(baddr2);
	}

	/* Insert nulls to pad out the end of the field. */
	while (baddr != end_baddr) {
		ctlr_add(baddr, CG_null);
		INC_BA(baddr);
	}

	/* Set the MDT and move the cursor. */
	mdt_set(fa);
	cursor_move(front_baddr);
}



/*
 * Delete field key.  Similar to EraseEOF, but it wipes out the entire field
 * rather than just to the right of the cursor, and it leaves the cursor at
 * the front of the field.
 *
 * Which is to say, does a ^U.
 */
void
key_DeleteField()
{
	register int	baddr;
	register unsigned char	*fa;

	if (IN_ANSI) {
		net_send_kill();
		return;
	}
	if (kybdlock || !formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(status_protected);
		return;
	}
	while (!IS_FA(screen_buf[baddr]))
		DEC_BA(baddr);
	INC_BA(baddr);
	cursor_move(baddr);
	while (!IS_FA(screen_buf[baddr])) {
		ctlr_add(baddr, CG_null);
		INC_BA(baddr);
	}
	mdt_set(fa);
}



/*
 * Set insert mode key.
 */
void
key_Insert()
{
	if (IN_ANSI || kybdlock)
		return;
	insert_mode(True);
}


/*
 * Toggle insert mode key.
 */
void
key_ToggleInsert()
{
	if (IN_ANSI || kybdlock)
		return;
	if (insert)
		insert_mode(False);
	else
		insert_mode(True);
}


/*
 * Move the cursor to the first blank after the last nonblank in the
 * field, or if the field is full, to the last character in the field.
 */
void
key_FieldEnd()
{
	int	baddr;
	unsigned char	*fa, c;
	int	last_nonblank = -1;

	if (IN_ANSI || kybdlock || !formatted)
		return;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (fa == &screen_buf[baddr] || FA_IS_PROTECTED(*fa))
		return;

	baddr = fa - screen_buf;
	while (True) {
		INC_BA(baddr);
		c = screen_buf[baddr];
		if (IS_FA(c))
			break;
		if (c != CG_null && c != CG_space)
			last_nonblank = baddr;
	}

	if (last_nonblank == -1) {
		baddr = fa - screen_buf;
		INC_BA(baddr);
	} else {
		baddr = last_nonblank;
		INC_BA(baddr);
		if (IS_FA(screen_buf[baddr]))
			baddr = last_nonblank;
	}
	cursor_move(baddr);
}


/*
 * X-dependent code starts here.
 */

/*
 * Translate a keymap (from an XQueryKeymap or a KeymapNotify event) into
 * a bitmap of Shift, Meta or Alt keys pressed.
 */
#define key_is_down(kc, bitmap) (kc && ((bitmap)[(kc)/8] & (1<<((kc)%8))))
int
state_from_keymap(keymap)
char keymap[32];
{
	static Boolean	initted = False;
	static KeyCode	kc_Shift_L, kc_Shift_R;
	static KeyCode	kc_Meta_L, kc_Meta_R;
	static KeyCode	kc_Alt_L, kc_Alt_R;
	int	pseudo_state = 0;

	if (!initted) {
		kc_Shift_L = XKeysymToKeycode(display, XK_Shift_L);
		kc_Shift_R = XKeysymToKeycode(display, XK_Shift_R);
		kc_Meta_L  = XKeysymToKeycode(display, XK_Meta_L);
		kc_Meta_R  = XKeysymToKeycode(display, XK_Meta_R);
		kc_Alt_L   = XKeysymToKeycode(display, XK_Alt_L);
		kc_Alt_R   = XKeysymToKeycode(display, XK_Alt_R);
		initted = True;
	}
	if (key_is_down(kc_Shift_L, keymap) ||
	    key_is_down(kc_Shift_R, keymap))
		pseudo_state |= ShiftKeyDown;
	if (key_is_down(kc_Meta_L, keymap) ||
	    key_is_down(kc_Meta_R, keymap))
		pseudo_state |= MetaKeyDown;
	if (key_is_down(kc_Alt_L, keymap) ||
	    key_is_down(kc_Alt_R, keymap))
		pseudo_state |= AltKeyDown;
	return pseudo_state;
}
#undef key_is_down

/*
 * Process shift keyboard events.  The code has to look for the raw Shift keys,
 * rather than using the handy "state" field in the event structure.  This is
 * because the event state is the state _before_ the key was pressed or
 * released.  This isn't enough information to distinguish between "left
 * shift released" and "left shift released, right shift still held down"
 * events, for example.
 *
 * This function is also called as part of Focus event processing.
 */
/*ARGSUSED*/
void
key_Shift(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	char	keys[32];

	XQueryKeymap(display, keys);
	shift_event(state_from_keymap(keys));
}

/* Translate a keysym name to a keysym, including APL characters. */
static KeySym
StringToKeysym(s, keytypep)
char *s;
enum keytype *keytypep;
{
	KeySym k;

	if (!strncmp(s, "apl_", 4)) {
		k = APLStringToKeysym(s);
		*keytypep = APL;
	} else {
		k = XStringToKeysym(s);
		*keytypep = LATIN;
	}
	if (k == NoSymbol && strlen(s) == 1)
		k = s[0] & 0xff;
	if (k < ' ' || k > 0xff)
		k = NoSymbol;
	return k;
}

static Boolean
build_composites()
{
	char *c;
	char *cn;
	char *ln;
	char ksname[3][64];
	char junk[2];
	KeySym k[3];
	enum keytype a[3];
	int i;
	struct composite *cp;

	if (!appres.compose_map) {
		XtWarning("Compose: No composeMap defined");
		return False;
	}
	cn = xs_buffer("composeMap.%s", appres.compose_map);
	if ((c = get_resource(cn)) == NULL) {
		xs_warning("Compose: Can't find composeMap %s",
		    appres.compose_map);
		return False;
	}
	XtFree(cn);
	while (ln = strtok(c, "\n")) {
		Boolean okay = True;

		c = NULL;
		if (sscanf(ln, " %63[^+ \t] + %63[^= \t] =%63s%1s",
		    ksname[0], ksname[1], ksname[2], junk) != 3) {
			xs_warning("Compose: Invalid syntax: %s", ln);
			continue;
		}
		for (i = 0; i < 3; i++) {
			k[i] = StringToKeysym(ksname[i], &a[i]);
			if (k[i] == NoSymbol) {
				xs_warning("Compose: Invalid KeySym: '%s'",
				    ksname[i]);
				okay = False;
				break;
			}
		}
		if (!okay)
			continue;
		composites = (struct composite *) XtRealloc((char *)composites,
		    (n_composites + 1) * sizeof(struct composite));
		cp = composites + n_composites;
		cp->k1.keysym = k[0];
		cp->k1.keytype = a[0];
		cp->k2.keysym = k[1];
		cp->k2.keytype = a[1];
		cp->translation = k[2];
		n_composites++;
	}
	return True;
}

/*
 * Called by the toolkit when the "Compose" key is pressed.  "Compose" is
 * implemented by pressing and releasing three keys: "Compose" and two
 * data keys.  For example, "Compose" "s" "s" gives the German "ssharp"
 * character, and "Compose" "C", "," gives a capital "C" with a cedilla
 * (symbol Ccedilla).
 *
 * The mechanism breaks down a little when the user presses "Compose" and
 * then a non-data key.  Oh well.
 */
/*ARGSUSED*/
static void
Compose(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (!composites && !build_composites())
		return;

	if (composing != NONE) {
		composing = NONE;
		status_compose(False, 0);
		return;
	}
	composing = COMPOSE;
	status_compose(True, 0);
}

/*
 * Called by the toolkit for any key without special actions.
 */
/*ARGSUSED*/
static void
Default(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	XKeyEvent	*kevent = (XKeyEvent *)event;
	char		buf[10];
	KeySym		ks;
	int		ll;

	ll = XLookupString(kevent, buf, 10, &ks, (XComposeStatus *) 0);
	if (ll != 1)
		return;

	key_ACharacter((unsigned char) buf[0], LATIN);
}


/*
 * Key action.
 */
/*ARGSUSED*/
static void
key(w, event, params, nparams)
Widget w;
XEvent *event;
String *params;
Cardinal *nparams;
{
	int i;
	KeySym k;
	enum keytype keytype;

	for (i = 0; i < *nparams; i++) {
		char *s = params[i];

		k = StringToKeysym(s, &keytype);
		if (k == NoSymbol) {
			xs_popup_an_error("Key: Nonexistent or invalid KeySym: %s", s);
			continue;
		}
		if (k & ~0xff) {
			xs_popup_an_error("Key: Invalid KeySym: %s", s);
			continue;
		}
		key_ACharacter((unsigned char)(k & 0xff), keytype);
	}
}

/*
 * String action.
 */
/*ARGSUSED*/
static void
string(w, event, params, nparams)
Widget w;
XEvent *event;
String *params;
Cardinal *nparams;
{
	int i;
	int len = 0;
	char *s;

	/* Determine the total length of the strings. */
	for (i = 0; i < *nparams; i++)
		len += strlen(params[i]);
	if (!len)
		return;

	/* Allocate a block of memory and copy them in. */
	s = XtMalloc(len + 1);
	*s = '\0';
	for (i = 0; i < *nparams; i++)
		(void) strcat(s, params[i]);

	/* Set the pending string to that and process it. */
	ps_set(s, False);
	ps_process();
}

static void
do_pa(n)
int n;
{
	if (kybdlock)
		return;
	if (n < 1 || n > PA_SZ) {
		popup_an_error("Unknown PA key");
		return;
	}
	key_AID(pa_xlate[n-1]);
}

static void
do_pf(n)
int n;
{
	if (kybdlock)
		return;
	if (n < 1 || n > PF_SZ) {
		popup_an_error("Unknown PF key");
		return;
	}
	key_AID(pf_xlate[n-1]);
}

/*
 * Pretend that a sequence of keys was entered at the keyboard.
 *
 * "Pasting" means that the sequence came from the X clipboard.  Returns are
 * ignored; newlines mean "move to beginning of next line".  Backslashes are
 * not special.
 *
 * "Not pasting" means that the sequence is a login string specified in the
 * hosts file, or a parameter to the String action.  Returns are "move to
 * beginning of next line"; newlines mean "Enter AID" and the termination of
 * processing the string.  Backslashes are processed as in C.
 *
 * Returns the number of unprocessed characters.
 */
int
emulate_input(s, len, pasting)
char *s;
int len;
Boolean pasting;
{
	char c;
	enum { BASE, BACKSLASH, BACKX, BACKP, BACKPA, BACKPF, OCTAL, HEX }
		state = BASE;
	int literal;
	int nc;
	static char dxl[] = "0123456789abcdef";

	/*
	 * In the switch statements below, "break" generally means "consume
	 * this character," while "continue" means "rescan this character."
	 */
	while (len) {
		c = *s;
		switch (state) {
		    case BASE:
			switch (c) {
			    case '\b':
				key_Left();
				continue;
			    case '\f':
				key_Clear();
				if (IN_3270)
					return len-1;
				else
					break;
			    case '\n':
				if (pasting)
					key_Newline();
				else {
					key_Enter();
					if (IN_3270)
						return len-1;
				}
				break;
			    case '\r':	/* ignored */
				break;
			    case '\t':
				key_FTab();
				break;
			    case '\\':	/* backslashes are NOT special when pasting */
				if (!pasting)
					state = BACKSLASH;
				else
					key_ACharacter((unsigned char) c, LATIN);
				break;
			default:
				key_ACharacter((unsigned char) c, LATIN);
				break;
			}
			break;
		    case BACKSLASH:	/* last character was a backslash */
			switch (c) {
			    case 'a':
				popup_an_error("String: bell not supported");
				state = BASE;
				break;
			    case 'b':
				key_Left();
				state = BASE;
				break;
			    case 'f':
				key_Clear();
				state = BASE;
				if (IN_3270)
					return len-1;
				else
					break;
			    case 'n':
				key_Enter();
				state = BASE;
				if (IN_3270)
					return len-1;
				else
					break;
			    case 'p':
				state = BACKP;
				break;
			    case 'r':
				key_Newline();
				state = BASE;
				break;
			    case 't':
				key_FTab();
				state = BASE;
				break;
			    case 'v':
				popup_an_error("String: vertical tab not supported");
				state = BASE;
				break;
			    case 'x':
				state = BACKX;
				break;
			    case '0': 
			    case '1': 
			    case '2': 
			    case '3':
			    case '4': 
			    case '5': 
			    case '6': 
			    case '7':
				state = OCTAL;
				literal = 0;
				nc = 0;
				continue;
			default:
				state = BASE;
				continue;
			}
			break;
		    case BACKP:	/* last two characters were "\p" */
			switch (c) {
			    case 'a':
				literal = 0;
				nc = 0;
				state = BACKPA;
				break;
			    case 'f':
				literal = 0;
				nc = 0;
				state = BACKPF;
				break;
			    default:
				popup_an_error("String: unknown character after \\p");
				state = BASE;
				break;
			}
			break;
		    case BACKPF: /* last three characters were "\pf" */
			if (nc < 2 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("String: unknown character after \\pf");
				state = BASE;
			} else {
				do_pf(literal);
				if (IN_3270)
					return len-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKPA: /* last three characters were "\pa" */
			if (nc < 1 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("String: unknown character after \\pa");
				state = BASE;
			} else {
				do_pa(literal);
				if (IN_3270)
					return len-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKX:	/* last two characters were "\x" */
			if (isxdigit(c)) {
				state = HEX;
				literal = 0;
				nc = 0;
				continue;
			} else {
				popup_an_error("String: missing hex digits after \\x");
				state = BASE;
				continue;
			}
		    case OCTAL:	/* have seen \ and one or more octal digits */
			if (nc < 3 && isdigit(c) && c < '8') {
				literal = (literal * 8) + (strchr(dxl, c) - dxl);
				nc++;
				break;
			} else {
				key_ACharacter((unsigned char) literal, LATIN);
				state = BASE;
				continue;
			}
		    case HEX:	/* have seen \ and one or more hex digits */
			if (nc < 2 && isxdigit(c)) {
				literal = (literal * 16) + (strchr(dxl, tolower(c)) - dxl);
				nc++;
				break;
			} else {
				key_ACharacter((unsigned char) literal, LATIN);
				state = BASE;
				continue;
			}
		}
		s++;
		len--;
	}

	switch (state) {
	    case OCTAL:
	    case HEX:
		key_ACharacter((unsigned char) literal, LATIN);
		state = BASE;
		break;
	    case BACKPF:
		if (nc > 0) {
			do_pf(literal);
			state = BASE;
		}
		break;
	    case BACKPA:
		if (nc > 0) {
			do_pa(literal);
			state = BASE;
		}
		break;
	}

	if (state != BASE)
		popup_an_error("String: missing data after \\");

	return len;
}

/*ARGSUSED*/
static void
paste_callback(w, client_data, selection, type, value, length, format)
Widget w;
XtPointer client_data;
Atom *selection;
Atom *type;
XtPointer value;
unsigned long *length;
int *format;
{
	char *s;
	unsigned long len;

	if ((value == NULL) || (*length == 0)) {
		XtFree(value);

		/* Try the next one. */
		if (n_pasting > pix)
			XtGetSelectionValue(w, paste_atom[pix++], XA_STRING,
			    paste_callback, NULL, paste_time);
		return;
	}

	s = (char *)value;
	len = *length;
	(void) emulate_input(s, (int) len, True);
	n_pasting = 0;

	XtFree(value);
}

static void
insert_selection(w, event, params, nparams)
Widget w;
XButtonEvent *event;
String *params;
Cardinal *nparams;
{
	int	i;
	Atom	a;

	n_pasting = 0;
	for (i = 0; i < *nparams; i++) {
		a = XInternAtom(display, params[i], True);
		if (a == None) {
			popup_an_error("insert-selection: no atom for selection");
			continue;
		}
		if (n_pasting < NP)
			paste_atom[n_pasting++] = a;
	}
	pix = 0;
	if (n_pasting > pix) {
		paste_time = event->time;
		XtGetSelectionValue(w, paste_atom[pix++], XA_STRING,
		    paste_callback, NULL, paste_time);
	}
}

/*ARGSUSED*/
static void
ignore(w, event, params, nparams)
Widget w;
XButtonEvent *event;
String *params;
Cardinal *nparams;
{
}

XtActionsRec actions[] = {
	{ "AltCursor",  	(XtActionProc)key_AltCr },
	{ "AnsiText",		(XtActionProc)ansi_text_fn },
	{ "Ascii",		(XtActionProc)ascii_fn },
	{ "AsciiField",		(XtActionProc)ascii_field_fn },
	{ "Attn",		(XtActionProc)key_Attn },
	{ "BackSpace",		(XtActionProc)key_BackSpace },
	{ "BackTab",		(XtActionProc)key_BTab },
	{ "Clear",		(XtActionProc)key_Clear },
	{ "CloseScript",	(XtActionProc)close_script_fn },
	{ "Compose",		(XtActionProc)Compose },
	{ "Connect",		(XtActionProc)Connect },
	{ "ContinueScript",	(XtActionProc)continue_script_fn },
	{ "CursorSelect",	(XtActionProc)key_CursorSelect },
	{ "Default",		(XtActionProc)Default },
	{ "Delete", 		(XtActionProc)key_Delete },
	{ "DeleteField",	(XtActionProc)key_DeleteField },
	{ "DeleteWord",		(XtActionProc)key_DeleteWord },
	{ "Disconnect",		(XtActionProc)Disconnect },
	{ "Down",		(XtActionProc)key_Down },
	{ "Dup",		(XtActionProc)key_Dup },
	{ "Ebcdic",		(XtActionProc)ebcdic_fn },
	{ "EbcdicField",	(XtActionProc)ebcdic_field_fn },
	{ "Enter",		(XtActionProc)key_Enter },
	{ "EnterLeave",		(XtActionProc)enter_leave },
	{ "Erase",		(XtActionProc)key_Erase },
	{ "EraseEOF",		(XtActionProc)key_EraseEOF },
	{ "EraseInput",		(XtActionProc)key_EraseInput },
	{ "Execute",		(XtActionProc)execute_fn },
	{ "FieldEnd",		(XtActionProc)key_FieldEnd },
	{ "FieldMark",		(XtActionProc)key_FieldMark },
	{ "FocusEvent",		(XtActionProc)focus_change },
	{ "HandleMenu",		(XtActionProc)handle_menu },
	{ "HardPrint",		(XtActionProc)print_text },
	{ "Home",		(XtActionProc)key_Home },
	{ "Info",		(XtActionProc)Info },
	{ "Insert",		(XtActionProc)key_Insert },
	{ "Key",		(XtActionProc)key },
	{ "KeymapEvent",	(XtActionProc)keymap_event },
	{ "Left",		(XtActionProc)key_Left },
	{ "Left2", 		(XtActionProc)key_Left2 },
	{ "MonoCase",		(XtActionProc)key_MonoCase },
	{ "MoveCursor",		(XtActionProc)MoveCursor },
	{ "Newline",		(XtActionProc)key_Newline },
	{ "PauseScript",	(XtActionProc)pause_script_fn },
	{ "PA",			(XtActionProc)key_PA },
	{ "PA1",		(XtActionProc)key_PA1 },
	{ "PA2",		(XtActionProc)key_PA2 },
	{ "PA3",		(XtActionProc)key_PA3 },
	{ "PF",			(XtActionProc)key_PF },
	{ "PF1",		(XtActionProc)key_PF1 },
	{ "PF10",		(XtActionProc)key_PF10 },
	{ "PF11",		(XtActionProc)key_PF11 },
	{ "PF12",		(XtActionProc)key_PF12 },
	{ "PF13",		(XtActionProc)key_PF13 },
	{ "PF14",		(XtActionProc)key_PF14 },
	{ "PF15",		(XtActionProc)key_PF15 },
	{ "PF16",		(XtActionProc)key_PF16 },
	{ "PF17",		(XtActionProc)key_PF17 },
	{ "PF18",		(XtActionProc)key_PF18 },
	{ "PF19",		(XtActionProc)key_PF19 },
	{ "PF2",		(XtActionProc)key_PF2 },
	{ "PF20",		(XtActionProc)key_PF20 },
	{ "PF21",		(XtActionProc)key_PF21 },
	{ "PF22",		(XtActionProc)key_PF22 },
	{ "PF23",		(XtActionProc)key_PF23 },
	{ "PF24",		(XtActionProc)key_PF24 },
	{ "PF3",		(XtActionProc)key_PF3 },
	{ "PF4",		(XtActionProc)key_PF4 },
	{ "PF5",		(XtActionProc)key_PF5 },
	{ "PF6",		(XtActionProc)key_PF6 },
	{ "PF7",		(XtActionProc)key_PF7 },
	{ "PF8",		(XtActionProc)key_PF8 },
	{ "PF9",		(XtActionProc)key_PF9 },
	{ "PlaybackStep",	(XtActionProc)net_playback_step },
	{ "PrintText",		(XtActionProc)print_text },
	{ "PrintWindow",	(XtActionProc)print_window },
	{ "Quit",		(XtActionProc)quit_event },
	{ "Redraw",		(XtActionProc)redraw },
	{ "Reset",		(XtActionProc)key_Reset },
	{ "Right",		(XtActionProc)key_Right },
	{ "Right2",		(XtActionProc)key_Right2 },
	{ "SetFont",		(XtActionProc)set_font },
	{ "Shift",		(XtActionProc)key_Shift },
	{ "StateChanged",	(XtActionProc)state_event },
	{ "String",		(XtActionProc)string },
	{ "SysReq",		(XtActionProc)key_SysReq },
	{ "Tab",		(XtActionProc)key_FTab },
	{ "ToggleInsert",	(XtActionProc)key_ToggleInsert },
	{ "Up",			(XtActionProc)key_Up },
	{ "WMProtocols",	(XtActionProc)wm_protocols },
	{ "Wait",		(XtActionProc)wait_fn },
	{ "ignore",		(XtActionProc)ignore },
	{ "insert-selection",	(XtActionProc)insert_selection },
	{ "move-select",	(XtActionProc)move_select },
	{ "select-end",		(XtActionProc)select_end },
	{ "select-extend",	(XtActionProc)select_extend },
	{ "select-start",	(XtActionProc)select_start },
	{ "start-extend",	(XtActionProc)start_extend },
};

int actioncount = XtNumber(actions);
