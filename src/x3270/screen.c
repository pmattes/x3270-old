/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 */

/*
 *	screen.c
 *		This module handles the X display.  It has been extensively
 *		optimized to minimize X drawing operations.
 */

#include "globals.h"
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>
#include <X11/Composite.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Scrollbar.h>
#include "Husk.h"
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <errno.h>
#include "3270ds.h"
#include "appres.h"
#include "screen.h"
#include "ctlr.h"
#include "cg.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "savec.h"
#include "screenc.h"
#include "scrollc.h"
#include "statusc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#if defined(SEPARATE_SELECT_H) /*[*/
#include <sys/select.h>		/* fd_set declaration */
#endif /*]*/

#include "x3270.bm"
#include "wait.bm"

#define SCROLLBAR_WIDTH	15
/* #define _ST */

/* Externals: main.c */
extern int      default_screen;

/* Externals: ctlr.c */
extern Boolean  screen_changed;
extern int      first_changed;
extern int      last_changed;

/* Globals */
unsigned char  *selected;	/* selection bitmap */
Dimension       main_width, main_height;
#include <X11/bitmaps/gray>
Boolean         scrollbar_changed = False;
Boolean         model_changed = False;
Boolean		efont_changed = False;
Boolean		oversize_changed = False;
Boolean		scheme_changed = False;
Pixel           colorbg_pixel;
Pixel           keypadbg_pixel;
Boolean         flipped = False;
int		field_colors[4];
Pixmap          icon;
Boolean		shifted = False;
struct font_list *font_list = (struct font_list *) NULL;
int             font_count = 0;
char	       *efontname;

