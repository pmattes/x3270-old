/*
 * Copyright 1993, 1994, 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	status.c
 *		This module handles the 3270 status line.
 */

#include "globals.h"
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include "3270ds.h"
#include "appres.h"
#include "screen.h"
#include "cg.h"

#include "kybdc.h"
#include "hostc.h"
#include "statusc.h"
#include "tablesc.h"
#include "utilc.h"

extern Window  *screen_window;
extern GC       screen_gc();
extern GC       screen_invgc();

static XChar2b *status_2b;
static unsigned char *status_1b;
static XChar2b *display_2b;
static Boolean  status_changed = False;

static struct status_line {
	Boolean         changed;
	int             start, len, color;
	XChar2b        *s2b;
	unsigned char  *s1b;
	XChar2b        *d2b;
}              *status_line;

static int offsets[] = {
	0,	/* connection status */
	8,	/* wait, locked */
	39,	/* shift, insert, timing, cursor position */
	-1
};
#define SSZ ((sizeof(offsets)/sizeof(offsets[0])) - 1)

#define CTLR_REGION	0
#define WAIT_REGION	1
#define MISC_REGION	2

static int colors[SSZ] =  {
	FA_INT_NORM_NSEL,
	FA_INT_HIGH_SEL,
	FA_INT_NORM_NSEL
};

static int colors3279[SSZ] =  {
	COLOR_BLUE,
	COLOR_WHITE,
	COLOR_BLUE
};

#define CM	(60 * 10)	/* csec per minute */

/*
 * The status line is laid out thusly (M is maxCOLS):
 *
 *   0		"4" in a square
 *   1		"A" underlined
 *   2		solid box if connected, "?" in a box if not
 *   3..7	empty
 *   8...	message area
 *   M-41	Meta indication ("M" or blank)
 *   M-40	Alt indication ("A" or blank)
 *   M-39	Shift indication (Special symbol/"^" or blank)
 *   M-38..M-37	empty
 *   M-36	Compose indication ("C" or blank)
 *   M-35	Compose first character
 *   M-34	empty
 *   M-33	Typeahead indication ("T" or blank)
 *   M-32	empty
 *   M-31	Alternate keymap indication ("K" or blank)
 *   M-30	Reverse input mode indication ("R" or blank)
 *   M-29	Insert mode indication (Special symbol/"I" or blank)
 *   M-28	empty
 *   M-27	Script indication ("S" or blank)
 *   M-26..M-16	empty
 *   M-15..M-9	command timing (Clock symbol and m:ss, or blank)
 *   M-7..M	cursor position (rrr/ccc or blank)
 */

/* Positions */

#define LBOX	0		/* left-hand box */
#define CNCT	1		/* connection between */
#define RBOX	2		/* right-hand box */

#define M0	8		/* message area */

#define SHIFT	(maxCOLS-39)	/* shift indication */

#define COMPOSE	(maxCOLS-36)	/* compose characters */

#define TYPEAHD	(maxCOLS-33)	/* typeahead */

#define KMAP	(maxCOLS-31)	/* alt keymap in effect */

#define REVERSE (maxCOLS-30)	/* reverse input mode in effect */

#define INSERT	(maxCOLS-29)	/* insert mode */

#define SCRIPT	(maxCOLS-27)	/* script in progress */

#define T0	(maxCOLS-15)	/* timings */
#define	TCNT	7

#define C0	(maxCOLS-7)	/* cursor position */
#define CCNT	7

#define STATUS_Y	(ROW_TO_Y(maxROWS)+SGAP-1)

static unsigned char	nullblank;
static Position		status_y;

/* Status line contents (high-level) */

static void do_disconnected(void);
static void do_connecting(void);
static void do_nonspecific(void);
static void do_inhibit(void);
static void do_blank(void);
static void do_twait(void);
static void do_syswait(void);
static void do_protected(void);
static void do_numeric(void);
static void do_overflow(void);
static void do_scrolled(void);

