/*
 * Copyright 1993, 1994, 1995 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	popups.c
 *		This module handles pop-up dialogs: errors, host names,
 *		font names, information.
 */

#include "globals.h"
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Text.h>
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "objects.h"

static char vmsgbuf[4096];


/*
 * General popup support
 */

/* Find the parent of a window */
static Window
parent_of(w)
Window w;
{
	Window root, parent, *children;
	unsigned int nchildren;

	XQueryTree(display, w, &root, &parent, &children, &nchildren);
	XFree((char *) children);
	return parent;
}

/*
 * Find the base window (the one with the wmgr decorations) and the virtual
 * root, so we can pop up a window relative to them.
 */
void
toplevel_geometry(x, y, width, height)
Dimension *x, *y, *width, *height;
{
	Window tlw = XtWindow(toplevel);
	Window win;
	Window parent;
	int nw;
	struct {
		Window w;
		XWindowAttributes wa;
	} ancestor[10];
	XWindowAttributes wa, *base_wa, *root_wa;

	/*
	 * Trace the family tree of toplevel.
	 */
	for (win = tlw, nw = 0; ; win = parent) {
		parent = parent_of(win);
		ancestor[nw].w = parent;
		XGetWindowAttributes(display, parent, &ancestor[nw].wa);
		++nw;
		if (parent == root_window)
			break;
	}

	/*
	 * Figure out if they're running a virtual desktop, by seeing if
	 * the 1st child of root is bigger than it is.  If so, pretend that
	 * the virtual desktop is the root.
	 */
	if (nw > 1 &&
	    (ancestor[nw-2].wa.width > ancestor[nw-1].wa.width ||
	     ancestor[nw-2].wa.height > ancestor[nw-1].wa.height))
		--nw;
	root_wa = &ancestor[nw-1].wa;

	/*
	 * Now identify the base window as the window below the root
	 * window.
	 */
	if (nw >= 2) {
		base_wa = &ancestor[nw-2].wa;
	} else {
		XGetWindowAttributes(display, tlw, &wa);
		base_wa = &wa;
	}

	*x = base_wa->x + root_wa->x;
	*y = base_wa->y + root_wa->y;
	*width = base_wa->width + 2*base_wa->border_width;
	*height = base_wa->height + 2*base_wa->border_width;
}

/* Pop up a popup shell */
void
popup_popup(shell, grab)
Widget shell;
XtGrabKind grab;
{
	XtPopup(shell, grab);
	XSetWMProtocols(display, XtWindow(shell), &a_delete_me, 1);
}

static enum placement CenterD = Center;
enum placement *CenterP = &CenterD;
static enum placement BottomD = Bottom;
enum placement *BottomP = &BottomD;
static enum placement LeftD = Left;
enum placement *LeftP = &LeftD;
static enum placement RightD = Right;
enum placement *RightP = &RightD;

/* Place a popped-up shell */
/*ARGSUSED*/
void
place_popup(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	Dimension width, height;
	Dimension x = 0, y = 0;
	Dimension win_width, win_height;
	Dimension popup_width, popup_height;
	enum placement p = *(enum placement *)client_data;
	extern Dimension main_width;

	/* Get and fix the popup's dimensions */
	XtRealizeWidget(w);
	XtVaGetValues(w,
	    XtNwidth, &width,
	    XtNheight, &height,
	    NULL);
	XtVaSetValues(w,
	    XtNheight, height,
	    XtNwidth, width,
	    XtNbaseHeight, height,
	    XtNbaseWidth, width,
	    XtNminHeight, height,
	    XtNminWidth, width,
	    XtNmaxHeight, height,
	    XtNmaxWidth, width,
	    NULL);

	/*
	 * Find the geometry of the parent of the toplevel window in order to
	 * place the popup window.  This is done with an Xlib call (asking the
	 * server explicitly) rather than the Xt call.  Why?  At startup,
	 * Xt may not yet know the parent window has been moved by the window
	 * manager.
	 */
	toplevel_geometry(&x, &y, &win_width, &win_height);

	switch (p) {
	    case Center:
		XtVaGetValues(w,
		    XtNwidth, &popup_width,
		    XtNheight, &popup_height,
		    NULL);
		XtVaSetValues(w,
		    XtNx, x + (win_width-popup_width) / (unsigned) 2,
		    XtNy, y + (win_height-popup_height) / (unsigned) 2,
		    NULL);
		break;
	    case Bottom:
		XtVaSetValues(w,
		    XtNx, x,
		    XtNy, y + win_height + 2,
		    NULL);
		break;
	    case Left:
		XtVaGetValues(w,
		    XtNwidth, &popup_width,
		    NULL);
		XtVaSetValues(w,
		    XtNx, x - popup_width - (win_width - main_width) - 2,
		    XtNy, y,
		    NULL);
		break;
	    case Right:
		XtVaSetValues(w,
		    XtNx, x + win_width + 2,
		    XtNy, y,
		    NULL);
		break;
	}
}