/* Statics */
static union sp *temp_image;	/* temporary for X display */
static Boolean  cursor_displayed = False;
static Boolean  cursor_enabled = True;
static Boolean  cursor_blink_pending = False;
static XtIntervalId cursor_blink_id;
static Boolean  in_focus = False;
static Boolean  line_changed = False;
static Boolean  cursor_changed = False;
static Boolean  iconic = False;
static Widget   container;
static Widget   scrollbar;
static Dimension menubar_height;
#if defined(X3270_KEYPAD) /*[*/
static Dimension keypad_height;
static Dimension keypad_xwidth;
#endif /*]*/
static Dimension container_width;
static Dimension cwidth_nkp;
static Dimension container_height;
static Dimension scrollbar_width;
static char    *aicon_text = CN;
static XFontStruct *ailabel_font;
static Dimension aicon_label_height = 0;
static GC       ailabel_gc;
static Pixel    cpx[16];
static Boolean  cpx_done[16];
static Pixel    normal_pixel;
static Pixel    select_pixel;
static Pixel    bold_pixel;
static Pixel    selbg_pixel;
static Pixel    cursor_pixel;
static Boolean  text_blinking_on = True;
static Boolean  text_blinkers_exist = False;
static Boolean  text_blink_scheduled = False;
static XtIntervalId text_blink_id;
static XtTranslations screen_t00 = NULL;
static XtTranslations screen_t0 = NULL;
static XtTranslations container_t00 = NULL;
static XtTranslations container_t0 = NULL;
static unsigned char    *rt_buf8 = (unsigned char *) NULL;
static XChar2b *rt_buf16 = (XChar2b *) NULL;
static char    *color_name[16] = {
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL,
    (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL
};
static Boolean	configure_ticking = False;
static XtIntervalId configure_id;
static Boolean	configure_first = True;

static Pixmap   inv_icon;
static Pixmap   wait_icon;
static Pixmap   inv_wait_icon;
static Boolean  icon_inverted = False;
static Widget	icon_shell;

static struct font_list *font_last = (struct font_list *) NULL;

/* Globals for undoing reconfigurations. */
static enum {
    REDO_NONE, REDO_FONT,
#if defined(X3270_MENUS) /*[*/
    REDO_MODEL,
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
    REDO_KEYPAD,
#endif /*]*/
    REDO_SCROLLBAR
} screen_redo = REDO_NONE;
static char *redo_old_font = CN;
#if defined(X3270_MENUS) /*[*/
static int redo_old_model;
static int redo_old_ov_cols;
static int redo_old_ov_rows;
#endif /*]*/

static unsigned char blank_map[32];
#define BKM_SET(n)	blank_map[(n)/8] |= 1 << ((n)%8)
#define BKM_ISSET(n)	((blank_map[(n)/8] & (1 << ((n)%8))) != 0)

enum fallback_color { FB_WHITE, FB_BLACK };
static enum fallback_color ibm_fb = FB_WHITE;

/*
 * The screen state structure.  This structure is swapped whenever we switch
 * between normal and active-iconic states.
 */
#define NGCS	16
struct sstate {
	Widget          widget;	/* the widget */
	Window          window;	/* the window */
	union sp       *image;	/* what's on the X display */
	int             cursor_daddr;	/* displayed cursor address */
	Boolean         exposed_yet;	/* have we been exposed yet? */
	Boolean         overstrike;	/* are we overstriking? */
	Dimension       screen_width;	/* screen dimensions in pixels */
	Dimension       screen_height;
	GC              gc[NGCS * 2],	/* standard, inverted GCs */
	                selgc[NGCS],	/* color selected text GCs */
	                mcgc,	/* monochrome block cursor GC */
	                ucgc,	/* unique-cursor-color cursor GC */
	                invucgc;/* inverse ucgc */
	int             char_height;
	int             char_width;
	Font		fid;
	int		ascent;
	int		descent;
	Boolean         standard_font;
	Boolean		extended_3270font;
	Boolean         latin1_font;
	Boolean         debugging_font;
	Boolean         obscured;
	Boolean         copied;
	unsigned char  *xfmap;	/* standard font CG-to-ASCII map */
};
static struct sstate nss;
static struct sstate iss;
static struct sstate *ss = &nss;

/* Globals based on nss, used mostly by status and select routines. */
Widget         *screen = &nss.widget;
Window         *screen_window = &nss.window;
int            *char_width = &nss.char_width;
int            *char_height = &nss.char_height;
int            *ascent = &nss.ascent;
int            *descent = &nss.descent;
Boolean        *standard_font = &nss.standard_font;
Boolean        *latin1_font = &nss.latin1_font;
Boolean        *debugging_font = &nss.debugging_font;
Boolean        *extended_3270font = &nss.extended_3270font;

/* Mouse-cursor state */
enum mcursor_state { LOCKED, NORMAL, WAIT };
static enum mcursor_state mcursor_state = LOCKED;
static enum mcursor_state icon_cstate = NORMAL;

static void aicon_init(void);
static void aicon_reinit(unsigned cmask);
static void screen_focus(Boolean in);
static void make_gc_set(struct sstate *s, int i, Pixel fg, Pixel bg);
static void make_gcs(struct sstate *s);
static void put_cursor(int baddr, Boolean on);
static void resync_display(union sp *buffer, int first, int last);
static void draw_fields(union sp *buffer, int first, int last);
static void render_text(union sp *buffer, int baddr, int len,
    Boolean block_cursor, union sp *attrs);
static void cursor_pos(void);
static void cursor_on(void);
static void schedule_cursor_blink(void);
static void schedule_text_blink(void);
static void inflate_screen(void);
static int fa_color(unsigned char fa);
static Boolean cursor_off(void);
static void draw_aicon_label(void);
static void set_mcursor(void);
static void scrollbar_init(Boolean is_reset);
static void init_rsfonts(void);
static void allocate_pixels(void);
static int fl_baddr(int baddr);
static GC get_gc(struct sstate *s, int color);
static GC get_selgc(struct sstate *s, int color);
static void default_color_scheme(void);
static Boolean xfer_color_scheme(char *cs, Boolean do_popup);
static void set_font_globals(XFontStruct *f, const char *ef, Font ff);
static void screen_connect(Boolean ignored);
static void configure_stable(XtPointer closure, XtIntervalId *id);
static void cancel_blink(void);
static void render_blanks(int baddr, int height, union sp *buffer);
static void resync_text(int baddr, int len, union sp *buffer);
static void screen_reinit(unsigned cmask);
static void aicon_font_init(void);
static void aicon_size(Dimension *iw, Dimension *ih);
static void invert_icon(Boolean inverted);
static void lock_icon(enum mcursor_state state);

/* Resize font list. */
struct rsfont {
	struct rsfont *next;
	char *name;
	int width;
	int height;
	int total_width;	/* transient */
	int total_height;	/* transient */
	int area;		/* transient */
};
static struct rsfont *rsfonts;

#define BASE_MASK		0x0f	/* mask for 16 actual colors */
#define INVERT_MASK		0x10	/* toggle for inverted colors */
#define GC_NONDEFAULT		0x20	/* distinguishes "color 0" from zeroed
					    memory */

#define COLOR_MASK		(GC_NONDEFAULT | BASE_MASK)
#define INVERT_COLOR(c)		((c) ^ INVERT_MASK)
#define NO_INVERT(c)		((c) & ~INVERT_MASK)

#define DEFAULT_PIXEL		(appres.m3279 ? COLOR_BLUE : FA_INT_NORM_NSEL)
#define PIXEL_INDEX(c)		((c) & BASE_MASK)


/*
 * Save 00 event translations.
 */
void
save_00translations(Widget w, XtTranslations *t00)
{
	*t00 = w->core.tm.translations;
}

/* 
 * Define our event translations
 */
void
set_translations(Widget w, XtTranslations *t00, XtTranslations *t0)
{
	struct trans_list *t;

	if (t00 != (XtTranslations *)NULL)
		XtOverrideTranslations(w, *t00);

	for (t = trans_list; t != NULL; t = t->next)
		XtOverrideTranslations(w, lookup_tt(t->name, CN));

	*t0 = w->core.tm.translations;
}

/*
 * Add or clear a temporary keymap.
 */
void
screen_set_temp_keymap(XtTranslations trans)
{
	if (trans != (XtTranslations)NULL) {
		XtOverrideTranslations(nss.widget, trans);
		XtOverrideTranslations(container, trans);
	} else {
		XtUninstallTranslations(nss.widget);
		XtOverrideTranslations(nss.widget, screen_t0);
		XtUninstallTranslations(container);
		XtOverrideTranslations(container, container_t0);
	}
}

/*
 * Change the baselevel keymap.
 */
void
screen_set_keymap(void)
{
	XtUninstallTranslations(nss.widget);
	set_translations(nss.widget, &screen_t00, &screen_t0);
	XtUninstallTranslations(container);
	set_translations(container, &container_t00, &container_t0);
}


/*
 * Initialize the screen.
 */
void
screen_init(void)
{
	register int i;

	/* Initialize ss. */
	nss.cursor_daddr = 0;
	nss.exposed_yet = False;

	/* Initialize "gray" bitmap. */
	if (appres.mono)
		gray = XCreatePixmapFromBitmapData(display,
		    root_window, (char *)gray_bits,
		    gray_width, gray_height,
		    appres.foreground, appres.background, screen_depth);

	/* Initialize the resize list. */
	init_rsfonts();

	/* Initialize the blank map. */
	(void) memset((char *)blank_map, '\0', sizeof(blank_map));
	BKM_SET(CG_null);
	BKM_SET(CG_nobreakspace);
	BKM_SET(CG_ff);
	BKM_SET(CG_cr);
	BKM_SET(CG_nl);
	BKM_SET(CG_em);
	BKM_SET(CG_space);
	for (i = 0; i <= 0xf; i++) {
		BKM_SET(0xc0 + i);
		BKM_SET(0xe0 + i);
	}

	/* Register state change callbacks. */
	register_schange(ST_HALF_CONNECT, screen_connect);
	register_schange(ST_CONNECT, screen_connect);
	register_schange(ST_3270_MODE, screen_connect);

	/* Initialize the emulated 3270 controller hardware. */
	ctlr_init(ALL_CHANGE);

	/* Initialize the actve icon. */
	aicon_init();

	/* Initialize the status line. */
	status_init();

	/* Initialize the placement of the pop-up keypad. */
	keypad_placement_init();

	/* Now call the "reinitialize" function to set everything else up. */
	screen_reinit(ALL_CHANGE);
}

/*
 * Re-initialize the screen.
 */
static void
screen_reinit(unsigned cmask)
{
	Dimension cwidth_curr;
#if defined(X3270_KEYPAD) /*[*/
	Dimension mkw;
#endif /*]*/

	/* Allocate colors. */
	if (cmask & COLOR_CHANGE) {
		if (appres.m3279) {
			default_color_scheme();
			(void) xfer_color_scheme(appres.color_scheme, False);
		}
		allocate_pixels();
	}

	/* Define graphics contexts. */
	if (cmask & (FONT_CHANGE | COLOR_CHANGE))
		make_gcs(&nss);

	/* Reinitialize the controller. */
	ctlr_reinit(cmask);

	/* Allocate buffers. */
	if (cmask & MODEL_CHANGE) {
		/* Selection bitmap */
		if (selected)
			XtFree((char *)selected);
		selected = (unsigned char *)XtCalloc(sizeof(unsigned char),
				(maxROWS * maxCOLS + 7) / 8);

		/* X display image */
		if (nss.image)
			XtFree((char *)nss.image);
		nss.image = (union sp *)XtCalloc(sizeof(union sp),
				maxROWS * maxCOLS);
		if (temp_image)
			XtFree((char *)temp_image);
		temp_image = (union sp *)XtCalloc(sizeof(union sp),
				maxROWS*maxCOLS);

		/* render_text buffers */
		if (rt_buf8 != (unsigned char *)CN)
			XtFree((char *)rt_buf8);
		rt_buf8 = (unsigned char *)XtMalloc(maxCOLS);
		if (rt_buf16 != (XChar2b *)NULL)
			XtFree((char *)rt_buf16);
		rt_buf16 = (XChar2b *)XtMalloc(maxCOLS * sizeof(XChar2b));
	} else
		(void) memset((char *) nss.image, 0,
		              sizeof(union sp) * maxROWS * maxCOLS);

	/* Set up a container for the menubar, screen and keypad */

	if (cmask & (FONT_CHANGE | MODEL_CHANGE)) {
		nss.screen_width  = SCREEN_WIDTH(ss->char_width);
		nss.screen_height = SCREEN_HEIGHT(ss->char_height);
	}

	if (toggled(SCROLL_BAR))
		scrollbar_width = SCROLLBAR_WIDTH;
	else
		scrollbar_width = 0;
	container_width = cwidth_nkp = nss.screen_width+2 + scrollbar_width;
#if defined(X3270_KEYPAD) /*[*/
	mkw = min_keypad_width();
	if (kp_placement == kp_integral && container_width < mkw) {
		keypad_xwidth = mkw - container_width;
		container_width = mkw;
	} else
		keypad_xwidth = 0;
#endif /*]*/

	if (container == (Widget)NULL) {
		container = XtVaCreateManagedWidget(
		    "container", huskWidgetClass, toplevel,
		    XtNborderWidth, 0,
		    XtNwidth, container_width,
		    XtNheight, 10,
			/* XXX -- a temporary lie to make Xt happy */
		    NULL);
		save_00translations(container, &container_t00);
		set_translations(container, (XtTranslations *)NULL,
		    &container_t0);
		if (appres.mono)
			XtVaSetValues(container, XtNbackgroundPixmap, gray,
			    NULL);
		else
			XtVaSetValues(container, XtNbackground, keypadbg_pixel,
			    NULL);
	}

	/* Initialize the menu bar and integral keypad */

#if defined(X3270_KEYPAD) /*[*/
	cwidth_curr = appres.keypad_on ? container_width : cwidth_nkp;
#else /*][*/
	cwidth_curr = container_width;
#endif /*]*/
	menubar_height = menubar_qheight(cwidth_curr);
	menubar_init(container, container_width, cwidth_curr);

	container_height = menubar_height + nss.screen_height+2;
#if defined(X3270_KEYPAD) /*[*/
	if (kp_placement == kp_integral) {
		(void) keypad_init(container, container_height,
		    container_width, False, False);
		keypad_height = keypad_qheight();
	} else
		keypad_height = 0;
	container_height += keypad_height;
#endif /*]*/

	/* Create screen and set container dimensions */
	inflate_screen();

	/* Create scrollbar */
	scrollbar_init((cmask & MODEL_CHANGE) != 0);

	XtRealizeWidget(toplevel);
	nss.window = XtWindow(nss.widget);
	set_mcursor();

	/* Reinitialize the active icon. */
	aicon_reinit(cmask);

	/* Reinitialize the status line. */
	status_reinit(cmask);

#if 0
	if (cmask & MODEL_CHANGE) {
		cursor_changed = True;
	} else {
		screen_connect(False);
	}
#endif
	/* now unconditional */ cursor_changed = True;

	line_changed = True;

	/* Redraw the screen. */
	action_internal(PA_Expose_action, IA_REDRAW, CN, CN);
}


static void
set_toplevel_sizes(void)
{
	Dimension tw, th;

#if defined(X3270_KEYPAD) /*[*/
	tw = container_width - (appres.keypad_on ? 0 : keypad_xwidth);
	th = container_height - (appres.keypad_on ? 0 : keypad_height);
#else /*][*/
	tw = container_width;
	th = container_height;
#endif /*]*/
	XtVaSetValues(toplevel,
	    XtNwidth, tw,
	    XtNheight, th,
	    NULL);
	if (!appres.allow_resize)
		XtVaSetValues(toplevel,
		    XtNbaseWidth, tw,
		    XtNbaseHeight, th,
		    XtNminWidth, tw,
		    XtNminHeight, th,
		    XtNmaxWidth, tw,
		    XtNmaxHeight, th,
		    NULL);
	XtVaSetValues(container,
	    XtNwidth, container_width,
	    XtNheight, container_height,
	    NULL);
	main_width = tw;
	main_height = th;

	/*
	 * Start a timer ticking, in case the window manager doesn't approve
	 * of the change.
	 */
	if (configure_ticking)
		XtRemoveTimeOut(configure_id);
	configure_id = XtAppAddTimeOut(appcontext, 500, configure_stable, 0);
	configure_ticking = True;

	keypad_move();
}

static void
inflate_screen(void)
{
	/* Create the screen window */
	if (nss.widget == NULL) {
		nss.widget = XtVaCreateManagedWidget(
		    "screen", widgetClass, container,
		    XtNwidth, nss.screen_width,
		    XtNheight, nss.screen_height,
		    XtNx,
#if defined(X3270_KEYPAD) /*[*/
			appres.keypad_on ? (keypad_xwidth / 2) : 0,
#else /*][*/
			0,
#endif /*]*/
		    XtNy, menubar_height,
		    XtNbackground,
			appres.mono ? appres.background : colorbg_pixel,
		    NULL);
		save_00translations(nss.widget, &screen_t00);
		set_translations(nss.widget, (XtTranslations *)NULL,
		    &screen_t0);
	} else {
		XtVaSetValues(nss.widget,
		    XtNwidth, nss.screen_width,
		    XtNheight, nss.screen_height,
		    XtNx,
#if defined(X3270_KEYPAD) /*[*/
			appres.keypad_on ? (keypad_xwidth / 2) : 0,
#else /*][*/
			0,
#endif /*]*/
		    XtNy, menubar_height,
		    XtNbackground,
			appres.mono ? appres.background : colorbg_pixel,
		    NULL);
	}

	/* Set the container and toplevel dimensions */
	XtVaSetValues(container,
	    XtNwidth, container_width,
	    XtNheight, container_height,
	    NULL);

	set_toplevel_sizes();
}

/* Scrollbar support. */
void
screen_set_thumb(float top, float shown)
{
	if (toggled(SCROLL_BAR))
		XawScrollbarSetThumb(scrollbar, top, shown);
}

static void
screen_scroll_proc(Widget w unused, XtPointer client_data unused,
    XtPointer position)
{
	scroll_proc((int)position, (int)nss.screen_height);
}

static void
screen_jump_proc(Widget w unused, XtPointer client_data unused,
    XtPointer percent_ptr)
{
	jump_proc(*(float *)percent_ptr);
}

/* Create, move, or reset the scrollbar. */
static void
scrollbar_init(Boolean is_reset)
{
	if (!scrollbar_width) {
		if (scrollbar != (Widget)NULL)
			XtUnmapWidget(scrollbar);
	} else {
		if (scrollbar == (Widget)NULL) {
			scrollbar = XtVaCreateManagedWidget(
			    "scrollbar", scrollbarWidgetClass,
			    container,
			    XtNx, nss.screen_width+1
#if defined(X3270_KEYPAD) /*[*/
				    + (appres.keypad_on ? (keypad_xwidth / 2) : 0)
#endif /*]*/
				    ,
			    XtNy, menubar_height,
			    XtNwidth, scrollbar_width-1,
			    XtNheight, nss.screen_height,
			    XtNbackground, appres.mono ?
				appres.background : keypadbg_pixel,
			    NULL);
			XtAddCallback(scrollbar, XtNscrollProc,
			    screen_scroll_proc, NULL);
			XtAddCallback(scrollbar, XtNjumpProc,
			    screen_jump_proc, NULL);
		} else {
			XtVaSetValues(scrollbar,
			    XtNx, nss.screen_width+1
#if defined(X3270_KEYPAD) /*[*/
				    + (appres.keypad_on ? (keypad_xwidth / 2) : 0)
#endif /*]*/
				    ,
			    XtNy, menubar_height,
			    XtNwidth, scrollbar_width-1,
			    XtNheight, nss.screen_height,
			    XtNbackground, appres.mono ?
				appres.background : keypadbg_pixel,
			    NULL);
			XtMapWidget(scrollbar);
		}
		XawScrollbarSetThumb(scrollbar, 0.0, 1.0);

	}

	/*
	 * If the screen dimensions have changed, reallocate the scroll
	 * save area.
	 */
	if (is_reset || !scroll_initted)
		scroll_init();
	else
		rethumb();
}

/* Turn the scrollbar on or off */
void
toggle_scrollBar(struct toggle *t unused, enum toggle_type tt unused)
{
	scrollbar_changed = True;

	if (toggled(SCROLL_BAR)) {
		scrollbar_width = SCROLLBAR_WIDTH;
		screen_redo = REDO_SCROLLBAR;
	} else {
		scroll_to_bottom();
		scrollbar_width = 0;
	}

	screen_reinit(SCROLL_CHANGE);
	if (toggled(SCROLL_BAR))
		rethumb();
}

/*
 * Called when a host connects, disconnects or changes ANSI/3270 mode.
 */
static void
screen_connect(Boolean ignored unused)
{
	if (!screen_buf)
		return;		/* too soon */

	if (CONNECTED) {
		ctlr_erase(True);
		if (IN_3270)
			scroll_round();
		cursor_on();
		schedule_cursor_blink();
	} else {
		if (appres.disconnect_clear)
			ctlr_erase(True);
		(void) cursor_off();
	}

	mcursor_normal();
}

/*
 * Mouse cursor changes
 */

static void
set_mcursor(void)
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
	lock_icon(mcursor_state);
}

void
mcursor_normal(void)
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
mcursor_waiting(void)
{
	mcursor_state = WAIT;
	set_mcursor();
}

void
mcursor_locked(void)
{
	mcursor_state = LOCKED;
	set_mcursor();
}

#if defined(X3270_KEYPAD) /*[*/
/*
 * Called from the keypad button to expose or hide the integral keypad.
 */
void
screen_showikeypad(Boolean on)
{
	if (on)
		screen_redo = REDO_KEYPAD;

	inflate_screen();
	if (keypad_xwidth > 0) {
		if (scrollbar != (Widget) NULL)
			scrollbar_init(False);
		menubar_resize(on ? container_width : cwidth_nkp);
	}
}
#endif /*]*/


/*
 * The host just wrote a blinking character; make sure it blinks
 */
void
blink_start(void)
{
	text_blinkers_exist = True;
	if (!text_blink_scheduled) {
		/* Start in "on" state and start first iteration */
		text_blinking_on = True;
		schedule_text_blink();
	}
}

/*
 * Restore blanked blinking text
 */
