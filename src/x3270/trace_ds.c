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
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#include "globals.h"
#include "3270ds.h"

/* Externals */
extern unsigned char ebc2asc[];
extern char *build;

/* Statics */
static int dscnt = 0;
static int tracewindow_pid = -1;

/* Globals */
FILE *tracef = (FILE *)0;

static char *
unknown(value)
unsigned char value;
{
	static char buf[64];

	(void) sprintf(buf, "unknown[%x]", value);
	return buf;
}

/* display a (row,col) */
char *
rcba(baddr)
int baddr;
{
	static char buf[16];

	(void) sprintf(buf, "(%d,%d)", baddr/COLS, baddr%COLS);
	return buf;
}

char *
see_ebc(ch)
unsigned char ch;
{
	static char buf[8];

	switch (ch) {
	    case FCORDER_NULL:
		return "NULL";
	    case FCORDER_SUB:
		return "SUB";
	    case FCORDER_DUP:
		return "DUP";
	    case FCORDER_FM:
		return "FM";
	    case FCORDER_FF:
		return "FF";
	    case FCORDER_CR:
		return "CR";
	    case FCORDER_NL:
		return "NL";
	    case FCORDER_EM:
		return "EM";
	    case FCORDER_EO:
		return "EO";
	}
	if (ebc2asc[ch])
		(void) sprintf(buf, "%c", ebc2asc[ch]);
	else
		(void) sprintf(buf, "\\%o", ch);
	return buf;
}

char *
see_aid(code)
unsigned char code;
{
	switch (code) {
	case AID_NO: 
		return "NoAID";
	case AID_ENTER: 
		return "Enter";
	case AID_PF1: 
		return "PF1";
	case AID_PF2: 
		return "PF2";
	case AID_PF3: 
		return "PF3";
	case AID_PF4: 
		return "PF4";
	case AID_PF5: 
		return "PF5";
	case AID_PF6: 
		return "PF6";
	case AID_PF7: 
		return "PF7";
	case AID_PF8: 
		return "PF8";
	case AID_PF9: 
		return "PF9";
	case AID_PF10: 
		return "PF10";
	case AID_PF11: 
		return "PF11";
	case AID_PF12: 
		return "PF12";
	case AID_PF13: 
		return "PF13";
	case AID_PF14: 
		return "PF14";
	case AID_PF15: 
		return "PF15";
	case AID_PF16: 
		return "PF16";
	case AID_PF17: 
		return "PF17";
	case AID_PF18: 
		return "PF18";
	case AID_PF19: 
		return "PF19";
	case AID_PF20: 
		return "PF20";
	case AID_PF21: 
		return "PF21";
	case AID_PF22: 
		return "PF22";
	case AID_PF23: 
		return "PF23";
	case AID_PF24: 
		return "PF24";
	case AID_OICR: 
		return "OICR";
	case AID_MSR_MHS: 
		return "MSR_MHS";
	case AID_SELECT: 
		return "Select";
	case AID_PA1: 
		return "PA1";
	case AID_PA2: 
		return "PA2";
	case AID_PA3: 
		return "PA3";
	case AID_CLEAR: 
		return "Clear";
	case AID_SYSREQ: 
		return "SysReq";
	default: 
		return unknown(code);
	}
}

char *
see_attr(fa)
unsigned char fa;
{
	static char buf[256];
	char *paren = "(";

	buf[0] = '\0';

	if (fa & 0x04) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "protected");
		paren = ",";
		if (fa & 0x08) {
			(void) strcat(buf, paren);
			(void) strcat(buf, "skip");
			paren = ",";
		}
	} else if (fa & 0x08) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "numeric");
		paren = ",";
	}
	switch (fa & 0x03) {
	case 0:
		break;
	case 1:
		(void) strcat(buf, paren);
		(void) strcat(buf, "detectable");
		paren = ",";
		break;
	case 2:
		(void) strcat(buf, paren);
		(void) strcat(buf, "intensified");
		paren = ",";
		break;
	case 3:
		(void) strcat(buf, paren);
		(void) strcat(buf, "nondisplay");
		paren = ",";
		break;
	}
	if (fa & 0x20) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "modified");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");
	else
		(void) strcpy(buf, "(default)");

	return buf;
}

