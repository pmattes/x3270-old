/*
 * Copyright 1993, 1994, 1995, 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	idle.c
 *		This module handles the idle command.
 */

#include "globals.h"

#if defined(X3270_SCRIPT) /*[*/

#if defined(X3270_DISPLAY) /*[*/
#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Shell.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/TextSrc.h>
#include <X11/Xaw/TextSink.h>
#include <X11/Xaw/AsciiSrc.h>
#include <X11/Xaw/AsciiSink.h>
#endif /*]*/
#include <errno.h>

#include "appres.h"
#include "dialogc.h"
#include "hostc.h"
#include "idlec.h"
#include "macrosc.h"
#include "objects.h"
#include "popupsc.h"
#include "resources.h"
#include "trace_dsc.h"
#include "utilc.h"

/* Macros. */
#define IDLE_MS		(7L * 60L * 1000L)	/* 7 minutes */

#if defined(X3270_DISPLAY) /*[*/
#define FILE_WIDTH	300	/* width of file name widgets */
#define MARGIN		3	/* distance from margins to widgets */
#define CLOSE_VGAP	0	/* distance between paired toggles */
#define FAR_VGAP	10	/* distance between single toggles and groups */
#define BUTTON_GAP	5	/* horizontal distance between buttons */
#endif /*]*/

#define BN	(Boolean *)NULL

/* Externals. */
#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
extern Pixmap diamond;
extern Pixmap no_diamond;
#endif /*]*/

/* Statics. */
static Boolean idle_was_in3270 = False;
static Boolean idle_enabled = False;	/* Enabled? */
static char *idle_command = CN;
static char *idle_timeout_string = CN;
static unsigned long idle_id;
static unsigned long idle_ms;
static Boolean idle_randomize = False;
static Boolean idle_ticking = False;

static void idle_in3270(Boolean in3270);

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
static Widget idle_dialog, idle_shell, command_value, timeout_value;
static Widget enable_toggle, disable_toggle;
static sr_t *idle_sr = (sr_t *)NULL;

static void idle_cancel(Widget w, XtPointer client_data, XtPointer call_data);
static void idle_popup_callback(Widget w, XtPointer client_data,
    XtPointer call_data);
static void idle_popup_init(void);
static int idle_start(void);
static void okay_callback(Widget w, XtPointer call_parms,
    XtPointer call_data);
static void toggle_enable(Widget w, XtPointer client_data,
    XtPointer call_data);
#endif /*]*/

/* Initialization. */
void
idle_init(void)
{
	/* Register for state changes. */
	register_schange(ST_3270_MODE, idle_in3270);

	/* Seed the random number generator (we seem to be the only user). */
	srandom(time(NULL));
}

/* Process a timeout value. */
int
process_timeout_value(char *t)
{
	char *s = t;
	unsigned long idle_n;
	char *ptr;

	if (s == CN || *s == '\0') {
		idle_ms = IDLE_MS;
		idle_randomize = True;
		return 0;
	}

	if (*s == '~') {
		idle_randomize = True;
		s++;
	}
	idle_n = strtoul(s, &ptr, 0);
	if (idle_n <= 0)
		goto bad_idle;
	switch (*ptr) {
	    case 'H':
	    case 'h':
		idle_n *= 60L * 60L;
		break;
	    case 'M':
	    case 'm':
		idle_n *= 60L;
		break;
	    case 'S':
	    case 's':
		break;
	    default:
		goto bad_idle;
	}
	idle_ms = idle_n * 1000L;
	return 0;

    bad_idle:
	popup_an_error("Invalid idle timeout value '%s'", t);
	idle_ms = 0L;
	idle_randomize = False;
	return -1;
}

/* Called when a host connects or disconnects. */
static void
idle_in3270(Boolean in3270)
{
	if (in3270 && !idle_was_in3270) {
		/* Get the values from resources. */
		char *cmd, *tmo;

		cmd = get_host_fresource("%s", ResIdleCommand);
		tmo = get_host_fresource("%s", ResIdleTimeout);
		if (cmd != CN &&
		    appres.idle_command_enabled &&
		    process_timeout_value(tmo) == 0) {
			Replace(idle_command, NewString(cmd));
			Replace(idle_timeout_string, NewString(tmo));
			idle_enabled = True;
		}
		idle_was_in3270 = True;
	} else {
		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}
		idle_was_in3270 = False;
	}
}

