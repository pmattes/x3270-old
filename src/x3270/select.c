/*
 * Portions of this code were taken from xterm/button.c:
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Modifications and original code:
 * Copyright 1993 Paul Mattes.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 */

/*
 *	select.c
 *		This module handles selections.
 */
#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>
#include "3270.h"
#include "3270_enc.h"
#include "globals.h"

/*
 * Mouse side.
 */

	/* A button click establishes the boundaries of the 'fixed' area. */
static int	f_start = 0;	/* 'fixed' area */
static int	f_end = 0;

	/* Mouse motion moves the boundaries of the 'varying' area. */
static int	v_start = 0;	/* 'varying' area */
static int	v_end = 0;

static unsigned long	down_time = 0;
static unsigned long	down1_time = 0;
static Dimension	down1_x, down1_y;
static unsigned long	up_time = 0;
static int		saw_motion = 0;
static int		num_clicks = 0;
static void		grab_sel();
#define NS		5
static Atom		want_sel[NS];
static Atom		own_sel[NS];
static Boolean		cursor_moved = False;
static int		saved_cursor_addr;
int			n_owned = -1;
Boolean	any_selected = False;

extern Boolean		kybdlock;
extern unsigned char	*screen_buf;
extern int		cursor_addr;
extern int		*char_width, *char_height;
extern Widget		*screen;

#define CLICK_INTERVAL	200

#define event_x(event)		event->xbutton.x
#define event_y(event)		event->xbutton.y
#define event_time(event)	event->xbutton.time

#define BOUNDED_XY(event, x, y) {	\
	x = X_TO_COL(event_x(event));	\
	if (x < 0)			\
		x = 0;			\
	if (x >= COLS)			\
		x = COLS - 1;		\
	y = Y_TO_ROW(event_y(event));	\
	if (y <= 0)			\
		y = 0;			\
	if (y >= ROWS)			\
		y = ROWS - 1;		\
}


static void
select_word(baddr, t)
int baddr;
Time t;
{
	unsigned char ch;
	unsigned char fa = *get_field_attribute(baddr);

	/* Is there anything there? */
	ch = screen_buf[baddr];
	if (IS_FA(ch) || ch == CG_NULLBLANK || ch == CG_BLANK || FA_IS_ZERO(fa)) {
		return;
	}

	/* Find the beginning */
	for (f_start = baddr; f_start % COLS; f_start--) {
		ch = screen_buf[f_start];
		fa = *get_field_attribute(f_start);
		if (IS_FA(ch) || ch == CG_NULLBLANK || ch == CG_BLANK || FA_IS_ZERO(fa)) {
			f_start++;
			break;
		}
	}

	/* Find the end */
	for (f_end = baddr; (f_end+1) % COLS; f_end++) {
		ch = screen_buf[f_end];
		fa = *get_field_attribute(f_end);
		if (IS_FA(ch) || ch == CG_NULLBLANK || ch == CG_BLANK || FA_IS_ZERO(fa)) {
			f_end--;
			break;
		}
	}

	grab_sel(f_start, f_end, True, t);
}

static void
select_line(baddr, t)
int baddr;
Time t;
{
	f_start = baddr - (baddr % COLS);
	f_end = f_start + COLS - 1;
	grab_sel(f_start, f_end, True, t);
}


/*
 * Start a new selection.
 * Usually bound to <Btn1Down>.
 */
/*ARGSUSED*/
void
select_start(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal num_params;
{
	int x, y;
	register int baddr;

	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);
	f_start = f_end = v_start = v_end = baddr;
	down1_time = down_time = event_time(event);
	down1_x = event_x(event);
	down1_y = event_y(event);
	if (down_time - up_time > CLICK_INTERVAL) {
		num_clicks = 0;
		/* Commit any previous cursor move. */
		cursor_moved = False;
	}
	if (num_clicks == 0)
		unselect(0, ROWS*COLS);
}

/*
 * Alternate form of select_start, which combines cursor motion with selection.
 * Usually bound to <Btn1Down> in a user-specified keymap.
 */
