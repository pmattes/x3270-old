/*
 * Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
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
#include "charsetc.h"
#include "ftc.h"
#include "hostc.h"
#include "idlec.h"
#include "keymapc.h"
#include "keypadc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "printerc.h"
#include "printc.h"
#include "savec.h"
#include "screenc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "utilc.h"
#include "xioc.h"

#define MACROS_MENU	"macrosMenu"

#define MAX_MENU_OPTIONS	30

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
static Widget options_menu;
static Widget fonts_option;
static int fm_index = 0;
static int fm_next_index = 1;
static struct kdcs {
	struct kdcs *next;
	int index;
	char *charset_name;
	char *menu_name;
	int count;
	int real_count;
	Widget *fw;	/* font widgets */
} *known_dcs = NULL;
static Pixel fm_background;
static Dimension fm_borderWidth;
static Pixel fm_borderColor;
static Dimension fm_leftMargin;

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
static void keypad_button_init(Position x, Position y);
static void connect_menu_init(Boolean regen, Position x, Position y);
static void macros_menu_init(Boolean regen, Position x, Position y);
static void file_menu_init(Boolean regen, Dimension x, Dimension y);
static void Bye(Widget w, XtPointer client_data, XtPointer call_data);
static void menubar_in3270(Boolean in3270);
static void menubar_linemode(Boolean in_linemode);
static void menubar_connect(Boolean ignored);
#if defined(X3270_PRINTER) /*[*/
static void menubar_printer(Boolean printer_on);
#endif /*]*/
static void menubar_remodel(Boolean ignored unused);
static void menubar_charset(Boolean ignored unused);

#define NO_BANG(s)	(((s)[0] == '!')? (s) + 1: (s))

#include "dot.bm"
#include "no_dot.bm"
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
#if defined(X3270_PRINTER) /*[*/
static Widget	printer_button;
static Widget	assoc_button;
static Widget	lu_button;
static Widget	printer_off_button;
#endif /*]*/
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
#if defined(X3270_SCRIPT) /*[*/
static Widget	idle_button;
#endif /*]*/

static Pixmap   arrow;
Pixmap	dot;
Pixmap	no_dot;
Pixmap	diamond;
Pixmap	no_diamond;
Pixmap	null;

static int	n_bye;

static void	toggle_init(Widget, int, const char *, const char *);

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
		no_dot = XCreateBitmapFromData(display, root_window,
		    (char *) no_dot_bits, no_dot_width, no_dot_height);
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
#if defined(X3270_PRINTER) /*[*/
		register_schange(ST_PRINTER, menubar_printer);
