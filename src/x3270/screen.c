/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 * Copyright 1990 Jeff Sparkes.
 * Copyright 1993 Paul Mattes.
 *
 *                         All Rights Reserved
 */

/*
 *	screen.c
 *		This module handles the X display.  It has been extensively
 *		optimized to minimize X drawing operations.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Composite.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "3270.h"
#include "3270_enc.h"
#include "globals.h"

#define MODIFIED_SEL	1	/* modified fields appear in "select" color */

#define IMAGE_COLOR(fa)		(fa_color(fa) << 8)
#define CG_FROM_IMAGE(i)	(((i) >> 8) & 0x7)

/* Externals: x3270.c */
extern int	foreground, background;
extern char	*charset;

/* Externals: ctlr.c */
extern int	cursor_addr, buffer_addr;
extern unsigned char	*screen_buf;	/* 3270 display buffer */
extern Boolean	screen_changed;

/* Externals: kybd.c */
extern unsigned char	cg2asc[];
extern unsigned char	cg2asc7[];

/* Globals */
char		*select_buf;	/* what's been selected */
unsigned char	*selected;	/* selection bitmap */
#include "gray.bm"

/* Statics */
static unsigned char	*blanks;
static unsigned int	*temp_image;	/* temporary for X display */
static Boolean	cursor_displayed = False;
static Boolean	blink_pending = False;
static XtIntervalId	blink_id;
static Boolean	in_focus = False;
static Boolean	image_changed = False;
static Boolean	line_changed = False;
static Boolean	cursor_changed = False;
static Boolean	iconic = False;
static Widget	container;
static Dimension	menubar_height;
static Dimension	keypad_height;
static Dimension	keypad_xwidth;
static Dimension	container_width;
static Dimension	container_height;
static unsigned char	cg2uc[256];
static char		*aicon_text = (char *) NULL;
static XFontStruct	*ailabel_font;
static Dimension	aicon_label_height = 0;
static GC		ailabel_gc;
static enum { LOCKED, NORMAL, WAIT } mcursor_state = LOCKED;

static void	toggle_monocase();
static void	toggle_altcursor();
static void	toggle_blink();
static void	toggle_cursorp();
static void	remap_ascii();
static void	remap_chars();
static void	screen_focus();
static void	make_gc_set();
static void	make_gcs();
static void	put_cursor();
static void	resync_display();
static void	draw_fields();
static void	render_text();
static void	cursor_pos();
static void	cursor_on();
static void	schedule_blink();
static void	inflate_screen();
static int	fa_color();
static Boolean	cursor_off();
static void	draw_aicon_label();
static void	set_mcursor();

/*
 * The screen state structure.  This structure is swapped whenever we switch
 * between normal and active-iconic states.
 */
struct sstate {
	Widget 		widget;		/* the widget */
	Window		window;		/* the window */
	unsigned int	*image;		/* what's on the X display */
	int		cursor_daddr;	/* displayed cursor address */
	Boolean		exposed_yet;	/* have we been exposed yet? */
	Boolean		overstrike;	/* are we overstriking? */
	Dimension	screen_width;	/* screen dimensions in pixels */
	Dimension	screen_height;
	GC		gc[4], invgc[4], selgc[4], mcgc; /* graphics contexts */
	int		char_height;
	int		char_width;
	XFontStruct	*efontinfo, *bfontinfo;
	Boolean		standard_font;
	Boolean		latin1_font;
};
static struct sstate nss;
static struct sstate iss;
static struct sstate *ss = &nss;

/* Globals based on nss, used mostly by status and select routines. */
Widget 		*screen = &nss.widget;
Window		*screen_window = &nss.window;
unsigned int	**x_image = &nss.image;
Boolean		*overstrike = &nss.overstrike;
GC		*gc = nss.gc;
GC		*invgc = nss.invgc;
int		*char_width = &nss.char_width;
int		*char_height = &nss.char_height;
XFontStruct	**efontinfo = &nss.efontinfo;
XFontStruct	**bfontinfo = &nss.bfontinfo;
Boolean		*standard_font = &nss.standard_font;
Boolean		*latin1_font = &nss.latin1_font;


/* 
 * Define our event translations
 */
void
set_translations(w)
Widget w;
{
	struct trans_list *t;
	XtTranslations trans;

	if (appres.default_translations)
		XtOverrideTranslations(w, appres.default_translations);

	for (t = trans_list; t != NULL; t = t->next) {
		trans = XtParseTranslationTable(t->translations);
		XtOverrideTranslations(w, trans);
	}
}


/*
 * Initialize or re-initialize the screen.
 */
