/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 2000, 2001 by Paul Mattes.
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
 *	c3270.c
 *		A curses-based 3270 Terminal Emulator
 *		Main proceudre.
 */

#include "globals.h"
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "ftc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "utilc.h"

#if defined(HAVE_LIBREADLINE) /*[*/
#include <readline/readline.h>
#if defined(HAVE_READLINE_HISTORY_H) /*[*/
#include <readline/history.h>
#endif /*]*/
#endif /*]*/

static void interact(void);
static void stop_pager(void);

#if defined(HAVE_LIBREADLINE) /*[*/
static CPPFunction attempted_completion;
static char *completion_entry(char *, int);
#endif /*]*/

/* Pager state. */
static FILE *pager = NULL;

/* Base keymap. */
static char *base_keymap =
"Ctrl<Key>]: Escape\n"
"Ctrl<Key>a Ctrl<Key>a: Key(0x01)\n"
"Ctrl<Key>a Ctrl<Key>]: Key(0x1d)\n"
"Ctrl<Key>a <Key>c: Clear\n"
"Ctrl<Key>a <Key>e: Escape\n"
"Ctrl<Key>a <Key>i: Insert\n"
"Ctrl<Key>a <Key>r: Reset\n"
"Ctrl<Key>a <Key>l: Redraw\n"
"Ctrl<Key>a <Key>m: Compose\n"
"Ctrl<Key>a <Key>^: Key(Notsign)\n"
"<Key>DC: Delete\n"
"<Key>UP: Up\n"
"<Key>DOWN: Down\n"
"<Key>LEFT: Left\n"
"<Key>RIGHT: Right\n"
"<Key>HOME: Home\n"
"Ctrl<Key>a <Key>1: PA(1)\n"
"Ctrl<Key>a <Key>2: PA(2)\n"
"Ctrl<Key>a <Key>3: PA(3)\n"
"<Key>F1: PF(1)\n"
"Ctrl<Key>a <Key>F1: PF(13)\n"
"<Key>F2: PF(2)\n"
"Ctrl<Key>a <Key>F2: PF(14)\n"
"<Key>F3: PF(3)\n"
"Ctrl<Key>a <Key>F3: PF(15)\n"
"<Key>F4: PF(4)\n"
"Ctrl<Key>a <Key>F4: PF(16)\n"
"<Key>F5: PF(5)\n"
"Ctrl<Key>a <Key>F5: PF(17)\n"
"<Key>F6: PF(6)\n"
"Ctrl<Key>a <Key>F6: PF(18)\n"
"<Key>F7: PF(7)\n"
"Ctrl<Key>a <Key>F7: PF(19)\n"
"<Key>F8: PF(8)\n"
"Ctrl<Key>a <Key>F8: PF(20)\n"
"<Key>F9: PF(9)\n"
"Ctrl<Key>a <Key>F9: PF(21)\n"
"<Key>F10: PF(10)\n"
"Ctrl<Key>a <Key>F10: PF(22)\n"
"<Key>F11: PF(11)\n"
"Ctrl<Key>a <Key>F11: PF(23)\n"
"<Key>F12: PF(12)\n"
"Ctrl<Key>a <Key>F12: PF(24)\n";

/* Base keymap, 3270 mode. */
static char *base_3270_keymap =
"Ctrl<Key>c: Clear\n"
"Ctrl<Key>r: Reset\n"
"Ctrl<Key>l: Redraw\n"
"Ctrl<Key>i: Tab\n"
"<Key>DC: Delete\n"
"Ctrl<Key>h: BackSpace\n"
"<Key>BACKSPACE: BackSpace\n"
"Ctrl<Key>j: Newline\n"
"Ctrl<Key>m: Enter\n"
"<Key>HOME: Home\n";

void
usage(char *msg)
{
	if (msg != CN)
		Warning(msg);
	xs_error("Usage: %s [options] [ps:][LUname@]hostname[:port]",
		programname);
}

/* Callback for connection state changes. */
static void
main_connect(Boolean ignored)
{       
	if (CONNECTED || appres.disconnect_clear)
                ctlr_erase(True);
} 

