/*
 * Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *      macros.c
 *              This module handles macro and script processing.
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <memory.h>
#include <sys/wait.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "globals.h"
#include "3270ds.h"
#include "screen.h"

#define ANSI_SAVE_SIZE	4096

/* Externals */
extern Boolean kybdlock, oia_twait, oia_locked;
extern unsigned char cg2asc[];
extern unsigned char cg2ebc[];
extern unsigned char *screen_buf;
extern Boolean formatted;
extern int cursor_addr;
extern char *current_host;
extern int linemode;
extern FILE *tracef;

/* Globals */
struct macro_def *macro_defs = (struct macro_def *)NULL;

/* Statics */
static struct macro_def *macro_last = (struct macro_def *)NULL;
static XtInputId stdin_id;
static char msc[1024];
static int msc_len = 0;
static enum {
    SS_NONE,		/* not scripted */
    SS_IDLE,		/* no script command active */
    SS_RUNNING,		/* script command executing */
    SS_KBWAIT,		/* script command awaiting keyboard unlock */
    SS_PAUSED,		/* script blocked in PauseScript */
    SS_CONNECT_WAIT,	/* script command awaiting connection to complete */
    SS_WAIT,		/* wait command in progress */
    SS_CLOSING		/* close command in progress */
} script_state = SS_NONE;
static Boolean script_success = True;
static unsigned char *ansi_save_buf;
static int ansi_save_cnt = 0;
static int ansi_save_ix = 0;
static void script_prompt();

/*
 * Macro definitions look something like keymaps:
 *	name: function(parm)
 * or
 *	name: function("parm")
 *
 * Where 'function' is 'String' or 'Script'.
 */

/* Parse the macros resource into the macro list */
void
macros_init()
{
	char *s, *name, *action;
	struct macro_def *m;
	int ns;
	int ix = 1;

	if (!(s = appres.macros))
		return;
	s = XtNewString(s);

	while ((ns = split_resource(&s, &name, &action)) == 1) {
		m = (struct macro_def *)XtMalloc(sizeof(*m));
		m->name = name;
		m->action = action;
		if (macro_last)
			macro_last->next = m;
		else
			macro_defs = m;
		m->next = (struct macro_def *)NULL;
		macro_last = m;
		ix++;
	}
	if (ns < 0) {
		char buf[256];

		(void) sprintf(buf, "Error in macro %d", ix);
		XtWarning(buf);
	}
}

/*
 * Script initialization.
 *
 * *Must* be called after the initial call to connect to the host from the
 * command line.
 */
void
script_init()
{
	if (!appres.scripted)
		return;

	(void) setlinebuf(stdout);	/* even if it's a pipe */

	if (HALF_CONNECTED || (CONNECTED && !IN_3270 && !ansi_host))
		script_state = SS_CONNECT_WAIT;
	else {
		script_state = SS_IDLE;
		stdin_id = XtAppAddInput(appcontext, fileno(stdin),
		    (XtPointer)XtInputReadMask,
		    (XtInputCallbackProc)script_input,
		    (XtPointer)fileno(stdin));
	}

	ansi_save_buf = (unsigned char *)XtMalloc(ANSI_SAVE_SIZE);
}


/*
 * Interpret and execute a script command.
 */
