/*
 * Copyright 1993, 1994, 1995, 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	menubar.c
 *		This module handles the menu bar.
 */

#include "globals.h"

#if defined(X3270_MENUS) /*[*/

#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/Dialog.h>
#include "Husk.h"
#include "CmplxMenu.h"
#include "CmeBSB.h"
#include "CmeLine.h"

#include "appres.h"
#include "objects.h"
#include "resources.h"

#include "actionsc.h"
#include "aboutc.h"
#include "ftc.h"
#include "hostc.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "printc.h"
#include "savec.h"
#include "screenc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "utilc.h"
#include "xioc.h"

#define MACROS_MENU	"macrosMenu"

extern Widget		keypad_shell;
extern int		linemode;
extern Boolean		keypad_popped;

static struct scheme {
	char *label;
	char *scheme;
	struct scheme *next;
} *schemes, *last_scheme;
static int scheme_count;
static Widget  *scheme_widgets;

static struct charset {
	char *label;
	char *charset;
	struct charset *next;
} *charsets, *last_charset;
static int charset_count;
static Widget  *charset_widgets;

static void scheme_init(void);
static void charsets_init(void);
static void options_menu_init(Boolean regen, Position x, Position y);
#if 0
static void help_button_init(Boolean regen, Position x, Position y);
#endif
static void keypad_button_init(Position x, Position y);
static void connect_menu_init(Boolean regen, Position x, Position y);
static void reconnect_menu_init(Boolean regen, Position x, Position y);
static void macros_menu_init(Boolean regen, Position x, Position y);
static void file_menu_init(Boolean regen, Dimension x, Dimension y);
static void Bye(Widget w, XtPointer client_data, XtPointer call_data);
static void menubar_in3270(Boolean in3270);
static void menubar_linemode(Boolean linemode);
static void menubar_connect(Boolean ignored);

#include "dot.bm"
#include "arrow.bm"
#include "diamond.bm"
#include "no_diamond.bm"
#include "ky.bm"
#include "null.bm"


/*
 * Menu Bar
 */

static Widget	menu_parent;
static Boolean  menubar_buttons;
static Widget   disconnect_button;
static Widget   exit_button;
static Widget   exit_menu;
static Widget   macros_button;
static Widget	ft_button;
static Widget   connect_button;
static Widget   keypad_button;
static Widget   linemode_button;
static Widget   charmode_button;
static Widget   models_option;
static Widget   model_2_button;
static Widget   model_3_button;
static Widget   model_4_button;
static Widget   model_5_button;
static Widget	oversize_button;
static Widget	extended_button;
static Widget	m3278_button;
static Widget	m3279_button;
static Widget   keypad_option_button;
static Widget	script_abort_button;
static Widget	scheme_button;
static Widget   connect_menu;

static Pixmap   arrow;
Pixmap	dot;
Pixmap	diamond;
Pixmap	no_diamond;
Pixmap	null;

static int	n_bye;

static void     toggle_init();

#define TOP_MARGIN	3
#define BOTTOM_MARGIN	3
#define LEFT_MARGIN	3
#define KEY_HEIGHT	18
#define KEY_WIDTH	70
#define BORDER		1
#define SPACING		3

#define MO_OFFSET	1
#define CN_OFFSET	2

#define MENU_BORDER	2

#define	MENU_MIN_WIDTH	(LEFT_MARGIN + 3*(KEY_WIDTH+2*BORDER+SPACING) + \
			 LEFT_MARGIN + (ky_width+8) + 2*BORDER + SPACING + \
			 2*MENU_BORDER)

/*
 * Compute the potential height of the menu bar.
 */
Dimension
menubar_qheight(Dimension container_width)
{
	if (!appres.menubar || container_width < (unsigned) MENU_MIN_WIDTH)
		return 0;
	else
		return TOP_MARGIN + KEY_HEIGHT+2*BORDER + BOTTOM_MARGIN +
			2*MENU_BORDER;
}

/*
 * Initialize the menu bar.
 */