/* Callback for application exit. */
static void
main_exiting(Boolean ignored)
{       
	if (escaped)
		stop_pager();
	else
		screen_suspend();
} 

int
main(int argc, char *argv[])
{
	const char	*cl_hostname = CN;

	add_resource("keymap.base", NewString(base_keymap));
	add_resource("keymap.base.3270", NewString(base_3270_keymap));

	argc = parse_command_line(argc, (const char **)argv, &cl_hostname);

	if (charset_init(appres.charset) != CS_OKAY) {
		xs_warning("Cannot find charset \"%s\"", appres.charset);
		(void) charset_init(CN);
	}

#if defined(HAVE_LIBREADLINE) /*[*/
	/* Set up readline. */
	rl_readline_name = "c3270";
	rl_initialize();
	rl_attempted_completion_function = attempted_completion;
	rl_completion_entry_function = (Function *)completion_entry;
#endif /*]*/

	screen_init();
	kybd_init();
	keymap_init();
	hostfile_init();
	hostfile_init();
	ansi_init();

	sms_init();

	register_schange(ST_CONNECT, main_connect);
        register_schange(ST_3270_MODE, main_connect);
        register_schange(ST_EXITING, main_exiting);
#if defined(X3270_FT) /*[*/
	ft_init();
#endif /*]*/

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

	/* Connect to the host. */
	screen_suspend();
	if (cl_hostname != CN) {
		appres.once = True;
		if (host_connect(cl_hostname) < 0)
			exit(1);
		/* Wait for negotiations to complete or fail. */
		while (!IN_ANSI && !IN_3270) {
			(void) process_events(True);
			if (!PCONNECTED)
				exit(1);
		}
	} else {
		appres.once = False;
		interact();
	}
	screen_resume();
	screen_disp();

	/* Process events forever. */
	while (1) {
		(void) process_events(True);
		if (escaped) {
			interact();
			screen_resume();
		}
		if (!CONNECTED) {
			screen_suspend();
			(void) printf("Disconnected.\n");
			if (appres.once)
				exit(0);
			interact();
			screen_resume();
		}

		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
		screen_disp();
	}
}

static void
interact(void)
{
	/* In case we got here because a command output, stop the pager. */
	stop_pager();

	for (;;) {
		int sl;
		char *s;
#if defined(HAVE_LIBREADLINE) /*[*/
		char *rl_s;
#else /*][*/
		char buf[1024];
#endif /*]*/

#if defined(HAVE_LIBREADLINE) /*[*/
		s = rl_s = readline("c3270> ");
		if (s == CN) {
			printf("\n");
			exit(0);
		}
#else /*][*/
		/* Display the prompt. */
		(void) printf("c3270> ");
		(void) fflush(stdout);

		/* Get the command, and trim white space. */
		if (fgets(buf, sizeof(buf), stdin) == CN) {
			printf("\n");
			exit(0);
		}
		s = buf;
#endif /*]*/
		while (isspace(*s))
			s++;
		sl = strlen(s);
		while (sl && isspace(s[sl-1]))
			s[--sl] = '\0';

		/* A null command means go back. */
		if (!sl) {
			if (CONNECTED)
				return;
			else
				continue;
		}

#if defined(HAVE_LIBREADLINE) /*[*/
		/* Save this command in the history buffer. */
		add_history(s);
#endif /*]*/

		/* "?" is an alias for "Help". */
		if (!strcmp(s, "?"))
			s = "Help";

		/*
		 * Process the command like a macro, and spin until it
		 * completes.
		 */
		push_command(s);
		while (sms_active()) {
			(void) process_events(True);
		}

		/* Close the pager. */
		stop_pager();

#if defined(HAVE_LIBREADLINE) /*[*/
		/* Give back readline's buffer. */
		free(rl_s);
#endif /*]*/

		/* If it succeeded, return to the session. */
		if (!macro_output && CONNECTED)
			break;
	}
}

