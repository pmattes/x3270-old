/*
 * Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001 by Paul Mattes.
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

#define RECONNECT_MS		2000	/* 2 sec before reconnecting to host */
#define RECONNECT_ERR_MS	5000	/* 5 sec before reconnecting to host */

#define MAX_RECENT	5

enum cstate	cstate = NOT_CONNECTED;
Boolean		std_ds_host = False;
Boolean		non_tn3270e_host = False;
Boolean		passthru_host = False;
#define		LUNAME_SIZE	16
char		luname[LUNAME_SIZE+1];
char		*connected_lu = CN;
char		*connected_type = CN;
Boolean		ever_3270 = False;

char           *current_host = CN;
char           *full_current_host = CN;
unsigned short  current_port;
char	       *reconnect_host = CN;

struct host *hosts = (struct host *)NULL;
static struct host *last_host = (struct host *)NULL;
static Boolean auto_reconnect_inprogress = False;
static int net_sock = -1;
static void save_recent(const char *);

#if defined(X3270_DISPLAY) /*[*/
static void try_reconnect(void);
#endif /*]*/

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
	struct host *h;

	if (hostfile_initted)
		return;
	else
		hostfile_initted = True;

	if (appres.hostsfile == CN)
		appres.hostsfile = xs_buffer("%s/ibm_hosts", appres.conf_dir);
	hf = fopen(appres.hostsfile, "r");
	if (hf != (FILE *)NULL) {
		while (fgets(buf, sizeof(buf), hf)) {
			char *s = buf;
			char *name, *entry_type, *hostname;
			char *slash;

			if (strlen(buf) > (unsigned)1 &&
			    buf[strlen(buf) - 1] == '\n') {
				buf[strlen(buf) - 1] = '\0';
			}
			while (isspace(*s))
				s++;
			if (!*s || *s == '#')
				continue;
			name = stoken(&s);
			entry_type = stoken(&s);
			hostname = stoken(&s);
			if (!name || !entry_type || !hostname) {
				popup_an_error("Bad %s syntax, entry skipped",
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
			h->prev = last_host;
			h->next = (struct host *)NULL;
			if (last_host)
				last_host->next = h;
			else
				hosts = h;
			last_host = h;
		}
		(void) fclose(hf);
	}

	/*
	 * Read the recent-connection file, and prepend it to the hosts list.
	 */
	save_recent(CN);
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
	for (h = hosts; h != (struct host *)NULL; h = h->next) {
		if (h->entry_type == RECENT)
			continue;
		if (!strcmp(name, h->name)) {
			*hostname = h->hostname;
			*loginstring = h->loginstring;
			return 1;
		}
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
 * 'needed' is returned True if anything was actually stripped.
 * Returns NULL if there is a syntax error.
 */
static char *
split_host(char *s, Boolean *ansi, Boolean *std_ds, Boolean *passthru,
	Boolean *non_e, char *xluname, char **port, Boolean *needed)
{
	char *lbracket = CN;

	*ansi = False;
	*std_ds = False;
	*passthru = False;
	*non_e = False;
	*xluname = '\0';
	*port = CN;

	*needed = False;

	/*
	 * The hostname may be in square brackets, to clearly delimit prefixes
	 * and suffixes, and to prevent colons in the hostname (IPv6) from
	 * being interpreted as delimiters.
	 */
	lbracket = strchr(s, '[');
	if (lbracket != CN) {
		char *rbracket = strchr(lbracket + 1, ']');

		if (rbracket == CN || rbracket == lbracket + 1)
			return CN;
	}

	for (;;) {
		char *at;

		if (!strncasecmp(s, "a:", 2)) {
			*ansi = True;
			s += 2;
			*needed = True;
			continue;
		}
		if (!strncasecmp(s, "s:", 2)) {
			*std_ds = True;
			s += 2;
			*needed = True;
			continue;
		}
		if (!strncasecmp(s, "p:", 2)) {
			*passthru = True;
			s += 2;
			*needed = True;
			continue;
		}
		if (!strncasecmp(s, "n:", 2)) {
			*non_e = True;
			s += 2;
			*needed = True;
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
			*needed = True;
			continue;
		}

		break;
	}
	if (*s) {
		char *r;
		char *sep;

		/*
		 * If the hostname is in square brackets, we had better be
		 * pointing at it now.
		 */
		if (lbracket != CN) {
			if (s != lbracket)
				return CN;
			else
				s++;
		}

		/* Allocate a new copy of the hostname. */
		r = NewString(s);

		/*
		 * If the hostname is in square brackets, terminate the
		 * hostname at the ']' and search the suffix after.
		 */
		if (lbracket != CN) {
			sep = strchr(r, ']');
			*sep++ = '\0';
			switch (*sep) {
			case '\0':
				return r;
			case ' ':
			case ':':
				break;
			default:
				Free(r);
				return CN;
			}
			sep++;
			while (*sep == ' ') {
				sep++;
			}
			*port = sep;
			return r;
		}

		/* Search for a delimter. */
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
			*needed = True;
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
	Boolean resolving;
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
	Replace(reconnect_host, NewString(nb));

	/* Remember this hostname in the recent connection list and file. */
	save_recent(nb);

#if defined(LOCAL_PROCESS) /*[*/
	if ((localprocess_cmd = parse_localprocess(nb)) != CN) {
		chost = localprocess_cmd;
		port = appres.port;
	} else
#endif /*]*/
	{
		Boolean needed;

		/* Strip off and remember leading qualifiers. */
		if ((s = split_host(nb, &ansi_host, &std_ds_host,
		    &passthru_host, &non_tn3270e_host, luname, &port,
		    &needed)) == CN)
			return -1;

		/* Look up the name in the hosts file. */
		if (!needed && hostfile_lookup(s, &target_name, &ps)) {
			/*
			 * Rescan for qualifiers.
			 * Qualifiers, LU names, and ports are all overridden
			 * by the hosts file.
			 */
			Free(s);
			if (!(s = split_host(target_name, &ansi_host,
			    &std_ds_host, &passthru_host, &non_tn3270e_host,
			    luname, &port, &needed)))
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
		Replace(full_current_host, NewString(n));
	}
	Replace(current_host, CN);
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
	net_sock = net_connect(chost, port, localprocess_cmd != CN, &resolving,
	    &pending);
	if (net_sock < 0 && !resolving) {
#if defined(X3270_DISPLAY) /*[*/
		if (appres.once) {
			/* Exit when the error pop-up pops down. */
			exiting = True;
		}
		else if (appres.reconnect) {
			auto_reconnect_inprogress = True;
			(void) AddTimeOut(RECONNECT_ERR_MS, try_reconnect);
		}
#endif /*]*/
		/* Redundantly signal a disconnect. */
		st_changed(ST_CONNECT, False);
		return -1;
	}

	/* Still thinking about it? */
	if (resolving) {
		cstate = RESOLVING;
		st_changed(ST_RESOLVING, True);
		return 0;
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
#if defined(X3270_DISPLAY) /*[*/
		if (appres.reconnect && error_popup_visible())
			popdown_an_error();
#endif /*]*/
	}

	return 0;
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * Reconnect to the last host.
 */
static void
host_reconnect(void)
{
	if (auto_reconnect_inprogress || current_host == CN ||
	    CONNECTED || HALF_CONNECTED)
		return;
	if (host_connect(reconnect_host) >= 0)
		auto_reconnect_inprogress = False;
}

/*
 * Called from timer to attempt an automatic reconnection.
 */
static void
try_reconnect(void)
{
	auto_reconnect_inprogress = False;
	host_reconnect();
}
#endif /*]*/

void
host_disconnect(Boolean failed)
{
	if (CONNECTED || HALF_CONNECTED) {
		x_remove_input();
		net_disconnect();
		net_sock = -1;
#if defined(X3270_DISPLAY) /*[*/
		if (appres.once) {
			if (error_popup_visible()) {
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
		} else if (appres.reconnect && !auto_reconnect_inprogress) {
			/* Schedule an automatic reconnection. */
			auto_reconnect_inprogress = True;
			(void) AddTimeOut(failed? RECONNECT_ERR_MS:
						   RECONNECT_MS,
					  try_reconnect);
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

#if defined(X3270_DISPLAY) /*[*/
	if (appres.reconnect && error_popup_visible())
		popdown_an_error();
#endif /*]*/
}

/* Comparison function for the qsort. */
static int
host_compare(const void *e1, const void *e2)
{
	const struct host *h1 = *(const struct host **)e1;
	const struct host *h2 = *(const struct host **)e2;
	int r;

	if (h1->connect_time > h2->connect_time)
		r = -1;
	else if (h1->connect_time < h2->connect_time)
		r = 1;
	else
		r = 0;
#if defined(CFDEBUG) /*[*/
	printf("%s %ld %d %s %ld\n",
	    h1->name, h1->connect_time,
	    r,
	    h2->name, h2->connect_time);
#endif /*]*/
	return r;
}

#if defined(CFDEBUG) /*[*/
static void
dump_array(const char *when, struct host **array, int nh)
{
	int i;

	printf("%s\n", when);
	for (i = 0; i < nh; i++) {
		printf(" %15s %ld\n", array[i]->name, array[i]->connect_time);
	}
}
#endif /*]*/

/* Save the most recent host in the recent host list. */
static void
save_recent(const char *hn)
{
	char *lcf_name = CN;
	FILE *lcf = (FILE *)NULL;
	struct host *h;
	struct host *rest = (struct host *)NULL;
	int n_ent = 0;
	struct host *h_array[(MAX_RECENT * 2) + 1];
	int nh = 0;
	int i, j;
	time_t t = time((time_t *)NULL);

	/* Allocate a new entry. */
	if (hn != CN) {
		h = (struct host *)Malloc(sizeof(*h));
		h->name = NewString(hn);
		h->hostname = NewString(hn);
		h->entry_type = RECENT;
		h->loginstring = CN;
		h->connect_time = t;
		h_array[nh++] = h;
	}

	/* Put the existing entries into the array. */
	for (h = hosts; h != (struct host *)NULL; h = h->next) {
		if (h->entry_type != RECENT)
			break;
		h_array[nh++] = h;
	}

	/* Save the ibm_hosts entries for later. */
	rest = h;
	if (rest != (struct host *)NULL)
		rest->prev = (struct host *)NULL;

	/*
	 * Read the last-connection file, to capture the any changes made by
	 * other instances of x3270.  
	 */
	if (appres.connectfile_name != CN &&
	    strcasecmp(appres.connectfile_name, "none")) {
		lcf_name = do_subst(appres.connectfile_name, True, True);
		lcf = fopen(lcf_name, "r");
	}
	if (lcf != (FILE *)NULL) {
		char buf[1024];

		while (fgets(buf, sizeof(buf), lcf) != CN) {
			int sl;
			time_t connect_time;
			char *ptr;

			/* Pick apart the entry. */
			sl = strlen(buf);
			if (buf[sl - 1] == '\n')
				buf[sl-- - 1] = '\0';
			if (!sl ||
			    buf[0] == '#' ||
			    (connect_time = strtoul(buf, &ptr, 10)) == 0L ||
			    ptr == buf ||
			    *ptr != ' ' ||
			    !*(ptr + 1))
				continue;

			h = (struct host *)Malloc(sizeof(*h));
			h->name = NewString(ptr + 1);
			h->hostname = NewString(ptr + 1);
			h->entry_type = RECENT;
			h->loginstring = CN;
			h->connect_time = connect_time;
			h_array[nh++] = h;
			if (nh > (MAX_RECENT * 2) + 1)
				break;
		}
		fclose(lcf);
	}

	/* Sort the array, in reverse order by connect time. */
#if defined(CFDEBUG) /*[*/
	dump_array("before", h_array, nh);
#endif /*]*/
	qsort(h_array, nh, sizeof(struct host *), host_compare);
#if defined(CFDEBUG) /*[*/
	dump_array("after", h_array, nh);
#endif /*]*/

	/*
	 * Filter out duplicate host names, and limit the array to
	 * MAX_RECENT entries total.
	 */
	hosts = (struct host *)NULL;
	last_host = (struct host *)NULL;
	for (i = 0; i < nh; i++) {
		h = h_array[i];
		if (h == (struct host *)NULL)
			continue;
		h->next = (struct host *)NULL;
		if (last_host != (struct host *)NULL)
			last_host->next = h;
		h->prev = last_host;
		last_host = h;
		if (hosts == (struct host *)NULL)
			hosts = h;
		n_ent++;

		/* Zap the duplicates. */
		for (j = i+1; j < nh; j++) {
			if (h_array[j] &&
			    (n_ent >= MAX_RECENT ||
			     !strcmp(h_array[i]->name, h_array[j]->name))) {
#if defined(CFDEBUG) /*[*/
				printf("%s is a dup of %s\n",
				    h_array[j]->name, h_array[i]->name);
#endif /*]*/
				Free(h_array[j]->name);
				Free(h_array[j]->hostname);
				Free(h_array[j]);
				h_array[j] = (struct host *)NULL;
			}
		}
	}

	/* Re-attach the ibm_hosts entries to the end. */
	if (rest != (struct host *)NULL) {
		if (last_host != (struct host *)NULL) {
			last_host->next = rest;
		} else {
			hosts = rest;
		}
		rest->prev = last_host;
	}

	/* If there's been a change, rewrite the file. */
	if (hn != CN &&
	    appres.connectfile_name != CN &&
	    strcasecmp(appres.connectfile_name, "none")) {
		lcf = fopen(lcf_name, "w");
		if (lcf != (FILE *)NULL) {
			fprintf(lcf, "# Created %s# by %s\n", ctime(&t), build);
			for (h = hosts; h != (struct host *)NULL; h = h->next) {
				if (h->entry_type != RECENT)
					break;
				(void) fprintf(lcf, "%lu %s\n", h->connect_time,
				    h->name);
			}
		}
		fclose(lcf);
		Free(lcf_name);
	}
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