static void
text_blink_it(XtPointer closure unused, XtIntervalId *id unused)
{
	/* Flip the state. */
	text_blinking_on = !text_blinking_on;

	/* Force a screen redraw. */
	ctlr_changed(0, ROWS*COLS);

	/* If there is still blinking text, schedule the next iteration */
	if (text_blinkers_exist)
		schedule_text_blink();
	else
		text_blink_scheduled = False;
}

/*
 * Schedule an event to restore blanked blinking text
 */
static void
schedule_text_blink(void)
{
	text_blink_scheduled = True;
	text_blink_id = XtAppAddTimeOut(appcontext, 500, text_blink_it, 0);
}


/*
 * Make the (displayed) cursor disappear.  Returns a Boolean indiciating if
 * the cursor was on before the call.
 */
static Boolean
cursor_off(void)
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
static void
cursor_blink_it(XtPointer closure unused, XtIntervalId *id unused)
{
	cursor_blink_pending = False;
	if (!CONNECTED || !toggled(CURSOR_BLINK))
		return;
	if (cursor_displayed) {
		if (in_focus)
			(void) cursor_off();
	} else
		cursor_on();
	schedule_cursor_blink();
}

/*
 * Schedule a cursor blink
 */
static void
schedule_cursor_blink(void)
{
	if (!toggled(CURSOR_BLINK) || cursor_blink_pending)
		return;
	cursor_blink_pending = True;
	cursor_blink_id = XtAppAddTimeOut(appcontext, 500, cursor_blink_it, 0);
}

/*
 * Cancel a cursor blink
 */
static void
cancel_blink(void)
{
	if (cursor_blink_pending) {
		XtRemoveTimeOut(cursor_blink_id);
		cursor_blink_pending = False;
	}
}

/*
 * Toggle cursor blinking (called from menu)
 */
void
toggle_cursorBlink(struct toggle *t unused, enum toggle_type tt unused)
{
	if (!CONNECTED)
		return;

	if (toggled(CURSOR_BLINK))
		schedule_cursor_blink();
	else
		cursor_on();
}

/*
 * Make the cursor visible at its (possibly new) location.
 */
static void
cursor_on(void)
{
	if (cursor_enabled && !cursor_displayed) {
		cursor_displayed = True;
		put_cursor(cursor_addr, True);
		ss->cursor_daddr = cursor_addr;
		cursor_changed = False;
	}
}


/*
 * Toggle the cursor (block/underline).
 */
void
toggle_altCursor(struct toggle *t, enum toggle_type tt unused)
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
cursor_move(int baddr)
{
	cursor_addr = baddr;
	cursor_pos();
}

/*
 * Display the cursor position on the status line
 */
static void
cursor_pos(void)
{
	if (!toggled(CURSOR_POS) || !CONNECTED)
		return;
	status_cursor_pos(cursor_addr);
}

/*
 * Toggle the display of the cursor position
 */
void
toggle_cursorPos(struct toggle *t unused, enum toggle_type tt unused)
{
	if (toggled(CURSOR_POS))
		cursor_pos();
	else
		status_uncursor_pos();
}

/*
 * Enable or disable cursor display (used by scroll logic)
 */
void
enable_cursor(Boolean on)
{
	if ((cursor_enabled = on) && CONNECTED) {
		cursor_on();
		cursor_changed = True;
	} else
		(void) cursor_off();
}


/*
 * Redraw the screen.
 */
static void
do_redraw(Widget w, XEvent *event, String *params unused,
    Cardinal *num_params unused)
{
	int	x, y, width, height;
	int	startcol, ncols;
	int	startrow, endrow, row;
	register int i;
	int     c0;

	if (w == nss.widget) {
		keypad_first_up();
		if (appres.active_icon && iconic) {
			ss = &nss;
			iconic = False;
		}
	} else if (appres.active_icon && w == iss.widget) {
		if (appres.active_icon && !iconic) {
			ss = &iss;
			iconic = True;
		}
	} else if (event)
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
		for (row = startrow; row < endrow; row++) {
			(void) memset((char *) &ss->image[ROWCOL_TO_BA(row, startcol)],
			      0, ncols * sizeof(union sp));
			if (ss->debugging_font) {
				c0 = ROWCOL_TO_BA(row, startcol);

				for (i = 0; i < ncols; i++)
					ss->image[c0 + i].bits.cg = CG_space;
			}
		}
					    
	} else {
		XFillRectangle(display, ss->window,
		    get_gc(ss, INVERT_COLOR(0)),
		    0, 0, ss->screen_width, ss->screen_height);
		(void) memset((char *) ss->image, 0,
		              (maxROWS*maxCOLS) * sizeof(union sp));
		if (ss->debugging_font)
			for (i = 0; i < maxROWS*maxCOLS; i++)
				ss->image[i].bits.cg = CG_space;
		ss->copied = False;
	}
	ctlr_changed(0, ROWS*COLS);
	cursor_changed = True;
	if (!appres.active_icon || !iconic) {
		line_changed = True;
		status_touch();
	}
}

/*
 * Explicitly redraw the screen (invoked from the keyboard).
 */
void
Redraw_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Redraw_action, event, params, num_params);
	do_redraw(w, event, params, num_params);
}

/*
 * Implicitly redraw the screen (triggered by Expose events).
 */
void
PA_Expose_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_Expose_action, event, params, num_params);
#endif /*]*/
	do_redraw(w, event, params, num_params);
}


/*
 * Redraw the changed parts of the screen.
 */

