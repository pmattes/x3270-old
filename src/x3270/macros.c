/*
 * Copyright 1993, 1994, 1995 by Paul Mattes.
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

#include "globals.h"
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include "3270ds.h"
#include "ctlr.h"
#include "screen.h"
#include "resources.h"

#define ANSI_SAVE_SIZE	4096

/* Externals */
extern int      linemode;
extern FILE    *tracef;

/* Globals */
struct macro_def *macro_defs = (struct macro_def *)NULL;

/* Statics */
static struct macro_def *macro_last = (struct macro_def *) NULL;
static XtInputId stdin_id;
static char     msc[1024];
static int      msc_len = 0;
static enum {
	SS_NONE,		/* not scripted */
	SS_IDLE,		/* no script command active */
	SS_RUNNING,		/* script command executing */
	SS_KBWAIT,		/* script command awaiting keyboard unlock */
	SS_PAUSED,		/* script blocked in PauseScript */
	SS_CONNECT_WAIT,	/* script command awaiting connection to
				 * complete */
	SS_WAIT,		/* wait command in progress */
	SS_EXPECTING,		/* expect command in progress */
	SS_CLOSING		/* close command in progress */
} script_state = SS_NONE;
static Boolean  script_success = True;
static unsigned char *ansi_save_buf;
static int      ansi_save_cnt = 0;
static int      ansi_save_ix = 0;
static void     script_prompt();
static char    *expect_text = CN;
int		expect_len = 0;
static XtIntervalId expect_id;
static Boolean	script_any_input = False;

/*
 * Macro that defines when it's safe to continue a script.
 */
#define CAN_PROCEED ( \
    (IN_3270 && formatted && cursor_addr && \
      !(kybdlock & (KL_OIA_LOCKED | KL_OIA_TWAIT | KL_DEFERRED_UNLOCK))) || \
    (IN_ANSI && !(kybdlock & KL_AWAITING_FIRST)) \
)


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
	char *s = CN;
	char *name, *action;
	struct macro_def *m;
	int ns;
	int ix = 1;
	static char *last_s = CN;

	/* Free the previous macro definitions. */
	while (macro_defs) {
		m = macro_defs->next;
		XtFree((XtPointer)macro_defs);
		macro_defs = m;
	}
	macro_defs = (struct macro_def *)NULL;
	macro_last = (struct macro_def *)NULL;
	if (last_s) {
		XtFree((XtPointer)last_s);
		last_s = CN;
	}

	/* Search for new ones. */
	if (PCONNECTED) {
		char *rname;
		char *space;

		rname = XtNewString(current_host);
		if ((space = strchr(rname, ' ')))
			*space = '\0';
		s = xs_buffer("%s.%s", ResMacros, rname);
		XtFree(rname);
		rname = s;
		s = get_resource(rname);
		XtFree(rname);
	}
	if (!s) {
		if (!(s = appres.macros))
			return;
		s = XtNewString(s);
	}
	last_s = s;

	while ((ns = split_dresource(&s, &name, &action)) == 1) {
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

	(void) SETLINEBUF(stdout);	/* even if it's a pipe */

	if (HALF_CONNECTED || (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))
		script_state = SS_CONNECT_WAIT;
	else {
		script_state = SS_IDLE;
		stdin_id = XtAppAddInput(appcontext, fileno(stdin),
		    (XtPointer)XtInputReadMask,
		    (XtInputCallbackProc)script_input,
		    (XtPointer)(long)fileno(stdin));
	}

	ansi_save_buf = (unsigned char *)XtMalloc(ANSI_SAVE_SIZE);
}


/*
 * Interpret and execute a script command.
 */
enum em_stat { EM_CONTINUE, EM_PAUSE, EM_ERROR };
static enum em_stat
execute_command(cause, s, np)
enum iaction cause;
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
	char aname[64+1];
	char parm[1024+1];
	int nx;
	Cardinal count = 0;
	String params[64];
	int i;
	int failreason = 0;
	static char *fail_text[] = {
		/*1*/ "Action name must begin with an alphanumeric character",
		/*2*/ "Syntax error, \"(\" expected",
		/*3*/ "Syntax error, \")\" expected",
		/*4*/ "Extra data after parameters"
	};