static char *
see_highlight(setting)
unsigned char setting;
{
	switch (setting) {
	    case XAH_DEFAULT:
		return "default";
	    case XAH_NORMAL:
		return "normal";
	    case XAH_REVERSE:
		return "reverse";
	    case XAH_UNDERSCORE:
		return "underscore";
	    default:
		return unknown(setting);
	}
}

static char *
see_color(setting)
unsigned char setting;
{
	static char *color_name[] = {
	    "neutral/black",
	    "blue",
	    "red",
	    "pink",
	    "green",
	    "turquoise",
	    "yellow",
	    "neutral/white",
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

static char *
see_transparency(setting)
unsigned char setting;
{
	switch (setting) {
	    case XAT_DEFAULT:
		return "default";
	    case XAT_OR:
		return "or";
	    case XAT_XOR:
		return "xor";
	    case XAT_OPAQUE:
		return "opaque";
	    default:
		return unknown(setting);
	}
}

static char *
see_validation(setting)
unsigned char setting;
{
	static char buf[64];
	char *paren = "(";

	(void) strcpy(buf, "");
	if (setting & XAV_FILL) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "fill");
		paren = ",";
	}
	if (setting & XAV_ENTRY) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "entry");
		paren = ",";
	}
	if (setting & XAV_TRIGGER) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "trigger");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");
	else
		(void) strcpy(buf, "(none)");
	return buf;
}

static char *
see_outline(setting)
unsigned char setting;
{
	static char buf[64];
	char *paren = "(";

	(void) strcpy(buf, "");
	if (setting & XAO_UNDERLINE) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "underline");
		paren = ",";
	}
	if (setting & XAO_RIGHT) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "right");
		paren = ",";
	}
	if (setting & XAO_OVERLINE) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "overline");
		paren = ",";
	}
	if (setting & XAO_LEFT) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "left");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");
	else
		(void) strcpy(buf, "(none)");
	return buf;
}

char *
see_efa(efa, value)
unsigned char efa;
unsigned char value;
{
	static char buf[64];

	switch (efa) {
	    case XA_ALL:
		(void) sprintf(buf, " all(%x)", value);
		break;
	    case XA_3270:
		(void) sprintf(buf, " 3270%s", see_attr(value));
		break;
	    case XA_VALIDATION:
		(void) sprintf(buf, " validation%s", see_validation(value));
		break;
	    case XA_OUTLINING:
		(void) sprintf(buf, " outlining(%s)", see_outline(value));
		break;
	    case XA_HIGHLIGHTING:
		(void) sprintf(buf, " highlighting(%s)", see_highlight(value));
		break;
	    case XA_FOREGROUND:
		(void) sprintf(buf, " foreground(%s)", see_color(value));
		break;
	    case XA_CHARSET:
		(void) sprintf(buf, " charset(%x)", value);
		break;
	    case XA_BACKGROUND:
		(void) sprintf(buf, " background(%s)", see_color(value));
		break;
	    case XA_TRANSPARENCY:
		(void) sprintf(buf, " transparency(%s)", see_transparency(value));
		break;
	    default:
		(void) sprintf(buf, " %s(%x)", unknown(efa), value);
	}
	return buf;
}

/* Data Stream trace print, handles line wraps */

void
trace_ds(s)
char *s;
{
	int len = strlen(s);
	Boolean nl = False;

	if (!toggled(TRACE))
		return;

	if (s && s[len-1] == '\n') {
		len--;
		nl = True;
	}
	while (dscnt + len >= 72) {
		int plen = 72-dscnt;

		(void) fprintf(tracef, "%.*s ...\n... ", plen, s);
		dscnt = 4;
		s += plen;
		len -= plen;
	}
	if (len) {
		(void) fprintf(tracef, "%.*s", len, s);
		dscnt += len;
	}
	if (nl) {
		(void) fprintf(tracef, "\n");
		dscnt = 0;
	}
}

