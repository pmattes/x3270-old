/*
 * Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2002 by Paul Mattes.
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
 *      macros.c
 *              This module handles string, macro and script (sms) processing.
 */

#include "globals.h"

#if defined(X3270_MENUS) /*[*/
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#endif /*]*/

#include <sys/wait.h>
#include <sys/signal.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include "3270ds.h"
#include "appres.h"
#include "ctlr.h"
#include "screen.h"
#include "resources.h"

#include "actionsc.h"
#include "childc.h"
#include "ctlrc.h"
#include "ftc.h"
#include "hostc.h"
#include "idlec.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#if defined(X3270_PRINTER) /*[*/
#include "printerc.h"
#endif /*]*/
#include "screenc.h"
#include "seec.h"
#include "statusc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"
#include "widec.h"

#define ANSI_SAVE_SIZE	4096

/* Externals */
extern int      linemode;

/* Globals */
struct macro_def *macro_defs = (struct macro_def *)NULL;
Boolean		macro_output = False;

/* Statics */
typedef struct sms {
	struct sms *next;	/* next sms on the stack */
	char	msc[1024];	/* input buffer */
	int	msc_len;	/* length of input buffer */
	char   *dptr;		/* data pointer (macros only) */
	enum sms_state {
		SS_IDLE,	/* no command active (scripts only) */
		SS_INCOMPLETE,	/* command(s) buffered and ready to run */
		SS_RUNNING,	/* command executing */
		SS_KBWAIT,	/* command awaiting keyboard unlock */
		SS_CONNECT_WAIT,/* command awaiting connection to complete */
#if defined(X3270_FT) /*[*/
		SS_FT_WAIT,	/* command awaiting file transfer to complete */
#endif /*]*/
		SS_PAUSED,	/* stopped in PauseScript action */
		SS_WAIT_ANSI,	/* awaiting completion of Wait(ansi) */
		SS_WAIT_3270,	/* awaiting completion of Wait(3270) */
		SS_WAIT_OUTPUT,	/* awaiting completion of Wait(Output) */
		SS_SWAIT_OUTPUT,/* awaiting completion of Snap(Wait) */
		SS_WAIT_DISC,	/* awaiting completion of Wait(Disconnect) */
		SS_WAIT,	/* awaiting completion of Wait() */
		SS_EXPECTING,	/* awaiting completion of Expect() */
		SS_CLOSING	/* awaiting completion of Close() */
	} state;
	enum sms_type {
		ST_STRING,	/* string */
		ST_MACRO,	/* macro */
		ST_COMMAND,	/* interactive command */
		ST_KEYMAP,	/* keyboard map */
		ST_IDLE,	/* idle command */
		ST_CHILD,	/* child process */
		ST_PEER		/* peer (external) process */
	} type;
	Boolean	success;
	Boolean	need_prompt;
	Boolean	is_login;
	Boolean	is_hex;		/* flag for ST_STRING only */
	Boolean output_wait_needed;
	Boolean executing;	/* recursion avoidance */
	Boolean accumulated;	/* accumulated time flag */
	Boolean idle_error;	/* idle command caused an error */
	unsigned long msec;	/* total accumulated time */
	FILE   *outfile;
	int	infd;
	int	pid;
	unsigned long expect_id;
	unsigned long wait_id;
} sms_t;
#define SN	((sms_t *)NULL)
static sms_t *sms = SN;
static int sms_depth = 0;

#if defined(X3270_TRACE) /*[*/
static const char *sms_state_name[] = {
	"IDLE",
	"INCOMPLETE",
	"RUNNING",
	"KBWAIT",
	"CONNECT_WAIT",
#if defined(X3270_FT) /*[*/
	"FT_WAIT",
#endif /*]*/
	"PAUSED",
	"WAIT_ANSI",
	"WAIT_3270",
	"WAIT_OUTPUT",
	"SWAIT_OUTPUT",
	"WAIT_DISC",
	"WAIT",
	"EXPECTING",
	"CLOSING"
};
#endif /*]*/

#if defined(X3270_MENUS) /*[*/
static struct macro_def *macro_last = (struct macro_def *) NULL;
#endif /*]*/
static unsigned long stdin_id = 0L;
static unsigned char *ansi_save_buf;
static int      ansi_save_cnt = 0;
static int      ansi_save_ix = 0;
static char    *expect_text = CN;
static int	expect_len = 0;
static const char *st_name[] = { "String", "Macro", "Command", "KeymapAction",
				 "IdleCommand", "ChildScript", "PeerScript" };
static enum iaction st_cause[] = { IA_MACRO, IA_MACRO, IA_COMMAND, IA_KEYMAP,
				 IA_IDLE, IA_MACRO, IA_MACRO };
#define ST_sNAME(s)	st_name[(int)(s)->type]
#define ST_NAME		ST_sNAME(sms)

static void script_prompt(Boolean success);
static void script_input(void);
static void sms_pop(Boolean can_exit);
static void wait_timed_out(void);

/* Macro that defines that the keyboard is locked due to user input. */
#define KBWAIT	(kybdlock & (KL_OIA_LOCKED|KL_OIA_TWAIT|KL_DEFERRED_UNLOCK))

/* Macro that defines when it's safe to continue a Wait()ing sms. */
#define CAN_PROCEED ( \
    IN_SSCP || \
    (IN_3270 && formatted && cursor_addr && !KBWAIT) || \
    (IN_ANSI && !(kybdlock & KL_AWAITING_FIRST)) \
)

/* Callbacks for state changes. */
static void
sms_connect(Boolean connected)
{
	/* Hack to ensure that disconnects don't cause infinite recursion. */
	if (sms != SN && sms->executing)
		return;

	if (!connected) {
		while (sms != SN && sms->is_login) {
			if (sms->type == ST_CHILD && sms->pid > 0)
				(void) kill(sms->pid, SIGTERM);
			sms_pop(False);
		}
	}
	sms_continue();
}

static void
sms_in3270(Boolean in3270)
{
	if (in3270 || IN_SSCP)
		sms_continue();
}

/* One-time initialization. */
void
sms_init(void)
{
	register_schange(ST_CONNECT, sms_connect);
	register_schange(ST_3270_MODE, sms_in3270);
}

#if defined(X3270_MENUS) /*[*/
/* Parse the macros resource into the macro list */
void
macros_init(void)
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
		Free(macro_defs);
		macro_defs = m;
	}
	macro_defs = (struct macro_def *)NULL;
	macro_last = (struct macro_def *)NULL;
	if (last_s) {
		Free(last_s);
		last_s = CN;
	}

	/* Search for new ones. */
	if (PCONNECTED) {
		char *rname;
		char *space;

		rname = NewString(current_host);
		if ((space = strchr(rname, ' ')))
			*space = '\0';
		s = get_fresource("%s.%s", ResMacros, rname);
		Free(rname);
	}
	if (s == CN) {
		if (appres.macros == CN)
			return;
		s = NewString(appres.macros);
	} else
		s = NewString(s);
	last_s = s;

	while ((ns = split_dresource(&s, &name, &action)) == 1) {
		m = (struct macro_def *)Malloc(sizeof(*m));
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
		Warning(buf);
	}
}
#endif /*]*/

/*
 * Enable input from a script.
 */
static void
script_enable(void)
{
	if (sms->infd >= 0 && stdin_id == 0) {
		trace_dsn("Enabling input for %s[%d]\n", ST_NAME, sms_depth);
		stdin_id = AddInput(sms->infd, script_input);
	}
}

/*
 * Disable input from a script.
 */
static void
script_disable(void)
{
	if (stdin_id != 0) {
		trace_dsn("Disabling input for %s[%d]\n", ST_NAME, sms_depth);
		RemoveInput(stdin_id);
		stdin_id = 0L;
	}
}

/* Allocate a new sms. */
static sms_t *
new_sms(enum sms_type type)
{
	sms_t *s;

	s = (sms_t *)Calloc(1, sizeof(sms_t));

	s->state = SS_IDLE;
	s->type = type;
	s->dptr = s->msc;
	s->success = True;
	s->need_prompt = False;
	s->is_login = False;
	s->outfile = (FILE *)NULL;
	s->infd = -1;
	s->pid = -1;
	s->expect_id = 0L;
	s->wait_id = 0L;
	s->output_wait_needed = False;
	s->executing = False;
	s->accumulated = False;
	s->idle_error = False;
	s->msec = 0L;

	return s;
}

