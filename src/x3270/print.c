/*
 * Copyright 1994, 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	print.c
 *		Screen printing functions.
 */

#include "globals.h"

#include "appres.h"
#include "3270ds.h"
#include "ctlr.h"

#include "ctlrc.h"
#include "tablesc.h"

#if defined(X3270_DISPLAY) /*[*/
#include <errno.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>

#include "objects.h"
#include "resources.h"

#include "actionsc.h"
#include "popupsc.h"
#include "printc.h"
#include "utilc.h"

/* Statics */

static Widget print_text_shell = (Widget)NULL;
static Widget print_window_shell = (Widget)NULL;
static char *print_window_command = CN;

#endif /*]*/


/* Print Text popup */

/*
 * Print the ASCIIfied contents of the screen onto a stream.
 * Returns True if anything printed, False otherwise.
 */
Boolean
fprint_screen(FILE *f, Boolean even_if_empty)
{
	register int i;
	unsigned char e;
	char c;
	int ns = 0;
	int nr = 0;
	Boolean any = False;
	unsigned char fa = *get_field_attribute(0);

	for (i = 0; i < ROWS*COLS; i++) {
		if (i && !(i % COLS)) {
			nr++;
			ns = 0;
		}
		e = screen_buf[i];
		if (IS_FA(e)) {
			c = ' ';
			fa = screen_buf[i];
		}
		if (FA_IS_ZERO(fa))
			c = ' ';
		else
			c = cg2asc[e];
		if (c == ' ')
			ns++;
		else {
			any = True;
			while (nr) {
				(void) fputc('\n', f);
				nr--;
			}
			while (ns) {
				(void) fputc(' ', f);
				ns--;
			}
			(void) fputc(c, f);
		}
	}
	nr++;
	if (!any && !even_if_empty)
		return False;
	while (nr) {
		(void) fputc('\n', f);
		nr--;
	}
	return True;
}

#if defined(X3270_DISPLAY) /*[*/

/* Termination code for print text process. */
static void
print_text_done(FILE *f, Boolean do_popdown)
{
	int status;

	status = pclose(f);
	if (status) {
		popup_an_error("Print program exited with status %d.",
		    (status & 0xff00) > 8);
	} else {
		if (do_popdown)
			XtPopdown(print_text_shell);
		if (appres.do_confirms)
			popup_an_info("Screen image printed.");
	}

}

/* Callback for "OK" button on print text popup. */
static void
print_text_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	char *filter;
	FILE *f;

	filter = XawDialogGetValueString((Widget)client_data);
	if (!filter) {
		XtPopdown(print_text_shell);
		return;
	}
	if (!(f = popen(filter, "w"))) {
		popup_an_errno(errno, "popen(%s)", filter);
		return;
	}
	(void) fprint_screen(f, True);
	print_text_done(f, True);
}

/* Print the contents of the screen as text. */
void
PrintText_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	char *filter = get_resource(ResPrintTextCommand);
	Boolean secure = appres.secure;

	action_debug(PrintText_action, event, params, num_params);
	if (*num_params > 0)
		filter = params[0];
	if (*num_params > 1)
		xs_warning("%s: extra arguments ignored",
		    action_name(PrintText_action));
	if (filter == CN) {
		xs_warning("%s: no %s defined",
		    action_name(PrintText_action), ResPrintTextCommand);
		return;
	}
	if (filter[0] == '@') {
		filter++;
		secure = True;
	}
	if (secure) {
		FILE *f;

		if (!(f = popen(filter, "w"))) {
			popup_an_errno(errno, "popen(%s)", filter);
			return;
		}
		(void) fprint_screen(f, True);
		print_text_done(f, False);
		return;
	}
	if (print_text_shell == NULL)
		print_text_shell = create_form_popup("PrintText",
		    print_text_callback, (XtCallbackProc)NULL, FORM_AS_IS);
	XtVaSetValues(XtNameToWidget(print_text_shell, ObjDialog),
	    XtNvalue, filter,
	    NULL);
	popup_popup(print_text_shell, XtGrabExclusive);
}

#if defined(X3270_MENUS) /*[*/
/* Callback for Print Text menu option. */
void
print_text_option(Widget w, XtPointer client_data unused,
    XtPointer call_data unused)
{
	Cardinal zero = 0;

	PrintText_action(w, (XEvent *)NULL, (String *)NULL, &zero);
}
#endif /*]*/


/* Print Window popup */

/*
 * Printing the window bitmap is a rather convoluted process:
 *    The PrintWindow action calls PrintWindow_action(), or a menu option calls
 *	print_window_option().
 *    print_window_option() pops up the dialog.
 *    The OK button on the dialog triggers print_window_callback.
 *    print_window_callback pops down the dialog, then schedules a timeout
 *     1 second away.
 *    When the timeout expires, it triggers snap_it(), which finally calls
 *     xwd.
 * The timeout indirection is necessary because xwd prints the actual contents
 * of the window, including any pop-up dialog in front of it.  We pop down the
 * dialog, but then it is up to the server and Xt to send us the appropriate
 * expose events to repaint our window.  Hopefully, one second is enough to do
 * that.
 */

/* Termination procedure for window print. */
static void
print_window_done(int status)
{
	if (status)
		popup_an_error("Print program exited with status %d.",
		    (status & 0xff00) >> 8);
	else if (appres.do_confirms)
		popup_an_info("Bitmap printed.");
}

/* Timeout callback for window print. */
static void
snap_it(XtPointer closure unused, XtIntervalId *id unused)
{
	if (!print_window_command)
		return;
	XSync(display, 0);
	print_window_done(system(print_window_command));
	print_window_command = CN;
}

/* Callback for "OK" button on print window popup. */
static void
print_window_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	print_window_command = XawDialogGetValueString((Widget)client_data);
	XtPopdown(print_window_shell);
	if (print_window_command)
		(void) XtAppAddTimeOut(appcontext, 1000, snap_it, 0);
}

/* Print the contents of the screen as a bitmap. */
void
PrintWindow_action(Widget w unused, XEvent *event, String *params,
    Cardinal *num_params)
{
	char *filter = get_resource(ResPrintWindowCommand);
	char *fb = XtMalloc(strlen(filter) + 16);
	char *xfb = fb;
	Boolean secure = appres.secure;

	action_debug(PrintWindow_action, event, params, num_params);
	if (*num_params > 0)
		filter = params[0];
	if (*num_params > 1)
		xs_warning("%s: extra arguments ignored",
		    action_name(PrintWindow_action));
	if (filter == CN) {
		xs_warning("%s: no %s defined",
		    action_name(PrintWindow_action), ResPrintWindowCommand);
		return;
	}
	(void) sprintf(fb, filter, XtWindow(toplevel));
	if (fb[0] == '@') {
		secure = True;
		xfb = fb + 1;
	}
	if (secure) {
		print_window_done(system(xfb));
		Free(fb);
		return;
	}
	if (print_window_shell == NULL)
		print_window_shell = create_form_popup("printWindow",
		    print_window_callback, (XtCallbackProc)NULL, False);
	XtVaSetValues(XtNameToWidget(print_window_shell, ObjDialog),
	    XtNvalue, fb,
	    NULL);
	popup_popup(print_window_shell, XtGrabExclusive);
}

#if defined(X3270_MENUS) /*[*/
/* Callback for menu Print Window option. */
void
print_window_option(Widget w, XtPointer client_data unused,
    XtPointer call_data unused)
{
	Cardinal zero = 0;

	PrintWindow_action(w, (XEvent *)NULL, (String *)NULL, &zero);
}
#endif /*]*/

#endif /*]*/
