/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 2000 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 */

/* 
 * tclAppInit.c --
 *
 *	Provides a default version of the main program and Tcl_AppInit
 *	procedure for Tcl applications (without Tk).
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tcl3270.c,v 1.1 2000/05/30 01:45:09 pdm Exp $
 */

/*
 *	tcl3270.c
 *		A tcl-based 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "tcl.h"

#include "globals.h"
#include <sys/wait.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

extern Boolean process_events(Boolean block);
extern int parse_command_line(int argc, char **argv, char **cl_hostname);

/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

extern int matherr();
int *tclDummyMathPtr = (int *) matherr;

static Tcl_ObjCmdProc x3270_cmd;

/* Local prototypes. */
static void ps_clear(void);
static int tcl3270_main(int argc, char *argv[]);
static void negotiate(void);


/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(int argc, char **argv)
{
    Tcl_Main(argc, argv, Tcl_AppInit);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(Tcl_Interp *interp)
{
    char *s0, *s;
    int tcl_argc;
    char **tcl_argv;
    int argc;
    char **argv;
    int i;

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /* Use argv and argv0 to figure out our command-line arguments. */
    s0 = Tcl_GetVar(interp, "argv0", 0);
    if (s0 == NULL)
	return TCL_ERROR;
    s = Tcl_GetVar(interp, "argv", 0);
    if (s == NULL)
	return TCL_ERROR;
    (void) Tcl_SplitList(interp, s, &tcl_argc, &tcl_argv);
    argc = tcl_argc + 1;
    argv = (char **)Malloc((argc + 1) * sizeof(char *));
    argv[0] = s0;
    for (i = 0; i < tcl_argc; i++) {
	argv[1 + i] = tcl_argv[i];
    }
    argv[argc] = NULL;

    /* Call main. */
    if (tcl3270_main(argc, argv) == TCL_ERROR)
	return TCL_ERROR;

    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */

    /*
     * Call Tcl_CreateCommands for the application-specific commands, if
     * they weren't already created by the init procedures called above.
     */
    for (i = 0; i < actioncount; i++) {
	if (Tcl_CreateObjCommand(interp, actions[i].string, x3270_cmd, NULL,
		NULL) == NULL) {
	    return TCL_ERROR;
	}
    }

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */

#if 0
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
#endif
    return TCL_OK;
}


void
usage(char *msg)
{
	if (msg != CN)
		Warning(msg);
	xs_error("Usage: %s [scriptname] [options] [ps:][LUname@]hostname[:port]",
		programname);
}

/*
 * Called when the host connects, disconnects, or changes modes.
 * When we connect or change modes, clear the screen.
 * When we disconnect, clear the pending string, so we don't keep trying to
 * feed it to a dead host.
 */
static void
main_connect(Boolean ignored)
{
	if (CONNECTED)
		ctlr_erase(True);
	else
		ps_clear();
}

/* Initialization procedure for tcl3270. */
static int
tcl3270_main(int argc, char *argv[])
{
	char	*cl_hostname = CN;

	argc = parse_command_line(argc, argv, &cl_hostname);

	if (!charset_init(appres.charset))
		xs_warning("Cannot find charset \"%s\"", appres.charset);
	ctlr_init(-1);
	ctlr_reinit(-1);
	kybd_init();
	ansi_init();

	register_schange(ST_CONNECT, main_connect);
	register_schange(ST_3270_MODE, main_connect);

	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Handle initial toggle settings. */
#if defined(X3270_TRACE) /*[*/
	if (!appres.debug_tracing) {
		appres.toggle[DS_TRACE].value = False;
		appres.toggle[EVENT_TRACE].value = False;
	}
#endif /*]*/
	initialize_toggles();

	/* Connect to the host, and wait for negotiation to complete. */
	if (argc > 1) {
		(void) host_connect(cl_hostname);
		if (CONNECTED || HALF_CONNECTED) {
			sms_connect_wait();
			negotiate();
		}
	}

	return TCL_OK;
}


/* Replacements for the logic in macros.c. */

static Boolean in_cmd = False;
static Tcl_Interp *sms_interp;
static int cmd_ret;
static Boolean connect_waiting = False;
static char *pending_string = NULL;
static char *pending_string_ptr = NULL;
static Boolean pending_hex = False;

#define KBWAIT	(HALF_CONNECTED || \
		 (kybdlock & \
		  (KL_AWAITING_FIRST|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK)))
#define CAN_PROCEED ( \
    IN_SSCP || \
    (IN_3270 && formatted && cursor_addr && !KBWAIT) || \
    (IN_ANSI && !(kybdlock & KL_AWAITING_FIRST)) \
)