/* A command is about to produce output.  Start the pager. */
FILE *
start_pager(void)
{
	static char *lesspath = LESSPATH;
	static char *lesscmd = LESSPATH " -EX";
	static char *morepath = MOREPATH;
	static char *or_cat = " || cat";
	char *pager_env;
	char *pager_cmd = CN;

	if (pager != NULL)
		return pager;

	if ((pager_env = getenv("PAGER")) != CN)
		pager_cmd = pager_env;
	else if (strlen(lesspath))
		pager_cmd = lesscmd;
	else if (strlen(morepath))
		pager_cmd = morepath;
	if (pager_cmd != CN) {
		char *s;

		s = Malloc(strlen(pager_cmd) + strlen(or_cat) + 1);
		(void) sprintf(s, "%s%s", pager_cmd, or_cat);
		pager = popen(s, "w");
		Free(s);
		if (pager == NULL)
			(void) perror(pager_cmd);
	}
	if (pager == NULL)
		pager = stdout;
	return pager;
}

/* Stop the pager. */
static void
stop_pager(void)
{
	if (pager != NULL) {
		if (pager != stdout)
			pclose(pager);
		pager = NULL;
	}
}

#if defined(HAVE_LIBREADLINE) /*[*/

static char **matches = (char **)NULL;
static char **next_match;

/* Generate a match list. */
static char **
attempted_completion(char *text, int start, int end)
{
	char *s;
	int i, j;
	int match_count;

	/* If this is not the first word, fail. */
	s = rl_line_buffer;
	while (*s && isspace(*s))
		s++;
	if (s - rl_line_buffer < start) {
		char *t = s;
		struct host *h;

		/*
		 * If the first word is 'Connect' or 'Open', and the
		 * completion is on the second word, expand from the
		 * hostname list.
		 */

		/* See if we're in the second word. */
		while (*t && !isspace(*t))
			t++;
		while (*t && isspace(*t))
			t++;
		if (t - rl_line_buffer < start)
			return NULL;

		/*
		 * See if the first word is 'Open' or 'Connect'.  In future,
		 * we might do other expansions, and this code would need to
		 * be generalized.
		 */
		if (!((!strncasecmp(s, "Open", 4) && isspace(*(s + 4))) ||
		      (!strncasecmp(s, "Connect", 7) && isspace(*(s + 7)))))
			return NULL;

		/* Look for any matches.  Note that these are case-sensitive. */
		for (h = hosts, match_count = 0; h; h = h->next) {
			if (!strncmp(h->name, t, strlen(t)))
				match_count++;
		}
		if (!match_count)
			return NULL;

		/* Allocate the return array. */
		next_match = matches =
		    Malloc((match_count + 1) * sizeof(char **));

		/* Scan again for matches to fill in the array. */
		for (h = hosts, j = 0; h; h = h->next) {
			int skip = 0;

			if (strncmp(h->name, t, strlen(t)))
				continue;

			/*
			 * Skip hostsfile entries that are duplicates of
			 * RECENT entries we've already recorded.
			 */
			if (h->entry_type != RECENT) {
				for (i = 0; i < j; i++) {
					if (!strcmp(matches[i],
					    h->name)) {
						skip = 1;
						break;
					}
				}
			}
			if (skip)
				continue;

			/*
			 * If the string contains spaces, put it in double
			 * quotes.  Otherwise, just copy it.  (Yes, this code
			 * is fairly stupid, and can be fooled by other
			 * whitespace and embedded double quotes.)
			 */
			if (strchr(h->name, ' ') != CN) {
				matches[j] = Malloc(strlen(h->name) + 3);
				(void) sprintf(matches[j], "\"%s\"", h->name);
				j++;
			} else {
				matches[j++] = NewString(h->name);
			}
		}
		matches[j] = CN;
		return NULL;
	}

	/* Search for matches. */
	for (i = 0, match_count = 0; i < actioncount; i++) {
		if (!strncasecmp(actions[i].string, s, strlen(s)))
			match_count++;
	}
	if (!match_count)
		return NULL;

	/* Return what we got. */
	next_match = matches = Malloc((match_count + 1) * sizeof(char **));
	for (i = 0, j = 0; i < actioncount; i++) {
		if (!strncasecmp(actions[i].string, s, strlen(s))) {
			matches[j++] = NewString(actions[i].string);
		}
	}
	matches[j] = CN;
	return NULL;
}