void
screen_disp(void)
{

	/* No point in doing anything if we aren't visible yet. */
	if (!ss->exposed_yet)
		return;

	/*
	 * We don't set "cursor_changed" when the host moves the cursor,
	 * 'cause he might just move it back later.  Set it here if the cursor
	 * has moved since the last call to screen_disp.
	 */
	if (cursor_addr != ss->cursor_daddr)
		cursor_changed = True;

	/*
	 * If only the cursor has changed (and not the screen image), draw it.
	 */
	if (cursor_changed && !screen_changed) {
		if (cursor_off())
			cursor_on();
	}

	/*
	 * Redraw the parts of the screen that need refreshing, and redraw the
	 * cursor if necessary.
	 */
	if (screen_changed) {
		Boolean	was_on = False;

		/* Draw the new screen image into "temp_image" */
		if (screen_changed)
			draw_fields(temp_image, first_changed, last_changed);

		/* Set "cursor_changed" if the text under it has changed. */
		if (ss->image[fl_baddr(cursor_addr)].word !=
		    temp_image[fl_baddr(cursor_addr)].word)
			cursor_changed = True;

		/* Undraw the cursor, if necessary. */
		if (cursor_changed)
			was_on = cursor_off();

		/* Intelligently update the X display with the new text. */
		resync_display(temp_image, first_changed, last_changed);

		/* Redraw the cursor. */
		if (was_on)
			cursor_on();

		screen_changed = False;
		first_changed = -1;
		last_changed = -1;
	}

	if (!appres.active_icon || !iconic) {
		/* Refresh the status line. */
		status_disp();

		/* Refresh the line across the bottom of the screen. */
		if (line_changed) {
			XDrawLine(display, ss->window,
			    get_gc(ss, GC_NONDEFAULT | DEFAULT_PIXEL),
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
static void
render_blanks(int baddr, int height, union sp *buffer)
{
	int x, y;

#if defined(_ST) /*[*/
	(void) printf("render_blanks(baddr=%s, height=%d)\n", rcba(baddr),
	    height);
#endif /*]*/

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));

	XFillRectangle(display, ss->window,
	    get_gc(ss, INVERT_COLOR(0)),
	    x, y - ss->ascent,
	    (ss->char_width * COLS) + 1, (ss->char_height * height));

	(void) MEMORY_MOVE((char *) &ss->image[baddr],
			   (char *) &buffer[baddr],
	                   COLS * height *sizeof(union sp));
}

/*
 * Check if a region of the screen is effectively empty: all blanks or nulls
 * with no extended highlighting attributes and not selected.
 *
 * Works _only_ with non-debug fonts.
 */
static Boolean
empty_space(register union sp *buffer, int len)
{
	register int i;

	for (i = 0; i < len; i++, buffer++) {
		if (buffer->bits.gr ||
		    buffer->bits.sel ||
		    (buffer->bits.fg & INVERT_MASK) ||
		    !BKM_ISSET(buffer->bits.cg))
			return False;
	}
	return True;
}


/*
 * Reconcile the differences between a region of 'buffer' (what we want on
 * the X display) and ss->image[] (what is on the X display now).  The region
 * must not span lines.
 */
static void
resync_text(int baddr, int len, union sp *buffer)
{
	static Boolean ever = False;
	static unsigned long cmask = 0L;
	static unsigned long gmask = 0L;

#if defined(_ST) /*[*/
	(void) printf("resync_text(baddr=%s, len=%d)\n", rcba(baddr), len);
#endif /*]*/
	if (!ever) {
		union sp b;

		/* Create masks for the "important" fields in an sp. */
		b.word = 0L;
		b.bits.fg = COLOR_MASK | INVERT_MASK;
		b.bits.sel = 1;
		b.bits.gr = GR_UNDERLINE | GR_INTENSIFY;
		cmask = b.word;

		b.word = 0L;
		b.bits.fg = INVERT_MASK;
		b.bits.sel = 1;
		b.bits.gr = 0xf;
		gmask = b.word;

		ever = True;
	}

	if (!ss->debugging_font &&
	    len > 1 &&
	    empty_space(&buffer[baddr], len)) {
		int x, y;

		x = ssCOL_TO_X(BA_TO_COL(baddr));
		y = ssROW_TO_Y(BA_TO_ROW(baddr));

		/* All empty, fill a rectangle */
#if defined(_ST) /*[*/
		(void) printf("FillRectangle(baddr=%s, len=%d)\n", rcba(baddr),
		    len);
#endif /*]*/
		XFillRectangle(display, ss->window,
		    get_gc(ss, INVERT_COLOR(0)),
		    x, y - ss->ascent,
		    (ss->char_width * len) + 1, ss->char_height);
	} else {
		unsigned long attrs, attrs2;
		Boolean has_gr, has_gr2;
		Boolean empty, empty2;
		union sp ra;
		int i;
		int i0 = 0;

		ra = buffer[baddr];

		/* Note the characteristics of the beginning of the region. */
		attrs = buffer[baddr].word & cmask;
		has_gr = (buffer[baddr].word & gmask) != 0;
		empty = !has_gr && BKM_ISSET(buffer[baddr].bits.cg);

		for (i = 0; i < len; i++) {
			/* Note the characteristics of this character. */
			attrs2 = buffer[baddr+i].word & cmask;
			has_gr2 = (buffer[baddr+i].word & gmask) != 0;
			empty2 = !has_gr2 && BKM_ISSET(buffer[baddr+i].bits.cg);

			/* If this character has exactly the same attributes
			   as the current region, simply add it, noting that
			   the region might now not be empty. */
			if (attrs2 == attrs) {
				if (!empty2)
					empty = 0;
				continue;
			}

			/* If this character is empty, and the current region
			   has no GR attributes, pretend it matches. */
			if (empty2 && !has_gr)
				continue;

			/* If the current region is empty, this character
			   isn't empty, and this character has no GR
			   attributes, change the current region's attributes
			   to this character's attributes and add it. */
			if (empty && !empty2 && !has_gr2) {
				attrs = attrs2;
				has_gr = has_gr2;
				empty = empty2;
				ra = buffer[baddr+i];
				continue;
			}

			/* Dump the region and start a new one with this
			   character. */
			render_text(&buffer[baddr+i0], baddr+i0, i - i0, False,
			    &ra);
			attrs = attrs2;
			has_gr = has_gr2;
			empty = empty2;
			i0 = i;
			ra = buffer[baddr+i];
		}

		/* Dump the remainder of the region. */
		render_text(&buffer[baddr+i0], baddr+i0, len - i0, False, &ra);
	}

	/* The X display is now correct; update ss->image[]. */
	(void) MEMORY_MOVE((char *) &ss->image[baddr],
			   (char *) &buffer[baddr],
	                   len*sizeof(union sp));
}

/*
 * Render text onto the X display.  The region must not span lines.
 */
static void
render_text(union sp *buffer, int baddr, int len, Boolean block_cursor,
    union sp *attrs)
{
	int color;
	int x, y;
	GC dgc;
	int sel = attrs->bits.sel;
	register int i;

#if defined(_ST) /*[*/
	(void) printf("render_text(baddr=%s, len=%d)\n", rcba(baddr), len);
#endif /*]*/

	if (ss->standard_font) {		/* Standard X font */
		register unsigned char *xfmap = ss->xfmap;

		if (toggled(MONOCASE)) {
			for (i = 0; i < len; i++) {
				switch (buffer[i].bits.cs) {
				    case 0:
					rt_buf8[i] =
					    asc2uc[xfmap[buffer[i].bits.cg]];
					break;
				    case 1:
					rt_buf8[i] = ge2asc[buffer[i].bits.cg];
					break;
				    case 2:
					rt_buf8[i] = buffer[i].bits.cg;
					break;
				}
			}
		} else for (i = 0; i < len; i++) {
			switch (buffer[i].bits.cs) {
			    case 0:
				rt_buf8[i] = xfmap[buffer[i].bits.cg];
				break;
			    case 1:
				rt_buf8[i] = ge2asc[buffer[i].bits.cg];
				break;
			    case 2:
				rt_buf8[i] = buffer[i].bits.cg;
				break;
			}
		}
	} else if (ss->extended_3270font) {	/* 16-bit (new) x3270 font */
		if (toggled(MONOCASE)) {
			for (i = 0; i < len; i++) {
				rt_buf16[i].byte1 = buffer[i].bits.cs;
				switch (buffer[i].bits.cs) {
				    case 0:
					rt_buf16[i].byte2 =
					    cg2uc[buffer[i].bits.cg];
					break;
				    case 1:
				    case 2:
					rt_buf16[i].byte2 = buffer[i].bits.cg;
					break;
				}
			}
		} else
			for (i = 0; i < len; i++) {
				rt_buf16[i].byte1 = buffer[i].bits.cs;
				rt_buf16[i].byte2 = buffer[i].bits.cg;
			}
	} else {				/* 8-bit (old) x3270 font */
		if (toggled(MONOCASE)) {
			for (i = 0; i < len; i++) {
				switch (buffer[i].bits.cs) {
				    case 0:
					rt_buf8[i] =
					    cg2uc[buffer[i].bits.cg];
					break;
				    case 1:
					rt_buf8[i] =
					    ge2cg8[buffer[i].bits.cg];
					break;
				    case 2:
					rt_buf8[i] = CG_boxsolid;
					break;
				}
			}
		} else
			for (i = 0; i < len; i++) {
				switch (buffer[i].bits.cs) {
				    case 0:
					rt_buf8[i] = buffer[i].bits.cg;
					break;
				    case 1:
					rt_buf8[i] =
					    ge2cg8[buffer[i].bits.cg];
					break;
				    case 2:
					rt_buf8[i] = CG_boxsolid;
					break;
				}
			}
	}

	x = ssCOL_TO_X(BA_TO_COL(baddr));
	y = ssROW_TO_Y(BA_TO_ROW(baddr));
	color = attrs->bits.fg;

	/* Draw. */
	if (sel && !block_cursor) {
		if (!appres.mono)
			dgc = get_selgc(ss, color);
		else
			dgc = get_gc(ss, INVERT_COLOR(color));
	} else if (block_cursor && !(appres.mono && sel)) {
		if (appres.use_cursor_color)
			dgc = ss->invucgc;
		else
			dgc = get_gc(ss, INVERT_COLOR(color));
	} else
		dgc = get_gc(ss, color);

	/* Draw the text */
	if (ss->extended_3270font)
		XDrawImageString16(display, ss->window, dgc, x, y, rt_buf16,
		    len);
	else
		XDrawImageString(display, ss->window, dgc, x, y,
		    (char *)rt_buf8, len);
	if (ss->overstrike &&
	    ((attrs->bits.gr & GR_INTENSIFY) ||
	     ((appres.mono || appres.highlight_bold) &&
	      ((color & BASE_MASK) == FA_INT_HIGH_SEL)))) {
		if (ss->extended_3270font)
			XDrawString16(display, ss->window, dgc, x+1, y,
			    rt_buf16, len);
		else
			XDrawString(display, ss->window, dgc, x+1, y,
			    (char *)rt_buf8, len);
	}
	if (attrs->bits.gr & GR_UNDERLINE)
		XDrawLine(display,
		    ss->window,
		    dgc,
		    x,
		    y - ss->ascent + ss->char_height - 1,
		    x + ss->char_width * len,
		    y - ss->ascent + ss->char_height - 1);
}


#if defined(X3270_ANSI) /*[*/
Boolean
screen_obscured(void)
{
	return ss->obscured;
}

/*
 * Scroll the screen image one row.
 *
 * This is the optimized path from ctlr_scroll(); it assumes that
 * screen_buf[] and ea_buf[] have already been modified and that the screen
 * can be brought into sync by hammering ss->image and the bitmap.
 */
void
screen_scroll(void)
{
	Boolean was_on;

	if (!ss->exposed_yet)
		return;

	was_on = cursor_off();
	(void) MEMORY_MOVE((char *)&ss->image[0], 
	                   (char *)&ss->image[COLS],
	                   (ROWS - 1) * COLS * sizeof(union sp));
	(void) MEMORY_MOVE((char *)&temp_image[0], 
	                   (char *)&temp_image[COLS],
	                   (ROWS - 1) * COLS * sizeof(union sp));
	(void) memset((char *)&ss->image[(ROWS - 1) * COLS], 0,
	              COLS * sizeof(union sp));
	(void) memset((char *)&temp_image[(ROWS - 1) * COLS], 0,
	              COLS * sizeof(union sp));
	XCopyArea(display, ss->window, ss->window, get_gc(ss, 0),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(1) - ss->ascent,
	    ss->char_width * COLS,
	    ss->char_height * (ROWS - 1),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(0) - ss->ascent);
	ss->copied = True;
	XFillRectangle(display, ss->window, get_gc(ss, INVERT_COLOR(0)),
	    ssCOL_TO_X(0),
	    ssROW_TO_Y(ROWS - 1) - ss->ascent,
	    (ss->char_width * COLS) + 1,
	    ss->char_height);
	if (was_on)
		cursor_on();
}
#endif /*]*/


/*
 * Toggle mono-/dual-case mode.
 */
void
toggle_monocase(struct toggle *t unused, enum toggle_type tt unused)
{
	(void) memset((char *) ss->image, 0,
		      (ROWS*COLS) * sizeof(union sp));
	ctlr_changed(0, ROWS*COLS);
}

/*
 * Toggle screen flip
 */
void
screen_flip(void)
{
	flipped = !flipped;

	action_internal(PA_Expose_action, IA_REDRAW, CN, CN);
}

/*
 * "Draw" screen_buf into a buffer
 */
static void
draw_fields(union sp *buffer, int first, int last)
{
	int	baddr = 0;
	unsigned char	*fap;
	unsigned char	fa;
	struct ea       *field_ea;
	struct ea	*char_ea = ea_buf;
	unsigned char	*sbp = screen_buf;
	int	field_color;
	int	zero;
	Boolean	any_blink = False;

	/* If there is any blinking text, override the suggested boundaries. */
	if (text_blinkers_exist) {
		first = -1;
		last = -1;
	}

	/* Adjust pointers to start of region. */
	if (first > 0) {
		baddr += first;
		char_ea += first;
		sbp += first;
		buffer += first;
	}
	fap = get_field_attribute(baddr);
	fa = *fap;
	field_ea = fa2ea(fap);

	/* Adjust end of region. */
	if (last == -1 || last >= ROWS*COLS)
		last = 0;

	zero = FA_IS_ZERO(fa);
	if (field_ea->fg && (!appres.modified_sel || !FA_IS_MODIFIED(fa)))
		field_color = field_ea->fg & COLOR_MASK;
	else
		field_color = fa_color(fa);

	do {
		unsigned char	c = *sbp;
		union sp	b;

		b.word = 0;	/* clear out all fields */

		if (IS_FA(c)) {
			fa = c;
			field_ea = char_ea;
			zero = FA_IS_ZERO(fa);
			if (field_ea->fg && (!appres.modified_sel || !FA_IS_MODIFIED(fa)))
				field_color = field_ea->fg & COLOR_MASK;
			else
				field_color = fa_color(fa);
			if (ss->debugging_font) {
				b.bits.cg = c;
				if (appres.m3279 || !zero)
					b.bits.fg = field_color;
			} /* otherwise, CG_null in color 0 */
		} else {
			unsigned short gr;
			int e_color;

			/* Find the right graphic rendition. */
			gr = char_ea->gr;
			if (!gr)
				gr = field_ea->gr;
			if (gr & GR_BLINK)
				any_blink = True;
			if (appres.highlight_bold && FA_IS_HIGH(fa))
				gr |= GR_INTENSIFY;

			/* Find the right color. */
			if (char_ea->fg)
				e_color = char_ea->fg & COLOR_MASK;
			else if (appres.mono && (gr & GR_INTENSIFY))
				e_color = fa_color(FA_INT_HIGH_SEL);
			else
				e_color = field_color;
			if (gr & GR_REVERSE)
				e_color = INVERT_COLOR(e_color);
			if (!appres.mono)
				b.bits.fg = e_color;

			/* Find the right character and character set. */
			if (zero) {
				if (ss->debugging_font)
					b.bits.cg = CG_space;
			} else if ((c != CG_null && c != CG_space) ||
			           (gr & (GR_REVERSE | GR_UNDERLINE)) ||
				   ss->debugging_font) {

				b.bits.fg = e_color;

				/*
				 * Replace blanked-out blinking text with
				 * spaces.
				 */
				if (!text_blinking_on && (gr & GR_BLINK))
					b.bits.cg = CG_space;
				else {
					b.bits.cg = c;
					if (char_ea->cs)
						b.bits.cs = char_ea->cs;
					else
						b.bits.cs = field_ea->cs;
					if (b.bits.cs & CS_GE)
						b.bits.cs = 1;
					else
						b.bits.cs &= CS_MASK;
				}

			} /* otherwise, CG_null */

			b.bits.gr = gr & (GR_UNDERLINE | GR_INTENSIFY);
		}
		if (SELECTED(baddr))
			b.bits.sel = 1;
		if (!flipped)
			*buffer++ = b;
		else
			*(buffer + fl_baddr(baddr)) = b;
		char_ea++;
		sbp++;
		INC_BA(baddr);
	} while (baddr != last);

	/* Cancel blink timeouts if none were seen this pass. */
	if (!any_blink)
		text_blinkers_exist = False;
}


/*
 * Resync the X display with the contents of 'buffer'
 */
static void
resync_display(union sp *buffer, int first, int last)
{
	register int	i, j;
	int		b = 0;
	int		i0 = -1;
	Boolean		ccheck;
	int		fca = fl_baddr(cursor_addr);
	int		first_row, last_row;
#	define SPREAD	10

	if (first < 0) {
		first_row = 0;
		last_row = ROWS;
	} else {
		first_row = first / COLS;
		b = first_row * COLS;
		last_row = (last + (COLS-1)) / COLS;
	}

	for (i = first_row; i < last_row; b += COLS, i++) {
		int d0 = -1;
		int s0 = -1;

		/* Has the line changed? */
		if (!memcmp((char *) &ss->image[b], (char *) &buffer[b],
		    COLS*sizeof(union sp))) {
			if (i0 >= 0) {
				render_blanks(i0 * COLS, i - i0, buffer);
				i0 = -1;
			}
			continue;
		}

		/* Is the new value empty? */
		if (!ss->debugging_font &&
		    !(fca >= b && fca < (b+COLS)) &&
		    empty_space(&buffer[b], COLS)) {
			if (i0 < 0)
				i0 = i;
			continue;
		}

		/* Yes, it changed, and it isn't blank.
		   Dump any pending blank lines. */
		if (i0 >= 0) {
			render_blanks(i0 * COLS, i - i0, buffer);
			i0 = -1;
		}

		/* New text.  Scan it. */
		ccheck = cursor_displayed &&
			 fca >= b &&
			 fca < (b+COLS);
		for (j = 0; j < COLS; j++) {
			if (ccheck && b+j == fca) {
				/* Don't repaint over the cursor. */

				/* Dump any pending "different" characters. */
				if (d0 >= 0)
					resync_text(b+d0, j-d0, buffer);

				/* Start over. */
				d0 = -1;
				s0 = -1;
				continue;
			}
			if (ss->image[b+j].word == buffer[b+j].word) {

				/* Character is the same. */

				if (d0 >= 0) {
					/* Something is pending... */
					if (s0 < 0) {
						/* Start of "same" area */
						s0 = j;
					} else {
						/* nth matching character */
						if (j - s0 > SPREAD) {
							/* too many */
							resync_text(b+d0,
							    s0-d0, buffer);
							d0 = -1;
							s0 = -1;
						}
					}
				}
			} else {

				/* Character is different. */

				/* Forget intermediate matches. */
				s0 = -1;

				if (d0 < 0)
					/* Mark the start. */
					d0 = j;
			}
		}

		/* Dump any pending "different" characters. */
		if (d0 >= 0)
			resync_text(b+d0, COLS-d0, buffer);
	}
	if (i0 >= 0)
		render_blanks(i0 * COLS, last_row - i0, buffer);
}


/*
 * Support code for cursor redraw.
 */

/*
 * Calculate a flipped baddr.
 */
static int
fl_baddr(int baddr)
{
	if (!flipped)
		return baddr;
	return ((baddr / COLS) * COLS) + (COLS - (baddr % COLS) - 1);
}

/*
 * Return the proper foreground color for a character position.
 */

static int
char_color(int baddr)
{
	unsigned char *fa;
	int color;

	fa = get_field_attribute(baddr);

	/*
	 * Find the color of the character or the field.
	 */
	if (ea_buf[baddr].fg)
		color = ea_buf[baddr].fg & COLOR_MASK;
	else if (fa2ea(fa)->fg && (!appres.modified_sel || !FA_IS_MODIFIED(*fa)))
		color = fa2ea(fa)->fg & COLOR_MASK;
	else
		color = fa_color(*fa);

	/*
	 * Now apply reverse video.
	 *
	 * One bit of strangeness:
	 *  If the buffer is a field attribute and we aren't using the
	 *  debug font, it's displayed as a blank; don't invert.
	 */
	if (!((IS_FA(screen_buf[baddr]) && !ss->debugging_font)) &&
	    ((ea_buf[baddr].gr & GR_REVERSE) || (fa2ea(fa)->gr & GR_REVERSE)))
		color = INVERT_COLOR(color);

	/*
	 * In monochrome, apply selection status as well.
	 */
	if (appres.mono && SELECTED(baddr))
		color = INVERT_COLOR(color);

	return color;
}


/*
 * Select a GC for drawing a hollow or underscore cursor.
 */
static GC
cursor_gc(int baddr)
{
	/*
	 * If they say use a particular color, use it.
	 */
	if (appres.use_cursor_color)
		return ss->ucgc;
	else
		return get_gc(ss, char_color(baddr));
}

/*
 * Redraw one character.
 * If 'invert' is true, invert the foreground and background colors.
 */
static void
redraw_char(int baddr, Boolean invert)
{
	union sp buffer;
	unsigned char *fa;
	int gr;
	int blank_it = 0;

	if (!invert) {
		int flb = fl_baddr(baddr);

		/* Simply put back what belongs there. */
		render_text(&ss->image[flb], flb, 1, False, &ss->image[flb]);
		return;
	}

	/*
	 * Fabricate the right thing.
	 * ss->image isn't going to help, because it may contain shortcuts
	 *  for faster display, so we have to construct a buffer to use.
	 */
	buffer.word = 0L;
	buffer.bits.cg = screen_buf[baddr];
	buffer.bits.cs = ea_buf[baddr].cs;
	if (buffer.bits.cs & CS_GE)
		buffer.bits.cs = 1;
	else
		buffer.bits.cs &= CS_MASK;
	fa = get_field_attribute(baddr);
	gr = ea_buf[baddr].gr;
	if (!gr)
		gr = fa2ea(fa)->gr;
	if (IS_FA(screen_buf[baddr])) {
		if (!ss->debugging_font)
			blank_it = 1;
	} else if (FA_IS_ZERO(*fa)) {
		blank_it = 1;
	} else if (text_blinkers_exist && !text_blinking_on) {
		if (gr & GR_BLINK)
			blank_it = 1;
	}
	if (blank_it) {
		buffer.bits.cg = CG_space;
		buffer.bits.cs = 0;
	}
	buffer.bits.fg = char_color(baddr);
	buffer.bits.gr |= (gr & GR_INTENSIFY);
	render_text(&buffer, fl_baddr(baddr), 1, True, &buffer);
}

/*
 * Draw a hollow cursor.
 */
static void
hollow_cursor(int baddr)
{
	XDrawRectangle(display,
	    ss->window,
	    cursor_gc(baddr),
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent +
		(appres.mono ? 1 : 0),
	    ss->char_width - 1,
	    ss->char_height - (appres.mono ? 2 : 1));
}

/*
 * Draw an underscore cursor.
 */
static void
underscore_cursor(int baddr)
{
	XDrawRectangle(display,
	    ss->window,
	    cursor_gc(baddr),
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent +
		ss->char_height - 2,
	    ss->char_width - 1,
	    1);
}

/*
 * Invert a square over a character.
 */
static void
small_inv_cursor(int baddr)
{
	XFillRectangle(display,
	    ss->window,
	    ss->mcgc,
	    ssCOL_TO_X(BA_TO_COL(fl_baddr(baddr))),
	    ssROW_TO_Y(BA_TO_ROW(baddr)) - ss->ascent + 1,
	    ss->char_width,
	    (ss->char_height > 2) ? (ss->char_height - 2) : 1);
}

/*
 * Draw or remove the cursor.
 */
static void
put_cursor(int baddr, Boolean on)
{
	/*
	 * If the cursor is being turned off, simply redraw the text under it.
	 */
	if (!on) {
		redraw_char(baddr, False);
		return;
	}

	/*
	 * If underscore cursor, redraw the character and draw the underscore.
	 */
	if (toggled(ALT_CURSOR)) {
		redraw_char(baddr, False);
		underscore_cursor(baddr);
		return;
	}

	/*
	 * On, and not an underscore.
	 *
	 * If out of focus, either draw an empty box in its place (if block
	 * cursor) or redraw the underscore (if underscore).
	 */
	if (!in_focus) {
		hollow_cursor(baddr);
		return;
	}

	/*
	 * If monochrome, invert a small square over the characters.
	 */
	if (appres.mono) {
		small_inv_cursor(baddr);
		return;
	}

	/*
	 * Color: redraw the character in reverse video.
	 */
	redraw_char(baddr, True);
}


/* Allocate a named color. */
static Boolean
alloc_color(char *name, enum fallback_color fb_color, Pixel *pixel)
{
	XColor cell, db;
	Screen *s;

	s = XtScreen(toplevel);

	if (XAllocNamedColor(display, XDefaultColormapOfScreen(s), name,
	    &cell, &db) != 0) {
		*pixel = db.pixel;
		return True;
	}
	switch (fb_color) {
	    case FB_WHITE:
		*pixel = XWhitePixelOfScreen(s);
		break;
	    case FB_BLACK:
		*pixel = XBlackPixelOfScreen(s);
		break;
	}
	return False;
}

/* Spell out a fallback color. */
static const char *
fb_name(enum fallback_color fb_color)
{
	switch (fb_color) {
	    case FB_WHITE:
		return "white";
	    case FB_BLACK:
		return "black";
	}
	return "chartreuse";	/* to keep Gcc -Wall happy */
}

/* Allocate color pixels. */
static void
allocate_pixels(void)
{

	if (appres.mono)
		return;

	/* Allocate constant elements. */
	if (!alloc_color(appres.colorbg_name, FB_BLACK, &colorbg_pixel))
		xs_warning("Cannot allocate colormap \"%s\" for screen background, using \"black\"",
		    appres.colorbg_name);
	if (!alloc_color(appres.selbg_name, FB_BLACK, &selbg_pixel))
		xs_warning("Cannot allocate colormap \"%s\" for select background, using \"black\"",
		    appres.selbg_name);
	if (!alloc_color(appres.keypadbg_name, FB_WHITE, &keypadbg_pixel))
		xs_warning("Cannot allocate colormap \"%s\" for keypad background, using \"white\"",
		    appres.keypadbg_name);
	if (appres.use_cursor_color) {
		if (!alloc_color(appres.cursor_color_name, FB_WHITE, &cursor_pixel))
			xs_warning("Cannot allocate colormap \"%s\" for cursor color, using \"white\"",
			    appres.cursor_color_name);
	}

	/* Allocate pseudocolors. */
	if (!appres.m3279) {
		if (!alloc_color(appres.normal_name, FB_WHITE, &normal_pixel))
			xs_warning("Cannot allocate colormap \"%s\" for text, using \"white\"",
			    appres.normal_name);
		if (!alloc_color(appres.select_name, FB_WHITE, &select_pixel))
			xs_warning("Cannot allocate colormap \"%s\" for selectable text, using \"white\"",
			    appres.select_name);
		if (!alloc_color(appres.bold_name, FB_WHITE, &bold_pixel))
			xs_warning("Cannot allocate colormap \"%s\" for bold text, using \"white\"",
			    appres.bold_name);
		return;
	}
}

#if defined(X3270_MENUS) /*[*/
/* Deallocate pixels. */
static void
destroy_pixels(void)
{
	int i;

	/*
	 * It would make sense to deallocate many of the pixels here, but
	 * the only available call (XFreeColors) would deallocate cells
	 * that may be in use by other Xt widgets.  Occh.
	 */

	for (i = 0; i < 16; i++)
		cpx_done[i] = False;
}
#endif /*]*/

/*
 * Create graphics contexts.
 */
static void
make_gcs(struct sstate *s)
{
	XGCValues xgcv;

	if (appres.m3279) {
		int i;

		for (i = 0; i < NGCS; i++) {
			if (s->gc[i] != (GC)None) {
				XtReleaseGC(toplevel, s->gc[i]);
				s->gc[i] = (GC)None;
			}
			if (s->gc[i + NGCS] != (GC)None) {
				XtReleaseGC(toplevel, s->gc[i + NGCS]);
				s->gc[i + NGCS] = (GC)None;
			}
			if (s->selgc[i] != (GC)None) {
				XtReleaseGC(toplevel, s->selgc[i]);
				s->selgc[i] = (GC)None;
			}
		}
	} else {
		if (!appres.mono) {
			make_gc_set(s, FA_INT_NORM_NSEL, normal_pixel,
			    colorbg_pixel);
			make_gc_set(s, FA_INT_NORM_SEL,  select_pixel,
			    colorbg_pixel);
			make_gc_set(s, FA_INT_HIGH_SEL,  bold_pixel,
			    colorbg_pixel);
		} else {
			make_gc_set(s, FA_INT_NORM_NSEL, appres.foreground,
			    appres.background);
			make_gc_set(s, FA_INT_NORM_SEL,  appres.foreground,
			    appres.background);
			make_gc_set(s, FA_INT_HIGH_SEL,  appres.foreground,
			    appres.background);
		}
	}

	/* Create monochrome block cursor GC. */
	if (appres.mono && s->mcgc == (GC)None) {
		xgcv.function = GXxor;
		xgcv.foreground = 1L;
		s->mcgc = XtGetGC(toplevel,
		    GCForeground|GCFunction,
		    &xgcv);
	}

	/* Create explicit cursor color cursor GCs. */
	if (appres.use_cursor_color) {
		if (s->ucgc != (GC)None) {
			XtReleaseGC(toplevel, s->ucgc);
			s->ucgc = (GC)None;
		}
		xgcv.foreground = cursor_pixel;
		s->ucgc = XtGetGC(toplevel, GCForeground, &xgcv);

		if (s->invucgc != (GC)None) {
			XtReleaseGC(toplevel, s->invucgc);
			s->invucgc = (GC)None;
		}
		xgcv.foreground = colorbg_pixel;
		xgcv.background = cursor_pixel;
		xgcv.font = s->fid;
		s->invucgc = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	}

	/* Set the flag for overstriking bold. */
	s->overstrike = (s->char_width > 1);
}

/* Set up a default color scheme. */
static void
default_color_scheme(void)
{
	static int default_attrib_colors[4] = {
	    GC_NONDEFAULT | COLOR_GREEN,/* default */
	    GC_NONDEFAULT | COLOR_RED,	/* intensified */
	    GC_NONDEFAULT | COLOR_BLUE,	/* protected */
	    GC_NONDEFAULT | COLOR_WHITE	/* protected, intensified */
	};
	int i;

	ibm_fb = FB_WHITE;
	for (i = 0; i < 16; i++) {
		if (color_name[i] != (char *)NULL)
			XtFree(color_name[i]);
		color_name[i] = XtNewString("white");
	}
	for (i = 0; i < 4; i++)
		field_colors[i] = default_attrib_colors[i];
}

/* Transfer the colorScheme resource into arrays. */
static Boolean
xfer_color_scheme(char *cs, Boolean do_popup)
{
	int i;
	char *scheme_name = CN;
	char *s0 = CN, *scheme = CN;
	char *tk;

	char *tmp_color_name[16];
	enum fallback_color tmp_ibm_fb = FB_WHITE;
	char *tmp_colorbg_name = CN;
	char *tmp_selbg_name = CN;
	int tmp_field_colors[4];

	if (cs == CN)
		goto failure;
	scheme_name = xs_buffer("%s.%s", ResColorScheme, cs);
	s0 = get_resource(scheme_name);
	if (s0 == CN) {
		if (do_popup)
			popup_an_error("Can't find resource %s", scheme_name);
		else
			xs_warning("Can't find resource %s", scheme_name);
		goto failure;
	}
	scheme = s0 = XtNewString(s0);
	for (i = 0; (tk = strtok(scheme, " \t\n")) != CN; i++) {
		scheme = CN;
		if (i > 22) {
			xs_warning("Ignoring excess data in %s resource",
			    scheme_name);
			break;
		}
		switch (i) {
		    case  0: case  1: case  2: case  3:
		    case  4: case  5: case  6: case  7:
		    case  8: case  9: case 10: case 11:
		    case 12: case 13: case 14: case 15:	/* IBM color name */
			tmp_color_name[i] = tk;
			break;
		    case 16:	/* default for IBM colors */
			if (!strcmp(tk, "white"))
				tmp_ibm_fb = FB_WHITE;
			else if (!strcmp(tk, "black"))
				tmp_ibm_fb = FB_BLACK;
			else {
				if (do_popup)
					popup_an_error("Invalid default color");
				else
					xs_warning("Invalid default color");
				goto failure;
			}
			break;
		    case 17:	/* screen background */
			tmp_colorbg_name = tk;
			break;
		    case 18:	/* select background */
			tmp_selbg_name = tk;
			break;
		    case 19: case 20: case 21: case 22:	/* attribute colors */
			tmp_field_colors[i-19] = atoi(tk);
			if (tmp_field_colors[i-19] < 0 ||
			    tmp_field_colors[i-19] > 0x0f) {
				if (do_popup)
					popup_an_error("Invalid %s resource, ignoring",
					    scheme_name);
				else
					xs_warning("Invalid %s resource, ignoring",
					    scheme_name);
				goto failure;
			}
			tmp_field_colors[i-19] |= GC_NONDEFAULT;
		}
	}
	if (i < 23) {
		if (do_popup)
			popup_an_error("Insufficient data in %s resource",
			    scheme_name);
		else
			xs_warning("Insufficient data in %s resource",
			    scheme_name);
		goto failure;
	}

	/* Success: transfer to live variables. */
	for (i = 0; i < 16; i++) {
		if (color_name[i] != (char *)NULL)
			XtFree(color_name[i]);
		color_name[i] = XtNewString(tmp_color_name[i]);
	}
	ibm_fb = tmp_ibm_fb;
	appres.colorbg_name = XtNewString(tmp_colorbg_name);
	appres.selbg_name = XtNewString(tmp_selbg_name);
	for (i = 0; i < 4; i++)
		field_colors[i] = tmp_field_colors[i];

	/* Clean up and exit. */
	if (scheme_name != CN)
		XtFree(scheme_name);
	if (s0 != CN)
		XtFree(s0);
	return True;

    failure:
	if (scheme_name != CN)
		XtFree(scheme_name);
	if (s0 != CN)
		XtFree(s0);
	return False;
}

/* Look up a GC, allocating it if necessary. */
static GC
get_gc(struct sstate *s, int color)
{
	int pixel_index;
	XGCValues xgcv;
	GC r;

	if (color & GC_NONDEFAULT)
		color &= ~GC_NONDEFAULT;
	else
		color = (color & INVERT_MASK) | DEFAULT_PIXEL;

	if ((r = s->gc[color]) != (GC)None)
		return r;

	/* Allocate the pixel. */
	pixel_index = PIXEL_INDEX(color);
	if (!cpx_done[pixel_index]) {
		if (!alloc_color(color_name[pixel_index], ibm_fb,
				 &cpx[pixel_index])) {
			static char nbuf[16];

			(void) sprintf(nbuf, "%d", pixel_index);
			xs_warning("Cannot allocate colormap \"%s\" for 3279 color %s (%s), using \"%s\"",
			    color_name[pixel_index], nbuf,
			    see_color((unsigned char)(pixel_index + 0xf0)),
			    fb_name(ibm_fb));
		}
		cpx_done[pixel_index] = True;
	}

	/* Allocate the GC. */
	xgcv.font = s->fid;
	if (!(color & INVERT_MASK)) {
		xgcv.foreground = cpx[pixel_index];
		xgcv.background = colorbg_pixel;
	} else {
		xgcv.foreground = colorbg_pixel;
		xgcv.background = cpx[pixel_index];
	}
	if (s == &nss && pixel_index == DEFAULT_PIXEL) {
		xgcv.graphics_exposures = True;
		r = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont|GCGraphicsExposures,
		    &xgcv);
	} else
		r = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont,
		    &xgcv);
	return s->gc[color] = r;
}