/* Process the pending string (set by the String command). */
static void
process_pending_string(void)
{
	if (pending_string_ptr == NULL || KBWAIT ||
	    (connect_waiting && !CAN_PROCEED)) {
		return;
	}

	if (pending_hex) {
		hex_input(pending_string_ptr);
		ps_clear();
	} else {
		int len = strlen(pending_string_ptr);
		int len_left;

		len_left = emulate_input(pending_string_ptr, len, False);
		if (len_left) {
			pending_string_ptr += len - len_left;
			return;
		} else
			ps_clear();
	}
}

/* Clear out the pending string. */
static void
ps_clear(void)
{
	if (pending_string != NULL) {
		pending_string_ptr = NULL;
		Free(pending_string);
		pending_string = NULL;
	}
}

/* The tcl "x3270" command: The root of all 3270 access. */
static int
x3270_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
	int i;
	char *action;
	int count;
	char **argv = NULL;

	/* Synchronously run any pending I/O's and timeouts.  Ugly. */
	while (process_events(False))
		;

	/* Verify minimal command syntax. */
	if (objc < 1) {
		Tcl_SetResult(interp, "Missing action name", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Look up the action. */
	action = Tcl_GetString(objv[0]);
	for (i = 0; i < actioncount; i++) {
		if (!strcmp(action, actions[i].string))
			break;
	}
	if (i >= actioncount) {
		Tcl_SetResult(interp, "No such action", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Stage the arguments. */
	count = objc - 1;
	if (count) {
		int j;

		argv = (char **)Malloc(count*sizeof(char *));
		for (j = 0; j < count; j++) {
			argv[j] = Tcl_GetString(objv[j + 1]);
		}
	}

	/* Set up the ugly global variables and run the action. */
	in_cmd = True;
	sms_interp = interp;
	cmd_ret = TCL_OK;
	(*actions[i].proc)((Widget)NULL, (XEvent *)NULL, argv, &count);

	/*
	 * Process responses and push any pending string, until
	 * we can proceed.
	 */
	process_pending_string();
	while (KBWAIT || (connect_waiting && !CAN_PROCEED)) {
		(void) process_events(True);
		process_pending_string();
	}
	in_cmd = False;
	sms_interp = NULL;
	connect_waiting = False;
	return cmd_ret;
}

/* Do initial connect negotiation. */
void
negotiate(void)
{
	while (KBWAIT || (connect_waiting && !CAN_PROCEED)) {
		(void) process_events(True);
	}
}

/* Indicates whether errors should go to stderr, or be returned to tcl. */
Boolean
sms_redirect(void)
{
	return in_cmd;
}

/* Returns an error to tcl. */
void
sms_error(char *s)
{
	Tcl_SetResult(sms_interp, s, TCL_VOLATILE);
	cmd_ret = TCL_ERROR;
}

/* For now, a no-op.  Used to implement 'Expect'. */
void
sms_store(unsigned char c)
{
}

/* Set the pending string.  Used by the 'String' action. */
void
ps_set(char *s, Boolean is_hex)
{
	pending_string = pending_string_ptr = s;
	pending_hex = is_hex;
}

/* Signal a new connection. */
void
sms_connect_wait(void)
{
	connect_waiting = True;
}

/* More no-ops. */
void
login_macro(char *s)
{
}
void
sms_continue(void)
{
}

/*
 * Data query actions.  Except for dump_range(), these are copied from macros.c,
 * and should be compiled from there directly instead.
 */

static void
dump_range(int first, int len, Boolean in_ascii)
{
	register int i;
	Tcl_Obj *o = NULL;
	Tcl_Obj *row = NULL;

	for (i = 0; i < len; i++) {
		unsigned char c;

		if (i && !((first + i) % COLS)) {
			/* Done with this row. */
			if (o == NULL)
				o = Tcl_NewListObj(0, NULL);
			Tcl_ListObjAppendElement(sms_interp, o, row);
			row = NULL;
		}
		if (!row) {
			if (in_ascii)
				row = Tcl_NewObj();
			else
				row = Tcl_NewListObj(0, NULL);
		}
		if (in_ascii) {
			c = cg2asc[screen_buf[first + i]];
			if (!c)
				c = ' ';
			Tcl_AppendToObj(row, &c, 1);
		} else {
			char s[5];

			(void) sprintf(s, "0x%02x",
				cg2ebc[screen_buf[first + i]]);
			Tcl_ListObjAppendElement(sms_interp, row,
				Tcl_NewStringObj(s, 5));
		}
	}

	/* Return it. */
	if (row) {
		if (o) {
			Tcl_ListObjAppendElement(sms_interp, o, row);
			Tcl_SetObjResult(sms_interp, o);
		} else
			Tcl_SetObjResult(sms_interp, row);
	}
}

static void
dump_fixed(String params[], Cardinal count, const char *name, Boolean in_ascii)
{
	int row, col, len, rows = 0, cols = 0;

	switch (count) {
	    case 0:	/* everything */
		row = 0;
		col = 0;
		len = ROWS*COLS;
		break;
	    case 1:	/* from cursor, for n */
		row = cursor_addr / COLS;
		col = cursor_addr % COLS;
		len = atoi(params[0]);
		break;
	    case 3:	/* from (row,col), for n */
		row = atoi(params[0]);
		col = atoi(params[1]);
		len = atoi(params[2]);
		break;
	    case 4:	/* from (row,col), for rows x cols */
		row = atoi(params[0]);
		col = atoi(params[1]);
		rows = atoi(params[2]);
		cols = atoi(params[3]);
		len = 0;
		break;
	    default:
		popup_an_error("%s requires 0, 1, 3 or 4 arguments", name);
		return;
	}

	if (
	    (row < 0 || row > ROWS || col < 0 || col > COLS || len < 0) ||
	    ((count < 4)  && ((row * COLS) + col + len > ROWS * COLS)) ||
	    ((count == 4) && (cols < 0 || rows < 0 ||
			      col + cols > COLS || row + rows > ROWS))
	   ) {
		popup_an_error("%s: Invalid argument", name);
		return;
	}
	if (count < 4)
		dump_range((row * COLS) + col, len, in_ascii);
	else {
		int i;

		for (i = 0; i < rows; i++)
			dump_range(((row+i) * COLS) + col, cols, in_ascii);
	}
}

static void
dump_field(Cardinal count, const char *name, Boolean in_ascii)
{
	unsigned char *fa;
	int start, baddr;
	int len = 0;

	if (count != 0) {
		popup_an_error("%s requires 0 arguments", name);
		return;
	}
	if (!formatted) {
		popup_an_error("%s: Screen is not formatted", name);
		return;
	}
	fa = get_field_attribute(cursor_addr);
	start = fa - screen_buf;
	INC_BA(start);
	baddr = start;
	do {
		if (IS_FA(screen_buf[baddr]))
			break;
		len++;
		INC_BA(baddr);
	} while (baddr != start);
	dump_range(start, len, in_ascii);
}

void
Ascii_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	dump_fixed(params, *num_params, action_name(Ascii_action), True);
}

void
AsciiField_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params)
{
	dump_field(*num_params, action_name(AsciiField_action), True);
}

void
Ebcdic_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	dump_fixed(params, *num_params, action_name(Ebcdic_action), False);
}