void
screen_init(keep_contents, model_changed, font_changed)
Boolean keep_contents;
Boolean model_changed;
Boolean font_changed;
{
	extern Dimension min_keypad_width;
	Widget w;
	static Boolean ever = False;

	if (!ever) {
		register int i;

		/* Initialize ss. */
		nss.cursor_daddr = 0;
		nss.exposed_yet = False;

		/* Set up monocase translation table. */
		for (i = 0; i < 256; i++)
			if ((i & 0xE0) == 0x40 || (i & 0xE0) == 0x80)
				cg2uc[i] = i | 0x20;
			else
				cg2uc[i] = i;

		/* Set up the character swaps. */
		if (charset)
			remap_chars(XtNewString(charset));

		/* Initialize "gray" bitmap. */
		gray = XCreateBitmapFromData(display, root_window,
		    (char *) gray_bits, gray_width, gray_height);

		/* Initialize the toggles. */
		appres.toggle[MONOCASE].upcall =  toggle_monocase;
		appres.toggle[ALTCURSOR].upcall = toggle_altcursor;
		appres.toggle[BLINK].upcall =     toggle_blink;
		appres.toggle[TIMING].upcall =    toggle_timing;
		appres.toggle[CURSORP].upcall =   toggle_cursorp;
		appres.toggle[TRACE3270].upcall = toggle_nop;
		appres.toggle[TRACETN].upcall =   toggle_tracetn;

		ever = True;
	}

	/* Define graphics contexts and character dimensions. */
	if (font_changed) {
		nss.char_width  = CHAR_WIDTH;
		nss.char_height = CHAR_HEIGHT;
		make_gcs(&nss);
		remap_ascii();
	}

	/* Initialize the controller. */
	ctlr_init(keep_contents, model_changed);

	/* Allocate buffers. */
	if (model_changed) {
		/* Selection bitmap */
		if (selected)
			XtFree((char *)selected);
		selected = (unsigned char *)XtCalloc(sizeof(unsigned char), (maxROWS * maxCOLS + 7) / 8);

		/* X display image */
		if (nss.image)
			XtFree((char *)nss.image);
		nss.image = (unsigned int *) XtCalloc(sizeof(unsigned int), maxROWS * maxCOLS);
		if (temp_image)
			XtFree((char *)temp_image);
		temp_image = (unsigned int *) XtCalloc(sizeof(unsigned int), maxROWS*maxCOLS);

		/* One line of blanks */
		if (blanks)
			XtFree((char *)blanks);
		blanks = (unsigned char *)XtCalloc(sizeof(unsigned int), maxCOLS);

		/* Selection buffer */
		if (select_buf)
			XtFree(select_buf);
		select_buf = XtCalloc(sizeof(char), maxROWS * (maxCOLS+1) + 1);
	} else
		(void) memset((char *) nss.image, 0,
		              sizeof(unsigned int) * maxROWS * maxCOLS);

	/* Set up a container for the menubar, screen and keypad */

	nss.screen_width  = ssCOL_TO_X(maxCOLS)+HHALO;
	nss.screen_height = ssROW_TO_Y(maxROWS)+VHALO+SGAP+VHALO;

	container_width = nss.screen_width+2;	/* add border width */
	if (kp_placement == kp_integral && container_width < min_keypad_width) {
		keypad_xwidth = min_keypad_width - container_width;
		container_width = min_keypad_width;
	} else
		keypad_xwidth = 0;

	container = XtVaCreateManagedWidget(
	    "container", compositeWidgetClass, toplevel,
	    XtNborderWidth, 0,
	    XtNwidth, container_width,
	    XtNheight, 10,	/* XXX -- a temporary lie to make Xt happy */
	    NULL);

	/* Initialize the menu bar and integral keypad */

	menubar_height = menubar_init(container, container_width,
	    keypad ? container_width : nss.screen_width+2);

	if (kp_placement == kp_integral) {
		w = keypad_init(container, menubar_height + nss.screen_height+2,
		    container_width, False, False);
		XtVaGetValues(w, XtNheight, &keypad_height, NULL);
	} else
		keypad_height = 0;
	container_height = menubar_height + nss.screen_height+2 + keypad_height;

	/* Create screen and set container dimensions */
	inflate_screen();

	set_translations(container);
	if (depth > 1)
		XtVaSetValues(container, XtNbackground, appres.keypadbg, NULL);
	else
		XtVaSetValues(container, XtNbackgroundPixmap, gray, NULL);

	XtRealizeWidget(toplevel);
	nss.window = XtWindow(nss.widget);
	set_mcursor();

	if (appres.active_icon)
		aicon_init(keep_contents, model_changed, font_changed);

	status_init(keep_contents, model_changed, font_changed);
	if (keep_contents) {
		cursor_changed = True;
	} else {
		status_ctlr_init();
		screen_connect();
	}
	line_changed = True;
}

static void
inflate_screen()
{
	Dimension tw, th;

	/* Create the screen window */
	if (nss.widget == NULL) {
		nss.widget = XtVaCreateManagedWidget(
		    "screen", widgetClass, container,
		    XtNwidth, nss.screen_width,
		    XtNx, keypad ? (keypad_xwidth / 2) : 0,
		    XtNheight, nss.screen_height,
		    XtNy, menubar_height,
		    XtNbackground, appres.mono ? appres.background : appres.colorbg,
		    NULL);
		set_translations(nss.widget);
	}

	/* Set the container and toplevel dimensions */
	XtVaSetValues(container,
	    XtNheight, container_height,
	    XtNwidth, container_width,
	    NULL);

	tw = container_width - (keypad ? 0 : keypad_xwidth);
	th = container_height - (keypad ? 0 : keypad_height);
	XtVaSetValues(toplevel,
	    XtNwidth, tw,
	    XtNheight, th,
	    XtNbaseWidth, tw,
	    XtNbaseHeight, th,
	    XtNminWidth, tw,
	    XtNminHeight, th,
	    XtNmaxWidth, tw,
	    XtNmaxHeight, th,
	    NULL);
}

/*
 * Called when a host connects or disconnects
 */
void
screen_connect()
{
	if (!screen_buf)
		return;		/* too soon */

	status_connect();

	if (CONNECTED) {
		ctlr_erase(True);
		cursor_on();
		schedule_blink();
	} else
		(void) cursor_off();

	mcursor_normal();
}

/*
 * Mouse cursor changes
 */

static void
set_mcursor()
{
	switch (mcursor_state) {
	    case LOCKED:
		XDefineCursor(display, nss.window, appres.locked_mcursor);
		break;
	    case NORMAL:
		XUndefineCursor(display, nss.window);
		break;
	    case WAIT:
		XDefineCursor(display, nss.window, appres.wait_mcursor);
		break;
	}
}

void
mcursor_normal()
{
	if (CONNECTED)
		mcursor_state = NORMAL;
	else if (HALF_CONNECTED)
		mcursor_state = WAIT;
	else
		mcursor_state = LOCKED;
	set_mcursor();
}

void
mcursor_waiting()
{
	mcursor_state = WAIT;
	set_mcursor();
}

void
mcursor_locked()
{
	mcursor_state = LOCKED;
	set_mcursor();
}

/*
 * Called from the keypad button to expose or hide the integral keypad.
 */
void
screen_showkeypad(on)
int on;
{
	if (keypad_xwidth > 0) {
		XtDestroyWidget(nss.widget);
		nss.widget = (Widget) NULL;
		inflate_screen();
		nss.window = XtWindow(nss.widget);
		redraw(0, 0, 0, 0);
		menubar_resize(on ? container_width : nss.screen_width+2);
	} else
		inflate_screen();
}


/*
 * Make the (displayed) cursor disappear.  Returns a Boolean indiciating if
 * the cursor was on before the call.
 */