static Boolean  oia_undera = True;
static Boolean  oia_boxsolid = False;
static int      oia_shift = 0;
static Boolean  oia_typeahead = False;
static Boolean  oia_compose = False;
static unsigned char oia_compose_char = 0;
static enum keytype oia_compose_keytype = KT_STD;
static enum msg {
	DISCONNECTED,		/* X Not Connected */
	CONNECTING,		/* X Connecting */
	NONSPECIFIC,		/* X */
	INHIBIT,		/* X Inhibit */
	BLANK,			/* (blank) */
	TWAIT,			/* X Wait */
	SYSWAIT,		/* X SYSTEM */
	PROTECTED,		/* X Protected */
	NUMERIC,		/* X Numeric */
	OVERFLOW,		/* X Overflow */
	SCROLLED		/* X Scrolled */
}               oia_msg = DISCONNECTED, saved_msg;
static Boolean  msg_is_saved = False;
static int      n_scrolled = 0;
static void     (*msg_proc[])(void) = {
	do_disconnected,
	do_connecting,
	do_nonspecific,
	do_inhibit,
	do_blank,
	do_twait,
	do_syswait,
	do_protected,
	do_numeric,
	do_overflow,
	do_scrolled,
};
static int      msg_color[] = {
	FA_INT_HIGH_SEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
};
static int      msg_color3279[] = {
	COLOR_WHITE,
	COLOR_WHITE,
	COLOR_WHITE,
	COLOR_WHITE,
	COLOR_BLUE,
	COLOR_WHITE,
	COLOR_WHITE,
	COLOR_RED,
	COLOR_RED,
	COLOR_RED,
	COLOR_WHITE,
};
static Boolean  oia_insert = False;
static Boolean  oia_reverse = False;
static Boolean  oia_kmap = False;
static Boolean	oia_script = False;
static char    *oia_cursor = (char *) 0;
static char    *oia_timing = (char *) 0;

static unsigned char disc_pfx[] = {
	CG_lock, CG_space, CG_badcommhi, CG_commjag, CG_commlo, CG_space
};
static unsigned char *disc_msg;
static int      disc_len = sizeof(disc_pfx);

static unsigned char cnct_pfx[] = {
	CG_lock, CG_space, CG_commhi, CG_commjag, CG_commlo, CG_space
};
static unsigned char *cnct_msg;
static int      cnct_len = sizeof(cnct_pfx);

static unsigned char *a_not_connected;
static unsigned char *a_connecting;
static unsigned char *a_inhibit;
static unsigned char *a_twait;
static unsigned char *a_syswait;
static unsigned char *a_protected;
static unsigned char *a_numeric;
static unsigned char *a_overflow;
static unsigned char *a_scrolled;

static unsigned char *make_amsg(const char *key);
static unsigned char *make_emsg(unsigned char prefix[], const char *key,
	int *len);

static void status_render(int region);
static void do_ctlr(void);
static void do_msg(enum msg t);
static void paint_msg(enum msg t);
static void do_insert(Boolean on);
static void do_reverse(Boolean on);
static void do_kmap(Boolean on);
static void do_script(Boolean on);
static void do_shift(int state);
static void do_typeahead(int state);
static void do_compose(Boolean on, unsigned char c, enum keytype keytype);
static void do_timing(char *buf);
static void do_cursor(char *buf);

static void status_connect(Boolean connected);
static void status_3270_mode(Boolean connected);
static void status_half_connect(Boolean ignored);


/* Initialize the status line */
void
status_init(void)
{
	a_not_connected = make_amsg("statusNotConnected");
	disc_msg = make_emsg(disc_pfx, "statusNotConnected",
	    &disc_len);
	a_connecting = make_amsg("statusConnecting");
	cnct_msg = make_emsg(cnct_pfx, "statusConnecting", &cnct_len);
	a_inhibit = make_amsg("statusInhibit");
	a_twait = make_amsg("statusTwait");
	a_syswait = make_amsg("statusSyswait");
	a_protected = make_amsg("statusProtected");
	a_numeric = make_amsg("statusNumeric");
	a_overflow = make_amsg("statusOverflow");
	a_scrolled = make_amsg("statusScrolled");

	register_schange(ST_HALF_CONNECT, status_half_connect);
	register_schange(ST_CONNECT, status_connect);
	register_schange(ST_3270_MODE, status_3270_mode);
}