/*
 * Push an sms definition on the stack.
 * Returns whether or not that is legal.
 */
static Boolean
sms_push(enum sms_type type)
{
	sms_t *s;

	/* Preempt any running sms. */
	if (sms != SN) {
		/* Remove the running sms's input. */
		script_disable();
	}

	s = new_sms(type);
	if (sms != SN)
		s->is_login = sms->is_login;	/* propagate from parent */
	s->next = sms;
	sms = s;

	/* Enable the abort button on the menu and the status indication. */
	if (++sms_depth == 1) {
		menubar_as_set(True);
		status_script(True);
	}

	if (ansi_save_buf == (unsigned char *)NULL)
		ansi_save_buf = (unsigned char *)Malloc(ANSI_SAVE_SIZE);
	return True;
}

/*
 * Add an sms definition to the _bottom_ of the stack.
 */
static sms_t *
sms_enqueue(enum sms_type type)
{
	sms_t *s, *t, *t_prev = SN;

	/* Allocate and initialize a new structure. */
	s = new_sms(type);

	/* Find the bottom of the stack. */
	for (t = sms; t != SN; t = t->next)
		t_prev = t;

	if (t_prev == SN) {	/* Empty stack. */
		s->next = sms;
		sms = s;

		/*
		 * Enable the abort button on the menu and the status
		 * line indication.
		 */
		menubar_as_set(True);
		status_script(True);
	} else {			/* Add to bottom. */
		s->next = SN;
		t_prev->next = s;
	}

	sms_depth++;

	if (ansi_save_buf == (unsigned char *)NULL)
		ansi_save_buf = (unsigned char *)Malloc(ANSI_SAVE_SIZE);

	return s;
}

/* Pop an sms definition off the stack. */
static void
sms_pop(Boolean can_exit)
{
	sms_t *s;

	trace_dsn("%s[%d] complete\n", ST_NAME, sms_depth);

	/* When you pop the peer script, that's the end of x3270. */
	if (sms->type == ST_PEER && can_exit)
		x3270_exit(0);

	/* Remove the input event. */
	script_disable();

	/* Close the files. */
	if (sms->type == ST_CHILD) {
		(void) fclose(sms->outfile);
		(void) close(sms->infd);
	}

	/* Cancel any pending timeouts. */
	if (sms->expect_id != 0L)
		RemoveTimeOut(sms->expect_id);
	if (sms->wait_id != 0L)
		RemoveTimeOut(sms->wait_id);

	/*
	 * If this was an idle command that generated an error, now is the
	 * time to announce that.  (If we announced it when the error first
	 * occurred, we might be telling the wrong party, such as a script.)
	 */
	if (sms->idle_error)
		popup_an_error("Idle command disabled due to error");

	/* Release the memory. */
	s = sms;
	sms = s->next;
	Free(s);
	sms_depth--;

	if (sms == SN) {
		/* Turn off the menu option. */
		menubar_as_set(False);
		status_script(False);
	} else if (KBWAIT && (int)sms->state < (int)SS_KBWAIT) {
		/* The child implicitly blocked the parent. */
		sms->state = SS_KBWAIT;
		trace_dsn("%s[%d] implicitly paused %s\n",
			    ST_NAME, sms_depth, sms_state_name[sms->state]);
	} else if (sms->state == SS_IDLE) {
		/* The parent needs to be restarted. */
		script_enable();
	}
}

/*
 * Peer script initialization.
 *
 * Must be called after the initial call to connect to the host from the
 * command line, so that the initial state can be set properly.
 */
void
peer_script_init(void)
{
	sms_t *s;
	Boolean on_top;

	if (!appres.scripted)
		return;

	if (sms == SN) {
		/* No login script running, simply push a new sms. */
		(void) sms_push(ST_PEER);
		s = sms;
		on_top = True;
	} else {
		/* Login script already running, pretend we started it. */
		s = sms_enqueue(ST_PEER);
		s->state = SS_RUNNING;
		on_top = False;
	}

	s->infd = fileno(stdin);
	s->outfile = stdout;
	(void) SETLINEBUF(s->outfile);	/* even if it's a pipe */

	if (on_top) {
		if (HALF_CONNECTED ||
		    (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))
			s->state = SS_CONNECT_WAIT;
		else
			script_enable();
	}
}


/*
 * Interpret and execute a script or macro command.
 */