#define fail(n) { failreason = n; goto failure; }

	parm[0] = '\0';
	params[count] = parm;

	while ((c = *s++)) switch (state) {
	    case ME_GND:
		if (isspace(c))
			continue;
		else if (isalnum(c)) {
			state = ME_FUNCTION;
			nx = 0;
			aname[nx++] = c;
		} else
			fail(1);
		break;
	    case ME_FUNCTION:
		if (c == '(' || isspace(c)) {
			aname[nx] = '\0';
			if (c == '(') {
				nx = 0;
				state = ME_LPAREN;
			} else
				state = ME_FUNCTIONx;
		} else if (isalnum(c)) {
			if (nx < 64)
				aname[nx++] = c;
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
		if (c == 'n' && nx < 1024)
			parm[nx++] = '\n';
		else {
			if (c != '"' && nx < 1024)
				parm[nx++] = '\\';
			if (nx < 1024)
				parm[nx++] = c;
		}
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
		aname[nx] = '\0';
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
				fail(4);
		} else if (np)
			*np = s;
	} else if (np)
		*np = s-1;

	/* If it's a macro, do variable substitutions. */
	if (cause == IA_MACRO) {
		int j;

		for (j = 0; j < count; j++)
			params[j] = do_subst(params[j], True, False);
	}

	/* Search the action list. */
	for (i = 0; i < actioncount; i++) {
		if (!strcmp(aname, actions[i].string)) {
			ia_cause = cause;
			(*actions[i].proc)((Widget)NULL, (XEvent *)NULL,
				count ? params : (String *)NULL,
				&count);
			screen_disp();
			break;
		}
	}

	/* If it's a macro, undo variable substitutions. */
	if (cause == IA_MACRO) {
		int j;

		for (j = 0; j < count; j++)
			XtFree(params[j]);
	}

	if (i >= actioncount) {
		popup_an_error("Unknown action: %s", aname);
		return EM_ERROR;
	}

	if (kybdlock & (KL_OIA_LOCKED | KL_OIA_TWAIT | KL_DEFERRED_UNLOCK))
		return EM_PAUSE;
	else
		return EM_CONTINUE;

    failure:
	popup_an_error(fail_text[failreason-1]);
	return EM_ERROR;
#undef fail
}

/* Public wrapper for macro version of execute_command(). */

static char *pending_macro = CN;

static void
run_pending_macro()
{
	char *a = pending_macro;
	char *nextm;

	pending_macro = CN;

	/* Keep executing commands off the line until one pauses. */
	while (1) {
		switch (execute_command(IA_MACRO, a, &nextm)) {
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
		if (toggled(DS_TRACE))
			(void) fprintf(tracef, "Script: %s\n", msc);
		es = execute_command(IA_SCRIPT, msc, (char **)NULL);
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
			if (toggled(DS_TRACE))
				(void) fprintf(tracef, "script error\n");
			script_prompt(False);
		} else
			script_prompt(script_success);
		if (script_state == SS_RUNNING)
			script_state = SS_IDLE;
		else {
			if (toggled(DS_TRACE))
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
		popup_an_errno(errno, "script read");
		return;
	}
	if (nr == 0) {
		x3270_exit(0);
		return;
	}

	/* Note that the script has spoken. */
	script_any_input = True;

	/* Append to the pending command, ignoring returns. */
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
	if (script_any_input)
		script_prompt(script_success);
	stdin_id = XtAppAddInput(appcontext, fileno(stdin),
	    (XtPointer)XtInputReadMask, (XtInputCallbackProc)script_input,
	    (XtPointer)(long)fileno(stdin));
	script_state = SS_IDLE;
	if (toggled(DS_TRACE))
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
		if (kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK))
			return;
		run_pending_macro();
		if (pending_macro)
			return;
	}

	if ((int)script_state <= (int)SS_RUNNING)
		return;

	if (script_state == SS_PAUSED)
		return;

	if (script_state == SS_EXPECTING)
		return;

	if ((int)script_state >= (int)SS_CONNECT_WAIT) {
		if (HALF_CONNECTED ||
		    (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))
			return;
	}

	if ((script_state == SS_WAIT) && !CAN_PROCEED)
		return;

	if (script_state == SS_KBWAIT) {
		if (kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK))
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
	int row, col, len, rows, cols;

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
dump_field(count, name, in_ascii)
Cardinal count;
char *name;
Boolean in_ascii;
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

