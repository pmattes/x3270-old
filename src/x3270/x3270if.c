/*
 * Copyright 1995, 1996, 1999, 2000, 2001, 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270, c3270, and s3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 * Script interface utility for x3270, c3270 and s3270.
 *
 * Accesses an x3270 command stream on the file descriptors defined by the
 * environment variables X3270OUTPUT (output from x3270, input to script) and
 * X3270INPUT (input to x3270, output from script).
 */

#include "conf.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_SYS_SELECT_H) /*[*/
#include <sys/select.h>
#endif /*]*/
#if defined(HAVE_GETOPT_H) /*[*/
#include <getopt.h>
#endif /*]*/

#define IBS	1024

#define NO_STATUS	(-1)
#define ALL_FIELDS	(-2)

extern int optind;
extern char *optarg;

static char *me;
static int verbose = 0;
static char buf[IBS];

static void iterative_io(void);
static void single_io(int fn, char *cmd);

static void
usage(void)
{
	(void) fprintf(stderr, "\
usage: %s [-v] [-S] [-s field] [action[(param[,...])]]\n\
       %s -i\n", me, me);
	exit(2);
}

/* Get a file descriptor from the environment. */
static int
fd_env(const char *name)
{
	char *fdname;
	int fd;

	fdname = getenv(name);
	if (fdname == (char *)NULL) {
		(void) fprintf(stderr, "%s: %s not set in the environment\n",
				me, name);
		exit(2);
	}
	fd = atoi(fdname);
	if (fd <= 0) {
		(void) fprintf(stderr, "%s: invalid value '%s' for %s\n", me,
		    fdname, name);
		exit(2);
	}
	return fd;
}

int
main(int argc, char *argv[])
{
	int c;
	int fn = NO_STATUS;
	char *ptr;
	int iterative = 0;

	/* Identify yourself. */
	if ((me = strrchr(argv[0], '/')) != (char *)NULL)
		me++;
	else
		me = argv[0];

	/* Parse options. */
	while ((c = getopt(argc, argv, "is:Sv")) != -1) {
		switch (c) {
		    case 'i':
			if (fn >= 0)
				usage();
			iterative++;
			break;
		    case 's':
			if (fn >= 0 || iterative)
				usage();
			fn = (int)strtol(optarg, &ptr, 0);
			if (ptr == optarg || *ptr != '\0' || fn < 0) {
				(void) fprintf(stderr,
				    "%s: Invalid field number: '%s'\n", me,
				    optarg);
				usage();
			}
			break;
		    case 'S':
			if (fn >= 0 || iterative)
				usage();
			fn = ALL_FIELDS;
			break;
		    case 'v':
			verbose++;
			break;
		    default:
			usage();
			break;
		}
	}

	/* Validate positional arguments. */
	if (optind == argc) {
		/* No positional arguments. */
		if (fn == NO_STATUS && !iterative)
			usage();
	} else {
		/* Got positional arguments. */
		if (iterative)
			usage();
	}

	/* Ignore broken pipes. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Do the I/O. */
	if (!iterative) {
		single_io(fn, argv[optind]);
	} else {
		iterative_io();
	}
	return 0;
}

/* Do a single command, and interpret the results. */
static void
single_io(int fn, char *cmd)
{
	FILE *inf = NULL, *outf = NULL;
	char status[IBS] = "";
	int xs = -1;

	/* Verify the environment and open files. */
	inf = fdopen(fd_env("X3270OUTPUT"), "r");
	if (inf == (FILE *)NULL) {
		perror("x3270if: input: fdopen($X3270OUTPUT)");
		exit(2);
	}
	outf = fdopen(fd_env("X3270INPUT"), "w");
	if (outf == (FILE *)NULL) {
		perror("x3270if: output: fdopen($X3270INPUT)");
		exit(2);
	}

	/* Speak to x3270. */
	if (fprintf(outf, "%s\n", (cmd != NULL)? cmd: "") < 0 ||
	    fflush(outf) < 0) {
		perror("x3270if: printf");
		exit(2);
	}
	if (verbose)
		(void) fprintf(stderr, "i+ out %s\n",
		    (cmd != NULL) ? cmd : "");

	/* Get the answer. */
	while (fgets(buf, IBS, inf) != (char *)NULL) {
		int sl = strlen(buf);

		if (sl > 0 && buf[sl-1] == '\n')
			buf[--sl] = '\0';
		if (verbose)
			(void) fprintf(stderr, "i+ in %s\n", buf);
		if (!strcmp(buf, "ok")) {
			(void) fflush(stdout);
			xs = 0;
			break;
		} else if (!strcmp(buf, "error")) {
			(void) fflush(stdout);
			xs = 1;
			break;
		} else if (!strncmp(buf, "data: ", 6)) {
			if (printf("%s\n", buf+6) < 0) {
				perror("x3270if: printf");
				exit(2);
			}
		} else
			(void) strcpy(status, buf);
	}

	/* If fgets() failed, so should we. */
	if (xs == -1) {
		if (feof(inf))
			(void) fprintf(stderr,
				    "x3270if: input: unexpected EOF\n");
		else
			perror("x3270if: input");
		exit(2);
	}

	if (fflush(stdout) < 0) {
		perror("x3270if: fflush");
		exit(2);
	}

	/* Print status, if that's what they want. */
	if (fn != NO_STATUS) {
		char *sf = (char *)NULL;
		char *sb = status;
		int rc;

		if (fn == ALL_FIELDS) {
			rc = printf("%s\n", status);
		} else {
			do {
				if (!fn--)
					break;
				sf = strtok(sb, " \t");
				sb = (char *)NULL;
			} while (sf != (char *)NULL);
			rc = printf("%s\n", (sf != (char *)NULL) ? sf : "");
		}
		if (rc < 0) {
			perror("x3270if: printf");
			exit(2);
		}
	}

	if (fflush(stdout) < 0) {
		perror("x3270if: fflush");
		exit(2);
	}

	exit(xs);
}