enum em_stat { EM_CONTINUE, EM_PAUSE, EM_ERROR };
static enum em_stat
execute_command(enum iaction cause, char *s, char **np)
{
	enum {
		ME_GND,		/* before action name */
		ME_FUNCTION,	/* within action name */
		ME_FUNCTIONx,	/* saw whitespace after action name */
		ME_LPAREN,	/* saw left paren */
		ME_P_PARM,	/* paren: within unquoted parameter */
		ME_P_QPARM,	/* paren: within quoted parameter */
		ME_P_BSL,	/* paren: after backslash in quoted parameter */
		ME_P_PARMx,	/* paren: saw whitespace after parameter */
		ME_S_PARM,	/* space: within unquoted parameter */
		ME_S_QPARM,	/* space: within quoted parameter */
		ME_S_BSL,	/* space: after backslash in quoted parameter */
		ME_S_PARMx	/* space: saw whitespace after parameter */
	} state = ME_GND;
	char c;
	char aname[64+1];
	char parm[1024+1];
	int nx = 0;
	Cardinal count = 0;
	String params[64];
	int i, any, exact;
	int failreason = 0;
	Boolean saw_paren = False;
	static const char *fail_text[] = {
		/*1*/ "Action name must begin with an alphanumeric character",
		/*2*/ "Syntax error in action name",
		/*3*/ "Syntax error: \")\" or \",\" expected",
		/*4*/ "Extra data after parameters",
		/*5*/ "Syntax error: \")\" expected"
	};
#define fail(n) { failreason = n; goto failure; }
#define free_params() { \
	if (cause == IA_MACRO ||   cause == IA_KEYMAP || \
	    cause == IA_COMMAND || cause == IA_IDLE) { \
		Cardinal j; \
		for (j = 0; j < count; j++) \
			Free(params[j]); \
	} \
}

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
	    case ME_FUNCTION:	/* within function name */
		if (c == '(' || isspace(c)) {
			aname[nx] = '\0';
			if (c == '(') {
				nx = 0;
				state = ME_LPAREN;
				saw_paren = True;
			} else
				state = ME_FUNCTIONx;
		} else if (isalnum(c) || c == '_' || c == '-') {
			if (nx < 64)
				aname[nx++] = c;
		} else {
			fail(2);
		}
		break;
	    case ME_FUNCTIONx:	/* space after function name */
		if (isspace(c))
			continue;
		else if (c == '(') {
			nx = 0;
			state = ME_LPAREN;
		} else if (c == '"') {
			nx = 0;
			state = ME_S_QPARM;
		}
		else { state = ME_S_PARM;
			nx = 0;
			parm[nx++] = c;
		}
		break;
	    case ME_LPAREN:
		if (isspace(c))
			continue;
		else if (c == '"')
			state = ME_P_QPARM;
		else if (c == ',') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
		} else if (c == ')')
			goto success;
		else {
			state = ME_P_PARM;
			parm[nx++] = c;
		}
		break;
	    case ME_P_PARM:
		if (isspace(c)) {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_P_PARMx;
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
	    case ME_P_BSL:
		if (c == 'n' && nx < 1024)
			parm[nx++] = '\n';
		else {
			if (c != '"' && nx < 1024)
				parm[nx++] = '\\';
			if (nx < 1024)
				parm[nx++] = c;
		}
		state = ME_P_QPARM;
		break;
	    case ME_P_QPARM:
		if (c == '"') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_P_PARMx;
		} else if (c == '\\') {
			state = ME_P_BSL;
		} else if (nx < 1024)
			parm[nx++] = c;
		break;
	    case ME_P_PARMx:
		if (isspace(c))
			continue;
		else if (c == ',')
			state = ME_LPAREN;
		else if (c == ')')
			goto success;
		else
			fail(3);
		break;
	    case ME_S_PARM:
		if (isspace(c)) {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_S_PARMx;
		} else {
			if (nx < 1024)
				parm[nx++] = c;
		}
		break;
	    case ME_S_BSL:
		if (c == 'n' && nx < 1024)
			parm[nx++] = '\n';
		else {
			if (c != '"' && nx < 1024)
				parm[nx++] = '\\';
			if (nx < 1024)
				parm[nx++] = c;
		}
		state = ME_S_QPARM;
		break;
	    case ME_S_QPARM:
		if (c == '"') {
			parm[nx++] = '\0';
			params[++count] = &parm[nx];
			state = ME_S_PARMx;
		} else if (c == '\\') {
			state = ME_S_BSL;
		} else if (nx < 1024)
			parm[nx++] = c;
		break;
	    case ME_S_PARMx:
		if (isspace(c))
			continue;
		else if (c == '"')
			state = ME_S_QPARM;
		else {
			parm[nx++] = c;
			state = ME_S_PARM;
		}
		break;
	}

	/* Terminal state. */
	switch (state) {
	    case ME_FUNCTION:	/* mid-function-name */
		aname[nx] = '\0';
		break;
	    case ME_FUNCTIONx:	/* space after function */
		break;
	    case ME_GND:	/* nothing */
		if (np)
			*np = s - 1;
		return EM_CONTINUE;
	    case ME_S_PARMx:	/* space after space-style parameter */
		break;
	    case ME_S_PARM:	/* mid space-style parameter */
		parm[nx++] = '\0';
		params[++count] = &parm[nx];
		break;
	    default:
		fail(5);
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
	if (cause == IA_MACRO || cause == IA_KEYMAP ||
	    cause == IA_COMMAND || cause == IA_IDLE) {
		Cardinal j;

		for (j = 0; j < count; j++)
			params[j] = do_subst(params[j], True, False);
	}

	/* Search the action list. */
	if (!strncasecmp(aname, PA_PFX, strlen(PA_PFX))) {
		popup_an_error("Invalid action: %s", aname);
		free_params();
		return EM_ERROR;
	}
	any = -1;
	exact = -1;
	for (i = 0; i < actioncount; i++) {
		if (!strcasecmp(aname, actions[i].string)) {
			exact = any = i;
			break;
		}
	}
	if (exact < 0) {
		for (i = 0; i < actioncount; i++) {
			if (!strncasecmp(aname, actions[i].string,
			    strlen(aname))) {
				if (any >= 0) {
					popup_an_error("Ambiguous action name: "
					    "%s", aname);
					free_params();
					return EM_ERROR;
				}
				any = i;
			}
		}
	}
	if (any >= 0) {
		sms->accumulated = False;
		sms->msec = 0L;
		ia_cause = cause;
		(*actions[any].proc)((Widget)NULL, (XEvent *)NULL,
			count? params: (String *)NULL, &count);
		free_params();
		screen_disp(False);
	} else {
		popup_an_error("Unknown action: %s", aname);
		free_params();
		return EM_ERROR;
	}

#if defined(X3270_FT) /*[*/
	if (ft_state != FT_NONE)
		sms->state = SS_FT_WAIT;
#endif /*]*/
	if (KBWAIT)
		return EM_PAUSE;
	else
		return EM_CONTINUE;

    failure:
	popup_an_error(fail_text[failreason-1]);
	return EM_ERROR;
#undef fail
#undef free_params
}

/* Run the string at the top of the stack. */
static void
run_string(void)
{
	int len;
	int len_left;

	trace_dsn("%s[%d] running\n", ST_NAME, sms_depth);

	sms->state = SS_RUNNING;
	len = strlen(sms->dptr);
	trace_dsn("%sString[%d]: '%s'\n",
		    sms->is_hex ? "Hex" : "",
		    sms_depth, sms->dptr);

	if (sms->is_hex) {
		if (KBWAIT) {
			sms->state = SS_KBWAIT;
			trace_dsn("%s[%d] paused %s\n",
				    ST_NAME, sms_depth,
				    sms_state_name[sms->state]);
		} else {
			hex_input(sms->dptr);
			sms_pop(False);
		}
	} else {
		if ((len_left = emulate_input(sms->dptr, len, False))) {
			sms->dptr += len - len_left;
			if (KBWAIT) {
				sms->state = SS_KBWAIT;
				trace_dsn("%s[%d] paused %s\n",
					    ST_NAME, sms_depth,
					    sms_state_name[sms->state]);
			}
		} else {
			sms_pop(False);
		}
	}
}

/* Run the macro at the top of the stack. */

static void
run_macro(void)
{
	char *a = sms->dptr;
	char *nextm;
	enum em_stat es;
	sms_t *s;

	trace_dsn("%s[%d] running\n", ST_NAME, sms_depth);

	/*
	 * Keep executing commands off the line until one pauses or
	 * we run out of commands.
	 */
	while (*a) {
		/*
		 * Check for command failure.
		 */
		if (!sms->success) {
			trace_dsn("%s[%d] failed\n", ST_NAME, sms_depth);

			/* Propogate it. */
			if (sms->next != SN)
				sms->next->success = False;
			break;
		}

		sms->state = SS_RUNNING;
		trace_dsn("%s[%d]: '%s'\n", ST_NAME, sms_depth, a);
		s = sms;
		s->success = True;
		s->executing = True;
		es = execute_command(st_cause[s->type], a, &nextm);
		s->executing = False;
		s->dptr = nextm;

		/*
		 * If a new sms was started, we will be resumed
		 * when it completes.
		 */
		if (sms != s) {
			return;
		}

		/* Macro could not execute.  Abort it. */
		if (es == EM_ERROR) {
			trace_dsn("%s[%d] error\n", ST_NAME, sms_depth);

			/* Propogate it. */
			if (sms->next != SN)
				sms->next->success = False;

			/* If it was an idle command, cancel it. */
			cancel_if_idle_command();
			break;
		}

		/* Macro paused, implicitly or explicitly.  Suspend it. */
		if (es == EM_PAUSE || (int)sms->state >= (int)SS_KBWAIT) {
			if (sms->state == SS_RUNNING)
				sms->state = SS_KBWAIT;
			trace_dsn("%s[%d] paused %s\n", ST_NAME, sms_depth,
				    sms_state_name[sms->state]);
			sms->dptr = nextm;
			return;
		}

		/* Macro ran. */
		a = nextm;
	}

	/* Finished with this macro. */
	sms_pop(False);
}

/* Push a macro (macro, command or keymap action) on the stack. */
static void
push_xmacro(enum sms_type type, char *s, Boolean is_login)
{
	macro_output = False;
	if (!sms_push(type))
		return;
	(void) strncpy(sms->msc, s, 1023);
	sms->msc[1023] = '\0';
	sms->msc_len = strlen(s);
	if (sms->msc_len > 1023)
		sms->msc_len = 1023;
	if (is_login) {
		sms->state = SS_WAIT;
		sms->is_login = True;
	} else
		sms->state = SS_INCOMPLETE;
	sms_continue();
}

/* Push a macro on the stack. */
void
push_macro(char *s, Boolean is_login)
{
	push_xmacro(ST_MACRO, s, is_login);
}

/* Push an interactive command on the stack. */
void
push_command(char *s)
{
	push_xmacro(ST_COMMAND, s, False);
}

/* Push an keymap action on the stack. */
void
push_keymap_action(char *s)
{
	push_xmacro(ST_KEYMAP, s, False);
}

/* Push an keymap action on the stack. */
void
push_idle(char *s)
{
	push_xmacro(ST_IDLE, s, False);
}

/* Push a string on the stack. */
static void
push_string(char *s, Boolean is_login, Boolean is_hex)
{
	if (!sms_push(ST_STRING))
		return;
	(void) strncpy(sms->msc, s, 1023);
	sms->msc[1023] = '\0';
	sms->msc_len = strlen(s);
	if (sms->msc_len > 1023)
		sms->msc_len = 1023;
	if (is_login) {
		sms->state = SS_WAIT;
		sms->is_login = True;
	} else
		sms->state = SS_INCOMPLETE;
	sms->is_hex = is_hex;
	if (sms_depth == 1)
		sms_continue();
}