/* Return the match list. */
static char *
completion_entry(char *text, int state)
{
	char *r;

	if (next_match == NULL)
		return CN;

	if ((r = *next_match++) == CN) {
		Free(matches);
		next_match = matches = NULL;
		return CN;
	} else
		return r;
}

#endif /*]*/

/* c3270-specific actions. */

/* Return a time difference in English */
static char *
hms(time_t ts)
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
		    hr, (hr == 1) ?
			get_message("hour") : get_message("hours"),
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else if (mn > 0)
		(void) sprintf(buf, "%ld %s %ld %s",
		    mn, (mn == 1) ?
			get_message("minute") : get_message("minutes"),
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));
	else
		(void) sprintf(buf, "%ld %s",
		    sc, (sc == 1) ?
			get_message("second") : get_message("seconds"));

	return buf;
}

static void
status_dump(void)
{
	const char *emode, *ftype, *ts;
#if defined(X3270_TN3270E) /*[*/
	const char *eopts;
#endif /*]*/
	extern int linemode; /* XXX */
	extern time_t ns_time;
	extern int ns_bsent, ns_rsent, ns_brcvd, ns_rrcvd;

	action_output("%s", build);
	action_output("%s %s: %d %s x %d %s, %s, %s",
	    get_message("model"), model_name,
	    maxCOLS, get_message("columns"),
	    maxROWS, get_message("rows"),
	    appres.m3279? get_message("fullColor"): get_message("mono"),
	    (appres.extended && !std_ds_host) ? get_message("extendedDs") :
		get_message("standardDs"));
	action_output("%s %s", get_message("terminalName"), termtype);
	if (connected_lu != CN && connected_lu[0])
		action_output("%s %s", get_message("luName"), connected_lu);
	action_output("%s %s", get_message("characterSet"),
	    get_charset_name());
	if (appres.key_map) {
		action_output("%s %s", get_message("keyboardMap"),
		    appres.key_map);
	}
	if (CONNECTED) {
		action_output("%s %s",
		    get_message("connectedTo"),
#if defined(LOCAL_PROCESS) /*[*/
		    (local_process && !strlen(current_host))? "(shell)":
#endif /*]*/
		    current_host);
#if defined(LOCAL_PROCESS) /*[*/
		if (!local_process) {
#endif /*]*/
			action_output("  %s %d", get_message("port"),
			    current_port);
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/
		ts = hms(ns_time);
		if (IN_E)
			emode = "TN3270E ";
		else
			emode = "";
		if (IN_ANSI) {
			if (linemode)
				ftype = get_message("lineMode");
			else
				ftype = get_message("charMode");
			action_output("  %s%s, %s", emode, ftype, ts);
		} else if (IN_SSCP) {
			action_output("  %s%s, %s", emode,
			    get_message("sscpMode"), ts);
		} else if (IN_3270) {
			action_output("  %s%s, %s", emode,
			    get_message("dsMode"), ts);
		} else
			action_output("  %s", ts);

#if defined(X3270_TN3270E) /*[*/
		eopts = tn3270e_current_opts();
		if (eopts != CN) {
			action_output("  %s %s", get_message("tn3270eOpts"),
			    eopts);
		} else if (IN_E) {
			action_output("  %s", get_message("tn3270eNoOpts"));
		}
#endif /*]*/

		if (IN_3270)
			action_output("%s %d %s, %d %s\n%s %d %s, %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rsent, (ns_rsent == 1) ?
				get_message("record") : get_message("records"),
			    get_message("Received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"),
			    ns_rrcvd, (ns_rrcvd == 1) ?
				get_message("record") : get_message("records"));
		else
			action_output("%s %d %s, %s %d %s",
			    get_message("sent"),
			    ns_bsent, (ns_bsent == 1) ?
				get_message("byte") : get_message("bytes"),
			    get_message("received"),
			    ns_brcvd, (ns_brcvd == 1) ?
				get_message("byte") : get_message("bytes"));

#if defined(X3270_ANSI) /*[*/
		if (IN_ANSI) {
			struct ctl_char *c = net_linemode_chars();
			int i;
			char buf[128];
			char *s = buf;

			action_output("%s", get_message("specialCharacters"));
			for (i = 0; c[i].name; i++) {
				if (i && !(i % 4)) {
					*s = '\0';
					action_output(buf);
					s = buf;
				}
				s += sprintf(s, "  %s %s", c[i].name,
				    c[i].value);
			}
			if (s != buf) {
				*s = '\0';
				action_output("%s", buf);
			}
		}
#endif /*]*/
	} else if (HALF_CONNECTED) {
		action_output("%s %s", get_message("connectionPending"),
		    current_host);
	} else
		action_output("%s", get_message("notConnected"));
}

void
Show_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	action_debug(Show_action, event, params, num_params);
	if (*num_params == 0) {
		action_output("  Show stats       connection statistics");
		action_output("  Show status      same as 'Show stats'");
		action_output("  Show keymap      current keymap");
		return;
	}
	if (!strncasecmp(params[0], "stats", strlen(params[0])) ||
	    !strncasecmp(params[0], "status", strlen(params[0]))) {
		status_dump();
	} else if (!strncasecmp(params[0], "keymap", strlen(params[0])))
		keymap_dump();
	else
		popup_an_error("Unknown 'Show' keyword");
}