enum em_stat { EM_CONTINUE, EM_PAUSE, EM_ERROR };
static enum em_stat
execute_command(s, np)
char *s;
char **np;
{
	enum {
		ME_GND,		/* before action name */
		ME_FUNCTION,	/* within action name */
		ME_FUNCTIONx,	/* saw whitespace after action name */
		ME_LPAREN,	/* saw left paren */
		ME_PARM,	/* within unquoted parameter */
		ME_QPARM,	/* within quoted parameter */
		ME_BSL,		/* after backslash in quoted parameter */
		ME_PARMx	/* saw whitespace after parameter */
	} state = ME_GND;
	char c;
	char action_name[64+1];
	char parm[1024+1];
	int nx;
	Cardinal count = 0;
	String params[64];
	int i;
	int failreason = 0;
	static char *fail_text[] = {
		/*1*/ "Action name must begin with an alphanumeric character",
		/*2*/ "Syntax error, '(' expected",
		/*3*/ "Syntax error, ')' expected"
	};
#define fail(n) { failreason = n; goto failure; }

	parm[0] = '\0';
	params[count] = parm;

	while (c = *s++) switch (state) {
	    case ME_GND:
		if (isspace(c))
			continue;
		else if (isalnum(c)) {
			state = ME_FUNCTION;
			nx = 0;
			action_name[nx++] = c;
		} else
			fail(1);
		break;
	    case ME_FUNCTION:
		if (c == '(' || isspace(c)) {
			action_name[nx] = '\0';
			if (c == '(') {
				nx = 0;
				state = ME_LPAREN;
			} else
				state = ME_FUNCTIONx;
		} else if (isalnum(c)) {
			if (nx < 64)
				action_name[nx++] = c;
		} else
			fail(2);
		break;
	    case ME_FUNCTIONx:
		if (isspace(c))
			continue;
		else if (c == '(') {
			nx = 0;
			state = ME_LPAREN;
		} else
			fail(2);
		break;
	    case ME_LPAREN:
		if (isspace(c))
			continue;
		else if (c == '"')
			state = ME_QPARM;
		else if (c == ',') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
		} else if (c == ')')
			goto success;
		else {
			state = ME_PARM;
			parm[nx++] = c;
		}
		break;
	    case ME_PARM:
		if (isspace(c)) {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_PARMx;
		} else if (c == ')') {
			parm[nx] = '\0';
			++count;
			goto success;
		} else if (c == ',') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_LPAREN;
		} else {
			if (nx < 1024)
				parm[nx++] = c;
		}
		break;
	    case ME_BSL:
		if (c != '"' && nx < 1024)
			parm[nx++] = '\\';
		if (nx < 1024)
			parm[nx++] = c;
		state = ME_QPARM;
		break;
	    case ME_QPARM:
		if (c == '"') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_PARMx;
		} else if (c == '\\') {
			state = ME_BSL;
		} else if (nx < 1024)
			parm[nx++] = c;
		break;
	    case ME_PARMx:
		if (isspace(c))
			continue;
		else if (c == ',')
			state = ME_LPAREN;
		else if (c == ')')
			goto success;
		else
			fail(3);
		break;
	}

	/* Terminal state. */
	switch (state) {
	    case ME_FUNCTION:
		action_name[nx] = '\0';
		break;
	    case ME_FUNCTIONx:
		break;
	    case ME_GND:
		if (np)
			*np = s - 1;
		return EM_CONTINUE;
	    default:
		fail(3);
	}

    success:
	if (c) {
		while (*s && isspace(*s))
			s++;
		if (*s) {
			if (np)
				*np = s;
			else
				popup_an_error("Extra data after parameters");
		} else if (np)
			*np = s;
	} else if (np)
		*np = s-1;

	/* Search the action list. */
	for (i = 0; i < actioncount; i++) {
		if (!strcmp(action_name, actions[i].string)) {
			(*actions[i].proc)((Widget)NULL, (XEvent *)NULL,
				count ? params : (String *)NULL,
				&count);
			screen_disp();
			break;
		}
	}

	if (i >= actioncount) {
		xs_popup_an_error("Unknown action: %s", action_name);
		return EM_ERROR;
	}

	if (oia_locked || oia_twait)
		return EM_PAUSE;
	else
		return EM_CONTINUE;

    failure:
	popup_an_error(fail_text[failreason-1]);
	return EM_ERROR;
}
#undef fail

/* Public wrapper for macro version of execute_command(). */

static char *pending_macro = (char *)NULL;