/*ARGSUSED*/
void
ascii_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_fixed(params, *num_params, action_name(ascii_fn), True);
}

/*ARGSUSED*/
void
ascii_field_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_field(*num_params, action_name(ascii_field_fn), True);
}

/*ARGSUSED*/
void
ebcdic_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_fixed(params, *num_params, action_name(ebcdic_fn), False);
}

/*ARGSUSED*/
void
ebcdic_field_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	dump_field(*num_params, action_name(ebcdic_fn), False);
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

	if (toggled(DS_TRACE))
		(void) fprintf(tracef, "Script prompting.\n");

	if (!kybdlock)
		kb_stat = 'U';
	else if (!CONNECTED ||
		 (kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK)))
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

	(void) printf("%c %c %c %s %c %d %d %d %d %d 0x%lx\n%s\n",
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
	if (check_usage(wait_fn, *num_params, 0, 0) < 0)
		return;
	if (script_state != SS_RUNNING) {
		popup_an_error("%s: can only be called from scripts",
		    action_name(wait_fn));
		return;
	}
	if (!(CONNECTED || HALF_CONNECTED)) {
		popup_an_error("%s: not connected", action_name(wait_fn));
		return;
	}

	/* Is it already okay? */
	if (CAN_PROCEED)
		return;

	/* No, wait for it to happen. */
	script_state = SS_WAIT;
}

void
script_connect_wait()
{
	if ((int)script_state >= (int)SS_RUNNING) {
		if (HALF_CONNECTED ||
		    (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))
			script_state = SS_CONNECT_WAIT;
	}
}

Boolean
scripting()
{
	return (int)script_state > (int)SS_IDLE;
}

/* Translate an expect string (uses C escape syntax). */
static void
expand_expect(s)
char *s;
{
	char *t = XtMalloc(strlen(s) + 1);
	char c;
	enum { XS_BASE, XS_BS, XS_O, XS_X } state = XS_BASE;
	int n;
	int nd;
	static char hexes[] = "0123456789abcdef";

	expect_text = t;

	while ((c = *s++)) {
		switch (state) {
		    case XS_BASE:
			if (c == '\\')
				state = XS_BS;
			else
				*t++ = c;
			break;
		    case XS_BS:
			switch (c) {
			    case 'x':
				nd = 0;
				n = 0;
				state = XS_X;
				break;
			    case 'r':
				*t++ = '\r';
				state = XS_BASE;
				break;
			    case 'n':
				*t++ = '\n';
				state = XS_BASE;
				break;
			    case 'b':
				*t++ = '\b';
				state = XS_BASE;
				break;
			    default:
				if (c >= '0' && c <= '7') {
					nd = 1;
					n = c - '0';
					state = XS_O;
				} else {
					*t++ = c;
					state = XS_BASE;
				}
				break;
			}
			break;
		    case XS_O:
			if (nd < 3 && c >= '0' && c <= '7') {
				n = (n * 8) + (c - '0');
				nd++;
			} else {
				*t++ = n;
				*t++ = c;
				state = XS_BASE;
			}
			break;
		    case XS_X:
			if (isxdigit(c)) {
				n = (n * 16) +
				    strchr(hexes, tolower(c)) - hexes;
				nd++;
			} else {
				if (nd) 
					*t++ = n;
				else
					*t++ = 'x';
				*t++ = c;
				state = XS_BASE;
			}
			break;
		}
	}
	expect_len = t - expect_text;
}

/* 'mem' version of strstr */
static char *
memstr(s1, s2, n1, n2)
char *s1;
char *s2;
int n1;
int n2;
{
	int i;

	for (i = 0; i <= n1 - n2; i++, s1++)
		if (*s1 == *s2 && !memcmp(s1, s2, n2))
			return s1;
	return CN;
}