static Boolean
cursor_off()
{
	if (cursor_displayed) {
		cursor_displayed = False;
		put_cursor(ss->cursor_daddr, False);
		return True;
	} else
		return False;
}



/*
 * Blink the cursor
 */
/*ARGSUSED*/
static void
blink_it(closure, id)
XtPointer closure;
XtIntervalId *id;
{
	blink_pending = False;
	if (!CONNECTED || !toggled(BLINK))
		return;
	if (cursor_displayed) {
		if (in_focus)
			(void) cursor_off();
	} else
		cursor_on();
	schedule_blink();
}

/*
 * Schedule a cursor blink
 */
static void
schedule_blink()
{
	if (!toggled(BLINK) || blink_pending)
		return;
	blink_pending = True;
	blink_id = XtAppAddTimeOut(appcontext, 500, blink_it, 0);
}

/*
 * Cancel a cursor blink
 */
void
cancel_blink()
{
	if (blink_pending) {
		XtRemoveTimeOut(blink_id);
		blink_pending = False;
	}
}

/*
 * Toggle cursor blinking (called from menu)
 */
static void
toggle_blink()
{
	if (!CONNECTED)
		return;

	if (toggled(BLINK))
		schedule_blink();
	else
		cursor_on();
}

/*
 * Make the cursor visible at its (possibly new) location.
 */
static void
cursor_on()
{
	if (!cursor_displayed) {
		cursor_displayed = True;
		put_cursor(cursor_addr, True);
		ss->cursor_daddr = cursor_addr;
		cursor_changed = False;
	}
}


/*
 * Toggle the cursor (block/underline).
 */
static void
toggle_altcursor(t)
struct toggle *t;
{
	Boolean was_on;

	/* do_toggle already changed the value; temporarily change it back */
	toggle_toggle(t);

	was_on = cursor_off();

	/* Now change it back again */
	toggle_toggle(t);

	if (was_on)
		cursor_on();
}


/*
 * Move the cursor to the specified buffer address.
 */
void
cursor_move(baddr)
int	baddr;
{
	cursor_addr = baddr;
	cursor_pos();
}

/*
 * Display the cursor position on the status line
 */
static void
cursor_pos()
{
	if (!toggled(CURSORP) || !CONNECTED)
		return;
	status_cursor_pos(cursor_addr);
}

/*
 * Toggle the display of the cursor position
 */
static void
toggle_cursorp()
{
	if (toggled(CURSORP))
		cursor_pos();
	else
		status_uncursor_pos();
}


/*
 * Redraw the screen.
 */
/*ARGSUSED*/
void
redraw(w, event, params, num_params)
Widget	w;
XEvent	*event;
String	*params;
Cardinal *num_params;
{
	int	x, y, width, height;
	int	startcol, ncols;
	int	startrow, endrow, row;

	if (w == nss.widget) {
		if (appres.active_icon && iconic) {
			ss = &nss;
			iconic = False;
		}
	} else if (appres.active_icon && w == iss.widget) {
		if (appres.active_icon && !iconic) {
			ss = &iss;
			iconic = True;
		}
	} else
		return;

	/* Only redraw as necessary for an expose event */
	if (event && event->type == Expose) {
		ss->exposed_yet = True;
		x = event->xexpose.x;
		y = event->xexpose.y;
		width = event->xexpose.width;
		height = event->xexpose.height;
		startrow = ssY_TO_ROW(y);
		if (startrow < 0)
			startrow = 0;
		if (startrow > 0)
			startrow--;
		endrow = ssY_TO_ROW(y+height);
		endrow = endrow >= maxROWS ? maxROWS : endrow + 1;
		startcol = ssX_TO_COL(x);
		if (startcol < 0)
			startcol = 0;
		if (startcol > 0)
			startcol--;
		ncols = ssX_TO_COL(width+ss->char_width) + 2;
		while ((ROWCOL_TO_BA(startrow, startcol) % maxCOLS) + ncols > maxCOLS)
			ncols--;
		for (row = startrow; row < endrow; row++)
			(void) memset((char *) &ss->image[ROWCOL_TO_BA(row, startcol)],
			              0, ncols * sizeof(unsigned int));
	} else {
		XFillRectangle(display, ss->window, ss->invgc[0],
		    0, 0, ss->screen_width, ss->screen_height);
		(void) memset((char *) ss->image, 0,
		              (maxROWS*maxCOLS) * sizeof(unsigned int));
	}
	screen_changed = True;
	cursor_changed = True;
	if (!appres.active_icon || !iconic) {
		line_changed = True;
		status_touch();
	}
}

/*
 * Redraw the changed parts of the screen.
 */

void
screen_disp()
{
	/* No point in doing anything if we aren't visible yet. */
	if (!ss->exposed_yet)
		return;

	/*
	 * We don't set "cursor_changed" when the host moves the cursor,
	 * 'cause he might just move it back later.  Set it here if the cursor
	 * has moved since the last call to screen_disp.
	 */
	if (cursor_addr != ss->cursor_daddr) {
		cursor_changed = True;
	}

	/*
	 * If only the cursor has changed (and not the screen image), draw it.
	 */
	if (cursor_changed && !(screen_changed || image_changed)) {
		if (cursor_off())
			cursor_on();
	}

	/*
	 * Redraw the parts of the screen that need refreshing, and redraw the
	 * cursor if necessary.
	 */
	if (screen_changed || image_changed) {
		Boolean	was_on = False;

		/* Draw the new screen image into "temp_image" */
		if (screen_changed)
			draw_fields(temp_image);

		/* Set "cursor_changed" if the text under it has changed. */
		if (ss->image[cursor_addr] != temp_image[cursor_addr])
			cursor_changed = True;

		/* Undraw the cursor, if necessary. */
		if (cursor_changed)
			was_on = cursor_off();

		/* Intelligently update the X display with the new text. */
		resync_display(temp_image);

		/* Redraw the cursor. */
		if (was_on)
			cursor_on();

		screen_changed = False;
		image_changed = False;
	}

	if (!appres.active_icon || !iconic) {
		/* Refresh the status line. */
		status_disp();

		/* Refresh the line across the bottom of the screen. */
		if (line_changed) {
			XDrawLine(display, ss->window, ss->gc[0],
			    0,
			    ssROW_TO_Y(maxROWS-1)+SGAP-1,
			    ssCOL_TO_X(maxCOLS)+HHALO,
			    ssROW_TO_Y(maxROWS-1)+SGAP-1);
			line_changed = False;
		}
	}
	draw_aicon_label();
}