/* Look up a selection GC, allocating it if necessary. */
static GC
get_selgc(struct sstate *s, int color)
{
	XGCValues xgcv;
	GC r;

	if (color & GC_NONDEFAULT)
		color = PIXEL_INDEX(color);
	else
		color = DEFAULT_PIXEL;

	if ((r = s->selgc[color]) != (GC)None)
		return r;

	/* Allocate the pixel. */
	if (!cpx_done[color]) {
		if (!alloc_color(color_name[color], FB_WHITE, &cpx[color])) {
			static char nbuf[16];

			(void) sprintf(nbuf, "%d", color);
			xs_warning("Cannot allocate colormap \"%s\" for 3279 color %s (%s), using \"white\"",
			    color_name[color], nbuf,
			    see_color((unsigned char)(color + 0xf0)));
		}
		cpx_done[color] = True;
	}

	/* Allocate the GC. */
	xgcv.font = s->fid;
	xgcv.foreground = cpx[color];
	xgcv.background = selbg_pixel;
	return s->selgc[color] =
	    XtGetGC(toplevel, GCForeground|GCBackground|GCFont, &xgcv);
}

/* External entry points for GC allocation. */

GC
screen_gc(int color)
{
	return get_gc(ss, color | GC_NONDEFAULT);
}

GC
screen_invgc(int color)
{
	return get_gc(ss, INVERT_COLOR(color | GC_NONDEFAULT));
}
 