/*
 * Idle timeout.
 */
static void
idle_timeout(void)
{
	trace_event("Idle timeout\n");
	push_macro(idle_command, False);
	reset_idle_timer();
}

/*
 * Reset (and re-enable) the idle timer.  Called when the user presses an AID
 * key.
 */
void
reset_idle_timer(void)
{
	if (idle_ms) {
		unsigned long idle_ms_now;

		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}
		idle_ms_now = idle_ms;
		if (idle_randomize) {
			idle_ms_now = idle_ms;
			if (random() % 2)
				idle_ms_now += random() % (idle_ms / 10L);
			else
				idle_ms_now -= random() % (idle_ms / 10L);
		}
#if defined(DEBUG_IDLE_TIMEOUT) /*[*/
		trace_event("Setting idle timeout to %lu\n", idle_ms_now);
#endif /*]*/
		idle_id = AddTimeOut(idle_ms_now, idle_timeout);
		idle_ticking = True;
	}
}

#if defined(X3270_DISPLAY) && defined(X3270_MENUS) /*[*/
/* "Idle Command" dialog. */

/*
 * Pop up the "Idle" menu.
 * Called back from the "Configure Idle Command" option on the Options menu.
 */
void
popup_idle(void)
{
	/* Initialize it. */
	if (idle_shell == (Widget)NULL)
		idle_popup_init();

	/* Pop it up. */
	dialog_set(&idle_sr, idle_dialog);
	XtVaSetValues(command_value,
	    XtNstring, idle_command,
	    NULL);
	XtVaSetValues(timeout_value,
	    XtNstring, idle_timeout_string,
	    NULL);
	popup_popup(idle_shell, XtGrabNone);
}