/* Reinitialize the status line */
void
status_reinit(unsigned cmask)
{
	unsigned i;

	if (cmask & FONT_CHANGE)
		nullblank = *standard_font ? ' ' : CG_space;
	if (cmask & (FONT_CHANGE | MODEL_CHANGE)) {
		status_y = STATUS_Y;
		if (!*descent)
			++status_y;
	}
	if (cmask & MODEL_CHANGE) {
		if (status_line)
			XtFree((char *)status_line);
		status_line = (struct status_line *)XtCalloc(sizeof(struct status_line), SSZ);
		if (status_2b != (XChar2b *)NULL)
			XtFree((char *)status_2b);
		status_2b = (XChar2b *)XtCalloc(sizeof(XChar2b), maxCOLS);
		if (status_1b != (unsigned char *)NULL)
			XtFree((char *)status_1b);
		status_1b = (unsigned char *)XtCalloc(sizeof(unsigned char),
		    maxCOLS);
		if (display_2b != (XChar2b *)NULL)
			XtFree((XtPointer)display_2b);
		display_2b = (XChar2b *)XtCalloc(sizeof(XChar2b), maxCOLS);
		offsets[SSZ] = maxCOLS;
		if (appres.mono)
			colors[1] = FA_INT_NORM_NSEL;
		for (i = 0; i < SSZ; i++) {
			status_line[i].start = offsets[i];
			status_line[i].len = offsets[i+1] - offsets[i];
			status_line[i].s2b = status_2b + offsets[i];
			status_line[i].s1b = status_1b + offsets[i];
			status_line[i].d2b = display_2b + offsets[i];
		}
	} else
		(void) memset(display_2b, 0, maxCOLS * sizeof(XChar2b));
	if (cmask & (COLOR_CHANGE | MODEL_CHANGE)) {
		for (i = 0; i < SSZ; i++) {
			status_line[i].color = appres.m3279 ?
			    colors3279[i] : colors[i];
		}
	}

	for (i = 0; i < SSZ; i++)
		status_line[i].changed = True;
	status_changed = True;

	/*
	 * Always redraw all the fields; it's easier than keeping track of
	 * what may have changed and why.
	 */
	do_ctlr();
	paint_msg(oia_msg);
	do_insert(oia_insert);
	do_reverse(oia_reverse);
	do_kmap(oia_kmap);
	do_script(oia_script);
	do_shift(oia_shift);
	do_typeahead(oia_typeahead);
	do_compose(oia_compose, oia_compose_char, oia_compose_keytype);
	do_cursor(oia_cursor);
	do_timing(oia_timing);
}

/* Render the status line onto the screen */
void
status_disp(void)
{
	unsigned i;

	if (!status_changed)
		return;
	for (i = 0; i < SSZ; i++)
		if (status_line[i].changed) {
			status_render(i);
			(void) MEMORY_MOVE(status_line[i].d2b,
				   (char *) status_line[i].s2b,
				   status_line[i].len * sizeof(XChar2b));
			status_line[i].changed = False;
		}
	status_changed = False;
}

/* Mark the entire status line as changed */
void
status_touch(void)
{
	unsigned i;

	for (i = 0; i < SSZ; i++) {
		status_line[i].changed = True;
		(void) memset(status_line[i].d2b, 0,
		    status_line[i].len * sizeof(XChar2b));
	}
	status_changed = True;
}

/* Keyboard lock status changed */
void
status_kybdlock(void)
{
	/* presently implemented as explicit calls */
}

/* Connected or disconnected */
static void
status_connect(Boolean connected)
{
	if (connected) {
		oia_boxsolid = IN_3270 && !IN_SSCP;
		do_ctlr();
		if (kybdlock & KL_AWAITING_FIRST)
			do_msg(NONSPECIFIC);
		else
			do_msg(BLANK);
		status_untiming();
	} else {
		oia_boxsolid = False;
		do_ctlr();
		do_msg(DISCONNECTED);
		status_uncursor_pos();
	}
}

/* Changed 3270 mode */
static void
status_3270_mode(Boolean connected)
{
	oia_boxsolid = IN_3270 && !IN_SSCP;
	do_ctlr();
	status_untiming();
}