/* Check for a match against an expect string. */
static Boolean
expect_matches()
{
	int ix, i;
	unsigned char buf[ANSI_SAVE_SIZE];
	unsigned char c;
	char *t;

	ix = (ansi_save_ix + ANSI_SAVE_SIZE - ansi_save_cnt) % ANSI_SAVE_SIZE;
	for (i = 0; i < ansi_save_cnt; i++) {
		c = ansi_save_buf[(ix + i) % ANSI_SAVE_SIZE];
		if (c)
			buf[i] = c;
	}
	t = memstr((char *)buf, expect_text, ansi_save_cnt, expect_len);
	if (t != CN) {
		ansi_save_cnt -=
		    ((unsigned char *)t - buf) + strlen(expect_text) - 1;
		XtFree(expect_text);
		expect_text = CN;
		return True;
	} else
		return False;
}

/* Store an ANSI character for use by the Ansi action. */
void
script_store(c)
unsigned char c;
{
	/* Save the character in the buffer. */
	ansi_save_buf[ansi_save_ix++] = c;
	ansi_save_ix %= ANSI_SAVE_SIZE;
	if (ansi_save_cnt < ANSI_SAVE_SIZE)
		ansi_save_cnt++;

	/* If a script is waiting to match a string, check now. */
	if (script_state == SS_EXPECTING && expect_matches()) {
		XtRemoveTimeOut(expect_id);
		script_restart();
	}
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
		popup_an_error("%s can only be called from a script",
		    action_name(pause_script_fn));
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
		popup_an_error("%s: No script waiting",
		    action_name(continue_script_fn));
		return;
	}
	if (check_usage(continue_script_fn, *num_params, 1, 1) < 0)
		return;
	(void) printf("data: %s\n", params[0]);
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
	if (check_usage(execute_fn, *num_params, 1, 1) < 0)
		return;
	(void) system(params[0]);
}

/* Timeout for Expect action. */
/*ARGSUSED*/
static void
expect_timed_out(closure, id)
XtPointer closure;
XtIntervalId *id;
{
	if (script_state != SS_EXPECTING)
		return;

	XtFree(expect_text);
	expect_text = CN;
	popup_an_error("%s(): Timed out", action_name(expect_fn));
	script_success = False;
	script_restart();
}

/* Wait for a string from the host (ANSI mode only). */
/*ARGSUSED*/
void
expect_fn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int tmo;

	/* Verify the environment and parameters. */
	if (w) {
		popup_an_error("%s can only be called from a script",
		    action_name(expect_fn));
		return;
	}
	if (check_usage(expect_fn, *num_params, 1, 2) < 0)
		return;
	if (!IN_ANSI) {
		popup_an_error("%s() is valid only when connected in ANSI mode",
		    action_name(expect_fn));
	}
	if (*num_params == 2) {
		tmo = atoi(params[1]);
		if (tmo < 1 || tmo > 600) {
			popup_an_error("%s(): Invalid timeout: %s",
			    action_name(expect_fn), params[1]);
			return;
		}
	} else
		tmo = 30;

	/* See if the text is there already; if not, wait for it. */
	expand_expect(params[0]);
	if (!expect_matches()) {
		expect_id = XtAppAddTimeOut(appcontext, tmo * 1000,
		    expect_timed_out, 0);
		script_state = SS_EXPECTING;
	}
	/* else allow script to proceed */
}


/* "Execute an Action" menu option */

static Widget execute_action_shell = (Widget)NULL;

/* Callback for "OK" button on execute action popup */
/*ARGSUSED*/
static void
execute_action_callback(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	char *text;

	text = XawDialogGetValueString((Widget)client_data);
	XtPopdown(execute_action_shell);
	if (!text)
		return;
	if (pending_macro) {
		popup_an_error("Macro already pending");
		return;
	}
	pending_macro = text;
	run_pending_macro();
}

/*ARGSUSED*/
void
execute_action_option(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	if (execute_action_shell == NULL)
		execute_action_shell = create_form_popup("ExecuteAction",
		    execute_action_callback, (XtCallbackProc)NULL, False);

	popup_popup(execute_action_shell, XtGrabExclusive);
}
