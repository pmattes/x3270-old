/*
 * Copyright 1993, 1994 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	trace_ds.c
 *		3270 data stream tracing.
 *
 */

#include "globals.h"
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#include <sys/types.h>
#if !defined(NO_SYS_TIME_H) /*[*/
#include <sys/time.h>
#endif /*]*/
#include <errno.h>
#include <signal.h>
#include <time.h>
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "3270ds.h"

/* Externals */
extern unsigned char ebc2asc[];
extern char *build;
extern Boolean formatted;
extern unsigned char obuf[], *obptr;

/* Statics */
static int dscnt = 0;
static int tracewindow_pid = -1;

/* Globals */
FILE *tracef = (FILE *)0;
struct timeval ds_ts;

char *
unknown(value)
unsigned char value;
{
	static char buf[64];

	(void) sprintf(buf, "unknown[0x%x]", value);
	return buf;
}

char *
see_color(setting)
unsigned char setting;
{
	static char *color_name[] = {
	    "neutralBlack",
	    "blue",
	    "red",
	    "pink",
	    "green",
	    "turquoise",
	    "yellow",
	    "neutralWhite",
	    "black",
	    "deepBlue",
	    "orange",
	    "purple",
	    "paleGreen",
	    "paleTurquoise",
	    "grey",
	    "white"
	};

	if (setting == XAC_DEFAULT)
		return "default";
	else if (setting < 0xf0 || setting > 0xff)
		return unknown(setting);
	else
		return color_name[setting - 0xf0];
}

static Widget trace_shell = (Widget)NULL;
static int trace_reason;

/* Callback for "OK" button on trace popup */
/*ARGSUSED*/
static void
tracefile_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *tfn;
	char *tracecmd;
	char *full_cmd;
	long clock;
	extern char *command_string;

	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
		tfn = (char *)client_data;
	tfn = do_subst(tfn, True, True);
	if (strchr(tfn, '\'') ||
	    (strlen(tfn) > 0 && tfn[strlen(tfn)-1] == '\\')) {
		popup_an_error("Illegal file name: %s\n", tfn);
		XtFree(tfn);
		return;
	}
	tracef = fopen(tfn, "a");
	if (tracef == (FILE *)NULL) {
		popup_an_errno(errno, tfn);
		XtFree(tfn);
		return;
	}
	(void) SETLINEBUF(tracef);

	/* Display current status */
	clock = time((long *)0);
	(void) fprintf(tracef, "Trace started %s", ctime(&clock));
	(void) fprintf(tracef, " Version: %s\n", build);
	save_yourself();
	(void) fprintf(tracef, " Command: %s\n", command_string);
	(void) fprintf(tracef, " Model %d", model_num);
	(void) fprintf(tracef, ", %s display", 
	    appres.mono ? "monochrome" : "color");
	if (appres.extended)
		(void) fprintf(tracef, ", extended data stream");
	if (!appres.mono)
		(void) fprintf(tracef, ", %scolor",
		    appres.m3279 ? "full " : "pseudo-");
	if (appres.charset)
		(void) fprintf(tracef, ", %s charset", appres.charset);
	if (appres.apl_mode)
		(void) fprintf(tracef, ", APL mode");
	(void) fprintf(tracef, "\n");
	if (CONNECTED)
		(void) fprintf(tracef, " Connected to %s, port %u\n",
		    current_host, current_port);

	/* Start the monitor window */
	tracecmd = get_resource("traceCommand");
	if (!tracecmd || !strcmp(tracecmd, "none"))
		goto done;
	switch (tracewindow_pid = fork()) {
	    case 0:	/* child process */
		full_cmd = xs_buffer2("%s <'%s'", tracecmd, tfn);
		(void) execlp("xterm", "xterm", "-title", tfn,
		    "-sb", "-e", "/bin/sh", "-c", full_cmd, CN);
		(void) perror("exec(xterm)");
		_exit(1);
	    default:	/* parent */
		break;
	    case -1:	/* error */
		popup_an_errno(errno, "fork()");
		break;
	}

    done:
	XtFree(tfn);

	/* We're really tracing, turn the flag on. */
	appres.toggle[trace_reason].value = True;
	appres.toggle[trace_reason].changed = True;
	menubar_retoggle(&appres.toggle[trace_reason]);

	if (w)
		XtPopdown(trace_shell);

	/* Snap the current TELNET options. */
	if (net_snap_options()) {
		(void) fprintf(tracef, " TELNET state:\n");
		trace_netdata('<', obuf, obptr - obuf);
	}

	/* Dump the screen contents and modes into the trace file. */
	if (CONNECTED && formatted) {
		(void) fprintf(tracef, " Screen contents:\n");
		ctlr_snap_buffer();
		net_add_eor(obuf, obptr - obuf);
		obptr += 2;
		trace_netdata('<', obuf, obptr - obuf);
	}
	if (CONNECTED && ctlr_snap_modes()) {
		(void) fprintf(tracef, " 3270 modes:\n");
		net_add_eor(obuf, obptr - obuf);
		obptr += 2;
		trace_netdata('<', obuf, obptr - obuf);
	}

	(void) fprintf(tracef, " Data stream:\n");
}