/* Set a pending string. */
void
ps_set(char *s, Boolean is_hex)
{
	push_string(s, False, is_hex);
}

#if defined(X3270_MENUS) /*[*/
/* Callback for macros menu. */
void
macro_command(struct macro_def *m)
{
	push_macro(m->action, False);
}
#endif /*]*/

/*
 * If the string looks like an action, e.g., starts with "Xxx(", run a login
 * macro.  Otherwise, set a simple pending login string.
 */
void
login_macro(char *s)
{
	char *t = s;
	Boolean looks_right = False;

	while (isspace(*t))
		t++;
	if (isalnum(*t)) {
		while (isalnum(*t))
			t++;
		while (isspace(*t))
			t++;
		if (*t == '(')
			looks_right = True;
	}

	if (looks_right)
		push_macro(s, True);
	else
		push_string(s, True, False);
}

/* Run the first command in the msc[] buffer. */
static void
run_script(void)
{
	trace_dsn("%s[%d] running\n", ST_NAME, sms_depth);

	for (;;) {
		char *ptr;
		int cmd_len;
		char *cmd;
		sms_t *s;
		enum em_stat es;

		/* If the script isn't idle, we're done. */
		if (sms->state != SS_IDLE)
			break;

		/* If a prompt is required, send one. */
		if (sms->need_prompt) {
			script_prompt(sms->success);
			sms->need_prompt = False;
		}

		/* If there isn't a pending command, we're done. */
		if (!sms->msc_len)
			break;

		/* Isolate the command. */
		ptr = memchr(sms->msc, '\n', sms->msc_len);
		if (!ptr)
			break;
		*ptr++ = '\0';
		cmd_len = ptr - sms->msc;
		cmd = sms->msc;

		/* Execute it. */
		sms->state = SS_RUNNING;
		sms->success = True;
		trace_dsn("%s[%d]: '%s'\n", ST_NAME, sms_depth, cmd);
		s = sms;
		s->executing = True;
		es = execute_command(IA_SCRIPT, cmd, (char **)NULL);
		s->executing = False;

		/* Move the rest of the buffer over. */
		if (cmd_len < s->msc_len) {
			s->msc_len -= cmd_len;
			(void) memmove(s->msc, ptr, s->msc_len);
		} else
			s->msc_len = 0;

		/*
		 * If a new sms was started, we will be resumed
		 * when it completes.
		 */
		if (sms != s) {
			s->need_prompt = True;
			return;
		}

		/* Handle what it did. */
		if (es == EM_PAUSE || (int)sms->state >= (int)SS_KBWAIT) {
			if (sms->state == SS_RUNNING)
				sms->state = SS_KBWAIT;
			script_disable();
			if (sms->state == SS_CLOSING) {
				sms_pop(False);
				return;
			}
			sms->need_prompt = True;
		} else if (es == EM_ERROR) {
			trace_dsn("%s[%d] error\n", ST_NAME, sms_depth);
			script_prompt(False);
			/* If it was an idle command, cancel it. */
			cancel_if_idle_command();
		} else
			script_prompt(sms->success);
		if (sms->state == SS_RUNNING)
			sms->state = SS_IDLE;
		else {
			trace_dsn("%s[%d] paused %s\n",
				    ST_NAME, sms_depth,
				    sms_state_name[sms->state]);
		}
	}
}

/* Handle an error generated during the execution of a script or macro. */
void
sms_error(const char *msg)
{
	/* Print the error message. */
	switch (sms->type) {
	case ST_PEER:
	case ST_CHILD:
		(void) fprintf(sms->outfile, "data: %s\n", msg);
		break;
	default:
		(void) fprintf(stderr, "%s\n", msg);
		break;
	}

	/* Fail the command. */
	sms->success = False;

	/* Cancel any login. */
	if (sms->is_login)
		host_disconnect(True);
}

/* Generate a response to a script command. */
void
sms_info(const char *fmt, ...)
{
	char *nl;
	char msgbuf[4096];
	char *msg = msgbuf;
	va_list args;

	va_start(args, fmt);
	vsprintf(msgbuf, fmt, args);
	va_end(args);

	do {
		int nc;

		nl = strchr(msg, '\n');
		if (nl != CN) {
			nc = nl - msg;
		} else
			nc = strlen(msg);
		if (nc) {
			switch (sms->type) {
			case ST_PEER:
			case ST_CHILD:
				(void) fprintf(sms->outfile, "data: %.*s\n",
				    nc, msg);
				break;
			default:
				(void) printf("%.*s\n", nc, msg);
				break;
			}
		}
		msg = nl + 1;
	} while (nl);

	macro_output = True;
}

/* Process available input from a script. */
static void
script_input(void)
{
	char buf[128];
	int nr;
	char *ptr;
	char c;

	trace_dsn("Input for %s[%d] %d\n", ST_NAME, sms_depth, sms->state);

	/* Read in what you can. */
	nr = read(sms->infd, buf, sizeof(buf));
	if (nr < 0) {
		popup_an_errno(errno, "%s[%d] read", ST_NAME, sms_depth);
		return;
	}
	if (nr == 0) {	/* end of file */
		trace_dsn("EOF %s[%d]\n", ST_NAME, sms_depth);
		sms_pop(True);
		sms_continue();
		return;
	}

	/* Append to the pending command, ignoring returns. */
	ptr = buf;
	while (nr--)
		if ((c = *ptr++) != '\r')
			sms->msc[sms->msc_len++] = c;

	/* Run the command(s). */
	sms->state = SS_INCOMPLETE;
	sms_continue();
}

/* Resume a paused sms, if conditions are now ripe. */
void
sms_continue(void)
{
	static Boolean continuing = False;

	if (continuing)
		return;
	continuing = True;

	while (True) {
		if (sms == SN) {
			continuing = False;
			return;
		}

		switch (sms->state) {

		    case SS_IDLE:
			continuing = False;
			return;		/* nothing to do */

		    case SS_INCOMPLETE:
		    case SS_RUNNING:
			break;		/* let it proceed */

		    case SS_KBWAIT:
			if (KBWAIT) {
				continuing = False;
				return;
			}
			break;

		    case SS_WAIT_ANSI:
			if (IN_ANSI) {
			    sms->state = SS_WAIT;
			    continue;
			}
			continuing = False;
			return;

		    case SS_WAIT_3270:
			if (IN_3270 | IN_SSCP) {
			    sms->state = SS_WAIT;
			    continue;
			}
			continuing = False;
			return;

		    case SS_WAIT:
			if (!CAN_PROCEED) {
				continuing = False;
				return;
			}
			/* fall through... */
		    case SS_CONNECT_WAIT:
			if (HALF_CONNECTED ||
			    (CONNECTED && (kybdlock & KL_AWAITING_FIRST))) {
				continuing = False;
				return;
			}
			if (!CONNECTED) {
				/* connection failed */
				if (sms->need_prompt) {
					script_prompt(False);
					sms->need_prompt = False;
				}
				break;
			}
			break;

#if defined(X3270_FT) /*[*/
		    case SS_FT_WAIT:
			if (ft_state == FT_NONE)
				break;
			else {
				continuing = False;
				return;
			}
#endif /*]*/

		    case SS_WAIT_OUTPUT:
		    case SS_SWAIT_OUTPUT:
			continuing = False;
			return;

		    case SS_WAIT_DISC:
			if (!CONNECTED)
				break;
			else {
				continuing = False;
				return;
			}

		    case SS_PAUSED:
			continuing = False;
			return;

		    case SS_EXPECTING:
			continuing = False;
			return;

		    case SS_CLOSING:
			continuing = False;
			return;	/* can't happen, I hope */

		}

		/* Restart the sms. */

		sms->state = SS_IDLE;

		if (sms->wait_id != 0L) {
			RemoveTimeOut(sms->wait_id);
			sms->wait_id = 0L;
		}

		switch (sms->type) {
		    case ST_STRING:
			run_string();
			break;
		    case ST_MACRO:
		    case ST_COMMAND:
		    case ST_KEYMAP:
		    case ST_IDLE:
			run_macro();
			break;
		    case ST_PEER:
		    case ST_CHILD:
			script_enable();
			run_script();
			break;
		}
	}

	continuing = False;
}