/* Half connected */
static void
status_half_connect(Boolean ignored unused)
{
	oia_boxsolid = False;
	do_ctlr();
	do_msg(CONNECTING);
	status_untiming();
	status_uncursor_pos();
}

/* Lock the keyboard (twait) */
void
status_twait(void)
{
	oia_undera = False;
	do_ctlr();
	do_msg(TWAIT);
}

/* Done with controller confirmation */
void
status_ctlr_done(void)
{
	oia_undera = True;
	do_ctlr();
}

/* Lock the keyboard (X SYSTEM) */
void
status_syswait(void)
{
	do_msg(SYSWAIT);
}

/* Lock the keyboard (operator error) */
void
status_oerr(int error_type)
{
	switch (error_type) {
	    case KL_OERR_PROTECTED:
		do_msg(PROTECTED);
		break;
	    case KL_OERR_NUMERIC:
		do_msg(NUMERIC);
		break;
	    case KL_OERR_OVERFLOW:
		do_msg(OVERFLOW);
		break;
	}
}

/* Lock the keyboard (X Scrolled) */
void
status_scrolled(int n)
{
	if (n != 0) {
		if (!msg_is_saved) {
			saved_msg = oia_msg;
			msg_is_saved = True;
		}
		n_scrolled = n;
		paint_msg(SCROLLED);
	} else {
		if (msg_is_saved) {
			msg_is_saved = False;
			paint_msg(saved_msg);
		}
	}
}

/* Unlock the keyboard */
void
status_reset(void)
{
	if (kybdlock & KL_ENTER_INHIBIT)
		do_msg(INHIBIT);
	else if (kybdlock & KL_DEFERRED_UNLOCK)
		do_msg(NONSPECIFIC);
	else
		do_msg(BLANK);
}

/* Toggle insert mode */
void
status_insert_mode(Boolean on)
{
	do_insert(oia_insert = on);
}

/* Toggle reverse mode */
void
status_reverse_mode(Boolean on)
{
	do_reverse(oia_reverse = on);
}

/* Toggle kmap mode */
void
status_kmap(Boolean on)
{
	do_kmap(oia_kmap = on);
}

/* Toggle script mode */
void
status_script(Boolean on)
{
	do_script(oia_script = on);
}

/* Toggle shift mode */
void
status_shift_mode(int state)
{
	do_shift(oia_shift = state);
}

/* Toggle typeahead */
void
status_typeahead(Boolean on)
{
	do_typeahead(oia_typeahead = on);
}

/* Set compose character */
void
status_compose(Boolean on, unsigned char c, enum keytype keytype)
{
	oia_compose = on;
	oia_compose_char = c;
	oia_compose_keytype = keytype;
	do_compose(on, c, keytype);
}

/* Display timing */
void
status_timing(struct timeval *t0, struct timeval *t1)
{
	static char	no_time[] = ":??.?";
	static char	buf[TCNT+1];

	if (t1->tv_sec - t0->tv_sec > (99*60)) {
		do_timing(oia_timing = no_time);
	} else {
		unsigned long cs;	/* centiseconds */

		cs = (t1->tv_sec - t0->tv_sec) * 10 +
		     (t1->tv_usec - t0->tv_usec + 50000) / 100000;
		if (cs < CM)
			(void) sprintf(buf, ":%02ld.%ld", cs / 10, cs % 10);
		else
			(void) sprintf(buf, "%02ld:%02ld", cs / CM, (cs % CM) / 10);
		do_timing(oia_timing = buf);
	}
}

/* Erase timing indication */
void
status_untiming(void)
{
	do_timing(oia_timing = (char *) 0);
}

/* Update cursor position */
void
status_cursor_pos(int ca)
{
	static char	buf[CCNT+1];

	(void) sprintf(buf, "%03d/%03d", ca/COLS + 1, ca%COLS + 1);
	do_cursor(oia_cursor = buf);
}

/* Erase cursor position */
void
status_uncursor_pos(void)
{
	do_cursor(oia_cursor = (char *) 0);
}


/* Internal routines */

