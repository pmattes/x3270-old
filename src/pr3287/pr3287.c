/*
 * Copyright 2000 by Paul Mattes.
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
 *	    -trace
 *		trace data stream to a file
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

#if defined(_IOLBF) /*[*/
#define SETLINEBUF(s)	setvbuf(s, (char *)NULL, _IOLBF, BUFSIZ)
#else /*][*/
#define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

/* External functions. */
extern int negotiate(int s, const char *lu, const char *assoc);
extern void process(int s, char *command);
extern char *build;

/* Globals. */
char *programname = NULL;	/* program name */
FILE *tracef = NULL;		/* trace file */

/* User options. */
static enum { NOT_DAEMON, WILL_DAEMON, AM_DAEMON } bdaemon = NOT_DAEMON;
static char *assoc = NULL;	/* TN3270 session to associate with */
char *command = "lpr";		/* command to run for printing */
static int tracing = 0;		/* are we tracing? */

/* Print a usage message and exit. */
static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [options] [lu[,lu...]@]host[:port]\n"
"Options:\n"
"  -daemon          become a daemon after connecting\n"
"  -assoc <session> associate with a session (TN3270E only)\n"
"  -command \"<cmd>\" use <cmd> for printing (default \"lpr\")\n"
"  -trace           trace data stream to /tmp/x3trc.<pid>\n",
		programname);
	exit(1);
}

/* Print an error message. */
void
errmsg(const char *fmt, ...)
{
	va_list args;
	char buf[4096];

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	if (bdaemon == AM_DAEMON)
		syslog(LOG_ERR, "%s: %s", programname, buf);
	else
		(void) fprintf(stderr, "%s: %s\n", programname, buf);
	va_end(args);
}

/* Memory allocation. */
void *
Malloc(unsigned len)
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
Realloc(void *p, unsigned len)
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
	print_eoj();
	errmsg("Exiting on signal %d", sig);
	exit(0);
}

int
main(int argc, char *argv[])
{
	int i;
	char *at, *colon;
	int len;
	char *lu = NULL;
	char *host = NULL;
	char *port = "telnet";
	struct hostent *he;
	struct in_addr ha;
	struct servent *se;
	u_short p;
	struct sockaddr_in sin;
	int s;

	/* Learn our name. */
	if ((programname = strrchr(argv[0], '/')) != NULL)
		programname++;
	else
		programname = argv[0];

	/* Gather the options. */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "-daemon"))
			bdaemon = 1;
		else if (!strcmp(argv[i], "-assoc")) {
			if (argc <= i + 1)
				usage();
			assoc = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "-command")) {
			if (argc <= i + 1)
				usage();
			command = argv[i + 1];
			i++;
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

	/* Resolve the host name. */
	he = gethostbyname(host);
	if (he != NULL) {
		(void) memcpy(&ha, he->h_addr_list[0], he->h_length);
	} else {
		if (!inet_aton(host, &ha)) {
			(void) fprintf(stderr, "Unknown host: %s\n", host);
			exit(1);
		}
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
		p = htons((u_short)l);
	}

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

	/* Connect to the host. */
	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		exit(1);
	}
	sin.sin_family = AF_INET;
	sin.sin_addr = ha;
	sin.sin_port = p;
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror(host);
		exit(1);
	}

	/* Negotiate. */
	if (negotiate(s, lu, assoc) < 0)
		exit(1);

	/* Become a daemon. */
	if (bdaemon) {
		switch (fork()) {
			case -1:
				perror("fork");
				exit(1);
				break;
			case 0:
				/* Child: Break away from the TTY. */
				if (setsid() < 0)
					exit(1);
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
	(void) signal(SIGPIPE, SIG_IGN);

	/* Process what we're told to process. */
	process(s, command);

	/* Flush any pending data and exit. */
	print_eoj();
	return 0;
}

/* Tracing function. */
void
trace_str(const char *s)
{
	if (tracef)
		fprintf(tracef, "%s", s);
}