/*
 * Macro- and script-specific actions.
 */

static void
dump_range(int first, int len, Boolean in_ascii, struct ea *buf,
    int rel_rows unused, int rel_cols)
{
	register int i;
	Boolean any = False;
	Boolean is_zero = False;
	char *linebuf;
	char *s;

	linebuf = Malloc(maxCOLS * 3 + 1);
	s = linebuf;

	/*
	 * If the client has looked at the live screen, then if they later
	 * execute 'Wait(output)', they will need to wait for output from the
	 * host.  output_wait_needed is cleared by sms_host_output,
	 * which is called from the write logic in ctlr.c.
	 */     
	if (sms != SN && buf == ea_buf)
		sms->output_wait_needed = True;

	is_zero = FA_IS_ZERO(get_field_attribute(first));

	for (i = 0; i < len; i++) {
		if (i && !((first + i) % rel_cols)) {
			*s = '\0';
			action_output("%s", linebuf);
			s = linebuf;
			any = False;
		}
		if (in_ascii) {
			unsigned char c;

			if (buf[first + i].fa) {
				is_zero = FA_IS_ZERO(buf[first + i].fa);
				c = ' ';
			} else if (is_zero)
				c = ' ';
			else
#if defined(X3270_DBCS) /*[*/
			if (IS_LEFT(ctlr_dbcs_state(first + i))) {
				int len;
				char mb[16];
				int j;

				len = dbcs_to_mb(buf[first + i].cc,
					      buf[first + i + 1].cc,
					      mb);
				for (j = 0; j < len; j++) {
					s += sprintf(s, "%c", mb[j]);
				}
				continue;
			} else if (IS_RIGHT(ctlr_dbcs_state(first + i))) {
				continue;
			} else
#endif /*]*/
			{
				c = ebc2asc[buf[first + i].cc];
			}
			s += sprintf(s, "%c", c ? c : ' ');
		} else {
			s += sprintf(s, "%s%02x",
				i ? " " : "",
				buf[first + i].cc);
		}
		if (!any)
			any = True;
	}
	if (any) {
		*s = '\0';
		action_output("%s", linebuf);
	}
	Free(linebuf);
}

static void
dump_fixed(String params[], Cardinal count, const char *name, Boolean in_ascii,
    struct ea *buf, int rel_rows, int rel_cols, int caddr)
{
	int row, col, len, rows = 0, cols = 0;

	switch (count) {
	    case 0:	/* everything */
		row = 0;
		col = 0;
		len = rel_rows*rel_cols;
		break;
	    case 1:	/* from cursor, for n */
		row = caddr / rel_cols;
		col = caddr % rel_cols;
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
	    (row < 0 || row > rel_rows || col < 0 || col > rel_cols || len < 0) ||
	    ((count < 4)  && ((row * rel_cols) + col + len > rel_rows * rel_cols)) ||
	    ((count == 4) && (cols < 0 || rows < 0 ||
			      col + cols > rel_cols || row + rows > rel_rows))
	   ) {
		popup_an_error("%s: Invalid argument", name);
		return;
	}
	if (count < 4)
		dump_range((row * rel_cols) + col, len, in_ascii, buf,
		    rel_rows, rel_cols);
	else {
		int i;

		for (i = 0; i < rows; i++)
			dump_range(((row+i) * rel_cols) + col, cols, in_ascii,
			    buf, rel_rows, rel_cols);
	}
}

static void
dump_field(Cardinal count, const char *name, Boolean in_ascii)
{
	int faddr;
	unsigned char fa;
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
	faddr = find_field_attribute(cursor_addr);
	fa = get_field_attribute(cursor_addr);
	start = faddr;
	INC_BA(start);
	baddr = start;
	do {
		if (ea_buf[baddr].fa)
			break;
		len++;
		INC_BA(baddr);
	} while (baddr != start);
	dump_range(start, len, in_ascii, ea_buf, ROWS, COLS);
}

void
Ascii_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	dump_fixed(params, *num_params, action_name(Ascii_action), True,
		ea_buf, ROWS, COLS, cursor_addr);
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
	dump_fixed(params, *num_params, action_name(Ebcdic_action), False,
		ea_buf, ROWS, COLS, cursor_addr);
}

void
EbcdicField_action(Widget w unused, XEvent *event unused,
    String *params unused, Cardinal *num_params)
{
	dump_field(*num_params, action_name(EbcdicField_action), False);
}

static unsigned char
calc_cs(unsigned char cs)
{
	switch (cs & CS_MASK) { 
	case CS_APL:
	    return 0xf1;
	case CS_LINEDRAW:
	    return 0xf2;
	case CS_DBCS:
	    return 0xf8;
	default:
	    return 0x00;
	}
}

/*
 * Internals of the ReadBuffer action.
 * Operates on the supplied 'buf' parameter, which might be the live
 * screen buffer 'ea_buf' or a copy saved with 'Snap'.
 */
static void
do_read_buffer(String *params, Cardinal num_params, struct ea *buf)
{
	register int	baddr;
	unsigned char	current_fg = 0x00;
	unsigned char	current_gr = 0x00;
	unsigned char	current_cs = 0x00;
	char field_buf[1024];
	char *s = field_buf;
	unsigned char c;
	Boolean in_ebcdic = False;

	if (num_params > 0) {
		if (num_params > 1) {
			popup_an_error("%s: extra agruments",
					action_name(ReadBuffer_action));
			return;
		}
		if (!strncasecmp(params[0], "Ascii", strlen(params[0])))
			in_ebcdic = False;
		else if (!strncasecmp(params[0], "Ebcdic", strlen(params[0])))
			in_ebcdic = True;
		else {
			popup_an_error("%s: first parameter must be "
					"Ascii or Ebcdic",
					action_name(ReadBuffer_action));
			return;
		}
					                                        
	}

	baddr = 0;
	do {
		if (!(baddr % COLS)) {
			if (baddr)
				action_output(field_buf + 1);
			s = field_buf;
		}
		if (buf[baddr].fa) {

			s += sprintf(s, " SF(%02x=%02x", XA_3270,
					buf[baddr].fa);
			if (buf[baddr].fg)
				s += sprintf(s, ",%02x=%02x", XA_FOREGROUND,
						buf[baddr].fg);
			if (buf[baddr].gr)
				s += sprintf(s, ",%02x=%02x", XA_HIGHLIGHTING,
						buf[baddr].gr | 0xf0);
			if (buf[baddr].cs & CS_MASK)
				s += sprintf(s, ",%02x=%02x", XA_CHARSET,
					     calc_cs(buf[baddr].cs));
			s += sprintf(s, ")");
		} else {
			if (buf[baddr].fg != current_fg) {
				s += sprintf(s, " SA(%02x=%02x)", XA_FOREGROUND,
							buf[baddr].fg);
				current_fg = buf[baddr].fg;
			}
			if (buf[baddr].gr != current_gr) {
				s += sprintf(s, " SA(%02x=%02x)",
						XA_HIGHLIGHTING,
						buf[baddr].gr | 0xf0);
				current_gr = buf[baddr].gr;
			}
			if ((buf[baddr].cs & ~CS_GE) !=
					(current_cs & ~CS_GE)) {
				s += sprintf(s, " SA(%02x=%02x)", XA_CHARSET,
						calc_cs(buf[baddr].cs));
				current_cs = buf[baddr].cs;
			}
			if (in_ebcdic) {
				if (buf[baddr].cs & CS_GE)
					s += sprintf(s, " GE(%02x)",
							buf[baddr].cc);
				else
					s += sprintf(s, " %02x", buf[baddr].cc);
			} else {
				switch (buf[baddr].cs & CS_MASK) {
				case CS_BASE:
				default:
					if (buf[baddr].cs & CS_GE)
						c = ge2asc[buf[baddr].cc];
					else if (buf[baddr].cc == EBC_null)
						c = 0;
					else 
						c = ebc2asc[buf[baddr].cc];
					break;
				case CS_APL:
					c = cg2asc[buf[baddr].cc];
					break;
				case CS_LINEDRAW:
					c = ' ';
					break;
#if defined(X3270_DBCS) /*[*/
				case CS_DBCS:
					c = ' '; /* XXX */
					break;
#endif /*]*/
				}
				s += sprintf(s, " %02x", c);
			}
		}
		INC_BA(baddr);
	} while (baddr != 0);
	action_output(field_buf + 1);
}