void
menubar_init(Widget container, Dimension overall_width, Dimension current_width)
{
	static Widget menu_bar;
	static Boolean ever = False;
	Boolean mb_old;
	Dimension height;

	if (!ever) {

		scheme_init();
		charsets_init();
		XtRegisterGrabAction(HandleMenu_action, True,
		    (ButtonPressMask|ButtonReleaseMask),
		    GrabModeAsync, GrabModeAsync);

		/* Create bitmaps. */
		dot = XCreateBitmapFromData(display, root_window,
		    (char *) dot_bits, dot_width, dot_height);
		arrow = XCreateBitmapFromData(display, root_window,
		    (char *) arrow_bits, arrow_width, arrow_height);
		diamond = XCreateBitmapFromData(display, root_window,
		    (char *) diamond_bits, diamond_width, diamond_height);
		no_diamond = XCreateBitmapFromData(display, root_window,
		    (char *) no_diamond_bits, no_diamond_width,
			no_diamond_height);
		null = XCreateBitmapFromData(display, root_window,
		    (char *) null_bits, null_width, null_height);

		/* Register interest in state transtions. */
		register_schange(ST_3270_MODE, menubar_in3270);
		register_schange(ST_LINE_MODE, menubar_linemode);
		register_schange(ST_HALF_CONNECT, menubar_connect);
		register_schange(ST_CONNECT, menubar_connect);

		ever = True;
	}

	height = menubar_qheight(current_width);
	mb_old = menubar_buttons;
	menubar_buttons = (height != 0);
	if (menubar_buttons) {
		if (menu_bar == (Widget)NULL) {
			/* Create the menu bar. */
			menu_bar = XtVaCreateManagedWidget(
			    "menuBarContainer", huskWidgetClass, container,
			    XtNborderWidth, MENU_BORDER,
			    XtNwidth, overall_width - 2*MENU_BORDER,
			    XtNheight, height - 2*MENU_BORDER,
			    NULL);
		} else {
			/* Resize and map the menu bar. */
			XtVaSetValues(menu_bar,
			    XtNborderWidth, MENU_BORDER,
			    XtNwidth, overall_width - 2*MENU_BORDER,
			    NULL);
			XtMapWidget(menu_bar);
		}
		menu_parent = menu_bar;
	} else if (menu_bar != (Widget)NULL) {
		/* Hide the menu bar. */
		XtUnmapWidget(menu_bar);
		menu_parent = container;
	}

	/* "File..." menu */

	file_menu_init(mb_old != menubar_buttons, LEFT_MARGIN, TOP_MARGIN);

	/* "Options..." menu */

	options_menu_init(mb_old != menubar_buttons,
	    LEFT_MARGIN + MO_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* "Connect..." menu */

	if (appres.reconnect)
		reconnect_menu_init(mb_old != menubar_buttons,
		    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
		    TOP_MARGIN);
	else
		connect_menu_init(mb_old != menubar_buttons,
		    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
		    TOP_MARGIN);

	/* "Macros..." menu */

	macros_menu_init(mb_old != menubar_buttons,
	    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

#if 0
	/* "Help..." menu */

	help_button_init(mb_old != menubar_buttons,
	    (Position) (current_width - LEFT_MARGIN - (ky_width+8) -
			    2*BORDER - 2*MENU_BORDER -
			    SPACING - KEY_WIDTH - 2*BORDER),
	    TOP_MARGIN);
#endif

	/* Keypad button */

	keypad_button_init(
	    (Position) (current_width - LEFT_MARGIN - (ky_width+8) -
			    2*BORDER - 2*MENU_BORDER),
	    TOP_MARGIN);
}

/*
 * External entry points
 */

/*
 * Called when connected to or disconnected from a host.
 */
static void
menubar_connect(Boolean ignored)
{
	/* Set the disconnect button sensitivity. */
	if (disconnect_button != (Widget)NULL)
		XtVaSetValues(disconnect_button,
		    XtNsensitive, PCONNECTED,
		    NULL);

	/* Set up the exit button, either with a pullright or a callback. */
	if (exit_button != (Widget)NULL) {
		if (PCONNECTED) {
			/* Remove the immediate callback. */
			if (n_bye) {
				XtRemoveCallback(exit_button, XtNcallback,
				    Bye, 0);
				n_bye--;
			}

			/* Set pullright for extra confirmation. */
			XtVaSetValues(exit_button,
			    XtNrightBitmap, arrow,
			    XtNmenuName, "exitMenu",
			    NULL);
		} else {
			/* Install the immediate callback. */
			if (!n_bye) {
				XtAddCallback(exit_button, XtNcallback,
				    Bye, 0);
				n_bye++;
			}

			/* Remove the pullright. */
			XtVaSetValues(exit_button,
			    XtNrightBitmap, NULL,
			    XtNmenuName, NULL,
			    NULL);
		}
	}

	/* Set up the connect menu. */
	if (appres.reconnect) {
		if (!PCONNECTED &&
		    connect_button != (Widget)NULL &&
		    auto_reconnect_disabled) {
			XtMapWidget(connect_button);
		}
	} else {
		if (connect_menu != (Widget)NULL) {
			if (PCONNECTED && connect_button != (Widget)NULL)
				XtUnmapWidget(connect_button);
			else {
				connect_menu_init(True,
				    LEFT_MARGIN +
					CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
				    TOP_MARGIN);
				if (menubar_buttons)
					XtMapWidget(connect_button);
			}
		}
	}

	/* Set up the macros menu. */
	macros_menu_init(True,
	    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* Set up the various option buttons. */
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button, XtNsensitive, IN_ANSI, NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button, XtNsensitive, IN_ANSI, NULL);
#if defined(X3270_ANSI) /*[*/
	if (appres.toggle[LINE_WRAP].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[LINE_WRAP].w[0],
		    XtNsensitive, IN_ANSI,
		    NULL);
#endif /*]*/
	if (appres.toggle[RECTANGLE_SELECT].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[RECTANGLE_SELECT].w[0],
		    XtNsensitive, IN_ANSI,
		    NULL);
	if (models_option != (Widget)NULL)
		XtVaSetValues(models_option, XtNsensitive, !PCONNECTED, NULL);
	if (extended_button != (Widget)NULL)
		XtVaSetValues(extended_button, XtNsensitive, !PCONNECTED,
		    NULL);
	if (m3278_button != (Widget)NULL)
		XtVaSetValues(m3278_button, XtNsensitive, !PCONNECTED,
		    NULL);
	if (m3279_button != (Widget)NULL)
		XtVaSetValues(m3279_button, XtNsensitive, !PCONNECTED,
		    NULL);
}

/* External entry point to display the Reconnect button. */
void
menubar_show_reconnect(void)
{
	if (connect_button != (Widget)NULL)
		XtMapWidget(connect_button);
}

void
menubar_keypad_changed(void)
{
	if (keypad_option_button != (Widget)NULL)
		XtVaSetValues(keypad_option_button,
		    XtNleftBitmap,
			appres.keypad_on || keypad_popped ? dot : None,
		    NULL);
}

/* Called when we switch between ANSI and 3270 modes. */
static void
menubar_in3270(Boolean in3270)
{
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button,
		    XtNsensitive, !in3270,
		    XtNleftBitmap, in3270 ? no_diamond
					: (linemode ? diamond : no_diamond),
		    NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button,
		    XtNsensitive, !in3270,
		    XtNleftBitmap, in3270 ? no_diamond
					: (linemode ? no_diamond : diamond),
		    NULL);
#if defined(X3270_ANSI) /*[*/
	if (appres.toggle[LINE_WRAP].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[LINE_WRAP].w[0],
		    XtNsensitive, !in3270,
		    NULL);
#endif /*]*/
	if (appres.toggle[RECTANGLE_SELECT].w[0] != (Widget)NULL)
		XtVaSetValues(appres.toggle[RECTANGLE_SELECT].w[0],
		    XtNsensitive, !in3270,
		    NULL);
}

/* Called when we switch between ANSI line and character. */
static void
menubar_linemode(Boolean linemode)
{
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button,
		    XtNleftBitmap, linemode ? diamond : no_diamond,
		    NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button,
		    XtNleftBitmap, linemode ? no_diamond : diamond,
		    NULL);
}