void
EbcdicField_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params)
{
	dump_field(*num_params, action_name(EbcdicField_action), False);
}

/* "Status" action, returns the s3270 prompt. */
void
Status_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	char kb_stat;
	char fmt_stat;
	char prot_stat;
	char *connect_stat;
	Boolean free_connect = False;
	char em_mode;
	char s[1024];

	if (!kybdlock)
		kb_stat = 'U';
	else if (!CONNECTED || KBWAIT)
		kb_stat = 'L';
	else
		kb_stat = 'E';

	if (formatted)
		fmt_stat = 'F';
	else
		fmt_stat = 'U';

	if (!formatted)
		prot_stat = 'U';
	else {
		unsigned char *fa;

		fa = get_field_attribute(cursor_addr);
		if (FA_IS_PROTECTED(*fa))
			prot_stat = 'P';
		else
			prot_stat = 'U';
	}

	if (CONNECTED) {
		connect_stat = xs_buffer("C(%s)", current_host);
		free_connect = True;
	} else
		connect_stat = NewString("N");

	if (CONNECTED) {
		if (IN_ANSI) {
			extern int linemode; /* XXX */
			if (linemode)
				em_mode = 'L';
			else
				em_mode = 'C';
		} else if (IN_SSCP)
			em_mode = 'S';
		else if (IN_3270)
			em_mode = 'I';
		else
			em_mode = 'P';
	} else
		em_mode = 'N';

	(void) sprintf(s, "%c %c %c %s %c %d %d %d %d %d",
	    kb_stat,
	    fmt_stat,
	    prot_stat,
	    connect_stat,
	    em_mode,
	    model_num,
	    ROWS, COLS,
	    cursor_addr / COLS, cursor_addr % COLS);

	Tcl_SetResult(sms_interp, s, TCL_VOLATILE);
	Free(connect_stat);
}
