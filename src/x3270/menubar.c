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
 *	menubar.c
 *		This module handles the menu bar.
 */

#include <stdio.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Dialog.h>
#include "globals.h"


extern Widget		keypad_shell;
extern int		linemode;
extern Boolean		keypad_popped;

static struct host {
	char *name;
	char *hostname;
	enum { PRIMARY, ALIAS } entry_type;
	char *loginstring;
	struct host *next;
} *hosts, *last_host;

static void	options_menu_init();
static void	keypad_button_init();
static void	connect_menu_init();
static void	quit_menu_init();

#include "dot.bm"
#include "ky.bm"


/*
 * Menu Bar
 */

static Widget menu_bar;
static Boolean menubar_buttons;
static Widget disconnect_button;
static Widget connect_button;
static Widget keypad_button;
static Widget linemode_button;
static Widget charmode_button;
static Widget model_2_button;
static Widget model_3_button;
static Widget model_4_button;
static Widget model_5_button;
static Widget keypad_option_button;
static Widget connect_menu;

static Pixmap dot;

#define TOP_MARGIN	3
#define BOTTOM_MARGIN	3
#define LEFT_MARGIN	3
#define KEY_HEIGHT	18
#define KEY_WIDTH	80
#define BORDER		1
#define SPACING		3

#define MO_OFFSET	1
#define CN_OFFSET	2

#define	MENU_MIN_WIDTH	(LEFT_MARGIN + 3*(KEY_WIDTH+2*BORDER+SPACING) + \
			 LEFT_MARGIN + (ky_width+8) + 2*BORDER + SPACING)

/*
 * Initialize the menu bar.
 */
