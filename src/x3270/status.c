/*
 * Copyright 1993 Paul Mattes.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 */

/*
 *	status.c
 *		This module handles the 3270 status line.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include "3270.h"
#include "3270_enc.h"
#include "globals.h"


extern int		*char_width, *char_height;
extern Window		*screen_window;
extern GC		*gc, *invgc;
extern unsigned char	asc2cg[];

static unsigned char	*status_buf;
static char		*status_display;
static Boolean		status_changed = False;

static struct status_line {
	Boolean changed;
	int start, len, color;
	unsigned char *buf;
	char *display;
} *status_line;

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

#define CM	(60 * 10)	/* csec per minute */

/* Positions */

#define LBOX	0		/* left-hand box */
#define CNCT	1		/* connection between */
#define RBOX	2		/* right-hand box */

#define M0	8		/* message area */

#define SHIFT	(maxCOLS-39)	/* shift indication */

#define COMPOSE	(maxCOLS-36)	/* compose characters */

#define INSERT	(maxCOLS-29)	/* insert mode */

#define T0	(maxCOLS-15)	/* timings */
#define	TCNT	7

#define C0	(maxCOLS-7)	/* cursor position */
#define CCNT	7

#define STATUS_Y	(ROW_TO_Y(maxROWS)+SGAP-1)

static unsigned char	nullblank;
static Position		status_y;

/* Status line contents (high-level) */

static void do_disconnected();
static void do_connecting();
static void do_blank();
static void do_twait();
static void do_syswait();
static void do_protected();
static void do_numeric();
static void do_overflow();

static Boolean oia_undera = False;
static Boolean oia_boxsolid = False;
static int oia_shift = 0;
static Boolean oia_compose = False;
static Boolean oia_compose_char = 0;
static enum msg {
	DISCONNECTED,		/* X Not Connected */
	CONNECTING,		/* X Connecting */
	BLANK,			/* (blank) */
	TWAIT,			/* X Wait */
	SYSWAIT,		/* X SYSTEM */
	PROTECTED,		/* X Protected */
	NUMERIC,		/* X Numeric */
	OVERFLOW		/* X Overflow */
} oia_msg = DISCONNECTED;
void (*msg_proc[])() = {
	do_disconnected,
	do_connecting,
	do_blank,
	do_twait,
	do_syswait,
	do_protected,
	do_numeric,
	do_overflow
};
int msg_color[] = {
	FA_INT_HIGH_SEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_NSEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL,
	FA_INT_NORM_SEL
};
static Boolean oia_insert = False;
static char *oia_cursor = (char *) 0;
static char *oia_timing = (char *) 0;

static unsigned char disc_pfx[] = {
	CG_LOCK, CG_BLANK, CG_BADCOMMHI, CG_COMMJAG, CG_COMMLO, CG_BLANK
};
static unsigned char *disc_msg;
static int disc_len = sizeof(disc_pfx);

static unsigned char cnct_pfx[] = {
	CG_LOCK, CG_BLANK, CG_COMMHI, CG_COMMJAG, CG_COMMLO, CG_BLANK
};
static unsigned char *cnct_msg;
static int cnct_len = sizeof(cnct_pfx);

static char *a_not_connected;
static char *a_connecting;
static char *a_twait;
static char *a_syswait;
static char *a_protected;
static char *a_numeric;
static char *a_overflow;

static char *make_amsg();
static unsigned char *make_emsg();

static void status_render();
static void do_ctlr();
static void do_msg();
static void do_insert();
static void do_shift();
static void do_compose();
static void do_timing();
static void do_cursor();