static Widget trace_shell = (Widget)NULL;

/* Callback for "OK" button on trace popup */
/*ARGSUSED*/
static void
trace_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *tfn;
	char *tracecmd;
	char *full_cmd;
	long clock;

	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
		tfn = (char *)client_data;
	tracef = fopen(tfn, "a");
	if (tracef == (FILE *)NULL) {
		popup_an_errno(tfn, errno);
		return;
	}
	(void) setlinebuf(tracef);
	clock = time((long *)0);
	(void) fprintf(tracef, "%s\nTrace started %s",
	    build, ctime(&clock));

	tracecmd = get_resource("traceCommand");
	if (!tracecmd || !strcmp(tracecmd, "none"))
		goto done;
	switch (tracewindow_pid = fork()) {
	    case 0:	/* child process */
		full_cmd = xs_buffer2("%s <%s", tracecmd, tfn);
		(void) execlp("xterm", "xterm", "-title", tfn,
		    "-sb", "-e", "/bin/sh", "-c", full_cmd, (char *)0);
		(void) perror("exec(xterm)");
		_exit(1);
	    default:	/* parent */
		break;
	    case -1:	/* error */
		popup_an_errno("fork()", errno);
		break;
	}

    done:
	/* We're really tracing, turn the flag on. */
	appres.toggle[TRACE].value = True;
	menubar_retoggle(&appres.toggle[TRACE]);

	if (w)
		XtPopdown(trace_shell);
}

/*ARGSUSED*/
void
toggle_trace(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	char tracefile[256];
	long clock;

	if (toggled(TRACE)) {
		(void) sprintf(tracefile, "%s/x3trc.%d", appres.trace_dir,
			getpid());
		if (tt == TT_INITIAL) {
			trace_callback((Widget)NULL, tracefile,
				(XtPointer)NULL);
			return;
		}
		if (trace_shell == NULL) {
			trace_shell = create_form_popup("trace",
			    trace_callback, (XtCallbackProc)NULL, True);
			XtVaSetValues(XtNameToWidget(trace_shell, "dialog"),
			    XtNvalue, tracefile,
			    NULL);
		}

		/* Turn tracing _off_ until the popup succeeds. */
		appres.toggle[TRACE].value = False;

		popup_popup(trace_shell, XtGrabExclusive);
	} else {
		clock = time((long *)0);
		(void) fprintf(tracef, "Trace stopped %s", ctime(&clock));
		if (tracewindow_pid != -1)
			(void) kill(tracewindow_pid, SIGTERM);
		tracewindow_pid = -1;
		(void) fclose(tracef);
	}
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
	if (!toggled(SCREENTRACE) | !screentracef)
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
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(tfn, errno);
		return;
	}

	/* We're really tracing, turn the flag on. */
	appres.toggle[SCREENTRACE].value = True;
	menubar_retoggle(&appres.toggle[SCREENTRACE]);

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
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(tfn, errno);
		return;
	}

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
toggle_screentrace(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	char tracefile[256];

	if (toggled(SCREENTRACE)) {
		(void) sprintf(tracefile, "%s/x3scr.%d", appres.trace_dir,
			getpid());
		if (tt == TT_INITIAL) {
			screentrace_callback((Widget)NULL, tracefile,
				(XtPointer)NULL);
			return;
		}
		if (screentrace_shell == NULL) {
			screentrace_shell = create_form_popup("screentrace",
			    screentrace_callback, onescreen_callback, True);
			XtVaSetValues(XtNameToWidget(screentrace_shell, "dialog"),
			    XtNvalue, tracefile,
			    NULL);
		}
		appres.toggle[SCREENTRACE].value = False;
		popup_popup(screentrace_shell, XtGrabExclusive);
	} else {
		if (ctlr_any_data())
			do_screentrace();
		(void) fclose(screentracef);
	}
}