/*
 * ReadBuffer action.
 */
void
ReadBuffer_action(Widget w unused, XEvent *event unused,
    String *params, Cardinal *num_params)
{
	do_read_buffer(params, *num_params, ea_buf);
}

/*
 * The sms prompt is preceeded by a status line with 11 fields:
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
 *  6 model number
 *  7 rows
 *  8 cols
 *  9 cursor row
 * 10 cursor col
 * 11 main window id
 */
static char *
status_string(void)
{
	char kb_stat;
	char fmt_stat;
	char prot_stat;
	char *connect_stat = CN;
	char em_mode;
	char s[1024];
	char *r;

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
		unsigned char fa;

		fa = get_field_attribute(cursor_addr);
		if (FA_IS_PROTECTED(fa))
			prot_stat = 'P';
		else
			prot_stat = 'U';
	}

	if (CONNECTED)
		connect_stat = xs_buffer("C(%s)", current_host);
	else
		connect_stat = NewString("N");

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

	(void) sprintf(s,
	    "%c %c %c %s %c %d %d %d %d %d 0x%lx",
	    kb_stat,
	    fmt_stat,
	    prot_stat,
	    connect_stat,
	    em_mode,
	    model_num,
	    ROWS, COLS,
	    cursor_addr / COLS, cursor_addr % COLS,
#if defined(X3270_DISPLAY) /*[*/
	    XtWindow(toplevel)
#else /*][*/
	    0L
#endif /*]*/
	    );

	r = NewString(s);
	Free(connect_stat);
	return r;
}

static void
script_prompt(Boolean success)
{
	char *s;
	char timing[64];

	if (sms != SN && sms->accumulated) {
		(void) sprintf(timing, "%ld.%03ld", sms->msec / 1000L,
			sms->msec % 1000L);
	} else {
		(void) strcpy(timing, "-");
	}
	s = status_string();
	(void) fprintf(sms->outfile, "%s %s\n%s\n", s, timing,
		       success ? "ok" : "error");
	(void) fflush(sms->outfile);
	Free(s);
}

/* Save the state of the screen for Snap queries. */
static char *snap_status = NULL;
static struct ea *snap_buf = NULL;
static int snap_rows = 0;
static int snap_cols = 0;
static int snap_field_start = -1;
static int snap_field_length = -1;
static int snap_caddr = 0;

static void
snap_save(void)
{
	sms->output_wait_needed = True;
	Replace(snap_status, status_string());

	Replace(snap_buf, (struct ea *)Malloc(ROWS*COLS*sizeof(struct ea)));
	(void) memcpy(snap_buf, ea_buf, ROWS*COLS*sizeof(struct ea));

	snap_rows = ROWS;
	snap_cols = COLS;

	if (!formatted) {
		snap_field_start = -1;
		snap_field_length = -1;
	} else {
		int baddr;

		snap_field_length = 0;
		snap_field_start = find_field_attribute(cursor_addr);
		INC_BA(snap_field_start);
		baddr = snap_field_start;
		do {
			if (ea_buf[baddr].fa)
				break;
			snap_field_length++;
			INC_BA(baddr);
		} while (baddr != snap_field_start);
	}
	snap_caddr = cursor_addr;
}

/*
 * "Snap" action, maintains a snapshot for consistent multi-field comparisons:
 *
 *  Snap [Save]
 *	updates the saved image from the live image
 *  Snap Rows
 *	returns the number of rows
 *  Snap Cols
 *	returns the number of columns
 *  Snap Staus
 *  Snap Ascii ...
 *  Snap AsciiField (not yet)
 *  Snap Ebcdic ...
 *  Snap EbcdicField (not yet)
 *  Snap ReadBuffer
 *	runs the named command
 *  Snap Wait [tmo] Output
 *      wait for the screen to change, then do a Snap Save
 */