/* Open the trace file. */
static void
tracefile_on(reason, tt)
int reason;
enum toggle_type tt;
{
	char tracefile[256];

	if (tracef)
		return;

	(void) sprintf(tracefile, "%s/x3trc.%d", appres.trace_dir,
		getpid());
	trace_reason = reason;
	if (tt == TT_INITIAL) {
		tracefile_callback((Widget)NULL, tracefile, PN);
		return;
	}
	if (trace_shell == NULL) {
		trace_shell = create_form_popup("trace",
		    tracefile_callback, (XtCallbackProc)NULL, True);
		XtVaSetValues(XtNameToWidget(trace_shell, "dialog"),
		    XtNvalue, tracefile,
		    NULL);
	}

	/* Turn the toggle _off_ until the popup succeeds. */
	appres.toggle[reason].value = False;
	appres.toggle[reason].changed = True;

	popup_popup(trace_shell, XtGrabExclusive);
}

/* Close the trace file. */
static void
tracefile_off()
{
	long clock;

	clock = time((long *)0);
	(void) fprintf(tracef, "Trace stopped %s", ctime(&clock));
	if (tracewindow_pid != -1)
		(void) kill(tracewindow_pid, SIGTERM);
	tracewindow_pid = -1;
	(void) fclose(tracef);
	tracef = (FILE *) NULL;
}

/*ARGSUSED*/
void
toggle_dsTrace(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	/* If turning on trace and no trace file, open one. */
	if (toggled(DS_TRACE) && !tracef)
		tracefile_on(DS_TRACE, tt);

	/* If turning off trace and not still tracing events, close the
	   trace file. */
	else if (!toggled(DS_TRACE) && !toggled(EVENT_TRACE))
		tracefile_off();

	if (toggled(DS_TRACE))
		(void) gettimeofday(&ds_ts, (struct timezone *)NULL);
}

/*ARGSUSED*/
void
toggle_eventTrace(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	/* If turning on event debug, and no trace file, open one. */
	if (toggled(EVENT_TRACE) && !tracef)
		tracefile_on(EVENT_TRACE, tt);

	/* If turning off event debug, and not tracing the data stream,
	   close the trace file. */
	else if (!toggled(EVENT_TRACE) && !toggled(DS_TRACE))
		tracefile_off();
}

/* Screen trace file support. */

static Widget screentrace_shell = (Widget)NULL;
static FILE *screentracef = (FILE *)0;

/*
 * Screen trace function, called when the host clears the screen.
 */
static void
do_screentrace()
{
	register int i;

	if (fprint_screen(screentracef, False)) {
		for (i = 0; i < COLS; i++)
			(void) fputc('=', screentracef);
		(void) fputc('\n', screentracef);
	}
}

void
trace_screen()
{
	if (!toggled(SCREEN_TRACE) | !screentracef)
		return;
	do_screentrace();
}

/* Callback for "OK" button on screentrace popup */
/*ARGSUSED*/
static void
screentrace_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *tfn;

	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
		tfn = (char *)client_data;
	tfn = do_subst(tfn, True, True);
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(errno, tfn);
		XtFree(tfn);
		return;
	}
	XtFree(tfn);

	/* We're really tracing, turn the flag on. */
	appres.toggle[SCREEN_TRACE].value = True;
	appres.toggle[SCREEN_TRACE].changed = True;
	menubar_retoggle(&appres.toggle[SCREEN_TRACE]);

	if (w)
		XtPopdown(screentrace_shell);
}

/* Callback for second "OK" button on screentrace popup */
/*ARGSUSED*/
static void
onescreen_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *tfn;

	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
		tfn = (char *)client_data;
	tfn = do_subst(tfn, True, True);
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(errno, tfn);
		XtFree(tfn);
		return;
	}
	XtFree(tfn);

	/* Save the current image, once. */
	do_screentrace();

	/* Close the file, we're done. */
	(void) fclose(screentracef);
	screentracef = (FILE *)NULL;

	if (w)
		XtPopdown(screentrace_shell);
}

/*ARGSUSED*/
void
toggle_screenTrace(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	char tracefile[256];

	if (toggled(SCREEN_TRACE)) {
		(void) sprintf(tracefile, "%s/x3scr.%d", appres.trace_dir,
			getpid());
		if (tt == TT_INITIAL) {
			screentrace_callback((Widget)NULL, tracefile, PN);
			return;
		}
		if (screentrace_shell == NULL) {
			screentrace_shell = create_form_popup("screentrace",
			    screentrace_callback, onescreen_callback, True);
			XtVaSetValues(XtNameToWidget(screentrace_shell, "dialog"),
			    XtNvalue, tracefile,
			    NULL);
		}
		appres.toggle[SCREEN_TRACE].value = False;
		appres.toggle[SCREEN_TRACE].changed = True;
		popup_popup(screentrace_shell, XtGrabExclusive);
	} else {
		if (ctlr_any_data())
			do_screentrace();
		(void) fclose(screentracef);
	}
}