/* Called to change the sensitivity of the "Abort Script" button. */
void
menubar_as_set(Boolean sensitive)
{
	if (script_abort_button != (Widget)NULL)
		XtVaSetValues(script_abort_button,
		    XtNsensitive, sensitive,
		    NULL);
}


/*
 * "File..." menu
 */
static Widget save_shell = (Widget) NULL;

/* Called from "Exit x3270" button on "File..." menu */
/*ARGSUSED*/
static void
Bye(Widget w, XtPointer client_data, XtPointer call_data)
{
	x3270_exit(0);
}

/* Called from the "Disconnect" button on the "File..." menu */
/*ARGSUSED*/
static void
disconnect(Widget w, XtPointer client_data, XtPointer call_data)
{
	host_disconnect(False);
}

/* Called from the "Abort Script" button on the "File..." menu */
/*ARGSUSED*/
static void
script_abort_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	abort_script();
}

/* "About x3270" popup */
/*ARGSUSED*/
static void
show_about(Widget w, XtPointer userdata, XtPointer calldata)
{
	popup_about();
}

/* Called from the "Save" button on the save options dialog */
/*ARGSUSED*/
static void
save_button_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!save_options(s))
		XtPopdown(save_shell);
}

/* Called from the "Save Options in File" button on the "File..." menu */
/*ARGSUSED*/
static void
do_save_options(Widget w, XtPointer client_data, XtPointer call_data)
{
	extern char *profile_name;

	if (save_shell == NULL)
		save_shell = create_form_popup("SaveOptions",
		    save_button_callback, (XtCallbackProc)NULL, False);
	XtVaSetValues(XtNameToWidget(save_shell, ObjDialog),
	    XtNvalue, profile_name,
	    NULL);
	popup_popup(save_shell, XtGrabExclusive);
}

static void
file_menu_init(Boolean regen, Dimension x, Dimension y)
{
	static Widget file_menu = (Widget)NULL;
	Widget w;

	if (regen && (file_menu != (Widget)NULL)) {
		XtDestroyWidget(file_menu);
		file_menu = (Widget)NULL;
	}
	if (file_menu != (Widget)NULL)
		return;

	file_menu = XtVaCreatePopupShell(
	    "fileMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    file_menu, NULL);

	/* About x3270... */
	w = XtVaCreateManagedWidget(
	    "aboutOption", cmeBSBObjectClass, file_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, show_about, NULL);

#if defined(X3270_FT) /*[*/
	/* File Transfer */
	if (!appres.secure) {
		(void) XtCreateManagedWidget(
		    "space", cmeLineObjectClass, file_menu,
		    NULL, 0);
		ft_button = XtVaCreateManagedWidget(
		    "ftOption", cmeBSBObjectClass, file_menu,
		    XtNsensitive, IN_3270,
		    NULL);
		XtAddCallback(ft_button, XtNcallback, popup_ft, NULL);
	}
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
	/* Trace Data Stream
	   Trace X Events
	   Save Screen(s) in File */
	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, file_menu,
	    NULL);
	if (appres.debug_tracing) {
		toggle_init(file_menu, DS_TRACE, "dsTraceOption", CN);
		toggle_init(file_menu, EVENT_TRACE, "eventTraceOption", CN);
	}
	toggle_init(file_menu, SCREEN_TRACE, "screenTraceOption", CN);
#endif /*]*/

	/* Print Screen Text */
	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, file_menu,
	    NULL);
	w = XtVaCreateManagedWidget(
	    "printTextOption", cmeBSBObjectClass, file_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, print_text_option, NULL);

	/* Print Window Bitmap */
	w = XtVaCreateManagedWidget(
	    "printWindowOption", cmeBSBObjectClass, file_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, print_window_option, NULL);

	if (!appres.secure) {

		/* Save Options */
		(void) XtVaCreateManagedWidget(
		    "space", cmeLineObjectClass, file_menu,
		    NULL);
		w = XtVaCreateManagedWidget(
		    "saveOption", cmeBSBObjectClass, file_menu,
		    NULL);
		XtAddCallback(w, XtNcallback, do_save_options, NULL);

		/* Execute an action */
		(void) XtVaCreateManagedWidget(
		    "space", cmeLineObjectClass, file_menu,
		    NULL);
		w = XtVaCreateManagedWidget(
		    "executeActionOption", cmeBSBObjectClass, file_menu,
		    NULL);
		XtAddCallback(w, XtNcallback, execute_action_option, NULL);
	}

	/* Abort script */
	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, file_menu,
	    NULL);
	script_abort_button = XtVaCreateManagedWidget(
	    "abortScriptOption", cmeBSBObjectClass, file_menu,
	    XtNsensitive, sms_active(),
	    NULL);
	XtAddCallback(script_abort_button, XtNcallback, script_abort_callback,
	    0);

	/* Disconnect */
	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, file_menu,
	    NULL);
	disconnect_button = XtVaCreateManagedWidget(
	    "disconnectOption", cmeBSBObjectClass, file_menu,
	    XtNsensitive, PCONNECTED,
	    NULL);
	XtAddCallback(disconnect_button, XtNcallback, disconnect, 0);

	/* Exit x3270 */
	if (exit_menu != (Widget)NULL)
		XtDestroyWidget(exit_menu);
	exit_menu = XtVaCreatePopupShell(
	    "exitMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	w = XtVaCreateManagedWidget(
	    "exitReallyOption", cmeBSBObjectClass, exit_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, Bye, 0);
	exit_button = XtVaCreateManagedWidget(
	    "exitOption", cmeBSBObjectClass, file_menu,
	    NULL);
	XtAddCallback(exit_button, XtNcallback, Bye, 0);
	n_bye = 1;

	/* File... */
	if (menubar_buttons) {
		w = XtVaCreateManagedWidget(
		    "fileMenuButton", menuButtonWidgetClass, menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmenuName, "fileMenu",
		    NULL);
	}
}


