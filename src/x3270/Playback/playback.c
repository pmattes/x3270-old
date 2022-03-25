/*
 * Copyright 1994 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * playback file facility for x3270
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <sys/types.h>
#if !defined(sco) /*[*/
#include <sys/time.h>
#endif /*]*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#if defined(_IBMR2) || defined(_SEQUENT_) /*[*/
#include <sys/select.h>
#endif /*]*/

#define PORT		4001
#define BSIZE		16384
#define LINEDUMP_MAX	32

int port = PORT;
char *me;
static enum {
	NONE, WRONG, BASE,
	LESS, SPACE, ZERO, X, N, SPACE2, D1, D2
} pstate = NONE;
static enum {
	T_NONE, T_IAC
} tstate = T_NONE;
int fdisp = 0;

extern int optind;
extern char *optarg;

void
usage()
{
	(void) fprintf(stderr, "usage: %s [-p port] file\n", me);
	exit(1);
}

main(argc, argv)
int argc;
char *argv[];
{
	int c;
	FILE *f;
	int s;
	struct sockaddr_in sin;
	int one = 1;
	int len;

	/* Parse command-line arguments */

	if (me = strrchr(argv[0], '/'))
		me++;
	else
		me = argv[0];

	while ((c = getopt(argc, argv, "p:")) != -1)
		switch (c) {
		    case 'p':
			port = atoi(optarg);
			break;
		    default:
			usage();
		}

	if (argc - optind != 1)
		usage();

	/* Open the file. */
	f = fopen(argv[optind], "r");
	if (f == (FILE *)NULL) {
		perror(argv[optind]);
		exit(1);
	}

	/* Listen on a socket. */
	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
	    sizeof(one)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	(void) memset((char *)&sin, '\0', sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(s, 1) < 0) {
		perror("listen");
		exit(1);
	}
	(void) signal(SIGPIPE, SIG_IGN);

	/* Accept connections and process them. */

	for (;;) {
		int s2;

		(void) memset((char *)&sin, '\0', sizeof(sin));
		sin.sin_family = AF_INET;
		len = sizeof(sin);
		(void) printf("Waiting for connection.\n");
		s2 = accept(s, (struct sockaddr *)&sin, &len);
		if (s2 < 0) {
			perror("accept");
			continue;
		}
		(void) printf("Connection from %s.%d.\n",
		    inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		rewind(f);
		pstate = BASE;
		fdisp = 0;
		process(f, s2);
	}
}

void
trace_netdata(direction, buf, len)
char *direction;
unsigned char *buf;
int len;
{
	int offset;

	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			(void) printf("%s%s 0x%-3x ",
			    (offset ? "\n" : ""), direction, offset);
		(void) printf("%02x", buf[offset]);
	}
	(void) printf("\n");
}

process(f, s)
FILE *f;
int s;
{
	char buf[BSIZE];
	int prompt = 1;

	/* Loop, looking for keyboard input or emulator response. */
	for (;;) {
		fd_set rfds;
		struct timeval t;
		int ns;

		if (prompt == 1) {
			(void) printf("playback> ");
			(void) fflush(stdout);
		}

		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		FD_SET(0, &rfds);
		t.tv_sec = 0;
		t.tv_usec = 500000;
		ns = select(s+1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &t);
		if (ns < 0) {
			perror("select");
			exit(1);
		}
		if (ns == 0) {
			prompt++;
			continue;
		}
		if (FD_ISSET(s, &rfds)) {
			int nr;

			(void) printf("\n");
			nr = read(s, buf, BSIZE);
			if (nr < 0) {
				perror("read");
				break;
			}
			if (nr == 0) {
				(void) printf("Emulator disconnected.\n");
				break;
			}
			trace_netdata("emul", (unsigned char *)buf, nr);
			prompt = 0;
		}
		if (FD_ISSET(0, &rfds)) {
			if (fgets(buf, BSIZE, stdin) == (char *)NULL) {
				(void) printf("\n");
				exit(0);
			}
			if (!strncmp(buf, "s", 1)) {		/* step line */
				if (!step(f, s, 0))
					break;
			} else if (!strncmp(buf, "r", 1)) {	/* step record */
				if (!step(f, s, 1))
					break;
			} else if (!strncmp(buf, "q", 1)) {	/* quit */
				exit(0);
			} else if (!strncmp(buf, "d", 1)) {	/* disconnect */
				break;
			} else if (buf[0] != '\n') {		/* junk */
				(void) printf("%c?\n", buf[0]);
			}
			prompt = 0;
		}
	}

	(void) close(s);
	pstate = NONE;
	tstate = T_NONE;
	fdisp = 0;
	return;
}

