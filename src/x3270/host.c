/*
 * Copyright 1993, 1994, 1995, 1996, 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	host.c
 *		This module handles the ibm_hosts file, connecting to and
 *		disconnecting from hosts, and state changes on the host
 *		connection.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "actionsc.h"
#include "hostc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "telnetc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#define RECONNECT_MS	2000	/* 2 sec before reconnecting to host */

enum cstate	cstate = NOT_CONNECTED;
Boolean		std_ds_host = False;
Boolean		passthru_host = False;
#define		LUNAME_SIZE	16
char		luname[LUNAME_SIZE+1];
char		*connected_lu = CN;
char		*connected_type = CN;
Boolean		ever_3270 = False;
Boolean		auto_reconnect_disabled = False;

char           *current_host = CN;
char           *full_current_host = CN;
char	       *reconnect_host = CN;
unsigned short  current_port;

struct host *hosts = (struct host *)NULL;
static struct host *last_host = (struct host *)NULL;
static Boolean auto_reconnect_inprogress = False;
static int net_sock = -1;

static char *
stoken(char **s)
{
	char *r;
	char *ss = *s;

	if (!*ss)
		return NULL;
	r = ss;
	while (*ss && *ss != ' ' && *ss != '\t')
		ss++;
	if (*ss) {
		*ss++ = '\0';
		while (*ss == ' ' || *ss == '\t')
			ss++;
	}
	*s = ss;
	return r;
}


/*
 * Read the host file
 */