/*
 * Render a blank rectangle on the X display.
 */
void
render_blanks(baddr, height)
int baddr, height;
{
	int x, y;

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));

	XFillRectangle(display, ss->window, ss->invgc[0],
	    x, y - ss->efontinfo->ascent,
	    (ss->char_width * COLS), (ss->char_height * height));
	(void) memset((char *) &ss->image[baddr], 0,
	              COLS * height * sizeof(unsigned int));
}

/*
 * Reconcile the differences between a region of 'buffer' (what we want on
 * the X display) and ss->image[] (what is on the X display now).  The region
 * must not span lines.
 */
void
resync_text(baddr, len, buffer)
int baddr, len;
unsigned int *buffer;
{
	if (!memcmp((char *) &buffer[baddr], (char *) blanks,
	    len*sizeof(unsigned int))) {
		int x, y;

		x = ssCOL_TO_X(BA_TO_COL(baddr));
		y = ssROW_TO_Y(BA_TO_ROW(baddr));

		/* All blanks, fill a rectangle */
		XFillRectangle(display, ss->window, ss->invgc[0],
		    x, y - ss->efontinfo->ascent,
		    (ss->char_width * len), ss->char_height);
	} else {
		int color = CG_FROM_IMAGE(buffer[baddr]);
		int color2;
		int i;
		int i0 = 0;

		for (i = 0; i < len; i++) {
			color2 = CG_FROM_IMAGE(buffer[baddr+i]);

			/*
			 * Blanks are the same, no matter what color, as long
			 * as they have the same selection status
			 */
			if (!buffer[baddr+i] && ((color & SEL_BIT) == (color2 & SEL_BIT)))
				continue;

			if (color2 != color) {
				render_text(&buffer[baddr+i0], baddr+i0, i - i0, False);
				color = color2;
				i0 = i;
			}
		}
		render_text(&buffer[baddr+i0], baddr+i0, len - i0, False);
	}

	/* The X display is now correct; update ss->image[]. */
	(void) MEMORY_MOVE((char *) &ss->image[baddr],
			   (char *) &buffer[baddr],
	                   len*sizeof(unsigned int));
}

/*
 * Render text onto the X display.  The region must not span lines.
 */
static void
render_text(buffer, baddr, len, block_cursor)
unsigned int *buffer;
int baddr, len;
Boolean block_cursor;
{
	int color = CG_FROM_IMAGE(buffer[0]);
	int x, y;
	unsigned char buf[128];
	GC dgc;
	int sel = (color & SEL_BIT) != 0;
	register int i;

	color &= 0x3;

	if (ss->standard_font) {
		unsigned char *xfmap = ss->latin1_font ? cg2asc : cg2asc7;

		if (toggled(MONOCASE)) {
			for (i = 0; i < len; i++) {
				char c = xfmap[buffer[i] & 0xff];

				if (c >= 'a' && c <= 'z')
					c += 'A' - 'a';
				buf[i] = c;
			}
		} else for (i = 0; i < len; i++)
			buf[i] = xfmap[buffer[i] & 0xff];
	} else {
		if (toggled(MONOCASE)) {
			for (i = 0; i < len; i++)
				buf[i] = cg2uc[buffer[i] & 0xff];
		} else
			for (i = 0; i < len; i++)
				buf[i] = buffer[i] & 0xff;
	}

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));

	if (sel && !block_cursor)
		dgc = ss->selgc[color];
	else if (block_cursor && !(appres.mono && sel))
		dgc = ss->invgc[color];
	else
		dgc = ss->gc[color];

	/* Draw the text */
	XDrawImageString(display, ss->window, dgc, x, y, (char *) buf, len);
	if (color == FA_INT_HIGH_SEL && ss->overstrike)
		XDrawString(display, ss->window, dgc, x+1, y, (char *) buf,
		    len);
}


/*
 * Toggle mono-/dual-case mode.
 */
static void
toggle_monocase()
{
	(void) memset((char *) ss->image, 0,
		      (ROWS*COLS) * sizeof(unsigned int));
	image_changed = True;
}

/*
 * "Draw" screen_buf into a buffer
 */
static void
draw_fields(buffer)
unsigned int *buffer;
{
	int	baddr = 0;
	unsigned char	fa = *get_field_attribute(baddr);
	int	color;
	int	zero;

	(void) memset((char *) buffer, 0, ROWS*COLS);

	zero = FA_IS_ZERO(fa);
	color = IMAGE_COLOR(fa);

	do {
		unsigned char	c = screen_buf[baddr];

		if (IS_FA(c)) {
			fa = c;
			zero = FA_IS_ZERO(fa);
			color = IMAGE_COLOR(fa);
			buffer[baddr] = 0;
		} else {
			int e_color = color;

			/* Quickish hack: If the extended-attribute value is
			   nonzero, highlight. */
			if (get_extended_attribute(baddr))
				e_color = IMAGE_COLOR(FA_INT_HIGH_SEL);
			if (zero || IS_FA(c) ||
			    c == CG_BLANK || c == CG_NULLBLANK)
				buffer[baddr] = 0;
			else
				buffer[baddr] = c | e_color;
		}
		if (SELECTED(baddr))
			buffer[baddr] |= IMAGE_SEL;
		INC_BA(baddr);
	} while (baddr);
}

/*
 * Resync the X display with the contents of 'buffer'
 */
