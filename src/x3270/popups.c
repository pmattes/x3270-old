/*
 * Copyright 1993, 1994, 1995, 1999, 2000 by Paul Mattes.
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
#include <stdarg.h>
#include "objects.h"
#include "appres.h"

#include "actionsc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "utilc.h"
#include "xioc.h"

static char vmsgbuf[4096];

static enum form_type forms[] = { FORM_NO_WHITE, FORM_NO_CC, FORM_AS_IS };


/*
 * General popup support
 */

/* Find the parent of a window */
static Window
parent_of(Window w)
{
	Window root, parent, *wchildren;
	unsigned int nchildren;

	XQueryTree(display, w, &root, &parent, &wchildren, &nchildren);
	XFree((char *)wchildren);
	return parent;
}

/*
 * Find the base window (the one with the wmgr decorations) and the virtual
 * root, so we can pop up a window relative to them.
 */
void
toplevel_geometry(Position *x, Position *y, Dimension *width,
    Dimension *height)
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
popup_popup(Widget shell, XtGrabKind grab)
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
void
place_popup(Widget w, XtPointer client_data, XtPointer call_data unused)
{
	Dimension width, height;
	Position x = 0, y = 0;
	Dimension win_width, win_height;
	Dimension popup_width, popup_height;
	enum placement p = *(enum placement *)client_data;

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
PA_confirm_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	Widget w2;

	/* Find the Confirm or Okay button */
	w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(XtParent(w), ObjConfirmButton);
	if (w2 == NULL)
		w2 = XtNameToWidget(w, ObjConfirmButton);
	if (w2 == NULL) {
		xs_warning("confirm: cannot find %s", ObjConfirmButton);
		return;
	}

	/* Call its "notify" event */
	XtCallActionProc(w2, "set", event, params, *num_params);
	XtCallActionProc(w2, "notify", event, params, *num_params);
	XtCallActionProc(w2, "unset", event, params, *num_params);
}

/* Callback for "Cancel" button in data entry popup */
static void
cancel_button_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	XtPopdown((Widget) client_data);
}

/*
 * Callback for text source changes.  Ensures that the dialog text does not
 * contain white space -- especially newlines.
 */
static void
popup_dialog_callback(Widget w, XtPointer client_data,
    XtPointer call_data unused)
{
	static Boolean called_back = False;
	XawTextBlock b, nullb;	/* firstPos, length, ptr, format */
	XawTextPosition pos = 0;
	int front_len = 0;
	int end_len = 0;
	int end_pos = 0;
	int i;
	enum { FRONT, MIDDLE, END } place = FRONT;
	enum form_type *ftp = (enum form_type *)client_data;

	if (*ftp == FORM_AS_IS)
		return;

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
		for (i = 0; i < b.length; i++) {
			char c;

			c = *(b.ptr + i);
			if (isspace(c) && (*ftp != FORM_NO_CC || c != ' ')) {
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
		}
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
create_form_popup(const char *name, XtCallbackProc callback,
    XtCallbackProc callback2, enum form_type form_type)
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
	XtAddCallback(shell, XtNpopupCallback, place_popup, (XtPointer)CenterP);

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

	if (form_type == FORM_AS_IS)
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
		XtAddCallback(w, XtNcallback, popup_dialog_callback,
		    &forms[(int)form_type]);

	return shell;
}


/*
 * Read-only popups.
 */
struct rop {
	const char *name;			/* resource name */
	XtGrabKind grab;			/* grab kind */
	Boolean is_error;			/* is it? */
	const char *itext;			/* initial text */
	Widget shell;				/* pop-up shell */
	Widget form;				/* dialog form */
	Widget cancel_button;			/* cancel button */
	abort_callback_t *cancel_callback;	/* callback for cancel button */
	Boolean visible;			/* visibility flag */
};

static struct rop error_popup = {
	"errorPopup", XtGrabExclusive, True,
	"first line\nsecond line",
	NULL, NULL, NULL, NULL,
	False
};
static struct rop info_popup = {
	"infoPopup", XtGrabNonexclusive, False,
	"first line\nsecond line",
	NULL, NULL, NULL, NULL,
	False
};

static struct rop printer_error_popup = {
	"printerErrorPopup", XtGrabExclusive, True,
	"first line\nsecond line\nthird line\nfourth line",
	NULL, NULL, NULL, NULL, False
};
static struct rop printer_info_popup = {
	"printerInfoPopup", XtGrabNonexclusive, False,
	"first line\nsecond line\nthird line\nfourth line",
	NULL,
	NULL, NULL, NULL, False
};

/* Called when OK is pressed in a read-only popup */
static void
rop_ok(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	struct rop *rop = (struct rop *)client_data;

	XtPopdown(rop->shell);
}

/* Called when Cancel is pressed in a read-only popup */
static void
rop_cancel(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	struct rop *rop = (struct rop *)client_data;

	XtPopdown(rop->shell);
	if (rop->cancel_callback != NULL)
		(*rop->cancel_callback)();
}

/* Called when a read-only popup is closed */
static void
rop_popdown(Widget w unused, XtPointer client_data, XtPointer call_data unused)
{
	struct rop *rop = (struct rop *)client_data;

	rop->visible = False;
	if (exiting && rop->is_error)
		x3270_exit(1);
}

/* Initialize a read-only pop-up. */
void
rop_init(struct rop *rop)
{
	Widget w;

	if (rop->shell != NULL)
		return;

	rop->shell = XtVaCreatePopupShell(
	    rop->name, transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(rop->shell, XtNpopupCallback, place_popup,
	    (XtPointer) CenterP);
	XtAddCallback(rop->shell, XtNpopdownCallback, rop_popdown, rop);

	/* Create a dialog in the popup */
	rop->form = XtVaCreateManagedWidget(
	    ObjDialog, dialogWidgetClass, rop->shell,
	    NULL);
	XtVaSetValues(XtNameToWidget(rop->form, XtNlabel),
	    XtNlabel, rop->itext,
	    NULL);

	/* Add "OK" button to the dialog */
	w = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, rop->form,
	    NULL);
	XtAddCallback(w, XtNcallback, rop_ok, rop);

	/* Add an unmapped "Cancel" button to the dialog */
	rop->cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, rop->form,
	    XtNright, w,
	    XtNmappedWhenManaged, False,
	    NULL);
	XtAddCallback(rop->cancel_button, XtNcallback, rop_cancel, rop);

	/* Force it into existence so it sizes itself with 4-line text */
	XtRealizeWidget(rop->shell);
}