/* Action called when "Return" is pressed in data entry popup */
void
Confirm(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	Widget w2;

	/* Find the Confirm or Okay button */
	w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(XtParent(w), ObjOkayButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(w, ObjOkayButton);
	if (w2 == NULL) {
		xs_warning("Confirm: cannot find %s or %s", ObjConfirmButton,
		    ObjOkayButton);
		return;
	}

	/* Call its "notify" event */
	XtCallActionProc(w2, "set", event, params, *num_params);
	XtCallActionProc(w2, "notify", event, params, *num_params);
	XtCallActionProc(w2, "unset", event, params, *num_params);
}

/* Callback for "Cancel" button in data entry popup */
/*ARGSUSED*/
static void
cancel_button_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	XtPopdown((Widget) client_data);
}

/*
 * Callback for text source changes.  Ensures that the dialog text does not
 * contain white space -- especially newlines.
 */
/*ARGSUSED*/
static void
popup_dialog_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	static Boolean called_back = False;
	XawTextBlock b, nullb;	/* firstPos, length, ptr, format */
	XawTextPosition pos = 0;
	int front_len = 0;
	int end_len = 0;
	int end_pos = 0;
	int i;
	enum { FRONT, MIDDLE, END } place = FRONT;

	if (called_back)
		return;
	else
		called_back = True;

	nullb.firstPos = 0;
	nullb.length = 0;
	nullb.ptr = NULL;

	/*
	 * Scan the text for whitespace.  Leading whitespace is deleted;
	 * embedded whitespace causes the rest of the text to be deleted.
	 */
	while (1) {
		XawTextSourceRead(w, pos, &b, 1024);
		if (b.length <= 0)
			break;
		nullb.format = b.format;
		if (place == END) {
			end_len += b.length;
			continue;
		}
		for (i = 0; i < b.length; i++)
			if (isspace(*(b.ptr + i))) {
				if (place == FRONT) {
					front_len++;
					continue;
				} else {
					end_pos = b.firstPos + i;
					end_len = b.length - i;
					place = END;
					break;
				}
			} else
				place = MIDDLE;
		pos += b.length;
		if (b.length < 1024)
			break;
	}
	if (front_len)
		XawTextSourceReplace(w, 0, front_len, &nullb);
	if (end_len)
		XawTextSourceReplace(w, end_pos - front_len,
		    end_pos - front_len + end_len, &nullb);
	called_back = False;
}

/* Create a simple data entry popup */
Widget
create_form_popup(name, callback, callback2, no_spaces)
char *name;
XtCallbackProc callback;
XtCallbackProc callback2;
Boolean no_spaces;
{
	char *widgetname;
	Widget shell;
	Widget dialog;
	Widget w;

	/* Create the popup shell */

	widgetname = xs_buffer("%sPopup", name);
	if (isupper(widgetname[0]))
		widgetname[0] = tolower(widgetname[0]);
	shell = XtVaCreatePopupShell(
	    widgetname, transientShellWidgetClass, toplevel,
	    NULL);
	XtFree(widgetname);
	XtAddCallback(shell, XtNpopupCallback, place_popup, (XtPointer) CenterP);

	/* Create a dialog in the popup */

	dialog = XtVaCreateManagedWidget(
	    ObjDialog, dialogWidgetClass, shell,
	    XtNvalue, "",
	    NULL);
	XtVaSetValues(XtNameToWidget(dialog, XtNlabel),
	    NULL);

	/* Add "Confirm" and "Cancel" buttons to the dialog */

	w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, dialog,
	    NULL);
	XtAddCallback(w, XtNcallback, callback, (XtPointer)dialog);
	if (callback2) {
		w = XtVaCreateManagedWidget(
			ObjConfirm2Button, commandWidgetClass, dialog,
			NULL);
		XtAddCallback(w, XtNcallback, callback2, (XtPointer)dialog);
	}
	w = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, dialog,
	    NULL);
	XtAddCallback(w, XtNcallback, cancel_button_callback, (XtPointer) shell);

	if (!no_spaces)
		return shell;

	/* Modify the translations for the objects in the dialog */

	w = XtNameToWidget(dialog, XtNvalue);
	if (w == NULL)
		xs_warning("Cannot find \"%s\" in dialog", XtNvalue);

	/* Set a callback for text modifications */
	w = XawTextGetSource(w);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, popup_dialog_callback, NULL);

	return shell;
}


