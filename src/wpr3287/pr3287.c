/*
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2007 by Paul Mattes.
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
 *		command to use to print (default "lpr", POSIX only)
 *          -charset name
 *          -charset @file
 *          -charset =spec
 *		use the specified character set
 *          -crlf
 *		expand newlines to CR/LF (POSIX only)
 *          -nocrlf
 *		expand newlines to CR/LF (Windows only)
 *          -blanklines
 *		display blank lines even if they're empty (formatted LU3)
 *          -eojtimeout n
 *              time out end of job after n seconds
 *          -ffthru
 *		pass through SCS FF orders
 *          -ffskip
 *		skip FF at top of page
 *	    -printer "printer name"
 *	        printer to use (default is $PRINTER or system default,
 *	        Windows only)
 *          -reconnect
 *		keep trying to reconnect
 *	    -trace
 *		trace data stream to a file
 *          -tracedir dir
 *              directory to write trace file in
 *          -v
 *		verbose output about negotiation
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#if !defined(_WIN32) /*[*/
#include <syslog.h>
#include <netdb.h>
#endif /*]*/
#include <sys/types.h>
#include <unistd.h>
#if !defined(_WIN32) /*[*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else /*][*/
#include <winsock2.h>
#include <ws2tcpip.h>
#undef AF_INET6
#endif /*]*/
#include <time.h>
#include <signal.h>

#include "globals.h"
#include "trace_dsc.h"
#include "ctlrc.h"
#include "telnetc.h"
#if defined(_WIN32) /*[*/
#include "wsc.h"
#endif /*]*/

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
#if defined(_WIN32) /*[*/
extern void sockstart(void);
#endif /*]*/
extern void sockerr(char *fmt, ...);

/* Globals. */
char *programname = NULL;	/* program name */
int blanklines = 0;
int ignoreeoj = 0;
int reconnect = 0;
#if defined(_WIN32) /*[*/
int crlf = 1;
#else /*][*/
int crlf = 0;
#endif /*]*/
int ffthru = 0;
int ffskip = 0;
int verbose = 0;
int ssl_host = 0;
unsigned long eoj_timeout = 0L; /* end of job timeout */

/* User options. */
#if !defined(_WIN32) /*[*/
static enum { NOT_DAEMON, WILL_DAEMON, AM_DAEMON } bdaemon = NOT_DAEMON;
#endif /*]*/
static char *assoc = NULL;	/* TN3270 session to associate with */
#if !defined(_WIN32) /*[*/
const char *command = "lpr";	/* command to run for printing */
#else /*][*/
const char *printer = NULL;	/* printer to use */
#endif /*]*/
static int tracing = 0;		/* are we tracing? */
static char *tracedir = "/tmp";	/* where we are tracing */

void pr3287_exit(int);

/* Print a usage message and exit. */
static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [options] [lu[,lu...]@]host[:port]\n"
"Options:\n%s\n%s\n%s\n", programname,
#if !defined(_WIN32) /*[*/
"  -daemon          become a daemon after connecting\n"
#endif /*]*/
"  -assoc <session> associate with a session (TN3270E only)\n"
"  -charset <name>  use built-in alternate EBCDIC-to-ASCII mappings\n"
"  -charset @<file> use alternate EBCDIC-to-ASCII mappings from file\n"
"  -charset =\"<ebc>=<asc> ...\"\n"
"                   specify alternate EBCDIC-to-ASCII mappings",
#if !defined(_WIN32) /*[*/
"  -command \"<cmd>\" use <cmd> for printing (default \"lpr\")\n"
#endif /*]*/
"  -blanklines      display blank lines even if empty (formatted LU3)\n"
#if defined(_WIN32) /*[*/
"  -nocrlf          don't expand newlines to CR/LF\n"
#else /*][*/
"  -crlf            expand newlines to CR/LF\n"
#endif /*]*/
"  -eojtimeout <seconds>\n"
"                   time out end of print job\n"
"  -ffthru          pass through SCS FF orders\n"
"  -ffskip          skip FF orders at top of page",
"  -ignoreeoj       ignore PRINT-EOJ commands\n"
#if defined(_WIN32) /*[*/
"  -printer \"printer name\"\n"
"                   use specific printer (default is $PRINTER or the system\n"
"                   default printer)\n"
#endif /*]*/
"  -reconnect       keep trying to reconnect\n"
"  -trace           trace data stream to /tmp/x3trc.<pid>\n"
"  -tracedir <dir>  directory to keep trace information in");
	pr3287_exit(1);
}