/* Initialize, or reinitialize the status line */
void
status_init(keep_contents, model_changed, font_changed)
Boolean keep_contents;
Boolean model_changed;
Boolean font_changed;
{
	int	i;
	static Boolean	ever = False;

	if (!ever) {
		a_not_connected = make_amsg("statusNotConnected");
		disc_msg = make_emsg(disc_pfx, "statusNotConnected",
		    &disc_len);
		a_connecting = make_amsg("statusConnecting");
		cnct_msg = make_emsg(cnct_pfx, "statusConnecting", &cnct_len);
		a_twait = make_amsg("statusTwait");
		a_syswait = make_amsg("statusSyswait");
		a_protected = make_amsg("statusProtected");
		a_numeric = make_amsg("statusNumeric");
		a_overflow = make_amsg("statusOverflow");
		ever = True;
	}
	if (font_changed)
		nullblank = *standard_font ? ' ' : CG_BLANK;
	if (font_changed || model_changed) {
		status_y = STATUS_Y;
		if (!(*efontinfo)->descent)
			++status_y;
	}
	if (model_changed) {
		if (status_line)
			XtFree((char *)status_line);
		status_line = (struct status_line *) XtCalloc(sizeof(struct status_line), SSZ);
		if (status_buf)
			XtFree((char *)status_buf);
		status_buf = (unsigned char *) XtCalloc(sizeof(unsigned char), maxCOLS);
		if (status_display)
			XtFree(status_display);
		status_display = XtCalloc(sizeof(char), maxCOLS);
		offsets[SSZ] = maxCOLS;
		if (appres.mono)
			colors[1] = FA_INT_NORM_NSEL;
		for (i = 0; i < SSZ; i++) {
			status_line[i].color = colors[i];
			status_line[i].start = offsets[i];
			status_line[i].len = offsets[i+1] - offsets[i];
			status_line[i].buf = status_buf + offsets[i];
			status_line[i].display = status_display + offsets[i];
		}
	} else
		(void) memset(status_display, 0, maxCOLS);

	for (i = 0; i < SSZ; i++)
		status_line[i].changed = keep_contents;
	status_changed = keep_contents;

	/*
	 * If reinitializing, redo the whole line, in case we switched between
	 * a 3270 font and a standard font.
	 */
	if (keep_contents && font_changed) {
		do_ctlr();
		do_msg(oia_msg);
		do_insert(oia_insert);
		do_shift(oia_shift);
		do_compose(oia_compose, oia_compose_char);
		do_cursor(oia_cursor);
		do_timing(oia_timing);
	}
}

/* Render the status line onto the screen */
void
status_disp()
{
	int	i;

	if (!status_changed)
		return;
	for (i = 0; i < SSZ; i++)
		if (status_line[i].changed) {
			status_render(i);
			(void) MEMORY_MOVE(status_line[i].display,
			                   (char *) status_line[i].buf,
					   status_line[i].len);
			status_line[i].changed = False;
		}
	status_changed = False;
}

/* Mark the entire status line as changed */
void
status_touch()
{
	int	i;

	for (i = 0; i < SSZ; i++) {
		status_line[i].changed = True;
		(void) memset(status_line[i].display, 0, status_line[i].len);
	}
	status_changed = True;
}

/* Initialize the controller status */
void
status_ctlr_init()
{
	oia_undera = True;
	do_ctlr();
}

/* Connected or disconnected */
void
status_connect()
{
	if (CONNECTED) {
		oia_boxsolid = True;
		do_ctlr();
		do_msg(BLANK);
		status_untiming();
	} else if (HALF_CONNECTED) {
		oia_boxsolid = False;
		do_ctlr();
		do_msg(CONNECTING);
		status_untiming();
		status_uncursor_pos();
	} else {
		oia_boxsolid = False;
		do_ctlr();
		do_msg(DISCONNECTED);
		status_uncursor_pos();
	}
}

/* Lock the keyboard (twait) */
void
status_twait()
{
	oia_undera = False;
	do_ctlr();
	do_msg(TWAIT);
}

/* Done with controller confirmation */
void
status_ctlr_done()
{
	oia_undera = True;
	do_ctlr();
}

/* Lock the keyboard (X SYSTEM) */
void
status_syswait()
{
	do_msg(SYSWAIT);
}

/* Lock the keyboard (protected) */
void
status_protected()
{
	do_msg(PROTECTED);
}

/* Lock the keyboard (numeric) */
void
status_numeric()
{
	do_msg(NUMERIC);
}

/* Lock the keyboard (insert overflow) */
void
status_overflow()
{
	do_msg(OVERFLOW);
}

/* Unlock the keyboard */
void
status_reset()
{
	do_msg(BLANK);
}

/* Toggle insert mode */
void
status_insert_mode(on)
Boolean on;
{
	do_insert(oia_insert = on);
}

/* Toggle shift mode */
void
status_shift_mode(state)
int state;
{
	do_shift(oia_shift = state);
}

/* Set compose character */
void
status_compose(on, c)
Boolean on;
unsigned char c;
{
	oia_compose = on;
	oia_compose_char = c;
	do_compose(on, c);
}