/* Update the status line by displaying "symbol" at column "col".  */
static void
status_add(int col, unsigned char symbol, enum keytype keytype)
{
	unsigned i;
	XChar2b n2b;

	n2b.byte1 = (keytype == KT_STD) ? 0 : 1;
	n2b.byte2 = symbol;
	if (status_2b[col].byte1 == n2b.byte1 &&
	    status_2b[col].byte2 == n2b.byte2)
		return;
	status_2b[col] = n2b;
	status_1b[col] = symbol;
	status_changed = True;
	for (i = 0; i < SSZ; i++)
		if (col >= status_line[i].start &&
		    col <  status_line[i].start + status_line[i].len) {
			status_line[i].changed = True;
			return;
		}
}

/*
 * Render a region of the status line onto the display, the idea being to
 * minimize the number of redundant X drawing operations performed.
 *
 * What isn't optimized is what happens when "ABC" becomes "XBZ" -- should we
 * redundantly draw over B or not?  Right now we don't.
 */
static void
status_render(int region)
{
	int	i;
	struct status_line *sl = &status_line[region];
	int	nd = 0;
	int	i0 = -1;

	/* The status region may change colors; don't be so clever */
	if (region == WAIT_REGION) {
		XDrawImageString(display, *screen_window, screen_gc(sl->color),
		    COL_TO_X(sl->start), status_y, (char *) sl->s1b, sl->len);
	} else {
		for (i = 0; i < sl->len; i++) {
			if (sl->s2b[i].byte1 == sl->d2b[i].byte1 &&
			    sl->s2b[i].byte2 == sl->d2b[i].byte2) {
				if (nd) {
					if (*extended_3270font)
						XDrawImageString16(display,
						    *screen_window,
						    screen_gc(sl->color),
						    COL_TO_X(sl->start + i0),
						    status_y,
						    sl->s2b + i0, nd);
					else
						XDrawImageString(display,
						    *screen_window,
						    screen_gc(sl->color),
						    COL_TO_X(sl->start + i0),
						    status_y,
						    (char *) sl->s1b + i0, nd);
					nd = 0;
					i0 = -1;
				}
			} else {
				if (!nd++)
					i0 = i;
			}
		}
		if (nd) {
			if (*extended_3270font)
				XDrawImageString16(display, *screen_window,
				    screen_gc(sl->color),
				    COL_TO_X(sl->start + i0), status_y,
				    sl->s2b + i0, nd);
			else
				XDrawImageString(display, *screen_window,
				    screen_gc(sl->color),
				    COL_TO_X(sl->start + i0), status_y,
				    (char *)sl->s1b + i0, nd);
		}
	}

	/* Leftmost region has unusual attributes */
	if (*standard_font && region == CTLR_REGION) {
		XDrawImageString(display, *screen_window,
		    screen_invgc(sl->color),
		    COL_TO_X(sl->start + LBOX), status_y,
		    (char *) sl->s1b + LBOX, 1);
		XDrawRectangle(display, *screen_window, screen_gc(sl->color),
		    COL_TO_X(sl->start + CNCT),
		    status_y - *ascent + *char_height - 1,
		    *char_width - 1, 0);
		XDrawImageString(display, *screen_window,
		    screen_invgc(sl->color),
		    COL_TO_X(sl->start + RBOX), status_y,
		    (char *) sl->s1b + RBOX, 1);
	}
}

/* Write into the message area of the status line */
static void
status_msg_set(unsigned const char *msg, int len)
{
	register int	i;

	for (i = 0; i < status_line[WAIT_REGION].len; i++) {
		status_add(M0+i, len ? msg[i] : nullblank, KT_STD);
		if (len)
			len--;
	}
}

/* Controller status */
static void
do_ctlr(void)
{
	if (*standard_font) {
		status_add(LBOX, '4', KT_STD);
		if (oia_undera)
			status_add(CNCT, (IN_E ? 'B' : 'A'), KT_STD);
		else
			status_add(CNCT, ' ', KT_STD);
		if (IN_ANSI)
			status_add(RBOX, 'N', KT_STD);
		else if (oia_boxsolid)
			status_add(RBOX, ' ', KT_STD);
		else if (IN_SSCP)
			status_add(RBOX, 'S', KT_STD);
		else
			status_add(RBOX, '?', KT_STD);
	} else {
		status_add(LBOX, CG_box4, KT_STD);
		if (oia_undera)
			status_add(CNCT, (IN_E ? CG_underB : CG_underA),
				KT_STD);
		else
			status_add(CNCT, CG_null, KT_STD);
		if (IN_ANSI)
			status_add(RBOX, CG_N, KT_STD);
		else if (oia_boxsolid)
			status_add(RBOX, CG_boxsolid, KT_STD);
		else if (IN_SSCP)
			status_add(RBOX, CG_boxhuman, KT_STD);
		else
			status_add(RBOX, CG_boxquestion, KT_STD);
	}
}