static void
resync_display(buffer)
unsigned int	*buffer;
{
	register int	i, j;
	int		b = 0;
	int		i0 = -1;
#	define SPREAD	10


	for (i = 0; i < ROWS; b += COLS, i++) {
		int d0 = -1;
		int s0 = -1;

		/* Has the line changed? */
		if (!memcmp((char *) &ss->image[b], (char *) &buffer[b],
		    COLS*sizeof(unsigned int))) {
			if (i0 >= 0) {
				render_blanks(i0 * COLS, i - i0);
				i0 = -1;
			}
			continue;
		}

		/* Is the new value blanks? */
		if (!memcmp((char *) &buffer[b], (char *) blanks,
		    COLS*sizeof(unsigned int))) {
			if (i0 < 0)
				i0 = i;
			continue;
		}

		/* Yes, it changed, and it isn't blank.
		   Dump any pending blank lines. */
		if (i0 >= 0) {
			render_blanks(i0 * COLS, i - i0);
			i0 = -1;
		}

		/* New text.  Scan it. */
		for (j = 0; j < COLS; j++) {
			if (ss->image[b+j] == buffer[b+j]) {

				/* Same. */
				if (d0 >= 0) {	/* something is pending... */
					if (s0 < 0) {	/* 1st match past a non-match */
						s0 = j;
					} else {	/* nth matching character */
						if (j - s0 > SPREAD) {	/* too many */
							resync_text(b+d0, s0-d0, buffer);
							d0 = -1;
							s0 = -1;
						}
					}
				}
			} else {

				/* Different. */
				s0 = -1;		/* swallow intermediate matches */
				if (d0 < 0)
					d0 = j;		/* mark the start */
			}
		}
		if (d0 >= 0)
			resync_text(b+d0, COLS-d0, buffer);
	}
	if (i0 >= 0)
		render_blanks(i0 * COLS, ROWS - i0);
}


/*
 * Draw or remove the cursor
 */
static void
put_cursor(baddr, on)
int baddr;
Boolean on;
{
	int	x, y;
	unsigned char 	fa = *get_field_attribute(baddr);
	int  	color = fa_color(fa);
	int	sel = SELECTED(baddr) ? SEL_BIT : 0;
	Boolean	hidden_text = False;
	unsigned int	buffer;
	GC	dgc;

	/* Figure out the proper color */
	if (color == FA_INT_ZERO_NSEL) {
		hidden_text = True;
		color = FA_INT_NORM_SEL;
	}
	if (get_extended_attribute(baddr))
		color = FA_INT_HIGH_SEL;

	/*
	 * Dummy up a display buffer for where the cursor is.  The difference
	 * is that if the text isn't supposed to be displayed, pretend it's a
	 * correct-colored blank.
	 */
	if (hidden_text)
		buffer = (color | sel) << 8;
	else
		buffer = screen_buf[baddr] | ((color | sel) << 8);

	if (appres.mono && sel)
		dgc = ss->invgc[color];
	else
		dgc = ss->gc[color];
	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));

	/* Normal (block) cursor */
	if (!toggled(ALTCURSOR)) {
		if (appres.mono && in_focus && on) {
			/* Small inverted-block cursor. */
			XFillRectangle(display, ss->window, ss->mcgc,
			    x, y - ss->efontinfo->ascent + 1,
			    ss->char_width,
			    (ss->char_height > 2) ? (ss->char_height - 2) : 1);
		} else if (in_focus || !on)
			render_text(&buffer, baddr, 1, on);
		else
			/* Not in focus.  Draw a hollow rectangle. */
			XDrawRectangle(display, ss->window, dgc,
			    x, y - ss->efontinfo->ascent + (appres.mono?1:0),
			    ss->char_width - 1, ss->char_height - (appres.mono?2:1));
		return;
	}

	/* Alternate (underscore) cursor */
	render_text(&buffer, baddr, 1, False);
	if (on)
		XDrawRectangle(display, ss->window, dgc,
		    x, y - ss->efontinfo->ascent + ss->char_height - 2,
		    ss->char_width - 1, 1);
}


/*
 * Create graphics contexts.
 */
static void
make_gcs(ss)
struct sstate *ss;
{
	if (!appres.mono) {
		make_gc_set(ss, FA_INT_NORM_NSEL, appres.normal, appres.colorbg,
		    False);
		make_gc_set(ss, FA_INT_NORM_SEL,  appres.select, appres.colorbg,
		    False);
		make_gc_set(ss, FA_INT_HIGH_SEL,  appres.bold,   appres.colorbg,
		    True);
		make_gc_set(ss, FA_INT_ZERO_NSEL, appres.colorbg, appres.colorbg,
		    False);
	} else {
		make_gc_set(ss, FA_INT_NORM_NSEL, appres.foreground,
		    appres.background, False);
		make_gc_set(ss, FA_INT_NORM_SEL,  appres.foreground,
		    appres.background, False);
		make_gc_set(ss, FA_INT_HIGH_SEL,  appres.foreground,
		    appres.background, True);
		make_gc_set(ss, FA_INT_ZERO_NSEL, appres.background,
		    appres.background, False);
	}

	/* Create monochrome block cursor GC. */
	if (appres.mono) {
		XGCValues xgcv;

		xgcv.function = GXinvert;
		ss->mcgc = XtGetGC(toplevel, GCFunction, &xgcv);
	}

	/* Set the flag for overstriking bold. */
	ss->overstrike = appres.mono && ss->bfontinfo == NULL && ss->char_width > 1;
}

/*
 * Create a set of graphics contexts for a given color.
 */
static void
make_gc_set(ss, i, fg, bg, bold)
struct sstate	*ss;
int		i;
Pixel 	fg, bg;
Boolean	bold;
{
	XGCValues xgcv;

	xgcv.foreground = fg;
	xgcv.background = bg;
	if (bold && ss->bfontinfo != NULL)
		xgcv.font = ss->bfontinfo->fid;
	else
		xgcv.font = ss->efontinfo->fid;
	ss->gc[i] = XtGetGC(toplevel, GCForeground|GCBackground|GCFont, &xgcv);
	xgcv.foreground = bg;
	xgcv.background = fg;
	ss->invgc[i] = XtGetGC(toplevel, GCForeground|GCBackground|GCFont, &xgcv);
	if (appres.mono)
		ss->selgc[i] = ss->invgc[i];
	else {
		xgcv.foreground = fg;
		xgcv.background = appres.selbg;
		ss->selgc[i] = XtGetGC(toplevel, GCForeground|GCBackground|GCFont,
		    &xgcv);
	}
}


/*
 * Convert an attribute to a color index.
 */
static int
fa_color(fa)
unsigned char fa;
{
	int c = fa & FA_INTENSITY;

	if (appres.mono || FA_IS_ZERO(fa))
		return c;
#ifdef MODIFIED_SEL
	else if (FA_IS_MODIFIED(fa))
		return FA_INT_NORM_SEL;
#endif
	else
		return c;
}


