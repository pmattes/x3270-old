/*
 * Copyright 2000, 2001, 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * pr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 * pr3287: A 3270 printer emulator for TELNET sessions.
 *
 *	pr3287 [options] [lu[,lu...]@]host[:port]
 *	Options are:
 *	    -dameon
 *		become a daemon after negotiating
 *	    -assoc session
 *		associate with a session (TN3270E only)
 *	    -command "string"
 *		command to use to print (default "lpr")
 *          -charset name
 *          -charset @file
 *          -charset =spec
 *		use the specified character set
 *          -crlf
 *		expand newlines to CR/LF
 *          -blanklines
 *		display blank lines even if they're empty (formatted LU3)
 *          -ffthru
 *		pass through SCS FF orders
 *          -ffskip
 *		skip FF at top of page
 *          -reconnect
 *		keep trying to reconnect
 *	    -trace
 *		trace data stream to a file
 *          -v
 *		verbose output about negotiation
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#include "globals.h"
#include "trace_dsc.h"
#include "ctlrc.h"
#include "telnetc.h"

#if defined(_IOLBF) /*[*/
#define SETLINEBUF(s)	setvbuf(s, (char *)NULL, _IOLBF, BUFSIZ)
#else /*][*/
#define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

#if !defined(INADDR_NONE) /*[*/
#define INADDR_NONE	0xffffffffL
#endif /*]*/

/* Externals. */
extern int charset_init(const char *csname);
extern char *build;
extern FILE *tracef;

/* Globals. */
char *programname = NULL;	/* program name */
int blanklines = 0;
int ignoreeoj = 0;
int reconnect = 0;
int crlf = 0;
int ffthru = 0;
int ffskip = 0;
int verbose = 0;
int ssl_host = 0;

/* User options. */
static enum { NOT_DAEMON, WILL_DAEMON, AM_DAEMON } bdaemon = NOT_DAEMON;
static char *assoc = NULL;	/* TN3270 session to associate with */
const char *command = "lpr";	/* command to run for printing */
static int tracing = 0;		/* are we tracing? */

/* Print a usage message and exit. */
static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [options] [lu[,lu...]@]host[:port]\n"
"Options:\n%s\n%s\n%s\n", programname,
"  -daemon          become a daemon after connecting\n"
"  -assoc <session> associate with a session (TN3270E only)\n"
"  -charset <name>  use built-in alternate EBCDIC-to-ASCII mappings\n"
"  -charset @<file> use alternate EBCDIC-to-ASCII mappings from file\n"
"  -charset =\"<ebc>=<asc> ...\"\n"
"                   specify alternate EBCDIC-to-ASCII mappings",
"  -command \"<cmd>\" use <cmd> for printing (default \"lpr\")\n"
"  -blanklines      display blank lines even if empty (formatted LU3)\n"
"  -crlf            expand newlines to CR/LF\n"
"  -ffthru          pass through SCS FF orders\n"
"  -ffskip          skip FF orders at top of page",
"  -ignoreeoj       ignore PRINT-EOJ commands\n"
"  -reconnect       keep trying to reconnect\n"
"  -trace           trace data stream to /tmp/x3trc.<pid>");
	exit(1);
}

/* Print an error message. */
void
errmsg(const char *fmt, ...)
{
	va_list args;
	static char buf[2][4096] = { "", "" };
	static int ix = 0;

	ix = !ix;
	va_start(args, fmt);
	(void) vsprintf(buf[ix], fmt, args);
	va_end(args);
	trace_ds("Error: %s\n", buf[ix]);
	if (!strcmp(buf[ix], buf[!ix])) {
		if (verbose)
			(void) fprintf(stderr, "Suppressed error '%s'\n",
			    buf[ix]);
		return;
	}
	if (bdaemon == AM_DAEMON)
		syslog(LOG_ERR, "%s: %s", programname, buf[ix]);
	else
		(void) fprintf(stderr, "%s: %s\n", programname, buf[ix]);
}

/* Memory allocation. */
void *
Malloc(size_t len)
{
	void *p = malloc(len);

	if (p == NULL) {
		errmsg("Out of memory");
		exit(1);
	}
	return p;
}