/* Message area */

/* Change the state of the message area, or if scrolled, the saved message */
static void
do_msg(enum msg t)
{
	if (msg_is_saved) {
		saved_msg = t;
		return;
	}
	paint_msg(t);
}

/* Paint the message area. */
static void
paint_msg(enum msg t)
{
	oia_msg = t;
	(*msg_proc[(int)t])();
	if (!appres.mono)
		status_line[WAIT_REGION].color = appres.m3279 ?
		    msg_color3279[(int)t] : msg_color[(int)t];
}

static void
do_blank(void)
{
	status_msg_set((unsigned char *) 0, 0);
}

static void
do_disconnected(void)
{
	if (*standard_font)
		status_msg_set(a_not_connected,
		    strlen((char *)a_not_connected));
	else
		status_msg_set(disc_msg, disc_len);
}

static void
do_connecting(void)
{
	if (*standard_font)
		status_msg_set(a_connecting, strlen((char *)a_connecting));
	else
		status_msg_set(cnct_msg, cnct_len);
}

static void
do_nonspecific(void)
{
	static unsigned char nonspecific[] = {
		CG_lock
	};

	if (*standard_font)
		status_msg_set((unsigned const char *)"X", 1);
	else
		status_msg_set(nonspecific, sizeof(nonspecific));
}

static void
do_inhibit(void)
{
	static unsigned char inhibit[] = {
		CG_lock, CG_space, CG_I, CG_n, CG_h, CG_i, CG_b, CG_i, CG_t
	};

	if (*standard_font)
		status_msg_set(a_inhibit, strlen((char *)a_inhibit));
	else
		status_msg_set(inhibit, sizeof(inhibit));
}

static void
do_twait(void)
{
	static unsigned char twait[] = {
		CG_lock, CG_space, CG_clockleft, CG_clockright
	};

	if (*standard_font)
		status_msg_set(a_twait, strlen((char *)a_twait));
	else
		status_msg_set(twait, sizeof(twait));
}

static void
do_syswait(void)
{
	static unsigned char syswait[] = {
		CG_lock, CG_space, CG_S, CG_Y, CG_S, CG_T, CG_E, CG_M
	};

	if (*standard_font)
		status_msg_set(a_syswait, strlen((char *)a_syswait));
	else
		status_msg_set(syswait, sizeof(syswait));
}

static void
do_protected(void)
{
	static unsigned char protected[] = {
		CG_lock, CG_space, CG_leftarrow, CG_human, CG_rightarrow
	};

	if (*standard_font)
		status_msg_set(a_protected, strlen((char *)a_protected));
	else
		status_msg_set(protected, sizeof(protected));
}

static void
do_numeric(void)
{
	static unsigned char numeric[] = {
		CG_lock, CG_space, CG_human, CG_N, CG_U, CG_M
	};

	if (*standard_font)
		status_msg_set(a_numeric, strlen((char *)a_numeric));
	else
		status_msg_set(numeric, sizeof(numeric));
}

static void
do_overflow(void)
{
	static unsigned char overflow[] = {
		CG_lock, CG_space, CG_human, CG_greater
	};

	if (*standard_font)
		status_msg_set(a_overflow, strlen((char *)a_overflow));
	else
		status_msg_set(overflow, sizeof(overflow));
}