#endif /*]*/
		register_schange(ST_REMODEL, menubar_remodel);
		register_schange(ST_CHARSET, menubar_charset);

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
	} else
		menu_parent = container;

	/* "File..." menu */

	file_menu_init(mb_old != menubar_buttons, LEFT_MARGIN, TOP_MARGIN);

	/* "Options..." menu */

	options_menu_init(mb_old != menubar_buttons,
	    LEFT_MARGIN + MO_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* "Connect..." menu */

	if (!appres.reconnect)
		connect_menu_init(mb_old != menubar_buttons,
		    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
		    TOP_MARGIN);

	/* "Macros..." menu */

	macros_menu_init(mb_old != menubar_buttons,
	    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

#if defined(X3270_KEYPAD) /*[*/
	/* Keypad button */

	keypad_button_init(
	    (Position) (current_width - LEFT_MARGIN - (ky_width+8) -
			    2*BORDER - 2*MENU_BORDER),
	    TOP_MARGIN);
#endif /*]*/
}

/*
 * External entry points
 */

/*
 * Called when connected to or disconnected from a host.
 */
static void
menubar_connect(Boolean ignored unused)
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
	if (!appres.reconnect && connect_menu != (Widget)NULL) {
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

	/* Set up the macros menu. */
	macros_menu_init(True,
	    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* Set up the various option buttons. */
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
#if defined(X3270_PRINTER) /*[*/
	if (printer_button != (Widget)NULL)
		XtVaSetValues(printer_button, XtNsensitive, IN_3270,
		    NULL);
	if (assoc_button != (Widget)NULL)
		XtVaSetValues(assoc_button, XtNsensitive,
		    !printer_running() && IN_3270 && IN_TN3270E,
		    NULL);
	if (lu_button != (Widget)NULL)
		XtVaSetValues(lu_button, XtNsensitive,
		    !printer_running() && IN_3270,
		    NULL);
#endif /*]*/
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

#if defined(X3270_PRINTER) /*[*/
/* Called when the printer starts or stops. */
static void
menubar_printer(Boolean printer_on)
{
	XtVaSetValues(assoc_button, XtNsensitive,
	    !printer_on && IN_3270 && IN_TN3270E,
	    NULL);
	XtVaSetValues(lu_button, XtNsensitive,
	    !printer_on && IN_3270,
	    NULL);
	XtVaSetValues(printer_off_button, XtNsensitive, printer_on, NULL);
}
#endif /*]*/

#if defined(X3270_KEYPAD) /*[*/
void
menubar_keypad_changed(void)
{
	if (keypad_option_button != (Widget)NULL)
		XtVaSetValues(keypad_option_button,
		    XtNleftBitmap,
			appres.keypad_on || keypad_popped ? dot : None,
		    NULL);
}
#endif /*]*/

/* Called when we switch between ANSI and 3270 modes. */
static void
menubar_in3270(Boolean in3270)
{
	if (ft_button != (Widget)NULL)
		XtVaSetValues(ft_button, XtNsensitive, IN_3270, NULL);
#if defined(X3270_PRINTER) /*[*/
	if (printer_button != (Widget)NULL)
		XtVaSetValues(printer_button, XtNsensitive, IN_3270,
		    NULL);
	if (assoc_button != (Widget)NULL)
		XtVaSetValues(assoc_button, XtNsensitive,
		    !printer_running() && IN_3270 && IN_TN3270E,
		    NULL);
	if (lu_button != (Widget)NULL)
		XtVaSetValues(lu_button, XtNsensitive,
		    !printer_running() && IN_3270,
		    NULL);
#endif /*]*/
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
#if defined(X3270_SCRIPT) /*[*/
	if (idle_button != (Widget)NULL)
		XtVaSetValues(idle_button,
		    XtNsensitive, in3270,
		    NULL);
#endif /*]*/
}

/* Called when we switch between ANSI line and character. */
static void
menubar_linemode(Boolean in_linemode)
{
	if (linemode_button != (Widget)NULL)
		XtVaSetValues(linemode_button,
		    XtNleftBitmap, in_linemode ? diamond : no_diamond,
		    NULL);
	if (charmode_button != (Widget)NULL)
		XtVaSetValues(charmode_button,
		    XtNleftBitmap, in_linemode ? no_diamond : diamond,
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
static void
Bye(Widget w unused, XtPointer client_data unused, XtPointer call_data unused)
{
	x3270_exit(0);
}

/* Called from the "Disconnect" button on the "File..." menu */
static void
disconnect(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	host_disconnect(False);
}

/* Called from the "Abort Script" button on the "File..." menu */
static void
script_abort_callback(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	abort_script();
}

/* "About x3270" popup */
static void
show_about(Widget w unused, XtPointer userdata unused,
    XtPointer calldata unused)
{
	popup_about();
}

/* Called from the "Save" button on the save options dialog */
static void
save_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!save_options(s))
		XtPopdown(save_shell);
}

/* Called from the "Save Options in File" button on the "File..." menu */
static void
do_save_options(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (save_shell == NULL)
		save_shell = create_form_popup("SaveOptions",
		    save_button_callback, (XtCallbackProc)NULL, FORM_NO_WHITE);
	XtVaSetValues(XtNameToWidget(save_shell, ObjDialog),
	    XtNvalue, profile_name,
	    NULL);
	popup_popup(save_shell, XtGrabExclusive);
}

#if defined(X3270_PRINTER) /*[*/
/* Callback for printer session options. */
static void
do_printer(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	if (client_data == NULL)
		printer_start(CN);
	else if (!strcmp(client_data, "lu"))
		printer_lu_dialog();
	else
		printer_stop();
}
#endif /*]*/

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

#if defined(X3270_PRINTER) /*[*/
	/* Printer start/stop */
	w = XtVaCreatePopupShell(
	    "printerMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	assoc_button = XtVaCreateManagedWidget(
	    "assocButton", cmeBSBObjectClass, w,
	    XtNsensitive, IN_3270 && IN_TN3270E,
	    NULL);
	XtAddCallback(assoc_button, XtNcallback, do_printer, NULL);
	lu_button = XtVaCreateManagedWidget(
	    "luButton", cmeBSBObjectClass, w,
	    NULL);
	XtAddCallback(lu_button, XtNcallback, do_printer, "lu");
	printer_off_button = XtVaCreateManagedWidget(
	    "printerOffButton", cmeBSBObjectClass, w,
	    XtNsensitive, printer_running(),
	    NULL);
	XtAddCallback(printer_off_button, XtNcallback, do_printer, "off");

	(void) XtCreateManagedWidget(
	    "space", cmeLineObjectClass, file_menu,
	    NULL, 0);
	printer_button = XtVaCreateManagedWidget(
	    "printerOption", cmeBSBObjectClass, file_menu,
	    XtNsensitive, IN_3270,
	    XtNrightBitmap, arrow,
	    XtNmenuName, "printerMenu",
	    NULL);
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
	if (!appres.secure)
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
static void
host_connect_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	(void) host_connect(client_data);
}

/* Called from the lone "Connect" button on the connect dialog */
static void
connect_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	if (!host_connect(s))
		XtPopdown(connect_shell);
}

/* Called from the "Other..." button on the "Connect..." menu */
static void
do_connect_popup(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (connect_shell == NULL)
		connect_shell = create_form_popup("Connect",
		    connect_button_callback, (XtCallbackProc)NULL, FORM_NO_CC);
	popup_popup(connect_shell, XtGrabExclusive);
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
	int n_primary = 0;
	int n_recent = 0;

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

	/* Walk the host list from the file to produce the host menu */

	for (h = hosts; h; h = h->next) {
		switch (h->entry_type) {
		case ALIAS:
			continue;
		case PRIMARY:
			/*
			 * If there's already a 'recent' entry with the same
			 * name, skip this one.
			 */
			{
				struct host *j;

				for (j = hosts;
				     j != (struct host *)NULL;
				     j = j->next) {
					if (j->entry_type != RECENT) {
						j = (struct host *)NULL;
						break;
					}
					if (!strcmp(j->name, h->name))
						break;
				}
				if (j != (struct host *)NULL)
					continue;
			}
			n_primary++;
			break;
		case RECENT:
			n_recent++;
			break;
		}
		if ((need_line && !any_hosts) ||
		    (n_recent > 0 && n_primary == 1)) {
			(void) XtVaCreateManagedWidget("space",
			    cmeLineObjectClass, connect_menu, NULL);
		}
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
 * Callback for macros
 */
static void
do_macro(Widget w unused, XtPointer client_data, XtPointer call_data unused)
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

#if defined(X3270_KEYPAD) /*[*/
/* Called toggle the keypad */
static void
toggle_keypad(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
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
#endif /*]*/

void
menubar_resize(Dimension width)
{
#if defined(X3270_KEYPAD) /*[*/
	keypad_button_init(
	    (Position) (width - LEFT_MARGIN - (ky_width+8) - 2*BORDER),
	    TOP_MARGIN);
#endif /*]*/
}


/*
 * "Options..." menu
 */

static void
toggle_callback(Widget w, XtPointer userdata, XtPointer calldata unused)
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

static Widget oversize_shell = NULL;

/* Called from the "Change" button on the oversize dialog */
static void
oversize_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
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
static void
do_oversize_popup(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (oversize_shell == NULL)
		oversize_shell = create_form_popup("Oversize",
		    oversize_button_callback, (XtCallbackProc)NULL,
		    FORM_NO_WHITE);
	popup_popup(oversize_shell, XtGrabExclusive);
}

/* Init a toggle, menu-wise */
static void
toggle_init(Widget menu, int ix, const char *name1, const char *name2)
{
	struct toggle *t = &appres.toggle[ix];

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

static Widget *font_widgets;
static Widget other_font;
static Widget font_shell = NULL;

static void
do_newfont(Widget w unused, XtPointer userdata, XtPointer calldata unused)
{
	screen_newfont((char *)userdata, True, False);
}

/* Called from the "Select Font" button on the font dialog */
static void
font_button_callback(Widget w, XtPointer client_data,
    XtPointer call_data unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (!s || !*s)
		return;
	XtPopdown(font_shell);
	do_newfont(w, s, PN);
}

static void
do_otherfont(Widget w unused, XtPointer userdata unused,
    XtPointer calldata unused)
{
	if (font_shell == NULL)
		font_shell = create_form_popup("Font", font_button_callback,
						(XtCallbackProc)NULL,
						FORM_NO_CC);
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

static void
do_newscheme(Widget w unused, XtPointer userdata, XtPointer calldata unused)
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

static void
do_newcharset(Widget w unused, XtPointer userdata, XtPointer calldata unused)
{
	struct charset *s;
	int i;

	/* Change the character set. */
	screen_newcharset((char *)userdata);

	/* Update the menu. */
	for (i = 0, s = charsets; i < charset_count; i++, s = s->next)
		XtVaSetValues(charset_widgets[i],
		    XtNleftBitmap,
			(strcmp(get_charset_name(), s->charset)) ?
			    no_diamond : diamond,
		    NULL);
}

static Widget keymap_shell = NULL;

/* Called from the "Set Keymap" button on the keymap dialog */
static void
keymap_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	char *s;

	s = XawDialogGetValueString((Widget)client_data);
	if (s != CN && !*s)
		s = CN;
	XtPopdown(keymap_shell);
	keymap_init(s, True);
}

/* Callback from the "Keymap" menu option */
static void
do_keymap(Widget w unused, XtPointer userdata unused, XtPointer calldata unused)
{
	if (keymap_shell == NULL)
		keymap_shell = create_form_popup("Keymap",
		    keymap_button_callback, (XtCallbackProc)NULL,
		    FORM_NO_WHITE);
	popup_popup(keymap_shell, XtGrabExclusive);
}

#if defined(X3270_SCRIPT) /*[*/
/* Callback from the "Idle Command" menu option */
static void
do_idle_command(Widget w unused, XtPointer userdata unused,
    XtPointer calldata unused)
{
	popup_idle();
}
#endif /*]*/

/* Called to change telnet modes */
static void
linemode_callback(Widget w unused, XtPointer client_data unused, XtPointer call_data unused)
{
	net_linemode();
}

static void
charmode_callback(Widget w unused, XtPointer client_data unused, 
    XtPointer call_data unused)
{
	net_charmode();
}

/* Called to change models */
static void
change_model_callback(Widget w, XtPointer client_data,
    XtPointer call_data unused)
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

/* Called to when model changes outside our control */
static void
menubar_remodel(Boolean ignored unused)
{
	XtVaSetValues(model_2_button, XtNleftBitmap,
	    (model_num == 2)? diamond: no_diamond, NULL);
	XtVaSetValues(model_3_button, XtNleftBitmap,
	    (model_num == 3)? diamond: no_diamond, NULL);
	XtVaSetValues(model_4_button, XtNleftBitmap,
	    (model_num == 4)? diamond: no_diamond, NULL);
	XtVaSetValues(model_5_button, XtNleftBitmap,
	    (model_num == 5)? diamond: no_diamond, NULL);
}

/* Compare a font name to the current emulator font name. */
static Boolean
is_efont(const char *font_name)
{
	return !strcmp(NO_BANG(font_name), NO_BANG(efontname)) ||
	       !strcmp(NO_BANG(font_name), NO_BANG(full_efontname));
}

/* Create, or re-create the font menu. */
static void
create_font_menu(Boolean regen, Boolean even_if_unknown)
{
	const char *efc;
	Widget t;
	struct font_list *f;
	int ix;
	struct kdcs *k;

	if (regen) {
		fm_index = 0;
		while (known_dcs != NULL) {
			k = known_dcs->next;
			Free(known_dcs->charset_name);
			Free(known_dcs->menu_name);
			Free(known_dcs->fw);
			Free(known_dcs);
			known_dcs = k;
		}
	}

	if (fm_index == 0 && !even_if_unknown)
		return;

	/* See if the name matches. */
	efc = display_charset();
	for (k = known_dcs; k != NULL; k = k->next) {
		if (!strcasecmp(k->charset_name, efc))
			break;
	}
	if (k != NULL) {
		/* See if the list matches. */
		if (k->count == font_count)
			for (f = font_list, ix = 0;
			     f != NULL;
			     f = f->next, ix++) {
				if (ix >= k->real_count) {
					f = NULL;
					break;
				}
				if (strcmp(XtName(k->fw[ix]), f->label))
					break;
			}
		if (k->count != font_count || f != NULL) {
			free(k->charset_name);
			k->charset_name = NewString("(invalid)");
			k = NULL;
		}
	}
	if (k != NULL && k->index == fm_index)
		return;
	if (k == NULL) {
		k = (struct kdcs *)Malloc(sizeof(struct kdcs));
		k->index = fm_next_index++;
		k->charset_name = NewString(efc);
		k->menu_name = xs_buffer("fontsMenu%d", k->index);
		k->next = known_dcs;
		known_dcs = k;
		t = XtVaCreatePopupShell(
		    k->menu_name, complexMenuWidgetClass, menu_parent,
		    XtNborderWidth, fm_borderWidth,
		    XtNborderColor, fm_borderColor,
		    XtNbackground, fm_background,
		    NULL);
		k->count = font_count;
		k->real_count = font_count;
		if (k->real_count > MAX_MENU_OPTIONS)
			k->real_count = MAX_MENU_OPTIONS;
		if (font_count)
			k->fw = (Widget *)XtCalloc(k->real_count,
						   sizeof(Widget));
		else
			k->fw = NULL;
		for (f = font_list, ix = 0; f; f = f->next, ix++) {
			if (ix >= MAX_MENU_OPTIONS)
				break;
			k->fw[ix] = XtVaCreateManagedWidget(
			    f->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				is_efont(f->font)? diamond: no_diamond,
			    XtNleftMargin, fm_leftMargin,
			    NULL);
			XtAddCallback(k->fw[ix], XtNcallback, do_newfont,
			    XtNewString(f->font));
		}
		if (!appres.no_other) {
			other_font = XtVaCreateManagedWidget(
			    "otherFontOption", cmeBSBObjectClass, t,
			    NULL);
			XtAddCallback(other_font, XtNcallback, do_otherfont, NULL);
		}
	}
	XtVaSetValues(fonts_option, XtNmenuName, k->menu_name, NULL);
	fm_index = k->index;
	font_widgets = k->fw;
    }

/* Called when the character set changes. */
static void
menubar_charset(Boolean ignored unused)
{
	create_font_menu(False, False);
}

/* Called to change emulation modes */
static void
toggle_extended(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
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

static void
toggle_m3279(Widget w, XtPointer client_data unused, XtPointer call_data unused)
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
	Widget t, w;
	struct font_list *f;
	struct scheme *s;
	int ix;
	static Widget options_menu_button = NULL;
	Widget dummy_font_menu, dummy_font_element;

	if (regen && (options_menu != (Widget)NULL)) {
		XtDestroyWidget(options_menu);
		options_menu = (Widget)NULL;
		if (options_menu_button != NULL) {
			XtDestroyWidget(options_menu_button);
			options_menu_button = NULL;
		}
	}
	if (options_menu != (Widget)NULL) {
		/* Set the current font. */
		for (f = font_list, ix = 0; f; f = f->next, ix++) {
			if (ix >= MAX_MENU_OPTIONS)
				break;
			XtVaSetValues(font_widgets[ix], XtNleftBitmap,
				is_efont(f->font)? diamond: no_diamond,
				NULL);
		}
		/* Set the current color scheme. */
		s = schemes;
		for (ix = 0, s = schemes;
		     ix < scheme_count;
		     ix++, s = s->next) {
			XtVaSetValues(scheme_widgets[ix], XtNleftBitmap,
				!strcmp(appres.color_scheme, s->scheme) ?
				    diamond : no_diamond,
			    NULL);
		}
		return;
	}

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
#if defined(X3270_KEYPAD) /*[*/
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
#endif /*]*/
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
	toggle_init(t, CROSSHAIR, "crosshairOption", CN);
	toggle_init(t, VISIBLE_CONTROL, "visibleControlOption", CN);
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

	/*
	 * Create a dummy menu with the well-known name, so we can get the
	 * values of background, borderWidth, borderColor and leftMargin
	 * from its resources.
	 */
	dummy_font_menu = XtVaCreatePopupShell(
	    "fontsMenu", complexMenuWidgetClass, menu_parent,
	    NULL);
	dummy_font_element =  XtVaCreateManagedWidget(
	    "entry", cmeBSBObjectClass, dummy_font_menu,
	    XtNleftBitmap, no_diamond,
	    NULL);
	XtRealizeWidget(dummy_font_menu);
	XtVaGetValues(dummy_font_menu,
	    XtNborderWidth, &fm_borderWidth,
	    XtNborderColor, &fm_borderColor,
	    XtNbackground, &fm_background,
	    NULL);
	XtVaGetValues(dummy_font_element,
	    XtNleftMargin, &fm_leftMargin,
	    NULL);

	fonts_option = XtVaCreateManagedWidget(
	    "fontsOption", cmeBSBObjectClass, options_menu,
	    XtNrightBitmap, arrow,
	    NULL);
	create_font_menu(regen, True);

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
	XtAddCallback(model_2_button, XtNcallback, change_model_callback,
		XtNewString("2"));
	model_3_button = XtVaCreateManagedWidget(
	    "model3Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 3 ? diamond : no_diamond,
	    NULL);
	XtAddCallback(model_3_button, XtNcallback, change_model_callback,
		XtNewString("3"));
	model_4_button = XtVaCreateManagedWidget(
	    "model4Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 4 ? diamond : no_diamond,
#if defined(RESTRICT_3279) /*[*/
	    XtNsensitive, !appres.m3279,
#endif /*]*/
	    NULL);
	XtAddCallback(model_4_button, XtNcallback,
	    change_model_callback, XtNewString("4"));
	model_5_button = XtVaCreateManagedWidget(
	    "model5Option", cmeBSBObjectClass, t,
	    XtNleftBitmap, model_num == 5 ? diamond : no_diamond,
#if defined(RESTRICT_3279) /*[*/
	    XtNsensitive, !appres.m3279,
#endif /*]*/
	    NULL);
	XtAddCallback(model_5_button, XtNcallback,
	    change_model_callback, XtNewString("5"));
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
		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);

		scheme_widgets = (Widget *)XtCalloc(scheme_count,
		    sizeof(Widget));
		t = XtVaCreatePopupShell(
		    "colorsMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		s = schemes;
		for (ix = 0, s = schemes; ix < scheme_count; ix++, s = s->next) {
			scheme_widgets[ix] = XtVaCreateManagedWidget(
			    s->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				!strcmp(appres.color_scheme, s->scheme) ?
				    diamond : no_diamond,
			    NULL);
			XtAddCallback(scheme_widgets[ix], XtNcallback,
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
		struct charset *cs;

		(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
		    options_menu,
		    NULL);

		charset_widgets = (Widget *)XtCalloc(charset_count,
		    sizeof(Widget));
		t = XtVaCreatePopupShell(
		    "charsetMenu", complexMenuWidgetClass, menu_parent,
		    NULL);
		for (ix = 0, cs = charsets;
                     ix < charset_count;
                     ix++, cs = cs->next) {
			charset_widgets[ix] = XtVaCreateManagedWidget(
			    cs->label, cmeBSBObjectClass, t,
			    XtNleftBitmap,
				(strcmp(get_charset_name(), cs->charset)) ?
				    no_diamond : diamond,
			    NULL);
			XtAddCallback(charset_widgets[ix], XtNcallback,
			    do_newcharset, cs->charset);
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

	/* Create the "display keymap" option */
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
	    options_menu,
	    NULL);
	w = XtVaCreateManagedWidget(
	    "keymapDisplayOption", cmeBSBObjectClass, options_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, do_keymap_display, NULL);

#if defined(X3270_SCRIPT) /*[*/
	/* Create the "Idle Command" option */
	(void) XtVaCreateManagedWidget("space", cmeLineObjectClass,
	    options_menu,
	    NULL);
	idle_button = XtVaCreateManagedWidget(
	    "idleCommandOption", cmeBSBObjectClass, options_menu,
	    XtNsensitive, IN_3270,
	    NULL);
	XtAddCallback(idle_button, XtNcallback, do_idle_command, NULL);
#endif /*]*/

	if (menubar_buttons) {
		options_menu_button = XtVaCreateManagedWidget(
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

void
HandleMenu_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
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