/* Act as a passive pipe between 'expect' and x3270. */
static void
iterative_io(void)
{
#	define N_IO 2
	struct {
		const char *name;
		int rfd, wfd;
		char buf[IBS];
		int offset, count;
	} io[N_IO];	/* [0] is program->x3270, [1] is x3270->program */
	fd_set rfds, wfds;
	int fd_max = 0;
	int i;

#ifdef DEBUG
	if (verbose) {
		freopen("/tmp/x3270if.dbg", "w", stderr);
		setlinebuf(stderr);
	}
#endif

	/* Get the x3270 file descriptors. */
	io[0].name = "program->x3270";
	io[0].rfd = fileno(stdin);
	io[0].wfd = fd_env("X3270INPUT");
	io[1].name = "x3270->program";
	io[1].rfd = fd_env("X3270OUTPUT");
	io[1].wfd = fileno(stdout);
	for (i = 0; i < N_IO; i++) {
		if (io[i].rfd > fd_max)
			fd_max = io[i].rfd;
		if (io[i].wfd > fd_max)
			fd_max = io[i].wfd;
		(void) fcntl(io[i].rfd, F_SETFL,
		    fcntl(io[i].rfd, F_GETFL, 0) | O_NDELAY);
		(void) fcntl(io[i].wfd, F_SETFL,
		    fcntl(io[i].wfd, F_GETFL, 0) | O_NDELAY);
		io[i].offset = 0;
		io[i].count = 0;
	}
	fd_max++;

	for (;;) {
		int rv;

		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for (i = 0; i < N_IO; i++) {
			if (io[i].count) {
				FD_SET(io[i].wfd, &wfds);
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "enabling output %s %d\n",
					    io[i].name, io[i].wfd);
#endif
			} else {
				FD_SET(io[i].rfd, &rfds);
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "enabling input %s %d\n",
					    io[i].name, io[i].rfd);
#endif
			}
		}

		if ((rv = select(fd_max, &rfds, &wfds, (fd_set *)NULL,
				(struct timeval *)NULL)) < 0) {
			perror("x3270if: select");
			exit(2);
		}
		if (verbose)
			(void) fprintf(stderr, "select->%d\n", rv);

		for (i = 0; i < N_IO; i++) {
			if (io[i].count) {
				if (FD_ISSET(io[i].wfd, &wfds)) {
					rv = write(io[i].wfd,
					    io[i].buf + io[i].offset,
					    io[i].count);
					if (rv < 0) {
						(void) fprintf(stderr,
						    "x3270if: write(%s): %s",
						    io[i].name,
						    strerror(errno));
						exit(2);
					}
					io[i].offset += rv;
					io[i].count -= rv;
#ifdef DEBUG
					if (verbose)
						(void) fprintf(stderr,
						    "write(%s)->%d\n",
						    io[i].name, rv);
#endif
				}
			} else if (FD_ISSET(io[i].rfd, &rfds)) {
				rv = read(io[i].rfd, io[i].buf, IBS);
				if (rv < 0) {
					(void) fprintf(stderr,
					    "x3270if: read(%s): %s",
					    io[i].name, strerror(errno));
					exit(2);
				}
				if (rv == 0)
					exit(0);
				io[i].offset = 0;
				io[i].count = rv;
#ifdef DEBUG
				if (verbose)
					(void) fprintf(stderr,
					    "read(%s)->%d\n", io[i].name, rv);
#endif
			}
		}
	}
}