/* Pop up a dialog.  Common logic for all forms. */
static void
popup_rop(struct rop *rop, abort_callback_t *a, const char *fmt, va_list args)
{
	(void) vsprintf(vmsgbuf, fmt, args);
	if (!rop->shell) {
		(void) fprintf(stderr, "%s: %s\n", programname, vmsgbuf);
		exit(1);
	}

	if (rop->is_error && sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	}

	XtVaSetValues(rop->form, XtNlabel, vmsgbuf, NULL);
	if (a != NULL)
		XtMapWidget(rop->cancel_button);
	else
		XtUnmapWidget(rop->cancel_button);
	rop->cancel_callback = a;
	if (!rop->visible) {
		if (rop->is_error)
			ring_bell();
		rop->visible = True;
		popup_popup(rop->shell, rop->grab);
	}
}

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(&error_popup, NULL, fmt, args);
	va_end(args);
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
	va_list args;
	char *s;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	s = XtNewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error(s);
	XtFree(s);
}

/* Pop up an info dialog. */
void
popup_an_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(&info_popup, NULL, fmt, args);
	va_end(args);
}

/* Initialization. */

void
error_popup_init(void)
{
	rop_init(&error_popup);
}

void
info_popup_init(void)
{
	rop_init(&info_popup);
}

void
printer_popup_init(void)
{
	if (printer_error_popup.shell != NULL)
		return;
	rop_init(&printer_error_popup);
	rop_init(&printer_info_popup);
}

/* Query. */
Boolean
error_popup_visible(void)
{
	return error_popup.visible;
}

/*
 * Printer pop-up.
 * Allows both error and info popups, and a cancel button.
 *   is_err	If True, this is an error pop-up.  If false, this is an info
 *		pop-up.
 *   a		If non-NULL, the Cancel button is enabled, and this is the
 *		callback function for it.  If NULL, there will be no Cancel
 *		button.
 *   fmt...	printf()-like format and arguments.
 */
void
popup_printer_output(Boolean is_err, abort_callback_t *a, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	popup_rop(is_err? &printer_error_popup: &printer_info_popup, a, fmt,
	    args);
	va_end(args);
}

/*
 * Script actions
 */
void
Info_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	action_debug(Info_action, event, params, num_params);
	if (check_usage(Info_action, *num_params, 1, 1) < 0)
		return;
	popup_an_info(params[0]);
}