/* Display timing */
void
status_timing(t0, t1)
struct timeval *t0, *t1;
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
			(void) sprintf(buf, ":%02d.%d", cs / 10, cs % 10);
		else
			(void) sprintf(buf, "%02d:%02d", cs / CM, (cs % CM) / 10);
		do_timing(oia_timing = buf);
	}
}

/* Erase timing indication */
void
status_untiming()
{
	do_timing(oia_timing = (char *) 0);
}

/* Update cursor position */
void
status_cursor_pos(ca)
int ca;
{
	static char	buf[CCNT+1];

	if (IN_3270)
		(void) sprintf(buf, "%03d/%03d", ca/COLS, ca%COLS);
	else
		(void) sprintf(buf, "%03d/%03d", ca/COLS + 1, ca%COLS + 1);
	do_cursor(oia_cursor = buf);
}

/* Erase cursor position */
void
status_uncursor_pos()
{
	do_cursor(oia_cursor = (char *) 0);
}


/* Internal routines */

/* Update the status line by displaying "symbol" at column "col".  */
static void
status_add(col, symbol)
	int	col;
	unsigned char symbol;
{
	int	i;

	if (status_buf[col] == symbol)
		return;
	status_buf[col] = symbol;
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
status_render(region)
	int	region;
{
	int	i;
	struct status_line *sl = &status_line[region];
	int	nd = 0;
	int	i0 = -1;

	/* The status region may change colors; don't be so clever */
	if (region == WAIT_REGION) {
		XDrawImageString(display, *screen_window, gc[sl->color],
		    COL_TO_X(sl->start), status_y, (char *) sl->buf, sl->len);
	} else {
		for (i = 0; i < sl->len; i++) {
			if ((unsigned char) sl->buf[i] == sl->display[i]) {
				if (nd) {
					XDrawImageString(display, *screen_window, gc[sl->color],
						COL_TO_X(sl->start + i0),
						status_y,
						(char *) sl->buf + i0, nd);
					nd = 0;
					i0 = -1;
				}
			} else {
				if (!nd++)
					i0 = i;
			}
		}
		if (nd)
			XDrawImageString(display, *screen_window, gc[sl->color],
				COL_TO_X(sl->start + i0), status_y,
				(char *) sl->buf + i0, nd);
	}

	/* Leftmost region has unusual attributes */
	if (*standard_font && region == CTLR_REGION) {
		XDrawImageString(display, *screen_window, invgc[sl->color],
			COL_TO_X(sl->start + LBOX), status_y,
			(char *) sl->buf + LBOX, 1);
		XDrawRectangle(display, *screen_window, gc[sl->color],
		    COL_TO_X(sl->start + CNCT),
		    status_y - (*efontinfo)->ascent + *char_height - 1,
		    *char_width - 1, 0);
		XDrawImageString(display, *screen_window, invgc[sl->color],
			COL_TO_X(sl->start + RBOX), status_y,
			(char *) sl->buf + RBOX, 1);
	}
}

/* Write into the message area of the status line */
static void
status_msg_set(msg, len)
unsigned char *msg;
int len;
{
	register int	i;

	for (i = 0; i < status_line[WAIT_REGION].len; i++) {
		status_add(M0+i, len ? msg[i] : nullblank);
		if (len)
			len--;
	}
}

/* Controller status */
static void
do_ctlr()
{
	if (*standard_font) {
		status_add(LBOX, '4');
		if (oia_undera)
			status_add(CNCT, 'A');
		else
			status_add(CNCT, ' ');
		if (oia_boxsolid)
			status_add(RBOX, ' ');
		else
			status_add(RBOX, '?');
	} else {
		status_add(LBOX, CG_BOX4);
		if (oia_undera)
			status_add(CNCT, CG_UNDERA);
		else
			status_add(CNCT, CG_NULLBLANK);
		if (oia_boxsolid)
			status_add(RBOX, CG_BOXSOLID);
		else
			status_add(RBOX, CG_BOXQUESTION);
	}
}

/* Message area */

static void
do_msg(t)
enum msg t;
{
	oia_msg = t;
	(*msg_proc[t])();
	if (!appres.mono)
		status_line[WAIT_REGION].color = msg_color[t];
}

static void
do_blank()
{
	status_msg_set((unsigned char *) 0, 0);
}

static void
do_disconnected()
{
	if (*standard_font)
		status_msg_set(a_not_connected, strlen(a_not_connected));
	else
		status_msg_set(disc_msg, disc_len);
}

static void
do_connecting()
{
	if (*standard_font)
		status_msg_set(a_connecting, strlen(a_connecting));
	else
		status_msg_set(cnct_msg, cnct_len);
}

static void
do_twait()
{
	static unsigned char twait[] = {
		CG_LOCK, CG_BLANK, CG_CLOCKLEFT, CG_CLOCKRIGHT
	};

	if (*standard_font)
		status_msg_set(a_twait, strlen(a_twait));
	else
		status_msg_set(twait, sizeof(twait));
}

static void
do_syswait()
{
	static unsigned char syswait[] = {
		CG_LOCK, CG_BLANK, CG_CS, CG_CY, CG_CS, CG_CT, CG_CE, CG_CM
	};

	if (*standard_font)
		status_msg_set(a_syswait, strlen(a_syswait));
	else
		status_msg_set(syswait, sizeof(syswait));
}

static void
do_protected()
{
	static unsigned char protected[] = {
		CG_LOCK, CG_BLANK, CG_LEFTARROW, CG_HUMAN, CG_RIGHTARROW
	};

	if (*standard_font)
		status_msg_set(a_protected, strlen(a_protected));
	else
		status_msg_set(protected, sizeof(protected));
}

static void
do_numeric()
{
	static unsigned char numeric[] = {
		CG_LOCK, CG_BLANK, CG_HUMAN, CG_CN, CG_CU, CG_CM
	};

	if (*standard_font)
		status_msg_set(a_numeric, strlen(a_numeric));
	else
		status_msg_set(numeric, sizeof(numeric));
}

static void
do_overflow()
{
	static unsigned char overflow[] = {
		CG_LOCK, CG_BLANK, CG_HUMAN, CG_GREATER
	};

	if (*standard_font)
		status_msg_set(a_overflow, strlen(a_overflow));
	else
		status_msg_set(overflow, sizeof(overflow));
}

/* Insert, shift, compose */

static void
do_insert(on)
Boolean on;
{
	status_add(INSERT, on ? (*standard_font ? 'I' : CG_INSERT) : nullblank);
}

static void
do_shift(state)
int state;
{
	status_add(SHIFT-2, (state & MetaKeyDown) ?
		(*standard_font ? 'M' : CG_CM) : nullblank);
	status_add(SHIFT-1, (state & AltKeyDown) ?
		(*standard_font ? 'A' : CG_CA) : nullblank);
	status_add(SHIFT, (state & ShiftKeyDown) ?
		(*standard_font ? '^' : CG_UPSHIFT) : nullblank);
}

static void
do_compose(on, c)
Boolean on;
unsigned char c;
{
	if (on) {
		status_add(COMPOSE, (*standard_font ? 'C' : CG_CC));
		status_add(COMPOSE+1, c ? (*standard_font ? c : asc2cg[c]) : nullblank);
	} else {
		status_add(COMPOSE, nullblank);
		status_add(COMPOSE+1, nullblank);
	}
}

/* Timing */
static void
do_timing(buf)
char *buf;
{
	register int	i;

	if (buf) {
		if (*standard_font) {
			status_add(T0, nullblank);
			status_add(T0+1, nullblank);
		} else {
			status_add(T0, CG_CLOCKLEFT);
			status_add(T0+1, CG_CLOCKRIGHT);
		}
		for (i = 0; i < (int) strlen(buf); i++)
			status_add(T0+2+i, *standard_font ? buf[i] : asc2cg[(unsigned char) buf[i]]);
	} else
		for (i = 0; i < TCNT; i++)
			status_add(T0+i, nullblank);
}

/* Cursor position */
static void
do_cursor(buf)
char *buf;
{
	register int	i;

	if (buf)
		for (i = 0; i < (int) strlen(buf); i++)
			status_add(C0+i, *standard_font ? buf[i] : asc2cg[(unsigned char) buf[i]]);
	else
		for (i = 0; i < CCNT; i++)
			status_add(C0+i, nullblank);
}

/* Prepare status messages */

static char *
make_amsg(key)
char *key;
{
	return xs_buffer("X %s", get_message(key));
}

static unsigned char *
make_emsg(prefix, key, len)
unsigned char prefix[];
char *key;
int *len;
{
	char *text = get_message(key);
	unsigned char *buf = (unsigned char *) XtMalloc(*len + strlen(key));
	extern unsigned char asc2g[];

	(void) MEMORY_MOVE(buf, (char *) prefix, *len);
	while (*text)
		buf[(*len)++] = asc2cg[*text++];

	return buf;
}