Dimension
menubar_init(container, overall_width, current_width)
Widget container;
Dimension overall_width;
Dimension current_width;
{
	Dimension height;

	/* Error popup */

	error_popup_init();

	if (!appres.menubar || current_width < (unsigned) MENU_MIN_WIDTH) {
		height = 0;
		menu_bar = container;
		menubar_buttons = False;
	} else {
		height = TOP_MARGIN + KEY_HEIGHT+2*BORDER + BOTTOM_MARGIN;
		menu_bar = XtVaCreateManagedWidget(
		    "menuBarContainer", compositeWidgetClass, container,
		    XtNborderWidth, 0,
		    XtNwidth, overall_width,
		    XtNheight, height,
		    NULL);
		if (depth > 1)
			XtVaSetValues(menu_bar,
			    XtNbackground, appres.keypadbg,
			    NULL);
		else
			XtVaSetValues(menu_bar,
			    XtNbackgroundPixmap, gray,
			    NULL);
		menubar_buttons = True;
	}


	/* "Quit..." menu */

	quit_menu_init(menu_bar, 
	    LEFT_MARGIN,
	    TOP_MARGIN);

	/* "Options..." menu */

	options_menu_init(menu_bar,
	    LEFT_MARGIN + MO_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* "Connect..." menu */

	connect_menu_init(menu_bar,
	    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
	    TOP_MARGIN);

	/* Keypad button */

	if (menubar_buttons)
		keypad_button_init(menu_bar,
		    (Position) (current_width - LEFT_MARGIN - (ky_width+8) - 2*BORDER),
		    TOP_MARGIN);

	/* Register a grab action for the popup versions of the menus */

	XtRegisterGrabAction(handle_menu, True,
	    (ButtonPressMask|ButtonReleaseMask),
	    GrabModeAsync, GrabModeAsync);

	return height;
}

/*
 * External entry points
 */
void
menubar_connect()	/* have connected to or disconnected from a host */
{
	if (disconnect_button)
		XtVaSetValues(disconnect_button,
		    XtNsensitive, PCONNECTED,
		    NULL);
	if (connect_menu) {
		if (PCONNECTED && connect_button)
			XtUnmapWidget(connect_button);
		else {
			connect_menu_init(menu_bar,
			    LEFT_MARGIN + CN_OFFSET*(KEY_WIDTH+2*BORDER+SPACING),
			    TOP_MARGIN);
			if (menubar_buttons)
				XtMapWidget(connect_button);
		}
	}
	if (linemode_button)
		XtVaSetValues(linemode_button, XtNsensitive, IN_ANSI, NULL);
	if (charmode_button)
		XtVaSetValues(charmode_button, XtNsensitive, IN_ANSI, NULL);
	if (model_2_button)
		XtVaSetValues(model_2_button, XtNsensitive, !PCONNECTED, NULL);
	if (model_3_button)
		XtVaSetValues(model_3_button, XtNsensitive, !PCONNECTED, NULL);
	if (model_4_button)
		XtVaSetValues(model_4_button, XtNsensitive, !PCONNECTED, NULL);
	if (model_5_button)
		XtVaSetValues(model_5_button, XtNsensitive, !PCONNECTED, NULL);
}

void
menubar_keypad_changed()	/* keypad snapped on or off */
{
	if (keypad_option_button)
		XtVaSetValues(keypad_option_button,
		    XtNleftBitmap, keypad || keypad_popped ? dot : None,
		    NULL);
}

void
menubar_newmode()	/* have switched telnet modes */
{
	if (linemode_button)
		XtVaSetValues(linemode_button,
		    XtNsensitive, IN_ANSI,
		    XtNleftBitmap, linemode ? dot : None,
		    NULL);
	if (charmode_button)
		XtVaSetValues(charmode_button,
		    XtNsensitive, IN_ANSI,
		    XtNleftBitmap, linemode ? None : dot,
		    NULL);
}

void
menubar_gone()		/* container has gone away */
{
	connect_menu = (Widget) NULL;
	connect_button = (Widget) NULL;
}


/*
 * "Quit..." menu
 */

/* Called from "Exit x3270" button on "Quit..." menu */
/*ARGSUSED*/
static void
Bye(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	exit(0);
}

/* Called from the "Disconnect" button on the "Quit..." menu */
/*ARGSUSED*/
static void
disconnect(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	x_disconnect();
	menubar_connect();
}

static void
quit_menu_init(container, x, y)
Widget container;
Dimension x, y;
{
	Widget quit_menu;
	Widget w;

	quit_menu = XtVaCreatePopupShell(
	    "quitMenu", simpleMenuWidgetClass, container,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		XtCreateManagedWidget("space", smeLineObjectClass, quit_menu, NULL, 0);

	disconnect_button = XtVaCreateManagedWidget(
	    "disconnectOption", smeBSBObjectClass, quit_menu,
	    XtNsensitive, PCONNECTED,
	    NULL);
	XtAddCallback(disconnect_button, XtNcallback, disconnect, 0);

	w = XtVaCreateManagedWidget(
	    "exitOption", smeBSBObjectClass, quit_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, Bye, 0);

	if (menubar_buttons)
		(void) XtVaCreateManagedWidget(
		    "quitMenuButton", menuButtonWidgetClass, container,
		    XtNx, x,
		    XtNy, y,
		    XtNwidth, KEY_WIDTH,
		    XtNheight, KEY_HEIGHT,
		    XtNmenuName, "quitMenu",
		    NULL);
}


/*
 * "Connect..." menu
 */

static Widget connect_shell = NULL;

/* Called from each button on the "Connect..." menu */
/*ARGSUSED*/
static void
host_connect(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	(void) x_connect(client_data);
}

/* Called from the "Connect" button on the connect dialog */
/*ARGSUSED*/
static void
connect_button_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *s;

	s = XawDialogGetValueString(client_data);
	if (!s || !*s)
		return;
	if (!x_connect(s))
		XtPopdown(connect_shell);
}

/* Called from the "Other..." button on the "Connect..." menu */
/*ARGSUSED*/
static void
do_connect_popup(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	if (connect_shell == NULL)
		connect_shell = create_form_popup("Connect", "Enter Hostname",
		    "Connect", connect_button_callback);
	popup_popup(connect_shell, XtGrabExclusive);
}

/*
 * Initialize the "Connect..." menu
 */
static void
connect_menu_init(container, x, y)
Widget container;
Position x, y;
{
	Widget w;
	int n_hosts = 0;
	Boolean any_hosts = False;
	struct host *h;
	Boolean need_line = False;

	/* Destroy any previous connect menu */

	if (connect_menu) {
		XtDestroyWidget(connect_menu);
		connect_menu = NULL;
	}
	if (connect_button) {
		XtDestroyWidget(connect_button);
		connect_button = NULL;
	}

	/* Create the menu */
	connect_menu = XtVaCreatePopupShell(
	    "hostMenu", simpleMenuWidgetClass, container,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		need_line = True;

	/* Start off with an opportunity to reconnect */

	if (current_host) {
		char *buf;

		if (need_line)
			(void) XtCreateManagedWidget("space",
			    smeLineObjectClass, connect_menu, NULL, 0);
		buf = xs_buffer2("%s %s", get_message("reconnect"),
		    current_host);
		w = XtVaCreateManagedWidget(
		    buf, smeBSBObjectClass, connect_menu, 
		    NULL);
		XtFree(buf);
		XtAddCallback(w, XtNcallback, host_connect,
		    XtNewString(full_current_host));
		need_line = True;
		n_hosts++;
	}

	/* Walk the host list from the file to produce the host menu */

	for (h = hosts; h; h = h->next) {
		if (h->entry_type != PRIMARY)
			continue;
		if (current_host && !strcmp(h->name, current_host))
			continue;
		if (need_line && !any_hosts)
			(void) XtCreateManagedWidget("space",
			    smeLineObjectClass, connect_menu, NULL, 0);
		any_hosts = True;
		w = XtVaCreateManagedWidget(
		    h->name, smeBSBObjectClass, connect_menu, 
		    NULL);
		XtAddCallback(w, XtNcallback, host_connect,
		    XtNewString(h->name));
		n_hosts++;
	}
	if (any_hosts)
		need_line = True;

	/* Add an "Other..." button at the bottom */

	if (need_line)
		(void) XtCreateManagedWidget("space", smeLineObjectClass,
		    connect_menu, NULL, 0);
	w = XtVaCreateManagedWidget(
	    "otherHostOption", smeBSBObjectClass, connect_menu,
	    NULL);
	XtAddCallback(w, XtNcallback, do_connect_popup, NULL);

	/* Add the "Connect..." button itself to the container. */

	if (menubar_buttons) {
		if (n_hosts)
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", menuButtonWidgetClass,
			    container,
			    XtNx, x,
			    XtNy, y,
			    XtNwidth, KEY_WIDTH,
			    XtNheight, KEY_HEIGHT,
			    XtNmenuName, "hostMenu",
			    XtNmappedWhenManaged, !PCONNECTED,
			    NULL);
		else {
			connect_button = XtVaCreateManagedWidget(
			    "connectMenuButton", commandWidgetClass,
			    container,
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

/* Called toggle the keypad */
/*ARGSUSED*/
static void
toggle_keypad(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	switch (kp_placement) {
	    case kp_integral:
		screen_showkeypad((int)(keypad = !keypad));
		break;
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
}

static void
keypad_button_init(container, x, y)
Widget container;
Position x, y;
{
	Pixmap pixmap;

	pixmap = XCreateBitmapFromData(display, root_window,
	    (char *) ky_bits, ky_width, ky_height);

	keypad_button = XtVaCreateManagedWidget(
	    "keypadButton", commandWidgetClass, container,
	    XtNbitmap, pixmap,
	    XtNx, x,
	    XtNy, y,
	    XtNwidth, ky_width+8,
	    XtNheight, KEY_HEIGHT,
	    NULL);
	XtAddCallback(keypad_button, XtNcallback, toggle_keypad, NULL);
}

void
menubar_resize(width)
Dimension width;
{
	if (menubar_buttons) {
		XtDestroyWidget(keypad_button);
		keypad_button_init(menu_bar,
		    (Position) (width - LEFT_MARGIN - (ky_width+8) - 2*BORDER),
		    TOP_MARGIN);
	}
}


/*
 * "Options..." menu (toggles)
 */

/*ARGSUSED*/
void
show_options(w, userdata, calldata)
Widget w;
XtPointer userdata;
XtPointer calldata;
{
	popup_options();
}

/*ARGSUSED*/
void
toggle_callback(w, userdata, calldata)
Widget w;
XtPointer userdata;
XtPointer calldata;
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

/* Init a toggle, menu-wise */
static void
toggle_init(menu, index, name1, name2)
Widget menu;
int index;
char *name1;
char *name2;
{
	struct toggle *t = &appres.toggle[index];

	t->label[0] = name1;
	t->label[1] = name2;
	t->w[0] = XtVaCreateManagedWidget(
	    name1, smeBSBObjectClass, menu,
	    XtNleftMargin, 20,
	    XtNleftBitmap, t->value ? dot : None,
	    NULL);
	XtAddCallback(t->w[0], XtNcallback, toggle_callback, (XtPointer) t);
	if (name2 != NULL) {
		t->w[1] = XtVaCreateManagedWidget(
		    name2, smeBSBObjectClass, menu,
		    XtNleftMargin, 20,
		    XtNleftBitmap, t->value ? None : dot,
		    NULL);
		XtAddCallback(t->w[1], XtNcallback, toggle_callback, (XtPointer) t);
	} else
		t->w[1] = NULL;
}

Widget large_font;
Widget small_font;
Widget other_font;
Widget font_shell = NULL;

/*ARGSUSED*/
void
do_newfont(w, userdata, calldata)
Widget w;
XtPointer userdata;
XtPointer calldata;
{
	char *ud = (char *)userdata;

	if (!strcmp(ud, appres.large_font) && !screen_newfont(ud, True)) {
			XtVaSetValues(large_font, XtNleftBitmap, dot, NULL);
			XtVaSetValues(small_font, XtNleftBitmap, None, NULL);
	} else if (!strcmp(ud, appres.small_font) && !screen_newfont(ud, True)) {
			XtVaSetValues(large_font, XtNleftBitmap, None, NULL);
			XtVaSetValues(small_font, XtNleftBitmap, dot, NULL);
	} else if (!screen_newfont(ud, True)) {
			XtVaSetValues(large_font, XtNleftBitmap, None, NULL);
			XtVaSetValues(small_font, XtNleftBitmap, None, NULL);
	}
}

/* Called from the "Select Font" button on the font dialog */
/*ARGSUSED*/
static void
font_button_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *s;

	s = XawDialogGetValueString(client_data);
	if (!s || !*s)
		return;
	XtPopdown(font_shell);
	do_newfont(w, s, (XtPointer) NULL);
}

/*ARGSUSED*/
void
do_otherfont(w, userdata, calldata)
Widget w;
XtPointer userdata;
XtPointer calldata;
{
	if (font_shell == NULL)
		font_shell = create_form_popup("Font", "Enter Font Name",
		    "Select Font", font_button_callback);
	popup_popup(font_shell, XtGrabExclusive);
}

/* Called to change telnet modes */
/*ARGSUSED*/
static void
linemode_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	net_linemode();
}

/*ARGSUSED*/
static void
charmode_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	net_charmode();
}
 
/* Called to change models */
/*ARGSUSED*/
static void
change_model_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	if (atoi(client_data) == model_num)
		return;
	switch (model_num) {
	    case 2:
		XtVaSetValues(model_2_button, XtNleftBitmap, None, NULL);
		break;
	    case 3:
		XtVaSetValues(model_3_button, XtNleftBitmap, None, NULL);
		break;
	    case 4:
		XtVaSetValues(model_4_button, XtNleftBitmap, None, NULL);
		break;
	    case 5:
		XtVaSetValues(model_5_button, XtNleftBitmap, None, NULL);
		break;
	}
	XtVaSetValues(w, XtNleftBitmap, dot, NULL);
	screen_change_model(client_data);
}

static void
options_menu_init(container, x, y)
Widget container;
Position x, y;
{
	Widget w;
	Widget m;

	dot = XCreateBitmapFromData(display, root_window,
	    (char *) dot_bits, dot_width, dot_height);

	m = XtVaCreatePopupShell(
	    "optionsMenu", simpleMenuWidgetClass, container,
	    menubar_buttons ? XtNlabel : NULL, NULL,
	    NULL);
	if (!menubar_buttons)
		XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);

	w = XtVaCreateManagedWidget(
	    "aboutOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, None,
	    NULL);
	XtAddCallback(w, XtNcallback, show_options, NULL);

	if (!menubar_buttons) {
		XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
		keypad_option_button = XtVaCreateManagedWidget(
		    "keypadOption", smeBSBObjectClass, m,
		    XtNleftMargin, 20,
		    XtNleftBitmap, keypad || keypad_popped ? dot : None,
		    NULL);
		XtAddCallback(keypad_option_button, XtNcallback, toggle_keypad,
		    (XtPointer)NULL);
	}

	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	toggle_init(m, MONOCASE, "monocaseOption", NULL);
	toggle_init(m, BLINK, "blinkOption", NULL);
	toggle_init(m, TIMING, "timingOption", NULL);
	toggle_init(m, CURSORP, "trackOption", NULL);
	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	toggle_init(m, TRACE3270, "traceDsOption", NULL);
	toggle_init(m, TRACETN, "traceTelnetOption", NULL);
	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	toggle_init(m, ALTCURSOR, "underlineOption", "blockOption");

	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	large_font = XtVaCreateManagedWidget(
	    "largeFontOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, !strcmp(efontname, appres.large_font) ? dot : None,
	    NULL);
	XtAddCallback(large_font, XtNcallback, do_newfont, appres.large_font);
	small_font = XtVaCreateManagedWidget(
	    "smallFontOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, !strcmp(efontname, appres.small_font) ? dot : None,
	    NULL);
	XtAddCallback(small_font, XtNcallback, do_newfont, appres.small_font);
	other_font = XtVaCreateManagedWidget(
	    "otherFontOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    NULL);
	XtAddCallback(other_font, XtNcallback, do_otherfont, NULL);

	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	linemode_button = XtVaCreateManagedWidget(
	    "lineModeOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, linemode ? dot : None,
	    XtNsensitive, IN_ANSI,
	    NULL);
	XtAddCallback(linemode_button, XtNcallback, linemode_callback, NULL);
	charmode_button = XtVaCreateManagedWidget(
	    "characterModeOption", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, linemode ? None : dot,
	    XtNsensitive, IN_ANSI,
	    NULL);
	XtAddCallback(charmode_button, XtNcallback, charmode_callback, NULL);

	XtCreateManagedWidget("space", smeLineObjectClass, m, NULL, 0);
	model_2_button = XtVaCreateManagedWidget(
	    "model2Option", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, model_num == 2 ? dot : None,
	    XtNsensitive, !PCONNECTED,
	    NULL);
	XtAddCallback(model_2_button, XtNcallback, change_model_callback, "2");
	model_3_button = XtVaCreateManagedWidget(
	    "model3Option", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, model_num == 3 ? dot : None,
	    XtNsensitive, !PCONNECTED,
	    NULL);
	XtAddCallback(model_3_button, XtNcallback, change_model_callback, "3");
	model_4_button = XtVaCreateManagedWidget(
	    "model4Option", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, model_num == 4 ? dot : None,
	    XtNsensitive, !PCONNECTED,
	    NULL);
	XtAddCallback(model_4_button, XtNcallback, change_model_callback, "4");
	model_5_button = XtVaCreateManagedWidget(
	    "model5Option", smeBSBObjectClass, m,
	    XtNleftMargin, 20,
	    XtNleftBitmap, model_num == 5 ? dot : None,
	    XtNsensitive, !PCONNECTED,
	    NULL);
	XtAddCallback(model_5_button, XtNcallback, change_model_callback, "5");

	if (menubar_buttons) {
		(void) XtVaCreateManagedWidget(
		    "optionsMenuButton", menuButtonWidgetClass, container,
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
menubar_retoggle(t)
struct toggle *t;
{
	XtVaSetValues(t->w[0],
	    XtNleftBitmap, t->value ? dot : None,
	    NULL);
	if (t->w[1] != NULL)
		XtVaSetValues(t->w[1],
		    XtNleftBitmap, t->value ? None : dot,
		    NULL);
}

void
handle_menu(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	Widget c;

	if (*num_params != 1) {
		XtWarning("HandleMenu must have one argument");
		return;
	}
	if (!(c = XtNameToWidget(menu_bar, params[0]))) {
		xs_warning("HandleMenu: can't find menu %s", params[0]);
		return;
	}
	XtCallActionProc(menu_bar, "XawPositionSimpleMenu", event, params, 1);
	XtCallActionProc(menu_bar, "MenuPopup", event, params, 1);
}



/*
 * Host file support
 */

static char *
stoken(s)
char **s;
{
	char *r;
	char *ss = *s;

	if (!*ss)
		return NULL;
	r = ss;
	while (*ss && *ss != ' ' && *ss != '\t')
		ss++;
	if (*ss) {
		*ss++ = '\0';
		while (*ss == ' ' || *ss == '\t')
			ss++;
	}
	*s = ss;
	return r;
}

/*
 * Read the host file
 */
void
hostfile_init()
{
	FILE *hf;
	char buf[1024];

	hf = fopen(appres.hostsfile, "r");
	if (!hf) {
		xs_warning("Can't read hostsFile '%s'", appres.hostsfile);
		return;
	}

	while (fgets(buf, 1024, hf)) {
		char *s = buf;
		char *name, *entry_type, *hostname;
		struct host *h;

		if (strlen(buf) > (unsigned) 1)
			buf[strlen(buf) - 1] = '\0';
		while (isspace(*s))
			s++;
		if (!*s || *s == '#')
			continue;
		name = stoken(&s);
		entry_type = stoken(&s);
		hostname = stoken(&s);
		if (!name || !entry_type || !hostname) {
			XtWarning("Bad hostsFile syntax, entry skipped");
			continue;
		}
		h = (struct host *)XtMalloc(sizeof(*h));
		h->name = XtNewString(name);
		h->hostname = XtNewString(hostname);
		if (!strcmp(entry_type, "primary"))
			h->entry_type = PRIMARY;
		else
			h->entry_type = ALIAS;
		if (*s)
			h->loginstring = XtNewString(s);
		else
			h->loginstring = (char *)0;
		h->next = (struct host *)0;
		if (last_host)
			last_host->next = h;
		else
			hosts = h;
		last_host = h;
	}
	(void) fclose(hf);
}

/*
 * Look up a host in the list.  Turns aliases into real hostnames, and
 * finds loginstrings.
 */
int
hostfile_lookup(name, hostname, loginstring)
char *name;
char **hostname;
char **loginstring;
{
	struct host *h;

	for (h = hosts; h; h = h->next)
		if (! strcmp(name, h->name)) {
			*hostname = h->hostname;
			*loginstring = h->loginstring;
			return 1;
		}
	return 0;
}