/* Print an error message. */
void
verrmsg(const char *fmt, va_list ap)
{
	static char buf[2][4096] = { "", "" };
	static int ix = 0;

	ix = !ix;
	(void) vsprintf(buf[ix], fmt, ap);
	trace_ds("Error: %s\n", buf[ix]);
	if (!strcmp(buf[ix], buf[!ix])) {
		if (verbose)
			(void) fprintf(stderr, "Suppressed error '%s'\n",
			    buf[ix]);
		return;
	}
#if !defined(_WIN32) /*[*/
	if (bdaemon == AM_DAEMON) {
		/* XXX: Need to put somethig in the Application Event Log. */
		syslog(LOG_ERR, "%s: %s", programname, buf[ix]);
	} else {
#endif /*]*/
		(void) fprintf(stderr, "%s: %s\n", programname, buf[ix]);
#if !defined(_WIN32) /*[*/
	}
#endif /*]*/
}

void
errmsg(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) verrmsg(fmt, args);
	va_end(args);
}

/* Memory allocation. */
void *
Malloc(size_t len)
{
	void *p = malloc(len);

	if (p == NULL) {
		errmsg("Out of memory");
		pr3287_exit(1);
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
		pr3287_exit(1);
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

#if !defined(_WIN32) /*[*/
/* Signal handler for SIGUSR1. */
static void
flush_signal(int sig)
{
	/* Flush any pending data and exit. */
	trace_ds("Flush signal %d\n", sig);
	(void) print_eoj();
}
#endif /*]*/

void
pr3287_exit(int status)
{
#if defined(_WIN32) && defined(NEED_PAUSE) /*[*/
	char buf[2];

	if (status) {
		printf("\n[Press <Enter>] ");
		fflush(stdout);
		(void) fgets(buf, 2, stdin);
	}
#endif /*]*/
	exit(status);
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
#if !defined(AF_INET6) /*[*/
	struct hostent *he;
	struct servent *se;
#endif /*]*/
	unsigned short p;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if defined(AF_INET6) /*[*/
		struct sockaddr_in6 sin6;
#endif /*]*/
	} ha;
	socklen_t ha_len = sizeof(ha);
	int s = -1;
	int rc = 0;
	int report_success = 0;
#if defined(HAVE_LIBSSL) /*[*/
	int any_prefixes = False;
#endif /*]*/

	/* Learn our name. */
#if defined(_WIN32) /*[*/
	if ((programname = strrchr(argv[0], '\\')) != NULL)
#else /*][*/
	if ((programname = strrchr(argv[0], '/')) != NULL)
#endif /*]*/
		programname++;
	else
		programname = argv[0];
#if !defined(_WIN32) /*[*/
	if (!programname[0])
		programname = "wpr3287";
#endif /*]*/

#if defined(_WIN32) /*[*/
	/*
	 * Get the printer name via the environment, because Windows doesn't
	 * let us put spaces in arguments.
	 */
	if ((printer = getenv("PRINTER")) == NULL) {
		printer = ws_default_printer();
	}
#endif /*]*/

	/* Gather the options. */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
#if !defined(_WIN32) /*[*/
		if (!strcmp(argv[i], "-daemon"))
			bdaemon = WILL_DAEMON;
		else
#endif /*]*/
		if (!strcmp(argv[i], "-assoc")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -assoc\n");
				usage();
			}
			assoc = argv[i + 1];
			i++;
#if !defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-command")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -command\n");
				usage();
			}
			command = argv[i + 1];
			i++;
#endif /*]*/
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
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-nocrlf")) {
			crlf = 0;
#else /*][*/
		} else if (!strcmp(argv[i], "-crlf")) {
			crlf = 1;
#endif /*]*/
		} else if (!strcmp(argv[i], "-eojtimeout")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -charset\n");
				usage();
			}
			eoj_timeout = strtoul(argv[i + 1], NULL, 0);
			i++;
		} else if (!strcmp(argv[i], "-ignoreeoj")) {
			ignoreeoj = 1;
		} else if (!strcmp(argv[i], "-ffthru")) {
			ffthru = 1;
		} else if (!strcmp(argv[i], "-ffskip")) {
			ffskip = 1;
#if defined(_WIN32) /*[*/
		} else if (!strcmp(argv[i], "-printer")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -printer\n");
				usage();
			}
			printer = argv[i + 1];
			i++;