int
step(f, s, to_eor)
FILE *f;
int s;
int to_eor;
{
	int c = 0;
	static int d1;
	static char hexes[] = "0123456789abcdef";
#	define isxd(c) strchr(hexes, c)
	static int again = 0;
	char obuf[BSIZE];
	char *cp = obuf;
#	define NO_FDISP { if (fdisp) { printf("\n"); fdisp = 0; } }

    top:
	while (again || ((c = fgetc(f)) != EOF)) {
		if (!again) {
			if (!fdisp || c == '\n') {
				printf("\nfile ");
				fdisp = 1;
			}
			if (c != '\n')
				putchar(c);
		}
		again = 0;
		switch (pstate) {
		    case WRONG:
			if (c == '\n')
				pstate = BASE;
			break;
		    case BASE:
			if (c == '<')
				pstate = LESS;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case LESS:
			if (c == ' ')
				pstate = SPACE;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE:
			if (c == '0')
				pstate = ZERO;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case ZERO:
			if (c == 'x')
				pstate = X;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case X:
			if (isxd(c))
				pstate = N;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case N:
			if (isxd(c))
				pstate = N;
			else if (c == ' ' || c == '\t')
				pstate = SPACE2;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case SPACE2:
			if (isxd(c)) {
				d1 = strchr(hexes, c) - hexes;
				pstate = D1;
				cp = obuf;
			} else if (c == ' ' || c == '\t')
				pstate = SPACE2;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case D1:
			if (isxd(c)) {
				int at_eor = 0;

				*cp = ((d1*16)+(strchr(hexes,c)-hexes));
				pstate = D2;
				switch (tstate) {
				    case T_NONE:
					if (*(unsigned char *)cp == IAC)
					    tstate = T_IAC;
					break;
				    case T_IAC:
					if (*(unsigned char *)cp == EOR &&
					    to_eor)
						at_eor = 1;
					tstate = T_NONE;
					break;
				}
				cp++;
				if (at_eor || (cp - obuf >= BUFSIZ))
					goto run_it;
			} else {
				NO_FDISP;
				(void) printf("Non-hex digit in playback file");
				pstate = WRONG;
				again = 1;
			}
			break;
		    case D2:
			if (isxd(c)) {
				d1 = strchr(hexes, c) - hexes;
				pstate = D1;
			} else if (c == '\n') {
				pstate = BASE;
				goto run_it;
			} else {
				NO_FDISP;
				(void) printf("Non-hex digit in playback file");
				pstate = WRONG;
				again = 1;
			}
			break;
		}
	}
	goto done;

    run_it:
	NO_FDISP;
	trace_netdata("host", (unsigned char *)obuf, cp - obuf);
	if (write(s, obuf, cp - obuf) < 0) {
		perror("socket write");
		return 0;
	}

	if (to_eor &&
	    (cp - obuf < 2 ||
	    (unsigned char)obuf[cp - obuf - 2] != IAC ||
	    (unsigned char)obuf[cp - obuf - 1] != EOR)) {
		cp = obuf;
		goto top;
	}
	return 1;

    done:
	if (c == EOF) {
		NO_FDISP;
		(void) printf("Playback file EOF.\n");
	}

	return 0;
}