/*
 * Error popup
 */

static Widget error_shell = NULL;
static Widget error_form;
Boolean error_popup_visible;

/* Called when OK is pressed on the error popup */
/*ARGSUSED*/
static void
saw_error(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	XtPopdown(error_shell);
}

/* Called when the error popup is closed */
/*ARGSUSED*/
static void
error_popdown(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	error_popup_visible = False;
	if (exiting)
		x3270_exit(1);
}

void
error_popup_init()
{
	Widget w;

	if (error_shell != NULL)
		return;

	error_shell = XtVaCreatePopupShell(
	    "errorPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(error_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(error_shell, XtNpopdownCallback, error_popdown, PN);

	/* Create a dialog in the popup */

	error_form = XtVaCreateManagedWidget(
	    "errorDialog", dialogWidgetClass, error_shell,
	    NULL);
	XtVaSetValues(XtNameToWidget(error_form, XtNlabel),
	    XtNlabel, "first line\nsecond line",
	    NULL);

	/* Add "OK" button to the dialog */

	w = XtVaCreateManagedWidget(
	    ObjOkayButton, commandWidgetClass, error_form,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_error, 0);

	/* Force it into existence so it sizes itself with 2-line text */

	XtRealizeWidget(error_shell);
}

/* Pop up an error dialog. */
void
#if defined(__STDC__)
popup_an_error(char *fmt, ...)
#else
popup_an_error(va_alist)
va_dcl
#endif
{
	va_list args;

#if defined(__STDC__)
	va_start(args, fmt);
#else
	char *fmt;
	va_start(args);
	fmt = va_arg(args, char *);
#endif

	(void) vsprintf(vmsgbuf, fmt, args);
	if (!error_shell) {
		(void) fprintf(stderr, "%s: %s\n", programname, vmsgbuf);
		exit(1);
	}

	if (scripting()) {
		script_error(vmsgbuf);
		return;
	}

	XtVaSetValues(error_form, XtNlabel, vmsgbuf, NULL);
	if (!error_popup_visible) {
		ring_bell();
		error_popup_visible = True;
		popup_popup(error_shell, XtGrabExclusive);
	}
}

/* Pop up an error dialog, based on an error number. */
void
#if defined(__STDC__)
popup_an_errno(int errn, char *fmt, ...)
#else
popup_an_errno(va_alist)
va_dcl
#endif
{
	va_list args;
	char *s;
	extern int sys_nerr;
#if !defined(__FreeBSD__)
	extern char *sys_errlist[];
#endif

#if defined(__STDC__)
	va_start(args, fmt);
#else
	int errn;
	char *fmt;
	va_start(args);
	errn = va_arg(args, int);
	fmt = va_arg(args, char *);
#endif
	(void) vsprintf(vmsgbuf, fmt, args);
	s = XtNewString(vmsgbuf);

	if (errn > 0) {
		if (errn < sys_nerr)
			popup_an_error("%s:\n%s", s, sys_errlist[errn]);
		else
			popup_an_error("%s:\nError %d", s, errn);
	} else
		popup_an_error(s);
	XtFree(s);
}


/*
 * Info popup
 */

static Widget info_shell = NULL;
static Widget info_form;
Boolean info_popup_visible;

/* Called when OK is pressed on the info popup */
/*ARGSUSED*/
static void
saw_info(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	XtPopdown(info_shell);
}

/* Called when the info popup is closed */
/*ARGSUSED*/
static void
info_popdown(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	info_popup_visible = False;
}

void
info_popup_init()
{
	Widget w;

	if (info_shell != NULL)
		return;

	info_shell = XtVaCreatePopupShell(
	    "infoPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(info_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(info_shell, XtNpopdownCallback, info_popdown, PN);

	/* Create a dialog in the popup */

	info_form = XtVaCreateManagedWidget(
	    "infoDialog", dialogWidgetClass, info_shell,
	    NULL);
	XtVaSetValues(XtNameToWidget(info_form, XtNlabel),
	    XtNlabel, "first line\nsecond line",
	    NULL);

	/* Add "OK" button to the dialog */

	w = XtVaCreateManagedWidget(
	    ObjOkayButton, commandWidgetClass, info_form,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_info, 0);

	/* Force it into existence so it sizes itself with 2-line text */

	XtRealizeWidget(info_shell);
}

/* Pop up an info dialog. */
void
#if defined(__STDC__)
popup_an_info(char *fmt, ...)
#else
popup_an_info(va_alist)
va_dcl
#endif
{
	va_list args;

#if defined(__STDC__)
	va_start(args, fmt);
#else
	char *fmt;
	va_start(args);
	fmt = va_arg(args, char *);
#endif

	(void) vsprintf(vmsgbuf, fmt, args);
	if (!info_shell) {
		(void) printf("%s: %s\n", programname, vmsgbuf);
		return;
	}

	XtVaSetValues(info_form, XtNlabel, vmsgbuf, NULL);
	if (!info_popup_visible) {
		info_popup_visible = True;
		popup_popup(info_shell, XtGrabNonexclusive);
	}
}


/*
 * Options popup
 */

static Widget options_shell = NULL;
static Widget options_form;
extern time_t ns_time;
extern int ns_brcvd;
extern int ns_rrcvd;
extern int ns_bsent;
extern int ns_rsent;
extern int linemode;
extern char *build;
extern Pixmap icon;
extern struct trans_list *trans_list;
extern struct trans_list *temp_keymaps;

/* Called when OK is pressed on the options popup */
/*ARGSUSED*/
static void
saw_options(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	XtPopdown(options_shell);
}

/* Called when the options popup is popped down */
/*ARGSUSED*/
static void
destroy_options(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	XtDestroyWidget(options_shell);
	options_shell = NULL;
}

/* Return a time difference in English */
static char *
hms(ts)
time_t ts;
{
	time_t t, td;
	long hr, mn, sc;
	static char buf[128];

	(void) time(&t);

	td = t - ts;
	hr = td / 3600;
	mn = (td % 3600) / 60;
	sc = td % 60;

	if (hr > 0)
		(void) sprintf(buf, "%ld %s %ld %s %ld %s",
		    hr, (hr == 1) ? get_message("hour") : get_message("hours"),
		    mn, (mn == 1) ? get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ? get_message("second") : get_message("seconds"));
	else if (mn > 0)
		(void) sprintf(buf, "%ld %s %ld %s",
		    mn, (mn == 1) ? get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ? get_message("second") : get_message("seconds"));
	else
		(void) sprintf(buf, "%ld %s",
		    sc, (sc == 1) ? get_message("second") : get_message("seconds"));

	return buf;
}

#define MAKE_SMALL(label, n) { \
	w_prev = w; \
	w = XtVaCreateManagedWidget( \
	    ObjSmallLabel, labelWidgetClass, options_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w, \
	    XtNleft, XtChainLeft, \
	    XtNvertDistance, (n), \
	    NULL); \
	vd = n; \
	}

#define MAKE_LABEL(label, n) { \
	w_prev = w; \
	w = XtVaCreateManagedWidget( \
	    ObjNameLabel, labelWidgetClass, options_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w, \
	    XtNfromHoriz, left_anchor, \
	    XtNleft, XtChainLeft, \
	    XtNvertDistance, (n), \
	    NULL); \
	vd = n; \
	}