static void
run_pending_macro()
{
	char *a = pending_macro;
	char *nextm;

	pending_macro = (char *)NULL;

	/* Keep executing commands off the line until one pauses. */
	while (1) {
		switch (execute_command(a, &nextm)) {
		    case EM_ERROR:
			return;			/* failed, abort others */
		    case EM_CONTINUE:
			if (*nextm) {
				a = nextm;	/* succeeded, more */
				continue;
			} else
				return;		/* succeeded, done */
		    case EM_PAUSE:
			pending_macro = nextm;	/* succeeded, paused */
			return;
		}
	}
}

void
macro_command(m)
struct macro_def *m;
{
	if (pending_macro) {
		popup_an_error("Macro already pending");
		return;
	}
	pending_macro = m->action;
	run_pending_macro();
}

/* Run the first command in the msc[] buffer. */
static void
run_msc()
{
	char *ptr;
	int cmd_len;
	enum em_stat es;
	char *np;

	while ((int)script_state <= (int)SS_IDLE && msc_len) {

		/* Isolate the command. */
		ptr = memchr(msc, '\n', msc_len);
		if (!ptr)
			break;
		*ptr++ = '\0';
		cmd_len = ptr - msc;

		/* Execute it. */
		script_state = SS_RUNNING;
		script_success = True;
		if (toggled(TRACE))
			(void) fprintf(tracef, "Script: %s\n", msc);
		es = execute_command(msc, (char **)NULL);
		if (es == EM_PAUSE || (int)script_state >= (int)SS_KBWAIT) {
			if (script_state == SS_RUNNING)
				script_state = SS_KBWAIT;
			XtRemoveInput(stdin_id);
			if (script_state == SS_CLOSING) {
				script_prompt(True);
				appres.scripted = False;
				script_state = SS_NONE;
				return;
			}
		} else if (es == EM_ERROR) {
			if (toggled(TRACE))
				(void) fprintf(tracef, "script error\n");
			script_prompt(False);
		} else
			script_prompt(script_success);
		if (script_state == SS_RUNNING)
			script_state = SS_IDLE;
		else {
			if (toggled(TRACE))
				(void) fprintf(tracef, "Script paused.\n");
		}

		/* Move the rest of the buffer over. */
		if (cmd_len < msc_len) {
			msc_len -= cmd_len;
			MEMORY_MOVE(msc, ptr, msc_len);
		} else
			msc_len = 0;
	}
}

/* Handle an error generated during the execution of a script command. */
void
script_error(msg)
char *msg;
{
	(void) fprintf(stderr, "%s\n", msg);
	script_success = False;
}

/*ARGSUSED*/
void
script_input(fd)
XtPointer fd;
{
	char buf[128];
	int nr;
	char *ptr;
	char c;

	/* Read in what you can. */
	nr = read((int)fd, buf, sizeof(buf));
	if (nr < 0) {
		popup_an_errno("script read", errno);
		return;
	}
	if (nr == 0)
		x3270_exit(0);

	/* Append it to the pending command, ignoring returns. */
	ptr = buf;
	while (nr--)
		if ((c = *ptr++) != '\r')
			msc[msc_len++] = c;

	/* Run the command(s). */
	run_msc();
}

static void
script_restart()
{
	script_prompt(script_success);
	stdin_id = XtAppAddInput(appcontext, fileno(stdin),
	    (XtPointer)XtInputReadMask, (XtInputCallbackProc)script_input,
	    (XtPointer)fileno(stdin));
	script_state = SS_IDLE;
	if (toggled(TRACE))
		(void) fprintf(tracef, "Script continued.\n");

	/* There may be other commands backed up behind this one. */
	run_msc();
}

/* Resume a paused script, if conditions are now ripe. */
void
script_continue()
{
	/* Handle pending macros first. */
	if (pending_macro) {
		if (oia_twait || oia_locked)
			return;
		run_pending_macro();
		if (pending_macro)
			return;
	}

	if ((int)script_state <= (int)SS_RUNNING)
		return;

	if (script_state == SS_PAUSED)
		return;

	if ((int)script_state >= (int)SS_CONNECT_WAIT) {
		if (HALF_CONNECTED || (CONNECTED && !IN_3270 && !ansi_host))
			return;
	}

	if (script_state == SS_WAIT) {
		if (!IN_3270 || !formatted || oia_twait || oia_locked ||
		    !cursor_addr)
		return;
	}

	if (script_state == SS_KBWAIT) {
		if (oia_twait || oia_locked)
			return;
	}
	script_restart();
}