/*
 * "Connect..." menu
 */

static Widget connect_shell = NULL;

/* Called from each button on the "Connect..." menu */
/*ARGSUSED*/
static void
host_connect_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	(void) host_connect(client_data);
}

/* Called from the lone "Connect" button on the connect dialog */
/*ARGSUSED*/
static void
connect_button_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!host_connect(s))
		XtPopdown(connect_shell);
}

/* Called from the "Other..." button on the "Connect..." menu */
/*ARGSUSED*/
static void
do_connect_popup(Widget w, XtPointer client_data, XtPointer call_data)
{
	if (connect_shell == NULL)
		connect_shell = create_form_popup("Connect",
		    connect_button_callback, (XtCallbackProc)NULL, False);
	popup_popup(connect_shell, XtGrabExclusive);
}

/* Called from the "Reconnect" button */
/*ARGSUSED*/
static void
do_reconnect(Widget w, XtPointer client_data, XtPointer call_data)
{
	host_reconnect();
}

/*
 * Initialize the "Connect..." menu
 */
static void
connect_menu_init(Boolean regen, Position x, Position y)
{
	Widget w;
	int n_hosts = 0;
	Boolean any_hosts = False;
	struct host *h;
	Boolean need_line = False;

	if (regen && (connect_menu != (Widget)NULL)) {
		XtDestroyWidget(connect_menu);
		connect_menu = (Widget)NULL;
		if (connect_button != (Widget)NULL) {
			XtDestroyWidget(connect_button);
			connect_button = (Widget)NULL;
		}
	}
	if (connect_menu != (Widget)NULL)
		return;

	/* Create the menu */
	connect_menu = XtVaCreatePopupShell(
	    "hostMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		need_line = True;

	/* Start off with an opportunity to reconnect */

	if (current_host != CN) {
		char *buf;

		if (need_line)
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass, connect_menu, NULL);
		buf = xs_buffer("%s %s", get_message("reconnect"),
		    current_host);
		w = XtVaCreateManagedWidget(
		    buf, cmeBSBObjectClass, connect_menu, 
		    NULL);
		XtFree(buf);
		XtAddCallback(w, XtNcallback, do_reconnect, PN);
		need_line = True;
		n_hosts++;
	}

	/* Walk the host list from the file to produce the host menu */

	for (h = hosts; h; h = h->next) {
		if (h->entry_type != PRIMARY)
			continue;
		if (need_line && !any_hosts)
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass, connect_menu, NULL);
		any_hosts = True;
		w = XtVaCreateManagedWidget(
		    h->name, cmeBSBObjectClass, connect_menu, 
		    NULL);
		XtAddCallback(w, XtNcallback, host_connect_callback,
		    XtNewString(h->name));
		n_hosts++;
	}
	if (any_hosts)
		need_line = True;

	/* Add an "Other..." button at the bottom */

	if (!any_hosts || !appres.no_other) {
		if (need_line)
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass,
			    connect_menu, NULL);
		w = XtVaCreateManagedWidget(
		    "otherHostOption", cmeBSBObjectClass, connect_menu,
		    NULL);
		XtAddCallback(w, XtNcallback, do_connect_popup, NULL);
	}

	/* Add the "Connect..." button itself to the menu_parent. */

	if (menubar_buttons) {
		if (n_hosts) {
			/* Connect button pops up a menu. */
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", menuButtonWidgetClass,
			    menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmenuName, "hostMenu",
			    XtNmappedWhenManaged, !PCONNECTED,
			    NULL);
		} else {
			/* Connect button pops up a dialog. */
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", commandWidgetClass,
			    menu_parent,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmappedWhenManaged, !PCONNECTED,
			    NULL);
			XtAddCallback(connect_button, XtNcallback,
			    do_connect_popup, NULL);
		}
	}
}

/*
 * Initialize the "Reconnect..." menu (used only with the -reconnect option)
 */
static void
reconnect_menu_init(Boolean regen, Position x, Position y)
{
	if (regen) {
		if (connect_menu != (Widget)NULL) {
			XtDestroyWidget(connect_menu);
			connect_menu = (Widget)NULL;
		}
		if (connect_button != (Widget)NULL) {
			XtDestroyWidget(connect_button);
			connect_button = (Widget)NULL;
		}
	}

	if (connect_menu != (Widget)NULL || connect_button != (Widget)NULL)
		return;

	if (menubar_buttons) {
		/* Create the button on the menu bar. */
		connect_button = XtVaCreateManagedWidget(
		    "reconnectButton", commandWidgetClass,
		    menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmappedWhenManaged,
			!PCONNECTED && auto_reconnect_disabled,
		    NULL);
		XtAddCallback(connect_button, XtNcallback, do_reconnect, NULL);
	} else {
		char *buf;
		Widget w;

		/* Create a menu to pop up with the mouse. */
		connect_menu = XtVaCreatePopupShell(
		    "hostMenu", complexMenuWidgetClass, menu_parent,
		    menubar_buttons ? XtNlabel : NULL, NULL,
		    NULL);
		buf = xs_buffer("%s %s", get_message("reconnect"),
		    current_host);
		w = XtVaCreateManagedWidget(
		    buf, cmeBSBObjectClass, connect_menu, 
		    NULL);
		XtFree(buf);
		XtAddCallback(w, XtNcallback, do_reconnect, PN);
	}
}

/*
 * Callback for macros
 */