/*ARGSUSED*/
void
move_select(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal num_params;
{
	int x, y;
	register int baddr;

	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	f_start = f_end = v_start = v_end = baddr;
	down1_time = down_time = event_time(event);
	down1_x = event_x(event);
	down1_y = event_y(event);

	if (down_time - up_time > CLICK_INTERVAL) {
		num_clicks = 0;
		/* Commit any previous cursor move. */
		cursor_moved = False;
	}
	if (num_clicks == 0) {
		if (any_selected) {
			unselect(0, ROWS*COLS);
		} else {
			cursor_moved = True;
			saved_cursor_addr = cursor_addr;
			cursor_move(baddr);
		}
	}
}

/*
 * Begin extending the current selection.
 * Usually bound to <Btn3Down>.
 */
/*ARGSUSED*/
void
start_extend(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal num_params;
{
	int x, y;
	int baddr;

	if (w != *screen)
		return;

	down1_time = 0L;

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	if (baddr < f_start)
		v_start = baddr;		/* extend above */
	else if (baddr > f_end)
		v_end = baddr;			/* extend below */
	else if (baddr - f_start > f_end - baddr)
		v_end = baddr;			/* shrink end */
	else
		v_start = baddr;		/* shrink start */

	grab_sel(v_start, v_end, True, event_time(event));
	saw_motion = 1;
	num_clicks = 0;
}

/*
 * Continuously extend the current selection.
 * Usually bound to <Btn1Motion> and <Btn3Motion>.
 */
/*ARGSUSED*/
void
select_extend(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal num_params;
{
	int x, y;
	int baddr;

	if (w != *screen)
		return;

	/* Ignore initial drag events if are too near. */
	if (down1_time != 0L &&
	    abs((int) event_x(event) - (int) down1_x) < CHAR_WIDTH &&
	    abs((int) event_y(event) - (int) down1_y) < CHAR_HEIGHT)
		return;
	else
		down1_time = 0L;

	/* If we moved the 3270 cursor on the first click, put it back. */
	if (cursor_moved) {
		cursor_move(saved_cursor_addr);
		cursor_moved = False;
	}

	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);

	/*
	 * If baddr falls outside if the v range, open up the v range.  In
	 * addition, if we are extending one end of the v range, make sure the
	 * other end at least covers the f range.
	 */
	if (baddr <= v_start) {
		v_start = baddr;
		v_end = f_end;
	}
	if (baddr >= v_end) {
		v_end = baddr;
		v_start = f_start;
	}

	/*
	 * If baddr falls within the v range, narrow up the nearer end of the
	 * v range.
	 */
	if (baddr > v_start && baddr < v_end) {
		if (baddr - v_start < v_end - baddr)
			v_start = baddr;
		else
			v_end = baddr;
	}

	num_clicks = 0;
	saw_motion = 1;
	grab_sel(v_start, v_end, False, event_time(event));
}

/*
 * End the selection.
 * Usually bound to <BtnUp>.
 */
/*ARGSUSED*/
void
select_end(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int i;
	int x, y;

	if (n_owned == -1) {
		for (i = 0; i < NS; i++)
			own_sel[i] = None;
		n_owned = 0;
	}
	for (i = 0; i < NS; i++)
		if (i < *num_params)
			 want_sel[i] = XInternAtom(display, params[i], False);
		else
			want_sel[i] = None;
	if (*num_params == 0)
		want_sel[0] = XA_PRIMARY;

	BOUNDED_XY(event, x, y);
	up_time = event_time(event);

	if (up_time - down_time > CLICK_INTERVAL)
		num_clicks = 0;

	if (++num_clicks > 3)
		num_clicks = 1;

	switch (num_clicks) {
	    case 1:
		if (saw_motion) {
			f_start = v_start;
			f_end = v_end;
			grab_sel(f_start, f_end, True, event_time(event));
		}
		break;
	    case 2:
		/*
		 * If we moved the 3270 cursor on the first click, put it back.
		 */
		if (cursor_moved) {
			cursor_move(saved_cursor_addr);
			cursor_moved = False;
		}
		select_word(f_start, event_time(event));
		break;
	    case 3:
		select_line(f_start, event_time(event));
		break;
	}
	saw_motion = 0;
}

/*
 * MoveCursor function.  Not a selection operation, but it uses the same macros
 * to map the mouse position onto a cursor position.
 * Usually bound to Shift<Btn1Down>.
 */
/*ARGSUSED*/
void
MoveCursor(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal num_params;
{
	int x, y;
	register int baddr;

	if (w != *screen)
		return;
	BOUNDED_XY(event, x, y);
	baddr = ROWCOL_TO_BA(y, x);
	cursor_move(baddr);
}


/*
 * Screen side.
 */
extern char	*select_buf;
extern unsigned char	*selected;
extern unsigned char	cg2asc[];
extern Boolean	screen_changed;
extern unsigned int	**x_image;

static Time	sel_time;


static Boolean
convert_sel(w, selection, target, type, value, length, format)
Widget w;
Atom *selection, *target, *type;
XtPointer *value;
unsigned long *length;
int *format;
{
	if (*target == XA_TARGETS(display)) {
		Atom* targetP;
		Atom* std_targets;
		unsigned long std_length;

		XmuConvertStandardSelection(w, sel_time, selection,
		    target, type, (caddr_t*) &std_targets, &std_length, format);
		*length = std_length + 5;
		*value = (XtPointer) XtMalloc(sizeof(Atom) * (*length));
		targetP = *(Atom**)value;
		*targetP++ = XA_STRING;
		*targetP++ = XA_TEXT(display);
		*targetP++ = XA_COMPOUND_TEXT(display);
		*targetP++ = XA_LENGTH(display);
		*targetP++ = XA_LIST_LENGTH(display);
		(void) MEMORY_MOVE((char *) targetP,
				   (char *) std_targets,
				   (int) (sizeof(Atom) * std_length));
		XtFree((char *) std_targets);
		*type = XA_ATOM;
		*format = 32;
		return True;
	}

	if (*target == XA_STRING ||
	    *target == XA_TEXT(display) ||
	    *target == XA_COMPOUND_TEXT(display)) {
		if (*target == XA_COMPOUND_TEXT(display))
			*type = *target;
		else
			*type = XA_STRING;
		*length = strlen(select_buf);
		*value = XtMalloc(*length);
		(void) MEMORY_MOVE(*value, select_buf, (int) *length);
		*format = 8;
		return True;
	}
	if (*target == XA_LIST_LENGTH(display)) {
		*value = XtMalloc(4);
		if (sizeof(long) == 4)
			*(long *)*value = 1;
		else {
			long temp = 1;
			(void) MEMORY_MOVE((char *) *value,
			                   ((char*) &temp) + sizeof(long) - 4,
					   4);
		}
		*type = XA_INTEGER;
		*length = 1;
		*format = 32;
		return True;
	}
	if (*target == XA_LENGTH(display)) {
		*value = XtMalloc(4);
		if (sizeof(long) == 4)
			*(long*)*value = strlen(select_buf);
		else {
			long temp = strlen(select_buf);
			(void) MEMORY_MOVE((char *) *value,
			                   ((char *) &temp) + sizeof(long) - 4,
					   4);
		}
		*type = XA_INTEGER;
		*length = 1;
		*format = 32;
		return True;
	}
	if (XmuConvertStandardSelection(w, sel_time, selection,
	    target, type, (caddr_t *)value, length, format))
		return True;

	/* else */
	return False;
}

/*ARGSUSED*/
static void
lose_sel(w, selection)
Widget w;
Atom *selection;
{
	int i;

	for (i = 0; i < NS; i++)
		if (own_sel[i] != None && own_sel[i] == *selection) {
			own_sel[i] = None;
			n_owned--;
			break;
		}
	if (!n_owned)
		unselect(0, ROWS*COLS);
}

static void
disown_sels()
{
	int i;

	if (!n_owned)
		return;

	for (i = 0; i < NS; i++)
		if (own_sel[i] != None)
			XtDisownSelection(*screen, own_sel[i], CurrentTime);
	n_owned = 0;
}

static void
grab_sel(start, end, really, t)
int start, end;
Boolean really;
Time t;
{
	register int i, j;
	int start_row, end_row;
	int ix = 0;

	unselect(0, ROWS*COLS);

	if (start > end) {
		int exch = end;

		end = start;
		start = exch;
	}

	start_row = start / COLS;
	end_row = end / COLS;

	if (ansi_host && !ever_3270) {	/* Continuous selections */
		for (i = start; i <= end; i++) {
			SET_SELECT(i);
			if (really) {
				if (i != start && !(i % COLS))
					select_buf[ix++] = '\n';
				select_buf[ix++] = cg2asc[(*x_image)[i] & 0xff];
			}
		}
		/* Check for newline extension */
		if ((end % COLS) != (COLS - 1)) {
			Boolean all_blank = True;

			for (i = end; i < end + (COLS - (end % COLS)); i++)
				if (cg2asc[(*x_image)[i] & 0xff] != ' ') {
					all_blank = False;
					break;
				}
			if (all_blank) {
				for (i = end; i < end + (COLS - (end % COLS)); i++)
					SET_SELECT(i);
				select_buf[ix++] = '\n';
			}
		}
	} else {	/* Rectangular selections */
		if (start_row == end_row) {
			for (i = start; i <= end; i++) {
				SET_SELECT(i);
				if (really)
					select_buf[ix++] = cg2asc[(*x_image)[i] & 0xff];
			}
			if (really && (end % COLS == COLS - 1))
				select_buf[ix++] = '\n';
		} else {
			int row, col;
			int start_col = start % COLS;
			int end_col = end % COLS;

			if (start_col > end_col) {
				int exch = end_col;

				end_col = start_col;
				start_col = exch;
			}

			for (row = start_row; row <= end_row; row++) {
				for (col = start_col; col <= end_col; col++) {
					SET_SELECT(row*COLS + col);
					if (really)
						select_buf[ix++] = cg2asc[(*x_image)[row*COLS + col] & 0xff];
				}
				if (really)
					select_buf[ix++] = '\n';
			}
		}
	}

	/* Trim trailing blanks on each line */
	if (really) {
		int k, j;
		char c;
		int nb = 0;
		int iy = 0;

		select_buf[ix] = '\0';
		for (k = 0; c = select_buf[k]; k++) {
			if (c == ' ')
				nb++;
			else {
				if (c != '\n') {
					for (j = 0; j < nb; j++)
						select_buf[iy++] = ' ';
				}
				select_buf[iy++] = c;
				nb = 0;
			}
		}
		select_buf[iy] = '\0';
	}
	any_selected = True;
	screen_changed = True;
	if (really) {
		/*
		 * Try to grab any new selections we may want.
		 */
		for (i = 0; i < NS; i++) {
			Boolean already_own = False;

			if (want_sel[i] == None)
				continue;

			/* Check if we already own it. */
			for (j = 0; j < NS; j++)
				if (own_sel[j] == want_sel[i]) {
					already_own = True;
					break;
				}
			if (already_own)
				continue;

			/* Find an empty slot for it. */
			for (j = 0; j < NS; j++)
				if (own_sel[j] == None)
					break;
			if (j >= NS)
				continue;

			if (XtOwnSelection(*screen, want_sel[i], t,
			    convert_sel, lose_sel, NULL)) {
				n_owned++;
				own_sel[j] = want_sel[i];
			}
		}
		if (!n_owned)
			unselect(0, ROWS*COLS);
		sel_time = t;
	}
}

/*
 * Check if any character in a given region is selected.
 */
Boolean
area_is_selected(baddr, len)
int baddr, len;
{
	register int i;

	for (i = 0; i < len; i++)
		if (SELECTED(baddr+i))
			return True;
	return False;
}

/*
 * Unhighlight the region of selected text -- but don't give up the selection.
 */
/*ARGSUSED*/
void
unselect(baddr, len)
int baddr;
int len;
{
	if (any_selected) {
		(void) memset((char *) selected, 0, (ROWS*COLS + 7) / 8);
		screen_changed = True;
		any_selected = False;
	}
}