#endif /*]*/
		} else if (!strcmp(argv[i], "-reconnect")) {
			reconnect = 1;
		} else if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-trace")) {
			tracing = 1;
		} else if (!strcmp(argv[i], "-tracedir")) {
			if (argc <= i + 1 || !argv[i + 1][0]) {
				(void) fprintf(stderr,
				    "Missing value for -tracedir\n");
				usage();
			}
			tracedir = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help")) {
			usage();
		} else
			usage();
	}
	if (argc != i + 1)
		usage();

	/* Pick apart the hostname, LUs and port. */
#if defined(_WIN32) /*[*/
	sockstart();
#endif /*]*/
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

	/*
	 * Allow the hostname to be enclosed in '[' and ']' to quote any
	 * IPv6 numeric-address ':' characters.
	 */
	if (host[0] == '[') {
		char *tmp;
		char *rbracket;

		rbracket = strchr(host+1, ']');
		if (rbracket != NULL) {
			len = rbracket - (host+1);
			tmp = Malloc(len + 1);
			(void) strncpy(tmp, host+1, len);
			tmp[len] = '\0';
			host = tmp;
		}
		if (*(rbracket + 1) == ':') {
			port = rbracket + 2;
		}
	} else {
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
	}

#if !defined(AF_INET6) /*[*/
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
			pr3287_exit(1);
		}
		if (l & ~0xffff) {
			(void) fprintf(stderr, "Invalid port: %s\n", port);
			pr3287_exit(1);
		}
		p = htons((unsigned short)l);
	}
#endif /* AF_INET6 */

	/* Remap the character set. */
	if (charset_init(charset) < 0)
		pr3287_exit(1);

	/* Try opening the trace file, if there is one. */
	if (tracing) {
		char tracefile[256];
		time_t clk;
		int i;

#if defined(_WIN32) /*[*/
		(void) sprintf(tracefile, "x3trc.%d.txt", getpid());
#else /*][*/
		(void) sprintf(tracefile, "%s/x3trc.%d", tracedir, getpid());
#endif /*]*/
		tracef = fopen(tracefile, "a");
		if (tracef == NULL) {
			perror(tracefile);
			pr3287_exit(1);
		}
		(void) SETLINEBUF(tracef);
		clk = time((time_t *)0);
		(void) fprintf(tracef, "Trace started %s", ctime(&clk));
		(void) fprintf(tracef, " Version %s\n", build);
		(void) fprintf(tracef, " Options:");
		for (i = 0; i < argc; i++) {
			(void) fprintf(tracef, " %s", argv[i]);
		}
		(void) fprintf(tracef, "\n");
	}

#if !defined(_WIN32) /*[*/
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
#endif /*]*/

	/* Handle signals. */
	(void) signal(SIGTERM, fatal_signal);
	(void) signal(SIGINT, fatal_signal);
#if !defined(_WIN32) /*[*/
	(void) signal(SIGHUP, fatal_signal);
	(void) signal(SIGUSR1, flush_signal);
	(void) signal(SIGPIPE, SIG_IGN);