/*
 * Preallocate a set of graphics contexts for a given color.
 *
 * This logic is used only in pseudo-color mode.  In full color mode,
 * GCs are allocated dynamically by get_gc().
 */
static void
make_gc_set(struct sstate *s, int i, Pixel fg, Pixel bg)
{
	XGCValues xgcv;

	if (s->gc[i] != (GC)None)
		XtReleaseGC(toplevel, s->gc[i]);
	xgcv.foreground = fg;
	xgcv.background = bg;
	xgcv.graphics_exposures = True;
	xgcv.font = s->fid;
	if (s == &nss && !i)
		s->gc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont|GCGraphicsExposures,
		    &xgcv);
	else
		s->gc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	if (s->gc[NGCS + i] != (GC)None)
		XtReleaseGC(toplevel, s->gc[NGCS + i]);
	xgcv.foreground = bg;
	xgcv.background = fg;
	s->gc[NGCS + i] = XtGetGC(toplevel, GCForeground|GCBackground|GCFont,
	    &xgcv);
	if (!appres.mono) {
		if (s->selgc[i] != (GC)None)
			XtReleaseGC(toplevel, s->selgc[i]);
		xgcv.foreground = fg;
		xgcv.background = selbg_pixel;
		s->selgc[i] = XtGetGC(toplevel,
		    GCForeground|GCBackground|GCFont, &xgcv);
	}
}


/*
 * Convert an attribute to a color index.
 */
static int
fa_color(unsigned char fa)
{
	if (appres.m3279) {
		/*
		 * Color indices are the low-order 4 bits of a 3279 color
		 * identifier (0 through 15)
		 */
		if (appres.modified_sel && FA_IS_MODIFIED(fa))
			return GC_NONDEFAULT |
				(appres.modified_sel_color & 0xf);
		else if (appres.visual_select && FA_IS_SEL(fa))
			return GC_NONDEFAULT |
				(appres.visual_select_color & 0xf);
		else
			return field_colors[(fa >> 1) & 0x03];
	} else {
		/*
		 * Color indices are the intensity bits (0 through 2)
		 */
		if (FA_IS_ZERO(fa) ||
		    (appres.modified_sel && FA_IS_MODIFIED(fa)))
			return GC_NONDEFAULT | FA_INT_NORM_SEL;
		else
			return GC_NONDEFAULT | (fa & FA_INTENSITY);
	}
}



/*
 * Event handlers for toplevel FocusIn, FocusOut, KeymapNotify and
 * PropertyChanged events.
 */

static Boolean toplevel_focused = False;
static Boolean keypad_entered = False;

void
PA_Focus_action(Widget w unused, XEvent *event, String *params unused,
    Cardinal *num_params unused)
{
	XFocusChangeEvent *fe = (XFocusChangeEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_Focus_action, event, params, num_params);
#endif /*]*/
	switch (fe->type) {
	    case FocusIn:
		if (fe->detail != NotifyPointer) {
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

void
PA_EnterLeave_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params unused)
{
	XCrossingEvent *ce = (XCrossingEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_EnterLeave_action, event, params, num_params);
#endif /*]*/
	switch (ce->type) {
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

void
PA_KeymapNotify_action(Widget w unused, XEvent *event, String *params unused,
    Cardinal *num_params unused)
{
	XKeymapEvent *k = (XKeymapEvent *)event;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_KeymapNotify_action, event, params, num_params);
#endif /*]*/
	shift_event(state_from_keymap(k->key_vector));
}

static void
query_window_state(void)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long leftover;
	unsigned char *data = NULL;

	/* If using an active icon, the expose events are more reliable. */
	if (appres.active_icon)
		return;

	if (XGetWindowProperty(display, XtWindow(toplevel), a_state, 0L,
	    (long)BUFSIZ, False, a_state, &actual_type, &actual_format,
	    &nitems, &leftover, &data) != Success)
		return;
	if (actual_type == a_state && actual_format == 32) {
		if (*(unsigned long *)data == IconicState)
			iconic = True;
		else {
			iconic = False;
			invert_icon(False);
			keypad_first_up();
		}
	}
}

void
PA_StateChanged_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(PA_StateChanged_action, event, params, num_params);
	query_window_state();
}

/*
 * Handle Shift events (KeyPress and KeyRelease events, or KeymapNotify events
 * that occur when the mouse enters the window).
 */

void
shift_event(int event_state)
{
	static int old_state;
	Boolean shifted_now =
	    (event_state & (ShiftKeyDown | MetaKeyDown | AltKeyDown)) != 0;

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
screen_focus(Boolean in)
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
	if (in_focus && toggled(CURSOR_BLINK))
		schedule_cursor_blink();
}

/*
 * Change fonts.
 */
void
SetFont_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(SetFont_action, event, params, num_params);
	if (check_usage(SetFont_action, *num_params, 1, 1) < 0)
		return;
	screen_newfont(params[0], True);
}