/*
 * Generic toggle stuff
 */
void
do_toggle(index)
int index;
{
	struct toggle *t = &appres.toggle[index];

	/*
	 * Change the value, call the internal update routine, and reset the
	 * menu label(s).
	 */
	toggle_toggle(t);
	t->upcall(t);
	menubar_retoggle(t);
}

/*
 * Event handlers for toplevel FocusIn, FocusOut, KeymapNotify and
 * PropertyChanged events.
 */

static Boolean toplevel_focused = False;
static Boolean keypad_entered = False;

/*ARGSUSED*/
void
focus_change(w, event, params, num_params)
Widget w;
XFocusChangeEvent *event;
{
	switch (event->type) {
	    case FocusIn:
		if (event->detail != NotifyPointer) {
			toplevel_focused = True;
			screen_focus(True);
		}
		break;
	    case FocusOut:
		toplevel_focused = False;
		if (!toplevel_focused && !keypad_entered)
			screen_focus(False);
		break;
	}
}

/*ARGSUSED*/
void
enter_leave(w, event, params, num_params)
Widget w;
XCrossingEvent *event;
{
	extern Widget keypad_shell;

	switch (event->type) {
	    case EnterNotify:
		keypad_entered = True;
		screen_focus(True);
		break;
	    case LeaveNotify:
		keypad_entered = False;
		if (!toplevel_focused && !keypad_entered)
			screen_focus(False);
		break;
	}
}

/*ARGSUSED*/
void
keymap_event(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	XKeymapEvent *k = (XKeymapEvent *)event;

	shift_event(state_from_keymap(k->key_vector));
}

static void
query_window_state()
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long leftover;
	unsigned char *data = NULL;
	Atom a_state = XInternAtom(display, "WM_STATE", False);

	if (appres.active_icon)
		return;
	if (XGetWindowProperty(display, XtWindow(toplevel), a_state, 0L,
	    (long)BUFSIZ, False, a_state, &actual_type, &actual_format,
	    &nitems, &leftover, &data) != Success)
		return;
	if (actual_type == a_state && actual_format == 32) {
		if (*(unsigned long *)data == 3)
			iconic = True;
		else {
			iconic = False;
			invert_icon(False);
		}
	}
}

/*ARGSUSED*/
void
state_event(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	query_window_state();
}

/*
 * Handle Shift events (KeyPress and KeyRelease events, or KeymapNotify events
 * that occur when the mouse enters the window).
 */

void
shift_event(event_state)
int event_state;
{
	static int old_state;
	Boolean shifted_now = (event_state & ShiftKeyDown) != 0;

	if (event_state != old_state) {
		old_state = event_state;
		status_shift_mode(event_state);
		if (shifted != shifted_now) {
			shifted = shifted_now;
			keypad_shift();
		}
	}
}

/*
 * Handle the mouse entering and leaving the window.
 */
static void
screen_focus(in)
Boolean in;
{
	/*
	 * Cancel any pending cursor blink.  If we just came into focus and
	 * have a blinking cursor, we will start a fresh blink cycle below, so
	 * the filled-in cursor is visible for a full turn.
	 */
	cancel_blink();

	/*
	 * If the cursor is disabled, simply change internal state.
	 */
	if (!CONNECTED) {
		in_focus = in;
		return;
	}

	/*
	 * Change the appearance of the cursor.  Make it hollow out or fill in
	 * instantly, even if it was blinked off originally.
	 */
	(void) cursor_off();
	in_focus = in;
	cursor_on();

	/*
	 * If we just came into focus and we're supposed to have a blinking
	 * cursor, schedule a blink.
	 */
	if (in_focus && toggled(BLINK))
		schedule_blink();
}

void
quit_event()
{
	if (!CONNECTED)
		exit(0);
}

/*
 * Change fonts.
 */
/*ARGSUSED*/
void
set_font(w, event, params, num_params)
Widget	w;
XEvent	*event;
String	*params;
Cardinal *num_params;
{
	if (*num_params != 1) {
		XtWarning("SetFont: must have 1 argument");
		return;
	}
	screen_newfont(params[0], True);
}

int
screen_newfont(fontname, do_popup)
char *fontname;
Boolean do_popup;
{
	XFontStruct	*new_fontinfo;
	char		*buf = (char *) NULL;
	char		*boldname;
	char		*emsg;

	boldname = strchr(fontname, ',');
	if (boldname)
		*boldname++ = '\0';
	else {
		buf = xs_buffer("%sbold", fontname);
		boldname = buf;
	}
	if (efontname && !strcmp(fontname, efontname) &&
	    bfontname && !strcmp(boldname, bfontname)) {
		if (buf)
			XtFree(buf);
		return 0;
	}

	if (emsg = load_fixed_font(fontname, &new_fontinfo)) {
		if (buf)
			XtFree(buf);
		if (do_popup) {
			buf = xs_buffer2("Font '%s'\n%s", fontname, emsg);
			popup_an_error(buf);
			XtFree(buf);
		}
		return -1;
	}
	nss.efontinfo = new_fontinfo;
	if (appres.mono)
		(void) load_fixed_font(boldname, &nss.bfontinfo);
	set_font_globals(nss.efontinfo, fontname, boldname);

	XtDestroyWidget(container);
	container = (Widget) NULL;
	nss.widget = (Widget) NULL;
	menubar_gone();
	screen_init(True, False, True);
	if (buf)
		XtFree(buf);
	return 0;
}

/*
 * Load and query a font, and verify that it is fixed-width.
 * Returns NULL (okay) or an error message.
 */
char *
load_fixed_font(name, font)
char *name;
XFontStruct **font;
{
	XFontStruct *f;
	unsigned long svalue;
	Boolean test_spacing = True;
	extern Atom a_spacing, a_c;

	*font = NULL;
	if (*name == '!') {
		name++;
		test_spacing = False;
	} else if (!strncmp(name, "3270", 4))
		test_spacing = False;
	f = XLoadQueryFont(display, name);
	if (f == NULL)
		return "doesn't exist";

	if (test_spacing) {
		if (XGetFontProperty(f, a_spacing, &svalue)) {
			if ((Atom) svalue != a_c)
				return "doesn't use constant spacing";
		} else
			xs_warning("Font %s has no SPACING property", name);
	}

	*font = f;
	return (char *) NULL;
}