#if defined(X3270_TRACE) /*[*/
/* Trace([data|keyboard][on[filename]|off]) */
void
Trace_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	int tg = 0;
	Boolean both = False;
	Boolean on = False;

	action_debug(Trace_action, event, params, num_params);
	if (*num_params == 0) {
		action_output("Data tracing is %sabled.",
		    toggled(DS_TRACE)? "en": "dis");
		action_output("Keyboard tracing is %sabled.",
		    toggled(EVENT_TRACE)? "en": "dis");
		return;
	}
	if (!strcasecmp(params[0], "Data"))
		tg = DS_TRACE;
	else if (!strcasecmp(params[0], "Keyboard"))
		tg = EVENT_TRACE;
	else if (!strcasecmp(params[0], "Off")) {
		both = True;
		on = False;
		if (*num_params > 1) {
			popup_an_error("Trace(): Too many arguments for 'Off'");
			return;
		}
	} else if (!strcasecmp(params[0], "On")) {
		both = True;
		on = True;
	} else {
		popup_an_error("Trace(): Unknown trace type -- "
		    "must be Data or Keyboard");
		return;
	}
	if (!both) {
		if (*num_params == 1 || !strcasecmp(params[1], "On"))
			on = True;
		else if (!strcasecmp(params[1], "Off")) {
			on = False;
			if (*num_params > 2) {
				popup_an_error("Trace(): Too many arguments "
				    "for 'Off'");
				return;
			}
		} else {
			popup_an_error("Trace(): Must be 'On' or 'Off'");
			return;
		}
	}

	if (both) {
		if (on && *num_params > 1)
			appres.trace_file = NewString(params[1]);
		if ((on && !toggled(DS_TRACE)) || (!on && toggled(DS_TRACE)))
			do_toggle(DS_TRACE);
		if ((on && !toggled(EVENT_TRACE)) ||
		    (!on && toggled(EVENT_TRACE)))
			do_toggle(EVENT_TRACE);
	} else if ((on && !toggled(tg)) || (!on && toggled(tg))) {
		if (on && *num_params > 2)
			appres.trace_file = NewString(params[2]);
		do_toggle(tg);
	}
}
#endif /*]*/

/* Break to the command prompt. */
void
Escape_action(Widget w unused, XEvent *event unused, String *params unused,
    Cardinal *num_params unused)
{
	action_debug(Escape_action, event, params, num_params);
	screen_suspend();
}

/* Support for c3270 profiles. */

#define PROFILE_ENV	"C3270PRO"
#define NO_PROFILE_ENV	"NOC3270PRO"
#define DEFAULT_PROFILE	"~/.c3270pro"

/* Read in the .c3270pro file. */
void
merge_profile(void)
{
	const char *fname;
	char *profile_name;

	/* Check for the no-profile environment variable. */
	if (getenv(NO_PROFILE_ENV) != CN)
		return;

	/* Read the file. */
	fname = getenv(PROFILE_ENV);
	if (fname == CN || *fname == '\0')
		fname = DEFAULT_PROFILE;
	profile_name = do_subst(fname, True, True);
	(void) read_resource_file(profile_name, False);
	Free(profile_name);
}