/*ARGSUSED*/
static void
do_macro(Widget w, XtPointer client_data, XtPointer call_data)
{
	macro_command((struct macro_def *)client_data);
}

/*
 * Initialize the "Macros..." menu
 */
static void
macros_menu_init(Boolean regen, Position x, Position y)
{
	static Widget macros_menu;
	Widget w;
	struct macro_def *m;
	Boolean any = False;

	if (regen && (macros_menu != (Widget)NULL)) {
		XtDestroyWidget(macros_menu);
		macros_menu = (Widget)NULL;
		if (macros_button != (Widget)NULL) {
			XtDestroyWidget(macros_button);
			macros_button = (Widget)NULL;
		}
	}
	if (macros_menu != (Widget)NULL || !PCONNECTED)
		return;

	/* Walk the list */

	macros_init();	/* possibly different for each host */
	for (m = macro_defs; m; m = m->next) {
		if (!any) {
			/* Create the menu */
			macros_menu = XtVaCreatePopupShell(
			    MACROS_MENU, complexMenuWidgetClass, menu_parent,
			    menubar_buttons ? XtNlabel : NULL, NULL,
			    NULL);
			if (!menubar_buttons)
				(void) XtVaCreateManagedWidget("space",
				    cmeLineObjectClass, macros_menu, NULL);
		}
		w = XtVaCreateManagedWidget(
		    m->name, cmeBSBObjectClass, macros_menu, 
		    NULL);
		XtAddCallback(w, XtNcallback, do_macro, (XtPointer)m);
		any = True;
	}

	/* Add the "Macros..." button itself to the menu_parent */

	if (any && menubar_buttons)
		macros_button = XtVaCreateManagedWidget(
		    "macrosMenuButton", menuButtonWidgetClass,
		    menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmenuName, MACROS_MENU,
		    NULL);
}

#if 0
/* Pop up the a help browser. */
static void
render_help(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *browser = getenv("BROWSER");
	char *cmd;

	if (browser == CN)
		browser = "netscape";

	cmd = xs_buffer("%s %s/html/README.html &", browser, LIBX3270DIR);
	system(cmd);
	XtFree(cmd);
}

/* Initialize or re-initialize the help button. */
static void
help_button_init(Boolean regen, Position x, Position y)
{
	static Widget help_button = (Widget)NULL;

	if (regen && (help_button != (Widget)NULL)) {
		XtDestroyWidget(help_button);
		help_button = (Widget)NULL;
	}

	if (!menubar_buttons)
		return;

	if (help_button == (Widget)NULL) {
		help_button = XtVaCreateManagedWidget(
		    "helpButton", commandWidgetClass, menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    NULL);
		XtAddCallback(help_button, XtNcallback,
		    render_help, NULL);
	} else {
		XtVaSetValues(help_button, XtNx, x, NULL);
	}
}
#endif

/* Called toggle the keypad */
/*ARGSUSED*/
static void
toggle_keypad(Widget w, XtPointer client_data, XtPointer call_data)
{
	switch (kp_placement) {
	    case kp_integral:
		screen_showikeypad(appres.keypad_on = !appres.keypad_on);
		break;
	    case kp_left:
	    case kp_right:
	    case kp_bottom:
		keypad_popup_init();
		if (keypad_popped) 
			XtPopdown(keypad_shell);
		else
			popup_popup(keypad_shell, XtGrabNone);
		break;
	}
	menubar_keypad_changed();
	keypad_changed = True;
}

static void
keypad_button_init(Position x, Position y)
{
	if (!menubar_buttons)
		return;
	if (keypad_button == (Widget)NULL) {
		Pixmap pixmap;

		pixmap = XCreateBitmapFromData(display, root_window,
		    (char *) ky_bits, ky_width, ky_height);
		keypad_button = XtVaCreateManagedWidget(
		    "keypadButton", commandWidgetClass, menu_parent,
		    XtNbitmap, pixmap,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, ky_width+8,
		    XtNheight, KEY_HEIGHT,
		    NULL);
		XtAddCallback(keypad_button, XtNcallback,
		    toggle_keypad, NULL);
	} else {
		XtVaSetValues(keypad_button, XtNx, x, NULL);
	}
}

void
menubar_resize(Dimension width)
{
	keypad_button_init(
	    (Position) (width - LEFT_MARGIN - (ky_width+8) - 2*BORDER),
	    TOP_MARGIN);
}


/*
 * "Options..." menu
 */

/*ARGSUSED*/
void
toggle_callback(Widget w, XtPointer userdata, XtPointer calldata)
{
	struct toggle *t = (struct toggle *) userdata;

	/*
	 * If this is a two-button radio group, rather than a simple toggle,
	 * there is nothing to do if they are clicking on the current value.
	 *
	 * t->w[0] is the "toggle true" button; t->w[1] is "toggle false".
	 */
	if (t->w[1] != 0 && w == t->w[!t->value])
		return;

	do_toggle(t - appres.toggle);
}

Widget oversize_shell = NULL;

/* Called from the "Change" button on the oversize dialog */
/*ARGSUSED*/
static void
oversize_button_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *s;
	int ovc, ovr;
	char junk;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (sscanf(s, "%dx%d%c", &ovc, &ovr, &junk) == 2) {
		XtPopdown(oversize_shell);
		screen_change_model(model_num, ovc, ovr);
	} else
		popup_an_error("Illegal size: %s", s);
}

/* Called from the "Oversize..." button on the "Models..." menu */
/*ARGSUSED*/
static void
do_oversize_popup(Widget w, XtPointer client_data, XtPointer call_data)
{
	if (oversize_shell == NULL)
		oversize_shell = create_form_popup("Oversize",
		    oversize_button_callback, (XtCallbackProc)NULL, True);
	popup_popup(oversize_shell, XtGrabExclusive);
}