/*
 * Macro functions.
 */

static void
dump_range(first, len, in_ascii)
int first;
int len;
Boolean in_ascii;
{
	register int i;
	Boolean any = False;

	for (i = 0; i < len; i++) {
		unsigned char c;

		if (i && !((first + i) % COLS)) {
			(void) putchar('\n');
			any = False;
		}
		if (!any) {
			(void) printf("data: ");
			any = True;
		}
		if (in_ascii) {
			c = cg2asc[screen_buf[first + i]];
			(void) printf("%c", c ? c : ' ');
		} else {
			(void) printf("%s%02x",
				i ? " " : "",
				cg2ebc[screen_buf[first + i]]);
		}
	}
	(void) printf("\n");
}

static void
dump_fixed(params, count, name, in_ascii)
String params[];
Cardinal count;
char *name;
Boolean in_ascii;
{
	int row, col, len;

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
	    case 3:	/* spelled out */
		row = atoi(params[0]);
		col = atoi(params[1]);
		len = atoi(params[2]);
		break;
	    default:
		xs_popup_an_error("%s requires 0, 1 or 3 arguments", name);
		return;
	}

	if (row < 0 || row > ROWS || col < 0 || col > COLS || len < 0 ||
	    (row * COLS) + col + len > ROWS * COLS) {
		xs_popup_an_error("%s: Invalid argument", name);
		return;
	}
	dump_range((row * COLS) + col, len, in_ascii);
}

static void
dump_field(count, name, in_ascii)
Cardinal count;
char *name;
Boolean in_ascii;
{
	unsigned char *fa;
	int start, baddr;
	int len = 0;

	if (count != 0) {
		xs_popup_an_error("%s requires 0 arguments", name);
		return;
	}
	if (!formatted) {
		xs_popup_an_error("%s: Screen is not formatted", name);
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

/*ARGSUSED*/
void
ascii_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_fixed(params, *num_params, "Ascii", True);
}

/*ARGSUSED*/
void
ascii_field_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_field(*num_params, "AsciiField", True);
}

/*ARGSUSED*/
void
ebcdic_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_fixed(params, *num_params, "Ebcdic", False);
}

/*ARGSUSED*/
void
ebcdic_field_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_field(*num_params, "EbcdicField", False);
}

/*
 * The script prompt is preceeded by a status line with 12 fields:
 *
 *  1 keyboard status
 *     U unlocked
 *     L locked, waiting for host response
 *     E locked, keying error
 *  2 formatting status of screen
 *     F formatted
 *     U unformatted
 *  3 protection status of current field
 *     U unprotected (modifiable)
 *     P protected
 *  4 connect status
 *     N not connected
 *     C(host) connected
 *  5 emulator mode
 *     N not connected
 *     C connected in ANSI character mode
 *     L connected in ANSI line mode
 *     P 3270 negotiation pending
 *     I connected in 3270 mode
 *  7 model number
 *  8 max rows
 *  9 max cols
 * 10 cursor row
 * 11 cursor col
 * 12 main window id
 */

/*ARGSUSED*/
static void
script_prompt(success)
Boolean success;
{
	char kb_stat;
	char fmt_stat;
	char prot_stat;
	char *connect_stat;
	Boolean free_connect = False;
	char em_mode;

	if (!kybdlock)
		kb_stat = 'U';
	else if (!CONNECTED || oia_twait || oia_locked)
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
		connect_stat = "N";

	if (CONNECTED) {
		if (IN_ANSI) {
			if (linemode)
				em_mode = 'L';
			else
				em_mode = 'C';
		} else if (IN_3270)
			em_mode = 'I';
		else
			em_mode = 'P';
	} else
		em_mode = 'N';

	(void) printf("%c %c %c %s %c %d %d %d %d %d 0x%x\n%s\n",
	    kb_stat,
	    fmt_stat,
	    prot_stat,
	    connect_stat,
	    em_mode,
	    model_num,
	    ROWS, COLS,
	    cursor_addr / COLS, cursor_addr % COLS,
	    XtWindow(toplevel),
	    success ? "ok" : "error");

	if (free_connect)
		XtFree(connect_stat);
}