void
Snap_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (sms == SN || sms->state != SS_RUNNING) {
		popup_an_error("%s can only be called from scripts or macros",
		    action_name(Snap_action));
		return;
	}

	if (*num_params == 0) {
		snap_save();
		return;
	}

	/* Handle 'Snap Wait' separately. */
	if (!strcasecmp(params[0], action_name(Wait_action))) {
		unsigned long tmo;
		char *ptr;
		int maxp = 0;

		if (*num_params > 1 &&
		    (tmo = strtoul(params[1], &ptr, 10)) > 0 &&
		    ptr != params[0] &&
		    *ptr == '\0') {
			maxp = 3;
		} else {
			tmo = 0L;
			maxp = 2;
		}
		if (*num_params > maxp) {
			popup_an_error("Too many arguments to %s %s",
			    action_name(Snap_action),
			    action_name(Wait_action));
			    return;
		}
		if (*num_params < maxp) {
			popup_an_error("Too few arguments to %s %s",
			    action_name(Snap_action),
			    action_name(Wait_action));
			    return;
		}
		if (strcasecmp(params[*num_params - 1], "Output")) {
			popup_an_error("Unknown parameter to %s %s",
			    action_name(Snap_action),
			    action_name(Wait_action));
			    return;
		}

		/* Must be connected. */
		if (!(CONNECTED || HALF_CONNECTED)) {
			popup_an_error("%s: Not connected",
			    action_name(Snap_action));
			return;
		}

		/*
		 * Make sure we need to wait.
		 * If we don't, then Snap(Wait) is equivalent to Snap().
		 */
		if (!sms->output_wait_needed) {
			snap_save();
			return;
		}

		/* Set the new state. */
		sms->state = SS_SWAIT_OUTPUT;

		/* Set up a timeout, if they want one. */
		if (tmo)
			sms->wait_id = AddTimeOut(tmo * 1000, wait_timed_out);
		return;
	}

	if (!strcasecmp(params[0], "Save")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		snap_save();
	} else if (!strcasecmp(params[0], "Status")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		action_output("%s", snap_status);
	} else if (!strcasecmp(params[0], "Rows")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		action_output("%d", snap_rows);
	} else if (!strcasecmp(params[0], "Cols")) {
		if (*num_params != 1) {
			popup_an_error("Extra argument(s)");
			return;
		}
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		action_output("%d", snap_cols);
	} else if (!strcasecmp(params[0], action_name(Ascii_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		dump_fixed(params + 1, *num_params - 1,
			action_name(Ascii_action), True, snap_buf,
			snap_rows, snap_cols, snap_caddr);
	} else if (!strcasecmp(params[0], action_name(Ebcdic_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		dump_fixed(params + 1, *num_params - 1,
			action_name(Ebcdic_action), False, snap_buf,
			snap_rows, snap_cols, snap_caddr);
	} else if (!strcasecmp(params[0], action_name(ReadBuffer_action))) {
		if (snap_status == NULL) {
			popup_an_error("No saved state");
			return;
		}
		do_read_buffer(params + 1, *num_params - 1, snap_buf);
	} else {
		popup_an_error("%s: Argument must be Save, Status, Rows, Cols, "
		    "%s, %s %s, or %s",
		    action_name(Snap_action),
		    action_name(Wait_action),
		    action_name(Ascii_action),
		    action_name(Ebcdic_action),
		    action_name(ReadBuffer_action));
	}
}

/*
 * Wait for various conditions.
 */
void
Wait_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	enum sms_state next_state = SS_WAIT;
	unsigned long tmo = 0;
	char *ptr;
	Cardinal np;
	String *pr;

	/* Pick off the timeout parameter first. */
	if (*num_params > 0 &&
	    (tmo = strtoul(params[0], &ptr, 10)) > 0 &&
	    ptr != params[0] &&
	    *ptr == '\0') {
		np = *num_params - 1;
		pr = params + 1;
	} else {
		np = *num_params;
		pr = params;
	}

	if (check_usage(Wait_action, np, 0, 1) < 0)
		return;
	if (sms == SN || sms->state != SS_RUNNING) {
		popup_an_error("%s can only be called from scripts or macros",
		    action_name(Wait_action));
		return;
	}
	if (np == 1) {
		if (!strcasecmp(pr[0], "NVTMode") ||
		    !strcasecmp(pr[0], "ansi")) {
			if (!IN_ANSI)
				next_state = SS_WAIT_ANSI;
		} else if (!strcasecmp(pr[0], "3270Mode") ||
			   !strcasecmp(pr[0], "3270")) {
			if (!IN_3270)
			    next_state = SS_WAIT_3270;
		} else if (!strcasecmp(pr[0], "Output")) {
			if (sms->output_wait_needed)
				next_state = SS_WAIT_OUTPUT;
			else
				return;
		} else if (!strcasecmp(pr[0], "Disconnect")) {
			if (CONNECTED)
				next_state = SS_WAIT_DISC;
			else
				return;
		} else if (strcasecmp(pr[0], "InputField")) {
			popup_an_error("%s argument must be InputField, "
			    "NVTmode, 3270Mode, Output or Disconnect",
			action_name(Wait_action));
			return;
		}
	}
	if (!(CONNECTED || HALF_CONNECTED)) {
		popup_an_error("%s: Not connected", action_name(Wait_action));
		return;
	}

	/* Is it already okay? */
	if (next_state == SS_WAIT && CAN_PROCEED)
		return;

	/* No, wait for it to happen. */
	sms->state = next_state;

	/* Set up a timeout, if they want one. */
	if (tmo)
		sms->wait_id = AddTimeOut(tmo * 1000, wait_timed_out);
}

/*
 * Callback from Connect() and Reconnect() actions, to minimally pause a
 * running sms.
 */
void
sms_connect_wait(void)
{
	if (sms != SN &&
	    (int)sms->state >= (int)SS_RUNNING &&
	    sms->state != SS_WAIT) {
		if (HALF_CONNECTED ||
		    (CONNECTED && (kybdlock & KL_AWAITING_FIRST)))
			sms->state = SS_CONNECT_WAIT;
	}
}

/*
 * Callback from ctlr.c, to indicate that the host has changed the screen.
 */
void
sms_host_output(void)
{
	if (sms != SN) {
		sms->output_wait_needed = False;

		switch (sms->state) {
		case SS_SWAIT_OUTPUT:
		    snap_save();
		    /* fall through... */
		case SS_WAIT_OUTPUT:
		    sms->state = SS_RUNNING;
		    sms_continue();
		    break;
		default:
		    break;
		}
	}
}

/* Return whether error pop-ups should be short-circuited. */
Boolean
sms_redirect(void)
{
	return sms != SN &&
	       (sms->type == ST_CHILD || sms->type == ST_PEER) &&
	       (sms->state == SS_RUNNING || sms->state == SS_CONNECT_WAIT ||
		sms->wait_id != 0L);
}

#if defined(X3270_MENUS) || defined(C3270) /*[*/
/* Return whether any scripts are active. */
Boolean
sms_active(void)
{
	return sms != SN;
}
#endif /*]*/

/* Translate an expect string (uses C escape syntax). */
static void
expand_expect(char *s)
{
	char *t = Malloc(strlen(s) + 1);
	char c;
	enum { XS_BASE, XS_BS, XS_O, XS_X } state = XS_BASE;
	int n = 0;
	int nd = 0;
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
memstr(char *s1, char *s2, int n1, int n2)
{
	int i;

	for (i = 0; i <= n1 - n2; i++, s1++)
		if (*s1 == *s2 && !memcmp(s1, s2, n2))
			return s1;
	return CN;
}

/* Check for a match against an expect string. */
static Boolean
expect_matches(void)
{
	int ix, i;
	unsigned char buf[ANSI_SAVE_SIZE];
	char *t;

	ix = (ansi_save_ix + ANSI_SAVE_SIZE - ansi_save_cnt) % ANSI_SAVE_SIZE;
	for (i = 0; i < ansi_save_cnt; i++) {
		buf[i] = ansi_save_buf[(ix + i) % ANSI_SAVE_SIZE];
	}
	t = memstr((char *)buf, expect_text, ansi_save_cnt, expect_len);
	if (t != CN) {
		ansi_save_cnt -= ((unsigned char *)t - buf) + expect_len;
		Free(expect_text);
		expect_text = CN;
		return True;
	} else
		return False;
}

/* Store an ANSI character for use by the Ansi action. */
void
sms_store(unsigned char c)
{
	if (sms == SN)
		return;

	/* Save the character in the buffer. */
	ansi_save_buf[ansi_save_ix++] = c;
	ansi_save_ix %= ANSI_SAVE_SIZE;
	if (ansi_save_cnt < ANSI_SAVE_SIZE)
		ansi_save_cnt++;

	/* If a script or macro is waiting to match a string, check now. */
	if (sms->state == SS_EXPECTING && expect_matches()) {
		RemoveTimeOut(sms->expect_id);
		sms->expect_id = 0L;
		sms->state = SS_INCOMPLETE;
		sms_continue();
	}
}

/* Dump whatever ANSI data has been sent by the host since last called. */
void
AnsiText_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	register int i;
	int ix;
	unsigned char c;
	char linebuf[ANSI_SAVE_SIZE * 4 + 1];
	char *s = linebuf;

	if (!ansi_save_cnt)
		return;
	ix = (ansi_save_ix + ANSI_SAVE_SIZE - ansi_save_cnt) % ANSI_SAVE_SIZE;
	for (i = 0; i < ansi_save_cnt; i++) {
		c = ansi_save_buf[(ix + i) % ANSI_SAVE_SIZE];
		if (!(c & ~0x1f)) switch (c) {
		    case '\n':
			s += sprintf(s, "\\n");
			break;
		    case '\r':
			s += sprintf(s, "\\r");
			break;
		    case '\b':
			s += sprintf(s, "\\b");
			break;
		    default:
			s += sprintf(s, "\\%03o", c);
			break;
		} else if (c == '\\')
			s += sprintf(s, "\\\\");
		else
			*s++ = (char)c;
	}
	*s = '\0';
	action_output("%s", linebuf);
	ansi_save_cnt = 0;
	ansi_save_ix = 0;
}

/* Pause a script. */
void
PauseScript_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	if (sms == SN || (sms->type != ST_PEER && sms->type != ST_CHILD)) {
		popup_an_error("%s can only be called from a script",
		    action_name(PauseScript_action));
		return;
	}
	sms->state = SS_PAUSED;
}

/* Continue a script. */
void
ContinueScript_action(Widget w, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (check_usage(ContinueScript_action, *num_params, 1, 1) < 0)
		return;

	/*
	 * If this is a nested script, this action aborts the current script,
	 * then applies to the previous one.
	 */
	if (w == (Widget)NULL && sms_depth > 1)
		sms_pop(False);

	/* Continue the previous script. */
	if (sms == SN || sms->state != SS_PAUSED) {
		popup_an_error("%s: No script waiting",
		    action_name(ContinueScript_action));
		sms_continue();
		return;
	}
	action_output("%s", params[0]);
	sms->state = SS_RUNNING;
	sms_continue();
}

/* Stop listening to stdin. */
void
CloseScript_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (sms != SN &&
	    (sms->type == ST_PEER || sms->type == ST_CHILD)) {

		/* Close this script. */
		sms->state = SS_CLOSING;
		script_prompt(True);

		/* If nonzero status passed, fail the calling script. */
		if (*num_params > 0 &&
		    atoi(params[0]) != 0 &&
		    sms->next != SN) {
			sms->next->success = False;
			if (sms->is_login)
				host_disconnect(True);
		}
	} else
		popup_an_error("%s can only be called from a script",
		    action_name(CloseScript_action));
}

/* Execute an arbitrary shell command. */
void
Execute_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (check_usage(Execute_action, *num_params, 1, 1) < 0)
		return;
	(void) system(params[0]);
}