/* Init a toggle, menu-wise */
static void
toggle_init(Widget menu, int index, char *name1, char *name2)
{
	struct toggle *t = &appres.toggle[index];

	t->label[0] = name1;
	t->label[1] = name2;
	t->w[0] = XtVaCreateManagedWidget(
	    name1, cmeBSBObjectClass, menu,
	    XtNleftBitmap,
	     t->value ? (name2 ? diamond : dot) : (name2 ? no_diamond : None),
	    NULL);
	XtAddCallback(t->w[0], XtNcallback, toggle_callback, (XtPointer) t);
	if (name2 != NULL) {
		t->w[1] = XtVaCreateManagedWidget(
		    name2, cmeBSBObjectClass, menu,
		    XtNleftBitmap, t->value ? no_diamond : diamond,
		    NULL);
		XtAddCallback(t->w[1], XtNcallback, toggle_callback,
		    (XtPointer) t);
	} else
		t->w[1] = NULL;
}

Widget *font_widgets;
Widget other_font;
Widget font_shell = NULL;

/*ARGSUSED*/
void
do_newfont(Widget w, XtPointer userdata, XtPointer calldata)
{
	screen_newfont((char *)userdata, True);
}

/* Called from the "Select Font" button on the font dialog */
/*ARGSUSED*/
static void
font_button_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	XtPopdown(font_shell);
	do_newfont(w, s, PN);
}

/*ARGSUSED*/
static void
do_otherfont(Widget w, XtPointer userdata, XtPointer calldata)
{
	if (font_shell == NULL)
		font_shell = create_form_popup("Font", font_button_callback,
						(XtCallbackProc)NULL, True);
	popup_popup(font_shell, XtGrabExclusive);
}

/* Initialze the color scheme list. */
static void
scheme_init(void)
{
	char *cm;
	char *label;
	char *scheme;
	struct scheme *s;

	cm = get_resource(ResSchemeList);
	if (cm == CN)
		return;
	cm = XtNewString(cm);

	scheme_count = 0;
	while (split_dresource(&cm, &label, &scheme) == 1) {
		s = (struct scheme *)XtMalloc(sizeof(struct scheme));
		s->label = label;
		s->scheme = scheme;
		s->next = (struct scheme *)NULL;
		if (last_scheme != (struct scheme *)NULL)
			last_scheme->next = s;
		else
			schemes = s;
		last_scheme = s;
		scheme_count++;
	}
}

/*ARGSUSED*/
static void
do_newscheme(Widget w, XtPointer userdata, XtPointer calldata)
{
	screen_newscheme((char *)userdata);
}

/* Initialze the character set list. */
static void
charsets_init(void)
{
	char *cm;
	char *label;
	char *charset;
	struct charset *s;

	cm = get_resource(ResCharsetList);
	if (cm == CN)
		return;
	cm = XtNewString(cm);

	charset_count = 0;
	while (split_dresource(&cm, &label, &charset) == 1) {
		s = (struct charset *)XtMalloc(sizeof(struct charset));
		s->label = label;
		s->charset = charset;
		s->next = (struct charset *)NULL;
		if (last_charset != (struct charset *)NULL)
			last_charset->next = s;
		else
			charsets = s;
		last_charset = s;
		charset_count++;
	}
}

/*ARGSUSED*/
static void
do_newcharset(Widget w, XtPointer userdata, XtPointer calldata)
{
	struct charset *s;
	int i;

	/* Change the character set. */
	screen_newcharset((char *)userdata);

	/* Update the menu. */
	for (i = 0, s = charsets; i < charset_count; i++, s = s->next)
		XtVaSetValues(charset_widgets[i],
		    XtNleftBitmap,
			((appres.charset == CN) ||
			 strcmp(appres.charset, s->charset)) ?
			    no_diamond : diamond,
		    NULL);
}

Widget keymap_shell = NULL;

/* Called from the "Set Keymap" button on the keymap dialog */
/*ARGSUSED*/
static void
keymap_button_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (s != CN && !*s)
		s = CN;
	XtPopdown(keymap_shell);
	keymap_init(s);
}

/* Callback from the "Keymap" menu option */
/*ARGSUSED*/
static void
do_keymap(Widget w, XtPointer userdata, XtPointer calldata)
{
	if (keymap_shell == NULL)
		keymap_shell = create_form_popup("Keymap",
		    keymap_button_callback, (XtCallbackProc)NULL, True);
	popup_popup(keymap_shell, XtGrabExclusive);
}

/* Called to change telnet modes */
/*ARGSUSED*/
static void
linemode_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	net_linemode();
}

/*ARGSUSED*/
static void
charmode_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	net_charmode();
}

/* Called to change models */
/*ARGSUSED*/
static void
change_model_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
	int m;

	m = atoi(client_data);
	switch (model_num) {
	    case 2:
		XtVaSetValues(model_2_button, XtNleftBitmap, no_diamond, NULL);
		break;
	    case 3:
		XtVaSetValues(model_3_button, XtNleftBitmap, no_diamond, NULL);
		break;
	    case 4:
		XtVaSetValues(model_4_button, XtNleftBitmap, no_diamond, NULL);
		break;
	    case 5:
		XtVaSetValues(model_5_button, XtNleftBitmap, no_diamond, NULL);
		break;
	}
	XtVaSetValues(w, XtNleftBitmap, diamond, NULL);
	screen_change_model(m, 0, 0);
}

/* Called to change emulation modes */
/*ARGSUSED*/
static void
toggle_extended(Widget w, XtPointer client_data, XtPointer call_data)
{
	appres.extended = !appres.extended;
	XtVaSetValues(extended_button, XtNleftBitmap,
		appres.extended ? dot : (Pixmap)NULL,
		NULL);
	XtVaSetValues(oversize_button, XtNsensitive, appres.extended, NULL);
	if (!appres.extended)
		screen_change_model(model_num, 0, 0);
	screen_extended(appres.extended);
}