#define MAKE_VALUE(label) { \
	v = XtVaCreateManagedWidget( \
	    ObjDataLabel, labelWidgetClass, options_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w_prev, \
	    XtNfromHoriz, w, \
	    XtNhorizDistance, 0, \
	    XtNvertDistance, vd, \
	    XtNleft, XtChainLeft, \
	    NULL); \
	}

#define MAKE_LABEL2(label) { \
	w = XtVaCreateManagedWidget( \
	    ObjNameLabel, labelWidgetClass, options_form, \
	    XtNborderWidth, 0, \
	    XtNlabel, label, \
	    XtNfromVert, w_prev, \
	    XtNfromHoriz, v, \
	    XtNhorizDistance, 0, \
	    XtNvertDistance, vd, \
	    XtNleft, XtChainLeft, \
	    NULL); \
	}

/* Called when the "Options... About x3270..." button is pressed */
void
popup_options()
{
	Widget w = NULL, w_prev = NULL;
	Widget v = NULL;
	Widget left_anchor = NULL;
	int vd = 4;
	char fbuf[1024];
	char *ftype;
	char *xbuf;

	/* Create the popup */

	options_shell = XtVaCreatePopupShell(
	    "optionsPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(options_shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(options_shell, XtNpopdownCallback, destroy_options,
	    NULL);

	/* Create a form in the popup */

	options_form = XtVaCreateManagedWidget(
	    "optionsForm", formWidgetClass, options_shell,
	    NULL);

	/* Pretty picture */

	left_anchor = XtVaCreateManagedWidget(
	    "icon", labelWidgetClass, options_form,
	    XtNborderWidth, 0,
	    XtNbitmap, icon,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    NULL);

	/* Miscellany */

	MAKE_LABEL(build, 4);
	MAKE_LABEL(get_message("processId"), 4);
	(void) sprintf(fbuf, "%d", getpid());
	MAKE_VALUE(fbuf);
	MAKE_LABEL2(get_message("windowId"));
	(void) sprintf(fbuf, "0x%lx", XtWindow(toplevel));
	MAKE_VALUE(fbuf);

	/* Everything else at the left margin under the bitmap */
	w = left_anchor;
	left_anchor = NULL;

	MAKE_SMALL("Copyright \251 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.\n\
 All Rights Reserved.  GTRC hereby grants public use of this software.  Derivative\n\
 works based on this software must incorporate this copyright notice.\n\
\n\
X11 Port Copyright \251 1990 by Jeff Sparkes.\n\
Additional X11 Modifications Copyright \251 1993, 1994, 1995 by Paul Mattes.\n\
 Permission to use, copy, modify, and distribute this software and its documentation\n\
 for any purpose and without fee is hereby granted, provided that the above copyright\n\
 notice appear in all copies and that both that copyright notice and this permission\n\
 notice appear in supporting documentation.", 4);

	(void) sprintf(fbuf, "%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.mono ? get_message("mono") :
		(appres.m3279 ? get_message("fullColor") :
		    get_message("pseudoColor")),
	    (appres.extended && !std_ds_host) ? get_message("extendedDs") :
		get_message("standardDs"));
	MAKE_LABEL(fbuf, 4);

	MAKE_LABEL(get_message("terminalName"), 4);
	MAKE_VALUE(termtype);

	MAKE_LABEL(get_message("emulatorFont"), 4);
	MAKE_VALUE(efontname);
	if (*standard_font) {
		if (*latin1_font)
			ftype = get_message("fullFont");
		else
			ftype = get_message("asciiFont");
	} else
		ftype = get_message("cgFont");
	xbuf = xs_buffer("  %s", ftype);
	MAKE_LABEL(xbuf, 0);
	XtFree(xbuf);

	if (appres.charset) {
		MAKE_LABEL(get_message("characterSet"), 4);
		MAKE_VALUE(appres.charset);
	} else
		MAKE_LABEL(get_message("defaultCharacterSet"), 4);

	if (appres.key_map || temp_keymaps != (struct trans_list *)NULL) {
		struct trans_list *t;

		fbuf[0] = '\0';
		for (t = trans_list; t; t = t->next) {
			if (fbuf[0])
				(void) strcat(fbuf, ",");
			(void) strcat(fbuf, t->name);
		}
		for (t = temp_keymaps; t; t = t->next) {
			if (fbuf[0])
				(void) strcat(fbuf, " ");
			(void) strcat(fbuf, "+");
			(void) strcat(fbuf, t->name);
		}
		MAKE_LABEL(get_message("keyboardMap"), 4)
		MAKE_VALUE(fbuf);
	} else
		MAKE_LABEL(get_message("defaultKeyboardMap"), 4);
	if (appres.compose_map) {
		MAKE_LABEL(get_message("composeMap"), 4);
		MAKE_VALUE(appres.compose_map);
	} else {
		MAKE_LABEL(get_message("noComposeMap"), 4);
	}

	if (appres.active_icon) {
		MAKE_LABEL(get_message("activeIcon"), 4);
		xbuf = xs_buffer("  %s", get_message("iconFont"));
		MAKE_LABEL(xbuf, 0);
		XtFree(xbuf);
		MAKE_VALUE(appres.icon_font);
		if (appres.label_icon) {
			xbuf = xs_buffer("  %s", get_message("iconLabelFont"));
			MAKE_LABEL(xbuf, 0);
			XtFree(xbuf);
			MAKE_VALUE(appres.icon_label_font);
		}
	} else
		MAKE_LABEL(get_message("staticIcon"), 4);

	if (CONNECTED) {
		MAKE_LABEL(get_message("connectedTo"), 4);
		MAKE_VALUE(current_host);
		(void) sprintf(fbuf, "  %s", get_message("port"));
		MAKE_LABEL2(fbuf);
		(void) sprintf(fbuf, "%d", current_port);
		MAKE_VALUE(fbuf);
		if (IN_ANSI) {
			if (linemode)
				ftype = get_message("lineMode");
			else
				ftype = get_message("charMode");
		} else
			ftype = get_message("dsMode");
		(void) sprintf(fbuf, "  %s, ", ftype);
		(void) strcat(fbuf, hms(ns_time));

		MAKE_LABEL(fbuf, 0);

		if (IN_3270)
			(void) sprintf(fbuf, "%s %d %s, %d %s\n%s %d %s, %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ? get_message("byte") : get_message("bytes"),
			    ns_rsent, (ns_rsent == 1) ? get_message("record") : get_message("records"),
			    get_message("Received"),
			    ns_brcvd, (ns_brcvd == 1) ? get_message("byte") : get_message("bytes"),
			    ns_rrcvd, (ns_rrcvd == 1) ? get_message("record") : get_message("records"));
		else
			(void) sprintf(fbuf, "%s %d %s, %s %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ? get_message("byte") : get_message("bytes"),
			    get_message("received"),
			    ns_brcvd, (ns_brcvd == 1) ? get_message("byte") : get_message("bytes"));
		MAKE_LABEL(fbuf, 4);

		if (IN_ANSI) {
			struct ctl_char *c = net_linemode_chars();
			int i;

			MAKE_LABEL(get_message("specialCharacters"), 4);
			for (i = 0; c[i].name; i++) {
				if (!i || !(i % 4)) {
					(void) sprintf(fbuf, "  %s", c[i].name);
					MAKE_LABEL(fbuf, 0);
				} else {
					MAKE_LABEL2(c[i].name);
				}
				MAKE_VALUE(c[i].value);
			}
		}
	} else if (HALF_CONNECTED) {
		MAKE_LABEL(get_message("connectionPending"), 4);
		MAKE_VALUE(current_host);
		MAKE_LABEL(hms(ns_time), 0);
	} else
		MAKE_LABEL(get_message("notConnected"), 4);

	/* Add "OK" button at the lower left */

	w = XtVaCreateManagedWidget(
	    ObjOkayButton, commandWidgetClass, options_form,
	    XtNfromVert, w,
	    XtNleft, XtChainLeft,
	    XtNbottom, XtChainBottom,
	    NULL);
	XtAddCallback(w, XtNcallback, saw_options, 0);

	/* Pop it up */

	popup_popup(options_shell, XtGrabExclusive);
}

#undef MAKE_LABEL
#undef MAKE_VALUE

/*
 * Script actions
 */
/*ARGSUSED*/
void
Info(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(Info, event, params, num_params);
	if (check_usage(Info, *num_params, 1, 1) < 0)
		return;
	popup_an_info(params[0]);
}