/* Timeout for Expect action. */
static void
expect_timed_out(void)
{
	if (sms == SN || sms->state != SS_EXPECTING)
		return;

	Free(expect_text);
	expect_text = CN;
	popup_an_error("%s: Timed out", action_name(Expect_action));
	sms->expect_id = 0L;
	sms->state = SS_INCOMPLETE;
	sms->success = False;
	if (sms->is_login)
		host_disconnect(True);
	sms_continue();
}

/* Timeout for Wait action. */
static void
wait_timed_out(void)
{
	/* Pop up the error message. */
	popup_an_error("%s: Timed out", action_name(Wait_action));

	/* Forget the ID. */
	sms->wait_id = 0L;

	/* If this is a login macro, it has failed. */
	if (sms->is_login)
		host_disconnect(True);

	sms->success = False;
	sms->state = SS_INCOMPLETE;

	/* Let the script proceed. */
	sms_continue();
}

/* Wait for a string from the host (ANSI mode only). */
void
Expect_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	int tmo;

	/* Verify the environment and parameters. */
	if (sms == SN || sms->state != SS_RUNNING) {
		popup_an_error("%s can only be called from a script or macro",
		    action_name(Expect_action));
		return;
	}
	if (check_usage(Expect_action, *num_params, 1, 2) < 0)
		return;
	if (!IN_ANSI) {
		popup_an_error("%s is valid only when connected in ANSI mode",
		    action_name(Expect_action));
	}
	if (*num_params == 2) {
		tmo = atoi(params[1]);
		if (tmo < 1 || tmo > 600) {
			popup_an_error("%s: Invalid timeout: %s",
			    action_name(Expect_action), params[1]);
			return;
		}
	} else
		tmo = 30;

	/* See if the text is there already; if not, wait for it. */
	expand_expect(params[0]);
	if (!expect_matches()) {
		sms->expect_id = AddTimeOut(tmo * 1000, expect_timed_out);
		sms->state = SS_EXPECTING;
	}
	/* else allow sms to proceed */
}


#if defined(X3270_MENUS) /*[*/

/* "Execute an Action" menu option */

static Widget execute_action_shell = (Widget)NULL;

/* Callback for "OK" button on execute action popup */
static void
execute_action_callback(Widget w unused, XtPointer client_data,
    XtPointer call_data unused)
{
	char *text;

	text = XawDialogGetValueString((Widget)client_data);
	XtPopdown(execute_action_shell);
	if (!text)
		return;
	push_macro(text, False);
}

void
execute_action_option(Widget w unused, XtPointer client_data unused,
    XtPointer call_data unused)
{
	if (execute_action_shell == NULL)
		execute_action_shell = create_form_popup("ExecuteAction",
		    execute_action_callback, (XtCallbackProc)NULL, FORM_NO_CC);

	popup_popup(execute_action_shell, XtGrabExclusive);
}

#endif /*]*/

/* "Script" action, runs a script as a child process. */
void
Script_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	int inpipe[2];
	int outpipe[2];

	if (*num_params < 1) {
		popup_an_error("%s requires at least one argument",
		    action_name(Script_action));
		return;
	}

	/* Create a new script description. */
	if (!sms_push(ST_CHILD))
		return;

	/*
	 * Create pipes and stdout stream for the script process.
	 *  inpipe[] is read by x3270, written by the script
	 *  outpipe[] is written by x3270, read by the script
	 */
	if (pipe(inpipe) < 0) {
		sms_pop(False);
		popup_an_error("pipe() failed");
		return;
	}
	if (pipe(outpipe) < 0) {
		(void) close(inpipe[0]);
		(void) close(inpipe[1]);
		sms_pop(False);
		popup_an_error("pipe() failed");
		return;
	}
	if ((sms->outfile = fdopen(outpipe[1], "w")) == (FILE *)NULL) {
		(void) close(inpipe[0]);
		(void) close(inpipe[1]);
		(void) close(outpipe[0]);
		(void) close(outpipe[1]);
		sms_pop(False);
		popup_an_error("fdopen() failed");
		return;
	}
	(void) SETLINEBUF(sms->outfile);

	/* Fork and exec the script process. */
	if ((sms->pid = fork_child()) < 0) {
		(void) close(inpipe[0]);
		(void) close(inpipe[1]);
		(void) close(outpipe[0]);
		sms_pop(False);
		popup_an_error("fork() failed");
		return;
	}

	/* Child processing. */
	if (sms->pid == 0) {
		char **argv;
		Cardinal i;
		char env_buf[2][32];

		/* Clean up the pipes. */
		(void) close(outpipe[1]);
		(void) close(inpipe[0]);

		/* Export the names of the pipes into the environment. */
		(void) sprintf(env_buf[0], "X3270OUTPUT=%d", outpipe[0]);
		(void) putenv(env_buf[0]);
		(void) sprintf(env_buf[1], "X3270INPUT=%d", inpipe[1]);
		(void) putenv(env_buf[1]);

		/* Set up arguments. */
		argv = (char **)Malloc((*num_params + 1) * sizeof(char *));
		for (i = 0; i < *num_params; i++)
			argv[i] = params[i];
		argv[i] = CN;

		/* Exec. */
		(void) execvp(params[0], argv);
		(void) fprintf(stderr, "exec(%s) failed\n", params[0]);
		(void) _exit(1);
	}

	/* Clean up our ends of the pipes. */
	sms->infd = inpipe[0];
	(void) close(inpipe[1]);
	(void) close(outpipe[0]);

	/* Enable input. */
	script_enable();

	/* Set up to reap the child's exit status. */
	++children;
}

/* "Macro" action, explicitly invokes a named macro. */
void
Macro_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	struct macro_def *m;

	if (check_usage(Script_action, *num_params, 1, 1) < 0)
		return;
	for (m = macro_defs; m != (struct macro_def *)NULL; m = m->next) {
		if (!strcmp(m->name, params[0])) {
			push_macro(m->action, False);
			return;
		}
	}
	popup_an_error("no such macro: '%s'", params[0]);
}

#if defined(X3270_SCRIPT) /*[*/
/*
 * Idle cancellation: cancels the idle command if the current sms or any sms
 * that called it caused an error.
 */
void
cancel_if_idle_command(void)
{
	sms_t *s;

	for (s = sms; s != NULL; s = s->next) {
		if (s->type == ST_IDLE) {
			cancel_idle_timer();
			s->idle_error = True;
			trace_dsn("Cancelling idle command");
			break;
		}
	}
}
#endif /*]*/

#if defined(X3270_PRINTER) /*[*/
/* "Printer" action, starts or stops a printer session. */
void
Printer_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	if (check_usage(Printer_action, *num_params, 1, 2) < 0)
		return;
	if (!strcasecmp(params[0], "Start")) {
		printer_start((*num_params > 1)? params[1] : CN);
	} else if (!strcasecmp(params[0], "Stop")) {
		if (*num_params != 1) {
			popup_an_error("%s: Extra argument(s)",
			    action_name(Printer_action));
			return;
		}
		printer_stop();
	} else {
		popup_an_error("%s: Argument must Start or Stop",
		    action_name(Printer_action));
	}
}
#endif /*]*/

/* Abort all running scripts. */
void
abort_script(void)
{
	while (sms != SN) {
		if (sms->type == ST_CHILD && sms->pid > 0)
			(void) kill(sms->pid, SIGTERM);
		sms_pop(True);
	}
}

/* "Abort" action, stops pending scripts. */
void
Abort_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	child_ignore_output();
	abort_script();
}

#if defined(X3270_SCRIPT) /*[*/
/* Accumulate command execution time. */
void
sms_accumulate_time(struct timeval *t0, struct timeval *t1)
{
    if (sms != SN) {
	sms->accumulated = True;
	sms->msec += (t1->tv_sec - t0->tv_sec) * 1000 +
		     (t1->tv_usec - t0->tv_usec + 500) / 1000;
#if defined(DEBUG_ACCUMULATE) /*[*/
	printf("%s: accumulated %lu msec\n", ST_NAME, sms->msec);
#endif /*]*/
    }
}
#endif /*]*/