void
screen_newfont(char *fontname, Boolean do_popup)
{
	const char *emsg;

	/* Can't change fonts in APL mode. */
	if (appres.apl_mode)
		return;

	if (efontname && !strcmp(fontname, efontname))
		return;

	if ((emsg = load_fixed_font(fontname))) {
		if (do_popup)
			popup_an_error("Font \"%s\"\n%s", fontname, emsg);
		return;
	}

	if (redo_old_font != CN)
		XtFree(redo_old_font);
	redo_old_font = XtNewString(efontname);
	screen_redo = REDO_FONT;

	screen_reinit(FONT_CHANGE);
	efont_changed = True;
}

/*
 * Load and query a font.
 * Returns NULL (okay) or an error message.
 */
const char *
load_fixed_font(const char *name)
{
	XFontStruct *f;
	Font ff;
	char **matches;
	int count;

	if (*name == '!')	/* backwards compatibility */
		name++;

	matches = XListFontsWithInfo(display, name, 1, &count, &f);
	if (matches == (char **)NULL)
		return "does not exist";
	ff = XLoadFont(display, matches[0]);
	set_font_globals(f, name, ff);
	XFreeFontInfo(matches, f, count);
	return CN;
}

/*
 * Set globals based on font name and info
 */
static void
set_font_globals(XFontStruct *f, const char *ef, Font ff)
{
	unsigned long svalue, svalue2;

	if (efontname)
		XtFree(efontname);
	efontname = XtNewString(ef);
	nss.char_width  = fCHAR_WIDTH(f);
	nss.char_height = fCHAR_HEIGHT(f);
	nss.fid = ff;
	nss.ascent = f->ascent;
	nss.descent = f->descent;
	if (XGetFontProperty(f, XA_FAMILY_NAME, &svalue))
		nss.standard_font = (Atom) svalue != a_3270;
	else if (!strncmp(efontname, "3270", 4))
		nss.standard_font = False;
	else
		nss.standard_font = True;
	if (nss.standard_font) {
		nss.extended_3270font = False;
		nss.latin1_font =
		    f->max_char_or_byte2 > 127 &&
		    XGetFontProperty(f, a_registry, &svalue) &&
		    (svalue == a_iso8859 || svalue == a_ISO8859) &&
		    XGetFontProperty(f, a_encoding, &svalue2) &&
		    svalue2 == a_1;
		nss.xfmap = nss.latin1_font ? cg2asc : cg2asc7;
		nss.debugging_font = False;
	} else {
#if defined(BROKEN_MACH32)
		nss.extended_3270font = False;
#else
		nss.extended_3270font = f->max_byte1 > 0 ||
			f->max_char_or_byte2 > 255;
#endif
		nss.latin1_font = False;
		nss.xfmap = (unsigned char *)NULL;
		nss.debugging_font = !strcmp(efontname, "3270d");
	}
}

/*
 * Font initialization.
 */
void
font_init(void)
{
	char *s, *label, *font;
	struct font_list *f;
	const char *ef, *emsg;

	/* Parse the fontMenuList resource. */
	if (!appres.font_list)
		xs_error("No %s resource", ResFontList);
	s = XtNewString(appres.font_list);
	while (split_dresource(&s, &label, &font) == 1) {
		f = (struct font_list *)XtMalloc(sizeof(*f));
		f->label = label;
		f->font = font;
		f->next = (struct font_list *)NULL;
		if (font_list)
			font_last->next = f;
		else
			font_list = f;
		font_last = f;
		font_count++;
	}
	if (!font_count)
		xs_error("Invalid %s resource", ResFontList);

	/* Now figure out which emulator font to load and load it. */
	if (appres.apl_mode) {
		/*
		 * APL mode overrides any explicit font selection, but if there
		 * isn't an APL font defined, APL mode is ignored.
		 */
		if ((appres.efontname = appres.afontname) == NULL) {
			xs_warning("No %s resource, ignoring APL mode",
			    ResAplFont);
			appres.apl_mode = False;
		}
	}

	/*
	 * If there's no explicit emulator font, take the first one off the
	 * menu list.
	 */
	if (!appres.efontname)
		appres.efontname = font_list->font;

	/*
	 * Try the user's selection, then the first off the menu list, then
	 * "fixed", then give up altogether.
	 */
	ef = appres.efontname;
	if ((emsg = load_fixed_font(ef)) != CN) {
		xs_warning("%s \"%s\" %s", ResEmulatorFont, ef, emsg);
		if (appres.apl_mode) {
			XtWarning("Ignoring APL mode");
			appres.apl_mode = False;
		}
		if (strcmp(ef, font_list->font)) {
			ef = font_list->font;
			emsg = load_fixed_font(ef);
			if (emsg != CN)
				xs_warning("%s \"%s\" %s", ResEmulatorFont, ef,
				    emsg);
		}
		if (emsg != CN) {
			ef = "fixed";
			if ((emsg = load_fixed_font(ef)) != CN)
				xs_error("%s \"%s\" %s", ResEmulatorFont, ef,
				    emsg);
		}
		xs_warning("Using font \"%s\"", ef);
	}
}

#if defined(X3270_MENUS) /*[*/
/*
 * Change models.
 */
void
screen_change_model(int mn, int ovc, int ovr)
{
	if (CONNECTED ||
	    (model_num == mn && ovc == ov_cols && ovr == ov_rows))
		return;

	redo_old_model = model_num;
	redo_old_ov_cols = ov_cols;
	redo_old_ov_rows = ov_rows;
	screen_redo = REDO_MODEL;

	model_changed = True;
	if (ov_cols != ovc || ov_rows != ovr)
		oversize_changed = True;
	set_rows_cols(mn, ovc, ovr);
	st_changed(ST_REMODEL, True);
	screen_reinit(MODEL_CHANGE);
}

/*
 * Change emulation modes.
 */
void
screen_extended(Boolean extended unused)
{
	set_rows_cols(model_num, ov_cols, ov_rows);
	model_changed = True;
}

void
screen_m3279(Boolean m3279 unused)
{
	destroy_pixels();
	screen_reinit(COLOR_CHANGE);
	set_rows_cols(model_num, ov_cols, ov_rows);
	model_changed = True;
}

/*
 * Change color schemes.  Alas, this is destructive if it fails.
 */
void
screen_newscheme(char *s)
{
	Boolean xferred;

	if (!appres.m3279)
		return;

	destroy_pixels();
	xferred = xfer_color_scheme(s, True);
	if (xferred)
		appres.color_scheme = s;
	screen_reinit(COLOR_CHANGE);
	scheme_changed = True;
}

/*
 * Change character sets.
 */
void
screen_newcharset(char *csname)
{
	if (!charset_init(csname))
		popup_an_error("Cannot find charset \"%s\"", csname);
	else {
		screen_reinit(CHARSET_CHANGE);
		charset_changed = True;
		popup_an_info("The new character set will only be reflected\n"
			"in new data from the host");
	}
}
#endif /*]*/

/*
 * Visual or not-so-visual bell
 */
void
ring_bell(void)
{
	static XGCValues xgcv;
	static GC bgc;
	static int initted;
	struct timeval tv;

	/* Ring the real display's bell. */
	if (!appres.visual_bell)
		XBell(display, appres.bell_volume);

	/* If we're iconic, invert the icon and return. */
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
#if defined(SELECT_INT) /*[*/
	(void) select(0, (int *)0, (int *)0, (int *)0, &tv);
#else /*][*/
	(void) select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
#endif /*]*/
	XFillRectangle(display, ss->window, bgc,
	    0, 0, ss->screen_width, ss->screen_height);
	XSync(display, 0);
}

/*
 * Window deletion
 */
void
PA_WMProtocols_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params)
{
	XClientMessageEvent *cme = (XClientMessageEvent *)event;

	action_debug(PA_WMProtocols_action, event, params, num_params);
	if ((Atom)cme->data.l[0] == a_delete_me) {
		if (w == toplevel)
			x3270_exit(0);
		else
			XtPopdown(w);
	} else if ((Atom)cme->data.l[0] == a_save_yourself && w == toplevel) {
		save_yourself();
	}
}


/* Initialize the icon. */
void
icon_init(void)
{
	icon = XCreateBitmapFromData(display, root_window,
	    (char *) x3270_bits, x3270_width, x3270_height);

	if (appres.active_icon) {
		Dimension iw, ih;

		aicon_font_init();
		aicon_size(&iw, &ih);
		icon_shell =  XtVaAppCreateShell(
		    "x3270icon",
		    "X3270",
		    overrideShellWidgetClass,
		    display,
		    XtNwidth, iw,
		    XtNheight, ih,
		    XtNmappedWhenManaged, False,
		    NULL);
		XtRealizeWidget(icon_shell);
		XtVaSetValues(toplevel,
		    XtNiconWindow, XtWindow(icon_shell),
		    NULL);
		if (appres.active_icon) {
			XtVaSetValues(icon_shell,
			    XtNbackground, appres.mono ? appres.background
						       : colorbg_pixel,
			    NULL);
		}
	} else {
		unsigned i;

		for (i = 0; i < sizeof(x3270_bits); i++)
			x3270_bits[i] = ~x3270_bits[i];
		inv_icon = XCreateBitmapFromData(display, root_window,
		    (char *) x3270_bits, x3270_width, x3270_height);
		wait_icon = XCreateBitmapFromData(display, root_window,
		    (char *) wait_bits, wait_width, wait_height);
		for (i = 0; i < sizeof(wait_bits); i++)
			wait_bits[i] = ~wait_bits[i];
		inv_wait_icon = XCreateBitmapFromData(display, root_window,
		    (char *) wait_bits, wait_width, wait_height);
		XtVaSetValues(toplevel,
		    XtNiconPixmap, icon,
		    XtNiconMask, icon,
		    NULL);
	}
}

/*
 * Initialize the active icon font information.
 */
static void
aicon_font_init(void)
{
	XFontStruct *f;
	Font ff;
	char **matches;
	int count;

	if (!appres.active_icon) {
		appres.label_icon = False;
		return;
	}

	matches = XListFontsWithInfo(display, appres.icon_font, 1, &count, &f);
	if (matches == (char **)NULL) {
		xs_warning("No font %s \"%s\"; activeIcon will not work",
		    ResIconFont, appres.icon_font);
		appres.active_icon = False;
		return;
	}
	ff = XLoadFont(display, matches[0]);
	iss.char_width = fCHAR_WIDTH(f);
	iss.char_height = fCHAR_HEIGHT(f);
	iss.fid = ff;
	iss.ascent = f->ascent;
	XFreeFontInfo(matches, f, count);
	iss.overstrike = False;
	iss.standard_font = True;
	iss.extended_3270font = False;
	iss.latin1_font = False;
	iss.debugging_font = False;
	iss.obscured = True;
	iss.xfmap = cg2asc7;
	if (appres.label_icon) {
		matches = XListFontsWithInfo(display, appres.icon_label_font,
		    1, &count, &ailabel_font);
		if (matches == (char **)NULL) {
			xs_warning("Cannot load %s \"%s\" font; labelIcon will not work",
			    ResIconLabelFont, appres.icon_label_font);
			appres.label_icon = False;
			return;
		}
		ailabel_font->fid = XLoadFont(display, matches[0]);
		aicon_label_height = fCHAR_HEIGHT(ailabel_font) + 2;
	}
}

/*
 * Determine the current size of the active icon.
 */
static void
aicon_size(Dimension *iw, Dimension *ih)
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
 * Initialize the active icon.  Assumes that aicon_font_init has already been
 * called.
 */
static void
aicon_init(void)
{
	if (!appres.active_icon)
		return;

	iss.widget = icon_shell;
	iss.window = XtWindow(iss.widget);
	iss.cursor_daddr = 0;
	iss.exposed_yet = False;
	if (appres.label_icon) {
		XGCValues xgcv;

		xgcv.font = ailabel_font->fid;
		xgcv.foreground = appres.foreground;
		xgcv.background = appres.background;
		ailabel_gc = XtGetGC(toplevel,
		    GCFont|GCForeground|GCBackground,
		    &xgcv);
	}
}