/*
 * Set globals based on font name and info
 */
void
set_font_globals(f, ef, bf)
XFontStruct *f;
char *ef, *bf;
{
	unsigned long svalue, svalue2;
	extern Atom a_family_name, a_3270;
	extern Atom a_registry, a_iso8859;
	extern Atom a_encoding, a_1;

	if (efontname)
		XtFree(efontname);
	efontname = XtNewString(ef);
	if (bfontname)
		XtFree(bfontname);
	bfontname = XtNewString(bf);
	if (XGetFontProperty(f, a_family_name, &svalue))
		nss.standard_font = (Atom) svalue != a_3270;
	else if (!strncmp(efontname, "3270", 4))
		nss.standard_font = False;
	else
		nss.standard_font = True;
	nss.latin1_font = nss.standard_font &&
	    nss.efontinfo->max_char_or_byte2 > 127 &&
	    XGetFontProperty(f, a_registry, &svalue) &&
	    svalue == a_iso8859 &&
	    XGetFontProperty(f, a_encoding, &svalue2) &&
	    svalue2 == a_1;
}

/*
 * Change models
 */
void
screen_change_model(mn)
char *mn;
{
	if (CONNECTED || model_num == atoi(mn))
		return;
	set_rows_cols(mn);
	XtDestroyWidget(container);
	container = (Widget) NULL;
	nss.widget = (Widget) NULL;
	menubar_gone();
	relabel();
	screen_init(False, True, False);
	ansi_init();
}

/*
 * Visual or not-so-visual bell
 */
void
ring_bell()
{
	static XGCValues xgcv;
	static GC bgc;
	static int initted;
	struct timeval tv;

	/* Ring the real display's bell. */
	if (!appres.visual_bell)
		XBell(display, 0);

	/* If we're iconic, flip the icon and return. */
	if (!appres.active_icon) {
		query_window_state();
		if (iconic) {
			invert_icon(True);
			return;
		}
	}

	if (!appres.visual_bell || !ss->exposed_yet)
		return;

	/* Do a screen flash. */

	if (!initted) {
		xgcv.function = GXinvert;
		bgc = XtGetGC(toplevel, GCFunction, &xgcv);
		initted = 1;
	}
	screen_disp();
	XFillRectangle(display, ss->window, bgc,
	    0, 0, ss->screen_width, ss->screen_height);
	XSync(display, 0);
	tv.tv_sec = 0;
	tv.tv_usec = 125000;
	(void) select(0, 0, 0, 0, &tv);
	XFillRectangle(display, ss->window, bgc,
	    0, 0, ss->screen_width, ss->screen_height);
	XSync(display, 0);
}

/*
 * Window deletion
 */
/*ARGSUSED*/
void
delete_window(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (w == toplevel)
		exit(0);
	else
		XtPopdown(w);
}

/*
 * Turn a keysym name or literal string into a character.  Return 0 if
 * there is a problem.
 */
unsigned char
parse_keysym(s)
char *s;
{
	KeySym	k;

	k = XStringToKeysym(s);
	if (k == NoSymbol) {
		if (strlen(s) == 1)
			k = *s & 0xff;
		else
			return 0;
	}
	if (k < ' ' || k > 0xff)
		return 0;
	else
		return (unsigned char) k;
}

/*
 * Parse an EBCDIC character map, a series of pairs of numeric EBCDIC codes
 * and ASCII characters or ISO Latin-1 character names, modifying the
 * CG-to-EBCDIC maps.
 */
static void
remap_chars(spec)
char *spec;
{
	char	*s, *line;
	char	ebcs[64], isos[64];
	char	*nl;
	unsigned char	ebc, iso, cg;
	int	ne = 0;
	extern	unsigned char ebc2cg[];
	extern	unsigned char asc2cg[];
	extern	unsigned char cg2ebc[];

	s = spec;
	while (s) {
		while (isspace(*s))
			s++;
		if (!*s)
			break;
		line = s;
		nl = strchr(line, '\n');
		if (nl) {
			*nl = '\0';
			s = nl + 1;
		} else {
			s = NULL;
		}
		ne++;
		if (sscanf(line, "%63[^: \t]: %63s", ebcs, isos) != 2 ||
		    !(ebc = strtol(ebcs, (char **)NULL, 0)) ||
		    !(iso = parse_keysym(isos))) {
			char	nbuf[16];

			(void) sprintf(nbuf, "%d", ne);
			xs_warning2("Can't parse charset '%s', entry %s",
			    appres.charset, nbuf);
			continue;
		}
		cg = asc2cg[iso];
		if (cg2asc[cg] == iso) {	/* well-defined */
			ebc2cg[ebc] = cg;
			cg2ebc[cg] = ebc;
		} else {			/* into a hole */
			ebc2cg[ebc] = CG_BOXSOLID;
		}
	}
	XtFree(spec);
}

/*
 * Parse an ASCII remap table, a series of pairs of keysyms, indicating that
 * the first ASCII code should be translated into the second, modifying the
 * ASCII-to-CG map.
 */
static void
remap_ascii()
{
	char	*mn;
	char	*s, *line;
	char	froms[64], tos[64];
	char	*nl;
	unsigned char	from, to, cg;
	int	ne = 0;
	extern	unsigned char asc2cg[];
	static	unsigned char asc2cg_save[256];
	static	Boolean ever = False;

	/* Restore the original ASCII-to-CG map. */
	if (!ever) {
		memcpy((char *) asc2cg_save, (char *) asc2cg, 256);
		ever = True;
	} else
		memcpy((char *) asc2cg, (char *) asc2cg_save, 256);

	/* Find one that matches this font. */
	mn = xs_buffer("cmap.%s", efontname);
	s = get_resource(mn);
	XtFree(mn);
	if (!s)
		return;

	/* Parse it. */
	while (s) {
		while (isspace(*s))
			s++;
		if (!*s)
			break;
		line = s;
		nl = strchr(line, '\n');
		if (nl) {
			*nl = '\0';
			s = nl + 1;
		} else {
			s = NULL;
		}
		ne++;
		if (sscanf(line, "%63[^: \t]: %63s", froms, tos) != 2 ||
		    !(from = parse_keysym(froms)) ||
		    !(to = parse_keysym(tos))) {
			char	nbuf[16];

			(void) sprintf(nbuf, "%d", ne);
			xs_warning2("Can't parse cmap '%s', entry %s",
			    efontname, nbuf);
			continue;
		}
		asc2cg[from] = asc2cg[to];
	}
}