void
Free(void *p)
{
	free(p);
}

void *
Realloc(void *p, size_t len)
{
	void *pn;

	pn = realloc(p, len);
	if (pn == NULL) {
		errmsg("Out of memory");
		exit(1);
	}
	return pn;
}

char *
NewString(const char *s)
{
	char *p;


	p = Malloc(strlen(s) + 1);
	return strcpy(p, s);
}

/* Signal handler for SIGTERM, SIGINT and SIGHUP. */
static void
fatal_signal(int sig)
{
	/* Flush any pending data and exit. */
	trace_ds("Fatal signal %d\n", sig);
	(void) print_eoj();
	errmsg("Exiting on signal %d", sig);
	exit(0);
}

/* Signal handler for SIGUSR1. */
static void
flush_signal(int sig)
{
	/* Flush any pending data and exit. */
	trace_ds("Flush signal %d\n", sig);
	(void) print_eoj();
}

int
main(int argc, char *argv[])
{
	int i;
	char *at, *colon;
	int len;
	char const *charset = NULL;
	char *lu = NULL;
	char *host = NULL;
	const char *port = "telnet";
	struct hostent *he;
	struct in_addr ha;
	struct servent *se;
	unsigned short p;
	struct sockaddr_in sin;
	int s = -1;
	int rc = 0;
	int report_success = 0;
#if defined(HAVE_LIBSSL) /*[*/
	int any_prefixes = False;
#endif /*]*/

	/* Learn our name. */
	if ((programname = strrchr(argv[0], '/')) != NULL)
		programname++;
	else
		programname = argv[0];

	/* Gather the options. */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "-daemon"))
			bdaemon = WILL_DAEMON;
		else if (!strcmp(argv[i], "-assoc")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -assoc\n");
				usage();
			}
			assoc = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-command")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -command\n");
				usage();
			}
			command = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-charset")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -charset\n");
				usage();
			}
			charset = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-blanklines")) {
			blanklines = 1;
		} else if (!strcmp(argv[i], "-crlf")) {
			crlf = 1;
		} else if (!strcmp(argv[i], "-ignoreeoj")) {
			ignoreeoj = 1;
		} else if (!strcmp(argv[i], "-ffthru")) {
			ffthru = 1;
		} else if (!strcmp(argv[i], "-ffskip")) {
			ffskip = 1;
		} else if (!strcmp(argv[i], "-reconnect")) {
			reconnect = 1;
		} else if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-trace")) {
			tracing = 1;
		} else if (!strcmp(argv[i], "--help")) {
			usage();
		} else
			usage();
	}
	if (argc != i + 1)
		usage();

	/* Pick apart the hostname, LUs and port. */
#if defined(HAVE_LIBSSL) /*[*/
	do {
		if (!strncasecmp(argv[i], "l:", 2)) {
			ssl_host = True;
			argv[i] += 2;
			any_prefixes = True;
		} else
			any_prefixes = False;
	} while (any_prefixes);