/*
 * Wait for the host to let us write onto the screen.
 */
/*ARGSUSED*/
void
wait_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (*num_params != 0) {
		popup_an_error("Wait requires 0 arguments");
		return;
	}
	if (script_state != SS_RUNNING) {
		popup_an_error("Wait: can only be called from scripts");
		return;
	}
	if (!(CONNECTED || HALF_CONNECTED)) {
		popup_an_error("Wait: not connected");
		return;
	}

	/* Is it already okay? */
	if (IN_3270 && formatted && !oia_twait && !oia_locked && cursor_addr)
		return;

	/* No, wait for it to happen. */
	script_state = SS_WAIT;
}

void
script_connect_wait()
{
	if ((int)script_state >= (int)SS_RUNNING) {
		if (HALF_CONNECTED || (CONNECTED && !IN_3270 && !ansi_host))
			script_state = SS_CONNECT_WAIT;
	}
}

Boolean
scripting()
{
	return (int)script_state > (int)SS_IDLE;
}

/* Store an ANSI character for use by the Ansi action. */
void
script_store(c)
unsigned char c;
{
	ansi_save_buf[ansi_save_ix++] = c;
	ansi_save_ix %= ANSI_SAVE_SIZE;
	if (ansi_save_cnt < ANSI_SAVE_SIZE)
		ansi_save_cnt++;
}

/* Dump whatever ANSI data has been sent by the host since last called. */
/*ARGSUSED*/
void
ansi_text_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int i;
	int ix;
	unsigned char c;

	if (!ansi_save_cnt)
		return;
	(void) printf("data: ");
	ix = (ansi_save_ix + ANSI_SAVE_SIZE - ansi_save_cnt) % ANSI_SAVE_SIZE;
	for (i = 0; i < ansi_save_cnt; i++) {
		c = ansi_save_buf[(ix + i) % ANSI_SAVE_SIZE];
		if (!(c & ~0x1f)) switch (c) {
		    case '\n':
			(void) printf("\\n");
			break;
		    case '\r':
			(void) printf("\\r");
			break;
		    case '\b':
			(void) printf("\\b");
			break;
		    default:
			(void) printf("\\%03o", c);
			break;
		} else if (c == '\\')
			(void) printf("\\\\");
		else
			(void) putchar((char)c);
	}
	(void) putchar('\n');
	ansi_save_cnt = 0;
	ansi_save_ix = 0;
}

/* Stop listening to stdin. */
/*ARGSUSED*/
void
close_script_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (script_state != SS_NONE)
		script_state = SS_CLOSING;
}

/* Pause a script until ContinueScript is called. */
/*ARGSUSED*/
void
pause_script_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (w) {
		popup_an_error("PauseScript can only be called from a script");
		return;
	}
	script_state = SS_PAUSED;
}

/* Continue a script. */
/*ARGSUSED*/
void
continue_script_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (script_state != SS_PAUSED) {
		popup_an_error("ContinueScript: No script waiting");
		return;
	}
	if (*num_params != 1) {
		popup_an_error("ContinueScript requires 1 argument");
		return;
	}
	printf("data: %s\n", params[0]);
	script_state = SS_RUNNING;
	script_restart();
}

/* Execute an arbitrary shell command.  Works from a script or the keyboard. */
/*ARGSUSED*/
void
execute_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	if (*num_params != 1) {
		popup_an_error("Execute requires 1 argument");
		return;
	}
	(void) system(params[0]);
}