/* Initialize the idle pop-up. */
static void
idle_popup_init(void)
{
	Widget w;
	Widget cancel_button;
	Widget command_label, timeout_label;
	Widget okay_button;

	/* Prime the dialog functions. */
	dialog_set(&idle_sr, idle_dialog);

	/* Create the menu shell. */
	idle_shell = XtVaCreatePopupShell(
	    "idlePopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(idle_shell, XtNpopupCallback, place_popup,
	    (XtPointer)CenterP);
	XtAddCallback(idle_shell, XtNpopupCallback, idle_popup_callback,
	    (XtPointer)NULL);

	/* Create the form within the shell. */
	idle_dialog = XtVaCreateManagedWidget(
	    ObjDialog, formWidgetClass, idle_shell,
	    NULL);

	/* Create the file name widgets. */
	command_label = XtVaCreateManagedWidget(
	    "command", labelWidgetClass, idle_dialog,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	command_value = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, idle_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, command_label,
	    XtNhorizDistance, 0,
	    NULL);
	dialog_match_dimension(command_label, command_value, XtNheight);
	w = XawTextGetSource(command_value);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_command);
	dialog_register_sensitivity(command_value,
	    BN, False,
	    BN, False,
	    BN, False);

	timeout_label = XtVaCreateManagedWidget(
	    "timeout", labelWidgetClass, idle_dialog,
	    XtNfromVert, command_label,
	    XtNvertDistance, 3,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	timeout_value = XtVaCreateManagedWidget(
	    "value", asciiTextWidgetClass, idle_dialog,
	    XtNeditType, XawtextEdit,
	    XtNwidth, FILE_WIDTH,
	    XtNdisplayCaret, False,
	    XtNfromVert, command_label,
	    XtNvertDistance, 3,
	    XtNfromHoriz, timeout_label,
	    XtNhorizDistance, 0,
	    NULL);
	dialog_match_dimension(timeout_label, timeout_value, XtNheight);
	dialog_match_dimension(command_label, timeout_label, XtNwidth);
	w = XawTextGetSource(timeout_value);
	if (w == NULL)
		XtWarning("Cannot find text source in dialog");
	else
		XtAddCallback(w, XtNcallback, dialog_text_callback,
		    (XtPointer)&t_timeout);
	dialog_register_sensitivity(timeout_value,
	    BN, False,
	    BN, False,
	    BN, False);

	/* Create send/receive toggles. */
	enable_toggle = XtVaCreateManagedWidget(
	    "enable", commandWidgetClass, idle_dialog,
	    XtNfromVert, timeout_label,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(enable_toggle, idle_enabled ? diamond : no_diamond);
	XtAddCallback(enable_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_true);
	disable_toggle = XtVaCreateManagedWidget(
	    "disable", commandWidgetClass, idle_dialog,
	    XtNfromVert, enable_toggle,
	    XtNvertDistance, CLOSE_VGAP,
	    XtNhorizDistance, MARGIN,
	    XtNborderWidth, 0,
	    NULL);
	dialog_apply_bitmap(disable_toggle, idle_enabled ? no_diamond : diamond);
	XtAddCallback(disable_toggle, XtNcallback, toggle_enable,
	    (XtPointer)&s_false);

	/* Set up the buttons at the bottom. */
	okay_button = XtVaCreateManagedWidget(
	    ObjConfirmButton, commandWidgetClass, idle_dialog,
	    XtNfromVert, disable_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNhorizDistance, MARGIN,
	    NULL);
	XtAddCallback(okay_button, XtNcallback, okay_callback,
	    (XtPointer)NULL);

	cancel_button = XtVaCreateManagedWidget(
	    ObjCancelButton, commandWidgetClass, idle_dialog,
	    XtNfromVert, disable_toggle,
	    XtNvertDistance, FAR_VGAP,
	    XtNfromHoriz, okay_button,
	    XtNhorizDistance, BUTTON_GAP,
	    NULL);
	XtAddCallback(cancel_button, XtNcallback, idle_cancel, 0);
}

/* Callbacks for all the idle widgets. */

/* Idle pop-up popping up. */
static void
idle_popup_callback(Widget w unused, XtPointer client_data unused,
	XtPointer call_data unused)
{
	/* Set the focus to the command widget. */
	PA_dialog_focus_action(command_value, (XEvent *)NULL, (String *)NULL,
	    (Cardinal *)NULL);
}

/* Cancel button pushed. */
static void
idle_cancel(Widget w unused, XtPointer client_data unused,
	XtPointer call_data unused)
{
	XtPopdown(idle_shell);
}

/* OK button pushed. */
static void
okay_callback(Widget w unused, XtPointer call_parms unused,
	XtPointer call_data unused)
{
	if (idle_start() == 0) {
		XtPopdown(idle_shell);
	}
}

/* Mark a toggle. */
static void
mark_toggle(Widget w, Pixmap p)
{
	XtVaSetValues(w, XtNleftBitmap, p, NULL);
}

/* Enable/disable options. */
static void
toggle_enable(Widget w unused, XtPointer client_data,
	XtPointer call_data unused)
{
	/* Toggle the flag */
	idle_enabled = *(Boolean *)client_data;

	/* Change the widget states. */
	mark_toggle(enable_toggle, idle_enabled ? diamond : no_diamond);
	mark_toggle(disable_toggle, idle_enabled ? no_diamond : diamond);
	dialog_check_sensitivity(&idle_enabled);
}

/*
 * Process the new parameters.
 * Returns 0 for success, -1 otherwise.
 */
static int
idle_start(void)
{
	char *cmd, *tmo;

	if (!idle_enabled) {
		/* If they're turned it off, cancel the timer. */
		if (idle_ticking) {
			RemoveTimeOut(idle_id);
			idle_ticking = False;
		}
		return 0;
	}

	/* They've turned it on, and possibly reconfigured it. */

	/* Check the command. */
	XtVaGetValues(command_value, XtNstring, &cmd, NULL);
	if (!*cmd) {
		popup_an_error("Must specify a command");
		goto cant_idle;
	}

	/* Validate the timeout. */
	XtVaGetValues(timeout_value, XtNstring, &tmo, NULL);
	if (process_timeout_value(tmo) < 0)
		goto cant_idle;

	/* Save the idle command. */
	Replace(idle_command, NewString(cmd));
	Replace(idle_timeout_string, NewString(tmo));

	/* Seems okay.  Reset to the new interval. */
	if (IN_3270)
		reset_idle_timer();
	return 0;

    cant_idle:
	idle_enabled = False;
	dialog_check_sensitivity(&idle_enabled);
	return -1;
}

#endif /*]*/

#endif /*]*/