#endif /*]*/
	if ((at = strchr(argv[i], '@')) != NULL) {
		len = at - argv[i];
		if (!len)
			usage();
		lu = Malloc(len + 1);
		(void) strncpy(lu, argv[i], len);
		lu[len] = '\0';
		host = at + 1;
	} else
		host = argv[i];
	colon = strchr(host, ':');
	if (colon != NULL) {
		char *tmp;

		len = colon - host;
		if (!len || !*(colon + 1))
			usage();
		port = colon + 1;
		tmp = Malloc(len + 1);
		(void) strncpy(tmp, host, len);
		tmp[len] = '\0';
		host = tmp;
	}

	/* Resolve the port number. */
	se = getservbyname(port, "tcp");
	if (se != NULL)
		p = se->s_port;
	else {
		unsigned long l;
		char *ptr;

		l = strtoul(port, &ptr, 0);
		if (ptr == port || *ptr != '\0') {
			(void) fprintf(stderr, "Unknown port: %s\n", port);
			exit(1);
		}
		if (l & ~0xffff) {
			(void) fprintf(stderr, "Invalid port: %s\n", port);
			exit(1);
		}
		p = htons((unsigned short)l);
	}

	/* Remap the character set. */
	if (charset_init(charset) < 0)
		exit(1);

	/* Try opening the trace file, if there is one. */
	if (tracing) {
		char tracefile[256];
		time_t clk;

		(void) sprintf(tracefile, "/tmp/x3trc.%d", getpid());
		tracef = fopen(tracefile, "a");
		if (tracef == NULL) {
			perror(tracefile);
			exit(1);
		}
		(void) SETLINEBUF(tracef);
		clk = time((time_t *)0);
		(void) fprintf(tracef, "Trace started %s", ctime(&clk));
		(void) fprintf(tracef, " Version %s\n", build);
	}

	/* Become a daemon. */
	if (bdaemon != NOT_DAEMON) {
		switch (fork()) {
			case -1:
				perror("fork");
				exit(1);
				break;
			case 0:
				/* Child: Break away from the TTY. */
				if (setsid() < 0)
					exit(1);
				bdaemon = AM_DAEMON;
				break;
			default:
				/* Parent: We're all done. */
				exit(0);
				break;

		}
	}

	/* Handle signals. */
	(void) signal(SIGTERM, fatal_signal);
	(void) signal(SIGINT, fatal_signal);
	(void) signal(SIGHUP, fatal_signal);
	(void) signal(SIGUSR1, flush_signal);
	(void) signal(SIGPIPE, SIG_IGN);

	/*
	 * One-time initialization is now complete.
	 * (Most) everything beyond this will now be retried, if the -reconnect
	 * option is in effect.
	 */
	for (;;) {

		/* Resolve the host name. */
		he = gethostbyname(host);
		if (he != NULL) {
			(void) memcpy(&ha, he->h_addr_list[0], he->h_length);
		} else {
			ha.s_addr = inet_addr(host);
			if (ha.s_addr == INADDR_NONE) {
				errmsg("Unknown host: %s", host);
				rc = 1;
				goto retry;
			}
		}

		/* Connect to the host. */
		s = socket(PF_INET, SOCK_STREAM, 0);
		if (s < 0) {
			errmsg("socket: %s", strerror(errno));
			exit(1);
		}
		(void) memset(&sin, '\0', sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr = ha;
		sin.sin_port = p;
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			errmsg("%s: %s", host, strerror(errno));
			rc = 1;
			goto retry;
		}

		/* Say hello. */
		if (verbose) {
			(void) fprintf(stderr, "Connected to %s, port %u%s\n",
			    inet_ntoa(sin.sin_addr),
			    ntohs(sin.sin_port),
			    ssl_host? " via SSL": "");
			if (assoc != NULL)
				(void) fprintf(stderr, "Associating with LU "
				    "%s\n", assoc);
			else if (lu != NULL)
				(void) fprintf(stderr, "Connecting to LU %s\n",
				    lu);
			(void) fprintf(stderr, "Command: %s\n", command);
		}

		/* Negotiate. */
		if (negotiate(s, lu, assoc) < 0) {
			rc = 1;
			goto retry;
		}

		/* Report sudden success. */
		if (report_success) {
			errmsg("Connected to %s, port %u",
			    inet_ntoa(sin.sin_addr),
			    ntohs(sin.sin_port));
			report_success = 0;
		}

		/* Process what we're told to process. */
		if (process(s) < 0) {
			rc = 1;
			if (verbose)
				(void) fprintf(stderr,
				    "Disconnected (error).\n");
			goto retry;
		}
		if (verbose)
			(void) fprintf(stderr, "Disconnected (eof).\n");

	    retry:
		/* Flush any pending data. */
		(void) print_eoj();

		/* Close the socket. */
		if (s >= 0) {
			net_disconnect();
			s = -1;
		}

		if (!reconnect)
			break;
		report_success = 1;

		/* Wait a while, to reduce thrash. */
		if (rc)
			sleep(5);

		rc = 0;
	}

	return rc;
}

/* Tracing function. */
void
trace_str(const char *s)
{
	if (tracef)
		fprintf(tracef, "%s", s);
}