/*ARGSUSED*/
static void
toggle_m3279(Widget w, XtPointer client_data, XtPointer call_data)
{
	if (w == m3278_button)
		appres.m3279 = False;
	else if (w == m3279_button)
		appres.m3279 = True;
	else
		return;
	XtVaSetValues(m3278_button, XtNleftBitmap,
		appres.m3279 ? no_diamond : diamond,
		NULL);
	XtVaSetValues(m3279_button, XtNleftBitmap,
		appres.m3279 ? diamond : no_diamond,
		NULL);
#if defined(RESTRICT_3279) /*[*/
	XtVaSetValues(model_4_button, XtNsensitive, !appres.m3279, NULL);
	XtVaSetValues(model_5_button, XtNsensitive, !appres.m3279, NULL);
	if (model_num == 4 || model_num == 5)
		screen_change_model(3, 0, 0);
#endif /*]*/
	if (scheme_button != (Widget)NULL)
		XtVaSetValues(scheme_button, XtNsensitive, appres.m3279, NULL);
	screen_m3279(appres.m3279);
}

static void
options_menu_init(Boolean regen, Position x, Position y)
{
	static Widget options_menu;
	Widget t, w;
	struct font_list *f;
	int ix;

	if (regen && (options_menu != (Widget)NULL)) {
		XtDestroyWidget(options_menu);
		options_menu = (Widget)NULL;
	}
	if (options_menu != (Widget)NULL)
		return;

	/* Create the shell */
	options_menu = XtVaCreatePopupShell(
	    "optionsMenu", complexMenuWidgetClass, menu_parent,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu, NULL);

	/* Create the "toggles" pullright */
	t = XtVaCreatePopupShell(
	    "togglesMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	if (!menubar_buttons) {
		keypad_option_button = XtVaCreateManagedWidget(
		    "keypadOption", cmeBSBObjectClass, t,
		    XtNleftBitmap,
			appres.keypad_on || keypad_popped ? dot : None,
		    NULL);
		XtAddCallback(keypad_option_button, XtNcallback, toggle_keypad,
		    PN);
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    t, NULL);
	}
	toggle_init(t, MONOCASE, "monocaseOption", CN);
	toggle_init(t, CURSOR_BLINK, "cursorBlinkOption", CN);
	toggle_init(t, BLANK_FILL, "blankFillOption", CN);
	toggle_init(t, SHOW_TIMING, "showTimingOption", CN);
	toggle_init(t, CURSOR_POS, "cursorPosOption", CN);
	toggle_init(t, SCROLL_BAR, "scrollBarOption", CN);
#if defined(X3270_ANSI) /*[*/
	toggle_init(t, LINE_WRAP, "lineWrapOption", CN);