static void
do_scrolled(void)
{
	static unsigned char scrolled[] = {
		CG_lock, CG_space, CG_S, CG_c, CG_r, CG_o, CG_l, CG_l, CG_e,
		CG_d, CG_space, CG_space, CG_space, CG_space, CG_space
	};
	static unsigned char spaces[] = {
		CG_space, CG_space, CG_space, CG_space
	};

	if (*standard_font) {
		char *t;

		t = XtMalloc(strlen((char *)a_scrolled) + 4);
		(void) sprintf(t, "%s %d", (char *)a_scrolled, n_scrolled);
		status_msg_set((unsigned char *)t, strlen(t));
		XtFree(t);
	} else {
		char nnn[5];
		int i;

		(void) sprintf(nnn, "%d", n_scrolled);
		(void) memcpy((char *)&scrolled[11], (char *)spaces,
		    sizeof(spaces));
		for (i = 0; nnn[i]; i++)
			scrolled[11 + i] = asc2cg[(int)nnn[i]];
		status_msg_set(scrolled, sizeof(scrolled));
	}
}

/* Insert, reverse, kmap, script, shift, compose */

static void
do_insert(Boolean on)
{
	status_add(INSERT, on ? (*standard_font ? 'I' : CG_insert) : nullblank, KT_STD);
}

static void
do_reverse(Boolean on)
{
	status_add(REVERSE, on ? (*standard_font ? 'R' : CG_R) : nullblank, KT_STD);
}

static void
do_kmap(Boolean on)
{
	status_add(KMAP, on ? (*standard_font ? 'K' : CG_K) : nullblank, KT_STD);
}

static void
do_script(Boolean on)
{
	status_add(SCRIPT, on ? (*standard_font ? 'S' : CG_S) : nullblank, KT_STD);
}

static void
do_shift(int state)
{
	status_add(SHIFT-2, (state & MetaKeyDown) ?
		(*standard_font ? 'M' : CG_M) : nullblank, KT_STD);
	status_add(SHIFT-1, (state & AltKeyDown) ?
		(*standard_font ? 'A' : CG_A) : nullblank, KT_STD);
	status_add(SHIFT, (state & ShiftKeyDown) ?
		(*standard_font ? '^' : CG_upshift) : nullblank, KT_STD);
}

static void
do_typeahead(int state)
{
	status_add(TYPEAHD, state ? (*standard_font ? 'T' : CG_T) : nullblank, KT_STD);
}

static void
do_compose(Boolean on, unsigned char c, enum keytype keytype)
{
	if (on) {
		status_add(COMPOSE,
		    (unsigned char)(*standard_font ? 'C' : CG_C), KT_STD);
		status_add(COMPOSE+1,
		    c ? (*standard_font ? c : asc2cg[c]) : nullblank, keytype);
	} else {
		status_add(COMPOSE, nullblank, KT_STD);
		status_add(COMPOSE+1, nullblank, KT_STD);
	}
}

/* Timing */
static void
do_timing(char *buf)
{
	register int	i;

	if (buf) {
		if (*standard_font) {
			status_add(T0, nullblank, KT_STD);
			status_add(T0+1, nullblank, KT_STD);
		} else {
			status_add(T0, CG_clockleft, KT_STD);
			status_add(T0+1, CG_clockright, KT_STD);
		}
		for (i = 0; i < (int) strlen(buf); i++)
			status_add(T0+2+i, *standard_font ? buf[i] : asc2cg[(unsigned char) buf[i]], KT_STD);
	} else
		for (i = 0; i < TCNT; i++)
			status_add(T0+i, nullblank, KT_STD);
}

/* Cursor position */
static void
do_cursor(char *buf)
{
	register int	i;

	if (buf)
		for (i = 0; i < (int) strlen(buf); i++)
			status_add(C0+i, *standard_font ? buf[i] : asc2cg[(unsigned char) buf[i]], KT_STD);
	else
		for (i = 0; i < CCNT; i++)
			status_add(C0+i, nullblank, KT_STD);
}

/* Prepare status messages */

static unsigned char *
make_amsg(const char *key)
{
	return (unsigned char *)xs_buffer("X %s", get_message(key));
}

static unsigned char *
make_emsg(unsigned char prefix[], const char *key, int *len)
{
	const char *text = get_message(key);
	unsigned char *buf = (unsigned char *)XtMalloc(*len + strlen(text));

	(void) MEMORY_MOVE((char *)buf, (char *)prefix, *len);
	while (*text)
		buf[(*len)++] = asc2cg[(int)*text++];

	return buf;
}