void
hostfile_init(void)
{
	FILE *hf;
	char buf[1024];
	static Boolean hostfile_initted = False;

	if (hostfile_initted)
		return;
	else
		hostfile_initted = True;

	if (appres.hostsfile == CN)
		appres.hostsfile = xs_buffer("%s/ibm_hosts", LIBX3270DIR);
	hf = fopen(appres.hostsfile, "r");
	if (!hf)
		return;

	while (fgets(buf, 1024, hf)) {
		char *s = buf;
		char *name, *entry_type, *hostname;
		struct host *h;
		char *slash;

		if (strlen(buf) > (unsigned)1 && buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = '\0';
		while (isspace(*s))
			s++;
		if (!*s || *s == '#')
			continue;
		name = stoken(&s);
		entry_type = stoken(&s);
		hostname = stoken(&s);
		if (!name || !entry_type || !hostname) {
			xs_warning("Bad %s syntax, entry skipped",
			    ResHostsFile);
			continue;
		}
		h = (struct host *)Malloc(sizeof(*h));
		h->name = NewString(name);
		h->hostname = NewString(hostname);

		/*
		 * Quick syntax extension to allow the hosts file to
		 * specify a port as host/port.
		 */
		if ((slash = strchr(h->hostname, '/')))
			*slash = ':';

		if (!strcmp(entry_type, "primary"))
			h->entry_type = PRIMARY;
		else
			h->entry_type = ALIAS;
		if (*s)
			h->loginstring = NewString(s);
		else
			h->loginstring = CN;
		h->next = (struct host *)0;
		if (last_host)
			last_host->next = h;
		else
			hosts = h;
		last_host = h;
	}
	(void) fclose(hf);
}

/*
 * Look up a host in the list.  Turns aliases into real hostnames, and
 * finds loginstrings.
 */
static int
hostfile_lookup(const char *name, char **hostname, char **loginstring)
{
	struct host *h;

	hostfile_init();
	for (h = hosts; h; h = h->next)
		if (! strcmp(name, h->name)) {
			*hostname = h->hostname;
			*loginstring = h->loginstring;
			return 1;
		}
	return 0;
}

#if defined(LOCAL_PROCESS) /*[*/
/* Recognize and translate "-e" options. */
static const char *
parse_localprocess(const char *s)
{
	int sl = strlen(OptLocalProcess);

	if (!strncmp(s, OptLocalProcess, sl)) {
		if (s[sl] == ' ')
			return(s + sl + 1);
		else if (s[sl] == '\0') {
			char *r;

			r = getenv("SHELL");
			if (r != CN)
				return r;
			else
				return "/bin/sh";
		}
	}
	return CN;
}
#endif /*]*/

/*
 * Strip qualifiers from a hostname.
 * Returns the hostname part in a newly-malloc'd string.
 */
static char *
split_host(char *s, Boolean *ansi, Boolean *std_ds, Boolean *passthru,
	char *xluname, char **port)
{
	*ansi = False;
	*std_ds = False;
	*passthru = False;
	*xluname = '\0';
	*port = CN;

	for (;;) {
		char *at;

		if (!strncmp(s, "a:", 2) || !strncmp(s, "A:", 2)) {
			*ansi = True;
			s += 2;
			continue;
		}
		if (!strncmp(s, "s:", 2) || !strncmp(s, "S:", 2)) {
			*std_ds = True;
			s += 2;
			continue;
		}
		if (!strncmp(s, "p:", 2) || !strncmp(s, "P:", 2)) {
			*passthru = True;
			s += 2;
			continue;
		}
		if ((at = strchr(s, '@')) != NULL) {
			if (at != s) {
				int nc = at - s;
		
				if (nc > LUNAME_SIZE)
					nc = LUNAME_SIZE;
				(void) strncpy(xluname, s, nc);
				xluname[nc] = '\0';
			}
			s = at + 1;
			continue;
		}

		break;
	}
	if (*s) {
		char *r;
		char *sep;

		r = NewString(s);
		sep = strrchr(r, ':');
		if (sep == NULL)
			sep = strrchr(r, ' ');
		else if (strrchr(r, ' ') != NULL) {
			Free(r);
			return CN;
		}
		if (sep != CN) {
			*sep++ = '\0';
			while (*sep == ' ')
				sep++;
		}
		if (port != (char **) NULL)
			*port = sep;
		return r;
	} else
		return CN;
}


/*
 * Network connect/disconnect operations, combined with X input operations.
 *
 * Returns 0 for success, -1 for error.
 * Sets 'reconnect_host', 'current_host' and 'full_current_host' as
 * side-effects.
 */
int
host_connect(const char *n)
{
	char nb[2048];		/* name buffer */
	char *s;		/* temporary */
	const char *chost;	/* to whom we will connect */
	char *target_name;
	char *ps = CN;
	char *port = CN;
	Boolean pending;
	static Boolean ansi_host;
	const char *localprocess_cmd = NULL;

	if (CONNECTED || auto_reconnect_inprogress)
		return 0;

	/* Skip leading blanks. */
	while (*n == ' ')
		n++;
	if (!*n) {
		popup_an_error("Invalid (empty) hostname");
		return -1;
	}

	/* Save in a modifiable buffer. */
	(void) strcpy(nb, n);

	/* Strip trailing blanks. */
	s = nb + strlen(nb) - 1;
	while (*s == ' ')
		*s-- = '\0';

	/* Remember this hostname, as the last hostname we connected to. */
	if (reconnect_host != CN)
		Free(reconnect_host);
	reconnect_host = NewString(nb);

#if defined(LOCAL_PROCESS) /*[*/
	if ((localprocess_cmd = parse_localprocess(nb)) != CN) {
		chost = localprocess_cmd;
		port = appres.port;
	} else
#endif /*]*/
	{
		/* Strip off and remember leading qualifiers. */
		if ((s = split_host(nb, &ansi_host, &std_ds_host,
		    &passthru_host, luname, &port)) == CN)
			return -1;

		/* Look up the name in the hosts file. */
		if (hostfile_lookup(s, &target_name, &ps)) {
			/*
			 * Rescan for qualifiers.
			 * Qualifiers, LU names, and ports are all overridden
			 * by the hosts file.
			 */
			Free(s);
			if (!(s = split_host(target_name, &ansi_host,
			    &std_ds_host, &passthru_host, luname, &port)))
				return -1;
		}
		chost = s;

		/* Default the port. */
		if (port == CN)
			port = appres.port;
	}

	/*
	 * Store the original name in globals, even if we fail the connect
	 * later:
	 *  current_host is the hostname part, stripped of qualifiers, luname
	 *   and port number
	 *  full_current_host is the entire string, for use in reconnecting
	 */
	if (n != full_current_host) {
		if (full_current_host != CN)
			Free(full_current_host);
		full_current_host = NewString(n);
	}
	if (current_host != CN)
		Free(current_host);
	if (localprocess_cmd != CN) {
		if (full_current_host[strlen(OptLocalProcess)] != '\0')
		current_host = NewString(full_current_host +
		    strlen(OptLocalProcess) + 1);
		else
			current_host = NewString("default shell");
	} else {
		current_host = s;
	}

	/* Attempt contact. */
	ever_3270 = False;
	net_sock = net_connect(chost, port, localprocess_cmd != CN, &pending);
	if (net_sock < 0) {
#if defined(X3270_DISPLAY) /*[*/
		if (appres.once) {
			/* Exit when the error pop-up pops down. */
			exiting = True;
		} else if (appres.reconnect) {
			/* Put up the reconnect button. */
			auto_reconnect_disabled = True;
			menubar_show_reconnect();
		}
#endif /*]*/
		return -1;
	}

	/* Success. */

	/* Set pending string. */
	if (ps != CN)
		login_macro(ps);

	/* Prepare Xt for I/O. */
	x_add_input(net_sock);

	/* Set state and tell the world. */
	if (pending) {
		cstate = PENDING;
		st_changed(ST_HALF_CONNECT, True);
	} else {
		cstate = CONNECTED_INITIAL;
		st_changed(ST_CONNECT, True);
	}

	return 0;
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * Called from timer to attempt an automatic reconnection.
 */
/*ARGSUSED*/
static void
try_reconnect(void)
{
	auto_reconnect_inprogress = False;
	host_reconnect();
}

/*
 * Reconnect to the last host.
 */
void
host_reconnect(void)
{
	if (auto_reconnect_inprogress || current_host == CN ||
	    CONNECTED || HALF_CONNECTED)
		return;
	if (host_connect(reconnect_host) >= 0)
		auto_reconnect_inprogress = False;
}
#endif /*]*/

void
host_disconnect(Boolean disable)
{
	auto_reconnect_disabled = disable;
	if (CONNECTED || HALF_CONNECTED) {
		x_remove_input();
		net_disconnect();
		net_sock = -1;
#if defined(X3270_DISPLAY) /*[*/
		if (appres.once) {
			if (error_popup_visible) {
				/*
				 * If there is an error pop-up, exit when it
				 * pops down.
				 */
				exiting = True;
			} else {
				/* Exit now. */
				x3270_exit(0);
				return;
			}
		} else if (appres.reconnect && !auto_reconnect_inprogress &&
		    !auto_reconnect_disabled) {
			/* Schedule an automatic reconnection. */
			auto_reconnect_inprogress = True;
			(void) AddTimeOut(RECONNECT_MS, try_reconnect);
		}
#endif /*]*/

		/*
		 * Remember a disconnect from ANSI mode, to keep screen tracing
		 * in sync.
		 */
#if defined(X3270_TRACE) /*[*/
		if (IN_ANSI && toggled(SCREEN_TRACE))
			trace_ansi_disc();
#endif /*]*/

		cstate = NOT_CONNECTED;

		/* Propagate the news to everyone else. */
		st_changed(ST_CONNECT, False);
	}
}

/* The host has entered 3270 or ANSI mode, or switched between them. */
void
host_in3270(enum cstate new_cstate)
{
	Boolean now3270 = (new_cstate == CONNECTED_3270 ||
			   new_cstate == CONNECTED_SSCP ||
			   new_cstate == CONNECTED_TN3270E);

	cstate = new_cstate;
	ever_3270 = now3270;
	st_changed(ST_3270_MODE, now3270);
}

void
host_connected(void)
{
	cstate = CONNECTED_INITIAL;
	st_changed(ST_CONNECT, True);
}

/* Support for state change callbacks. */

struct st_callback {
	struct st_callback *next;
	void (*func)(Boolean);
};
static struct st_callback *st_callbacks[N_ST];
static struct st_callback *st_last[N_ST];

/* Register a function interested in a state change. */
void
register_schange(int tx, void (*func)(Boolean))
{
	struct st_callback *st;

	st = (struct st_callback *)Malloc(sizeof(*st));
	st->func = func;
	st->next = (struct st_callback *)NULL;
	if (st_last[tx] != (struct st_callback *)NULL)
		st_last[tx]->next = st;
	else
		st_callbacks[tx] = st;
	st_last[tx] = st;
}

/* Signal a state change. */
void
st_changed(int tx, Boolean mode)
{
	struct st_callback *st;

	for (st = st_callbacks[tx];
	     st != (struct st_callback *)NULL;
	     st = st->next) {
		(*st->func)(mode);
	}
}

/* Explicit connect/disconnect actions. */

/*ARGSUSED*/
void
Connect_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Connect_action, event, params, num_params);
	if (check_usage(Connect_action, *num_params, 1, 1) < 0)
		return;
	if (CONNECTED || HALF_CONNECTED) {
		popup_an_error("Already connected");
		return;
	}
	(void) host_connect(params[0]);

	/*
	 * If called from a script and the connection was successful (or
	 * half-successful), pause the script until we are connected and
	 * we have identified the host type.
	 */
	if (!w && (CONNECTED || HALF_CONNECTED))
		sms_connect_wait();
}

#if defined(X3270_MENUS) /*[*/
/*ARGSUSED*/
void
Reconnect_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	action_debug(Reconnect_action, event, params, num_params);
	if (check_usage(Reconnect_action, *num_params, 0, 0) < 0)
		return;
	if (CONNECTED || HALF_CONNECTED) {
		popup_an_error("Already connected");
		return;
	}
	if (current_host == CN) {
		popup_an_error("No previous host to connect to");
		return;
	}
	host_reconnect();

	/*
	 * If called from a script and the connection was successful (or
	 * half-successful), pause the script until we are connected and
	 * we have identified the host type.
	 */
	if (!w && (CONNECTED || HALF_CONNECTED))
		sms_connect_wait();
}
#endif /*]*/

void
Disconnect_action(Widget w unused, XEvent *event, String *params,
	Cardinal *num_params)
{
	action_debug(Disconnect_action, event, params, num_params);
	if (check_usage(Disconnect_action, *num_params, 0, 0) < 0)
		return;
	host_disconnect(False);
}