#endif /*]*/

	/*
	 * One-time initialization is now complete.
	 * (Most) everything beyond this will now be retried, if the -reconnect
	 * option is in effect.
	 */
	for (;;) {
		/* Resolve the host name. */
#if defined(AF_INET6) /*[*/
		struct addrinfo hints, *res;
		int rc;

		(void) memset(&hints, '\0', sizeof(struct addrinfo));
		hints.ai_flags = 0;
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		rc = getaddrinfo(host, port, &hints, &res);
		if (rc != 0) {
			errmsg("%s/%s: %s", host, port, gai_strerror(rc));
			rc = 1;
			goto retry;
		}
		switch (res->ai_family) {
		case AF_INET:
			p = ((struct sockaddr_in *)res->ai_addr)->sin_port;
			break;
		case AF_INET6:
			p = ((struct sockaddr_in6 *)res->ai_addr)->sin6_port;
			break;
		default:
			errmsg("%s: unknown family %d", host, res->ai_family);
			rc = 1;
			freeaddrinfo(res);
			goto retry;
		}
		(void) memcpy(&ha.sa, res->ai_addr, res->ai_addrlen);
		ha_len = res->ai_addrlen;
		freeaddrinfo(res);
#else /*][*/
		(void) memset(&ha, '\0', sizeof(ha));
		ha.sin.sin_family = AF_INET;
		ha.sin.sin_port = p;
		he = gethostbyname(host);
		if (he != NULL) {
			(void) memcpy(&ha.sin.sin_addr, he->h_addr_list[0],
				      he->h_length);
		} else {
			ha.sin.sin_addr.s_addr = inet_addr(host);
			if (ha.sin.sin_addr.s_addr == INADDR_NONE) {
				errmsg("Unknown host: %s", host);
				rc = 1;
				goto retry;
			}
		}
#endif /*]*/

		/* Connect to the host. */
		s = socket(ha.sa.sa_family, SOCK_STREAM, 0);
		if (s < 0) {
			sockerr("socket");
			pr3287_exit(1);
		}
		if (connect(s, &ha.sa, ha_len) < 0) {
			sockerr("%s", host);
			rc = 1;
			goto retry;
		}

		/* Say hello. */
		if (verbose) {
			(void) fprintf(stderr, "Connected to %s, port %u%s\n",
			    host, ntohs(p),
			    ssl_host? " via SSL": "");
			if (assoc != NULL)
				(void) fprintf(stderr, "Associating with LU "
				    "%s\n", assoc);
			else if (lu != NULL)
				(void) fprintf(stderr, "Connecting to LU %s\n",
				    lu);
#if !defined(_WIN32) /*[*/
			(void) fprintf(stderr, "Command: %s\n", command);
#else /*][*/
			(void) fprintf(stderr, "Printer: %s\n",
				       printer? printer: "(none)");
#endif /*]*/
		}

		/* Negotiate. */
		if (negotiate(s, lu, assoc) < 0) {
			rc = 1;
			goto retry;
		}

		/* Report sudden success. */
		if (report_success) {
			errmsg("Connected to %s, port %u", host, ntohs(p));
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
#if !defined(_WIN32) /*[*/
			sleep(5);
#else /*][*/
			Sleep(5 * 1000000);
#endif /*]*/

		rc = 0;
	}

	pr3287_exit(rc);

	return rc;
}

/* Tracing function. */
void
trace_str(const char *s)
{
	if (tracef)
		fprintf(tracef, "%s", s);
}

/* Error pop-ups. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) verrmsg(fmt, args);
	va_end(args);
}

void
popup_an_errno(int err, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (err > 0) {
		char msgbuf[4096];

		(void) vsprintf(msgbuf, fmt, args);
		errmsg("%s: %s", msgbuf, strerror(err));

	} else {
		(void) verrmsg(fmt, args);
	}
	va_end(args);
}