/*
 * Initialize the active icon font information.
 */
void
aicon_font_init()
{
	if (!appres.active_icon) {
		appres.label_icon = False;
		return;
	}
	iss.efontinfo = XLoadQueryFont(display, appres.icon_font);
	if (iss.efontinfo == NULL) {
		xs_warning("Can't load iconFont '%s'; activeIcon won't work",
		    appres.icon_font);
		appres.active_icon = False;
		return;
	}
	iss.bfontinfo = NULL;
	iss.char_width = fCHAR_WIDTH(iss.efontinfo);
	iss.char_height = fCHAR_HEIGHT(iss.efontinfo);
	iss.overstrike = False;
	iss.standard_font = True;
	iss.latin1_font = False;
	if (appres.label_icon) {
		ailabel_font = XLoadQueryFont(display, appres.icon_label_font);
		if (ailabel_font == NULL) {
			xs_warning("Can't load iconLabelFont '%s' font; labelIcon won't work", appres.icon_label_font);
			appres.label_icon = False;
			return;
		}
		aicon_label_height = fCHAR_HEIGHT(ailabel_font) + 2;
	}
}

/*
 * Determine the current size of the active icon.
 */
void
aicon_size(iw, ih)
Dimension *iw;
Dimension *ih;
{
	XIconSize *is;
	int count;

	*iw = maxCOLS*iss.char_width + 2*VHALO;
	*ih = maxROWS*iss.char_height + 2*HHALO + aicon_label_height;
	if (XGetIconSizes(display, root_window, &is, &count)) {
		if (*iw > (unsigned) is[0].max_width)
			*iw = is[0].max_width;
		if (*ih > (unsigned) is[0].max_height)
			*ih = is[0].max_height;
	}
}

/*
 * Initialize or reinitialize the active icon.  Assumes that aicon_font_init
 * has already been called.
 */
void
aicon_init(keep_contents, model_changed, font_changed)
Boolean keep_contents;
Boolean model_changed;
Boolean font_changed;
{
	Widget w;
	static Boolean ever = False;
	extern Widget icon_shell;

	if (!ever) {
		make_gcs(&iss);
		iss.widget = icon_shell;
		iss.window = XtWindow(iss.widget);
		iss.cursor_daddr = 0;
		iss.exposed_yet = False;
		if (appres.label_icon) {
			XGCValues xgcv;

			xgcv.font = ailabel_font->fid;
			xgcv.foreground = foreground;
			xgcv.background = background;
			ailabel_gc = XtGetGC(toplevel,
			    GCFont|GCForeground|GCBackground,
			    &xgcv);
		}
		ever = True;
	}

	if (model_changed) {
		Dimension iw, ih;

		aicon_size(&iss.screen_width, &iss.screen_height);
		if (iss.image)
			XtFree((char *) iss.image);
		iss.image = (unsigned int *)
		    XtMalloc(sizeof(unsigned int) * maxROWS * maxCOLS);
		XtVaSetValues(iss.widget,
		    XtNwidth, iss.screen_width,
		    XtNheight, iss.screen_height,
		    NULL);
	}
	(void) memset((char *) iss.image, 0,
		      sizeof(unsigned int) * maxROWS * maxCOLS);
}

/* Draw the aicon label */
static void
draw_aicon_label()
{
	int len;
	Position x;

	if (!appres.label_icon || !iconic)
		return;
	XFillRectangle(display, iss.window, iss.invgc[0],
	    0, iss.screen_height - aicon_label_height,
	    iss.screen_width, aicon_label_height);
	len = strlen(aicon_text);
	x = ((int)iss.screen_width - XTextWidth(ailabel_font, aicon_text, len))
	     / 2;
	if (x < 0)
		x = 2;
	XDrawImageString(display, iss.window, ailabel_gc,
	    x,
	    iss.screen_height - aicon_label_height + ailabel_font->ascent,
	    aicon_text, len);
}

/* Set the aicon label */
void
set_aicon_label(l)
char *l;
{
	if (aicon_text)
		XtFree(aicon_text);
	aicon_text = XtNewString(l);
	draw_aicon_label();
}

/* Print the contents of the screen as text. */
void
print_text(w, event, params, num_params)
Widget	w;
XEvent	*event;
String	*params;
Cardinal *num_params;
{
	char *filter = get_resource("printTextCommand");
	FILE *f;
	register int i;
	char c;
	int ns = 0;

	if (*num_params > 0)
		filter = params[0];
	if (*num_params > 1)
		XtWarning("PrintText: extra arguments ignored");
	if (!filter) {
		XtWarning("PrintText: no command defined");
		return;
	}
	if (!(f = popen(filter, "w"))) {
		XtWarning("PrintText: popen of filter failed");
		return;
	}
	for (i = 0; i < ROWS*COLS; i++) {
		if (i && !(i % COLS)) {
			fputc('\n', f);
			ns = 0;
		}
		c = cg2asc[screen_buf[i]];
		if (c == ' ')
			ns++;
		else {
			while (ns) {
				fputc(' ', f);
				ns--;
			}
			fputc(c, f);
		}
	}
	fputc('\n', f);
	pclose(f);
}

/* Print the contents of the screen as a bitmap. */
void
print_window(w, event, params, num_params)
Widget	w;
XEvent	*event;
String	*params;
Cardinal *num_params;
{
	char *filter = get_resource("printWindowCommand");
	char *fb = XtMalloc(strlen(filter) + 16);

	if (*num_params > 0)
		filter = params[0];
	if (*num_params > 1)
		XtWarning("PrintWindow: extra arguments ignored");
	if (!filter) {
		XtWarning("PrintWindow: no command defined");
		return;
	}
	sprintf(fb, filter, XtWindow(toplevel));
	system(fb);
	XtFree(fb);
}