/*
 * Reinitialize the active icon.
 */
static void
aicon_reinit(unsigned cmask)
{
	if (!appres.active_icon)
		return;

	if (cmask & (FONT_CHANGE | COLOR_CHANGE))
		make_gcs(&iss);

	if (cmask & MODEL_CHANGE) {
		aicon_size(&iss.screen_width, &iss.screen_height);
		if (iss.image)
			XtFree((char *) iss.image);
		iss.image = (union sp *)
		    XtMalloc(sizeof(union sp) * maxROWS * maxCOLS);
		XtVaSetValues(iss.widget,
		    XtNwidth, iss.screen_width,
		    XtNheight, iss.screen_height,
		    NULL);
	}
	if (cmask & (MODEL_CHANGE | FONT_CHANGE | COLOR_CHANGE))
		(void) memset((char *)iss.image, 0,
			      sizeof(union sp) * maxROWS * maxCOLS);
}

/* Draw the aicon label */
static void
draw_aicon_label(void)
{
	int len;
	Position x;

	if (!appres.label_icon || !iconic)
		return;

	XFillRectangle(display, iss.window,
	    get_gc(&iss, INVERT_COLOR(0)),
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
set_aicon_label(char *l)
{
	if (aicon_text)
		XtFree(aicon_text);
	aicon_text = XtNewString(l);
	draw_aicon_label();
}

/* Change the bitmap icon. */
static void
flip_icon(Boolean inverted, enum mcursor_state mstate)
{
	Pixmap p = icon;
	
	if (mstate == LOCKED)
		mstate = NORMAL;
	if (appres.active_icon
	    || (inverted == icon_inverted && mstate == icon_cstate))
		return;
	switch (mstate) {
	    case WAIT:
		if (inverted)
			p = inv_wait_icon;
		else
			p = wait_icon;
		break;
	    case LOCKED:
	    case NORMAL:
		if (inverted)
			p = inv_icon;
		else
			p = icon;
		break;
	}
	XtVaSetValues(toplevel,
	    XtNiconPixmap, p,
	    XtNiconMask, p,
	    NULL);
	icon_inverted = inverted;
	icon_cstate = mstate;
}

/*
 * Invert the icon.
 */
static void
invert_icon(Boolean inverted)
{
	flip_icon(inverted, icon_cstate);
}

/*
 * Change to the lock icon.
 */
static void
lock_icon(enum mcursor_state state)
{
	flip_icon(icon_inverted, state);
}


/*
 * Resize font list parser.
 */
static void
init_rsfonts(void)
{
	char *s;
	char *name;
	struct rsfont *r;

	/* Can't resize in APL mode. */
	if (appres.apl_mode)
		return;

	if ((s = get_resource(ResResizeFontList)) == CN)
		return;

	s = XtNewString(s);
	while (split_lresource(&s, &name) == 1) {
		XFontStruct *f;
		char **matches;
		int count;

		matches = XListFontsWithInfo(display, name, 1, &count, &f);
		if (matches == (char **)NULL)
			continue;
		r = (struct rsfont *)XtMalloc(sizeof(*r));
		r->name = name;
		r->width = fCHAR_WIDTH(f);
		r->height = fCHAR_HEIGHT(f);
		XFreeFontInfo(matches, f, count);
		r->next = rsfonts;
		rsfonts = r;
	}
}

/*
 * Handle Configure events.
 */

/*
 * Timeout routine called 0.5 sec after PA_ConfigureNotify_action changes to a
 * new font.  This function clears configure_ticking; if
 * PA_ConfigureNotify_action is called again before this, it interprets it as
 * the window manager objecting to the new window size and posts an error
 * message.
 */
static void
configure_stable(XtPointer closure unused, XtIntervalId *id unused)
{
	configure_ticking = False;
}

void
PA_ConfigureNotify_action(Widget w unused, XEvent *event, String *params unused,
    Cardinal *num_params unused)
{
	XConfigureEvent *re = (XConfigureEvent *) event;
	Boolean needs_moving = False;
	Position xx, yy;
	static Position main_x = 0, main_y = 0;
	int current_area;
	int requested_area;
	struct rsfont *r;
	struct rsfont *smallest = (struct rsfont *) NULL;
	struct rsfont *largest = (struct rsfont *) NULL;
	struct rsfont *best = (struct rsfont *) NULL;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_ConfigureNotify_action, event, params, num_params);
#endif /*]*/

	/*
	 * Get the new window coordinates.  If the configure event reports it
	 * as (0,0), ask for it explicitly.
	 */
	if (re->x || re->y) {
		xx = re->x;
		yy = re->y;
	} else {
		XtVaGetValues(toplevel, XtNx, &xx, XtNy, &yy, NULL);
	}

	/* Save the new coordinates in globals for next time. */
	if (xx != main_x || yy != main_y) {
		main_x = re->x;
		main_y = re->y;
		needs_moving = True;
	}

	/* If the window dimensions are "correct", there is nothing to do. */
	if (re->width == main_width && re->height == main_height) {
		configure_first = False;
		goto done;
	}

	/*
	 * There are three possible reasons for being here:
	 * (1) The user has (through the window manager) resized our
	 *  window.
	 * (2) We resized our window when our configuration changed (font,
	 *  model, integral keypad or scrollbar); mwm thinks it is too big
	 *  and scaled it back.
	 * (3) Mwm thinks our initial screen size is too big, and scaled it
	 *  back.
	 *
	 * 'configure_ticking' and the associated timeout attempt to detect
	 * (2) and (3).  Whenever we set the toplevel dimensions, we set
	 * the timer; if a changed configure comes in while the timer is
	 * ticking, we assume it is an angry mwm.  If this is the very first
	 * time we've sized the window, we must crash; otherwise, we can
	 * simply back off to the previous configuration.
	 *
	 * If 'configure_ticking' is clear, we assume the cause is (1).
	 * If resize fonts are available, we pick the best one and try it.
	 * Otherwise, we restore the original toplevel dimensions.
	 */

	/* If this was our first resize, crash. */
	if (configure_first)
		xs_error("This x3270 configuration does not fit on the X display.");

	if (configure_ticking) {
		/*
		 * We must presume this is the window manager disapproving of
		 * our last resize attempt.
		 */
		XtRemoveTimeOut(configure_id);
		configure_ticking = False;
		popup_an_error("This configuration does not fit on the X display");

		/* Restore the old configuration... */
		switch (screen_redo) {
		    case REDO_FONT:
			screen_newfont(redo_old_font, False);
			break;
#if defined(X3270_MENUS) /*[*/
		    case REDO_MODEL:
			screen_change_model(redo_old_model,
			    redo_old_ov_cols, redo_old_ov_rows);
			break;
#endif /*]*/
#if defined(X3270_KEYPAD) /*[*/
		    case REDO_KEYPAD:
			screen_showikeypad(appres.keypad_on = False);
			break;
#endif /*]*/
		    case REDO_SCROLLBAR:
			if (toggled(SCROLL_BAR)) {
				toggle_toggle(&appres.toggle[SCROLL_BAR]);
				toggle_scrollBar(&appres.toggle[SCROLL_BAR],
				    TT_TOGGLE);
			}
			break;
		    case REDO_NONE:
		    default:
			xs_error("Internal reconfiguration error.");
			break;
		}
		screen_redo = REDO_NONE;

		goto done;
	}

	/*
	 * User-generated resize attempt.
	 */
	if (rsfonts == (struct rsfont *)NULL || !appres.allow_resize) {
		/* Illegal or impossible. */
		set_toplevel_sizes();
		goto done;
	}

	/*
	 * Recompute the resulting screen area for each font, based on the
	 * current keypad, model, and scrollbar settings.
	 */
	for (r = rsfonts; r != (struct rsfont *) NULL; r = r->next) {
		Dimension cw, ch;	/* container_width, container_height */

		cw = SCREEN_WIDTH(r->width)+2 + scrollbar_width;
#if defined(X3270_KEYPAD) /*[*/
		{
			Dimension mkw;

			mkw = min_keypad_width();
			if (kp_placement == kp_integral
			    && appres.keypad_on
			    && cw < mkw)
				cw = mkw;
		}

#endif /*]*/
		ch = SCREEN_HEIGHT(r->height)+2 + menubar_qheight(cw);
#if defined(X3270_KEYPAD) /*[*/
		if (kp_placement == kp_integral && appres.keypad_on)
			ch += keypad_qheight();
#endif /*]*/
		r->total_width = cw;
		r->total_height = ch;
		r->area = cw * ch;
	}

	/*
	 * Find the the best match for requested dimensions.
	 *
	 * If they tried to grow the screen, find the smallest font, larger
	 * than the current size, that is closest to the requested size.
	 *
	 * If they tried to shrink the screen, find the largest font, smaller
	 * than the current size, that is closest to the requested size.
	 *
	 * If they are asking for the same area (even with different
	 * proportions), do nothing.
	 */

	current_area = main_width * main_height;
	requested_area = re->width * re->height;
	if (current_area == requested_area) {
		set_toplevel_sizes();
		goto done;
	}
	smallest = (struct rsfont *) NULL;
	largest = (struct rsfont *) NULL;
	best = (struct rsfont *) NULL;

#define aabs(x)	\
	    (((x) < 0) ? -(x) : (x))
#define closer_area(this, prev, want) \
	    (aabs(this->area - want) < aabs(prev->area - want))

	for (r = rsfonts; r != (struct rsfont *) NULL; r = r->next) {
		if (!smallest || r->area < smallest->area)
			smallest = r;
		if (!largest || r->area > largest->area)
			largest = r;
		if (requested_area > current_area) {
			if (r->area <= current_area)
				continue;
			if (!best || closer_area(r, best, requested_area))
				best = r;
		} else {
			if (r->area >= current_area)
				continue;
			if (!best || closer_area(r, best, requested_area))
				best = r;
		}
	}

	if (best == (struct rsfont *) NULL) {	/* no good fit */
		if (requested_area > current_area) {
			best = largest;
		} else {
			best = smallest;
		}
	}

	/* Change fonts. */
	if (efontname && !strcmp(best->name, efontname)) {
		trace_event(" keeping font '%s' (%dx%d)\n",
		    best->name, best->total_width, best->total_height);
		set_toplevel_sizes();
	} else {
		trace_event(" switching to font '%s' (%dx%d)\n",
		    best->name, best->total_width, best->total_height);
		screen_newfont(best->name, False);
	}

    done:
	if (needs_moving && !iconic)
		keypad_move();
}

/*
 * Process a VisibilityNotify event, setting the 'visibile' flag in nss.
 * This will switch the behavior of screen scrolling.
 */
void
PA_VisibilityNotify_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params unused)
{
	XVisibilityEvent *e;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_VisibilityNotify_action, event, params, num_params);
#endif /*]*/
	e = (XVisibilityEvent *)event;
	nss.obscured = (e->state != VisibilityUnobscured);
}

/*
 * Process a GraphicsExpose event, refreshing the screen if we have done
 * one or more failed XCopyArea calls.
 */
void
PA_GraphicsExpose_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params unused)
{
	int i;

#if defined(INTERNAL_ACTION_DEBUG) /*[*/
	action_debug(PA_GraphicsExpose_action, event, params, num_params);
#endif /*]*/

	if (nss.copied) {
		/*
		 * Force a screen redraw.
		 */
		(void) memset((char *) ss->image, 0,
		              (maxROWS*maxCOLS) * sizeof(union sp));
		if (ss->debugging_font)
			for (i = 0; i < maxROWS*maxCOLS; i++)
				ss->image[i].bits.cg = CG_space;
		ctlr_changed(0, ROWS*COLS);
		cursor_changed = True;

		nss.copied = False;
	}
}

/* Display size functions. */
unsigned
display_width(void)
{
	return XDisplayWidth(display, default_screen);
}

unsigned
display_widthMM(void)
{
	return XDisplayWidthMM(display, default_screen);
}

unsigned
display_height(void)
{
	return XDisplayHeight(display, default_screen);
}

unsigned
display_heightMM(void)
{
	return XDisplayHeightMM(display, default_screen);
}