#endif /*]*/
	toggle_init(t, MARGINED_PASTE, "marginedPasteOption", CN);
	toggle_init(t, RECTANGLE_SELECT, "rectangleSelectOption", CN);
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass, t, NULL);
	toggle_init(t, ALT_CURSOR, "underlineCursorOption",
	    "blockCursorOption");
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass, t, NULL);
	linemode_button = XtVaCreateManagedWidget(
	    "lineModeOption", cmeBSBObjectClass, t,
	    XtNleftBitmap, linemode ? diamond : no_diamond,
	    XtNsensitive, IN_ANSI,
	    NULL);
	XtAddCallback(linemode_button, XtNcallback, linemode_callback, NULL);
	charmode_button = XtVaCreateManagedWidget(
	    "characterModeOption", cmeBSBObjectClass, t,
	    XtNleftBitmap, linemode ? no_diamond : diamond,
	    XtNsensitive, IN_ANSI,
	    NULL);
	XtAddCallback(charmode_button, XtNcallback, charmode_callback, NULL);
	if (!appres.mono) {
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass, t,
		    NULL);
		m3278_button = XtVaCreateManagedWidget(
		    "m3278Option", cmeBSBObjectClass, t,
		    XtNleftBitmap, appres.m3279 ? no_diamond : diamond,
		    XtNsensitive, !PCONNECTED,
		    NULL);
		XtAddCallback(m3278_button, XtNcallback, toggle_m3279, NULL);
		m3279_button = XtVaCreateManagedWidget(
		    "m3279Option", cmeBSBObjectClass, t,
		    XtNleftBitmap, appres.m3279 ? diamond : no_diamond,
		    XtNsensitive, !PCONNECTED,
		    NULL);
		XtAddCallback(m3279_button, XtNcallback, toggle_m3279, NULL);
	}
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass, t,
	    NULL);
	extended_button = XtVaCreateManagedWidget(
	    "extendedDsOption", cmeBSBObjectClass, t,
	    XtNleftBitmap, appres.extended ? dot : (Pixmap)NULL,
	    XtNsensitive, !PCONNECTED,
	    NULL);
	XtAddCallback(extended_button, XtNcallback, toggle_extended, NULL);
	(void) XtVaCreateManagedWidget(
	    "togglesOption", cmeBSBObjectClass, options_menu,
	    XtNrightBitmap, arrow,
	    XtNmenuName, "togglesMenu",
	    NULL);

	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, options_menu,
	    NULL);

	/* Create the "fonts" pullright */
	t = XtVaCreatePopupShell(
	    "fontsMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	font_widgets = (Widget *)XtCalloc(font_count, sizeof(Widget));
	for (f = font_list, ix = 0; f; f = f->next, ix++) {
		font_widgets[ix] = XtVaCreateManagedWidget(
		    f->label, cmeBSBObjectClass, t,
		    XtNleftBitmap,
			!strcmp(efontname, f->font) ? diamond : no_diamond,
		    NULL);
		XtAddCallback(font_widgets[ix], XtNcallback, do_newfont,
		    f->font);
	}
	if (!appres.no_other) {
		other_font = XtVaCreateManagedWidget(
		    "otherFontOption", cmeBSBObjectClass, t,
		    NULL);
		XtAddCallback(other_font, XtNcallback, do_otherfont, NULL);
	}
	(void) XtVaCreateManagedWidget(
	    "fontsOption", cmeBSBObjectClass, options_menu,
	    XtNrightBitmap, arrow,
	    XtNmenuName, "fontsMenu",
	    XtNsensitive, appres.apl_mode ? False : True,
	    NULL);

	(void) XtVaCreateManagedWidget(
	    "space", cmeLineObjectClass, options_menu,
	    NULL);

	/* Create the "models" pullright */
	t = XtVaCreatePopupShell(
	    "modelsMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	model_2_button = XtVaCreateManagedWidget(
	    "model2Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 2 ? diamond : no_diamond,
	    NULL);
	XtAddCallback(model_2_button, XtNcallback, change_model_callback, "2");
	model_3_button = XtVaCreateManagedWidget(
	    "model3Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 3 ? diamond : no_diamond,
	    NULL);
	XtAddCallback(model_3_button, XtNcallback, change_model_callback, "3");
	model_4_button = XtVaCreateManagedWidget(
	    "model4Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 4 ? diamond : no_diamond,
#if defined(RESTRICT_3279) /*[*/
	    XtNsensitive, !appres.m3279,
#endif /*]*/
	    NULL);
	XtAddCallback(model_4_button, XtNcallback,
	    change_model_callback, "4");
	model_5_button = XtVaCreateManagedWidget(
	    "model5Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 5 ? diamond : no_diamond,
#if defined(RESTRICT_3279) /*[*/
	    XtNsensitive, !appres.m3279,
#endif /*]*/
	    NULL);
	XtAddCallback(model_5_button, XtNcallback,
	    change_model_callback, "5");
	oversize_button = XtVaCreateManagedWidget(
	    "oversizeOption", cmeBSBObjectClass, t,
	    XtNsensitive, appres.extended,
	    NULL);
	XtAddCallback(oversize_button, XtNcallback, do_oversize_popup, NULL);
	models_option = XtVaCreateManagedWidget(
	    "modelsOption", cmeBSBObjectClass, options_menu,
	    XtNrightBitmap, arrow,
	    XtNmenuName, "modelsMenu",
	    XtNsensitive, !PCONNECTED,
	    NULL);

	/* Create the "colors" pullright */
	if (scheme_count) {
		struct scheme *s;
		int i;

		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);

		scheme_widgets = (Widget *)XtCalloc(scheme_count,
		    sizeof(Widget));
		t = XtVaCreatePopupShell(
		    "colorsMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		s = schemes;
		for (i = 0, s = schemes; i < scheme_count; i++, s = s->next) {
			scheme_widgets[i] = XtVaCreateManagedWidget(
			    s->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				!strcmp(appres.color_scheme, s->scheme) ?
				    diamond : no_diamond,
			    NULL);
			XtAddCallback(scheme_widgets[i], XtNcallback,
			    do_newscheme, s->scheme);
		}
		scheme_button = XtVaCreateManagedWidget(
		    "colorsOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "colorsMenu",
		    XtNsensitive, appres.m3279,
		    NULL);
	}

	/* Create the "character set" pullright */
	if (charset_count) {
		struct charset *s;
		int i;

		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);

		charset_widgets = (Widget *)XtCalloc(charset_count,
		    sizeof(Widget));
		t = XtVaCreatePopupShell(
		    "charsetMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		for (i = 0, s = charsets; i < charset_count; i++, s = s->next) {
			charset_widgets[i] = XtVaCreateManagedWidget(
			    s->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				((appres.charset == CN) ||
				 strcmp(appres.charset, s->charset)) ?
				    no_diamond : diamond,
			    NULL);
			XtAddCallback(charset_widgets[i], XtNcallback,
			    do_newcharset, s->charset);
		}
		(void) XtVaCreateManagedWidget(
		    "charsetOption", cmeBSBObjectClass, options_menu,
		    XtNrightBitmap, arrow,
		    XtNmenuName, "charsetMenu",
		    NULL);
	}

	/* Create the "keymap" option */
	if (!appres.no_other) {
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);
		w = XtVaCreateManagedWidget(
		    "keymapOption", cmeBSBObjectClass, options_menu,
		    NULL);
		XtAddCallback(w, XtNcallback, do_keymap, NULL);
	}

#if XtSpecificationRelease >= 5 /*[*/
	/* Create the "display keymap" option */
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
	    options_menu,
	    NULL);
	w = XtVaCreateManagedWidget(
	    "keymapDisplayOption", cmeBSBObjectClass, options_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, do_keymap_display, NULL);
#endif /*]*/

	if (menubar_buttons) {
		(void) XtVaCreateManagedWidget(
		    "optionsMenuButton", menuButtonWidgetClass, menu_parent,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmenuName, "optionsMenu",
		    NULL);
		keypad_option_button = NULL;
	}
}

/*
 * Change a menu checkmark
 */
void
menubar_retoggle(struct toggle *t)
{
	XtVaSetValues(t->w[0],
	    XtNleftBitmap, t->value ? (t->w[1] ? diamond : dot) : None,
	    NULL);
	if (t->w[1] != NULL)
		XtVaSetValues(t->w[1],
		    XtNleftBitmap, t->value ? no_diamond : diamond,
		    NULL);
}

/*ARGSUSED*/
void
HandleMenu_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	String p;

	action_debug(HandleMenu_action, event, params, num_params);
	if (check_usage(HandleMenu_action, *num_params, 1, 2) < 0)
		return;
	if (!CONNECTED || *num_params == 1)
		p = params[0];
	else
		p = params[1];
	if (!XtNameToWidget(menu_parent, p)) {
		if (strcmp(p, MACROS_MENU))
			popup_an_error("%s: cannot find menu %s",
			    action_name(HandleMenu_action), p);
		return;
	}
	XtCallActionProc(menu_parent, "XawPositionComplexMenu", event, &p, 1);
	XtCallActionProc(menu_parent, "MenuPopup", event, &p, 1);
}

#endif /*]*/
