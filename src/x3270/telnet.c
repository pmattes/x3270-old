/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	telnet.c
 *		This module initializes and manages a telnet socket to
 *		the given IBM host.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#define TELCMDS 1
#define TELOPTS 1
#if !defined(hpux) /*[*/
#include <arpa/telnet.h>
#else /*][*/
#include "telnet.h"
#endif /*]*/
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include "globals.h"

#define BUFSZ		4096
#define TRACELINE	72

/* Globals */
int		n_read = 0;			/* number of characters read */
time_t		ns_time;
int		ns_brcvd;
int		ns_rrcvd;
int		ns_bsent;
int		ns_rsent;
unsigned char	obuf[4*BUFSZ], *obptr;		/* 3270 output buffer */
int		linemode = 1;
char		ttype_val[] = "IBM-3278-4";
char		*ttype_model = &ttype_val[9];

/* Externals */
extern unsigned long	inet_addr();
extern FILE		*tracef;

/* Statics */
static int	sock = -1;			/* active socket */
static FILE	*playbackf = (FILE *)0;		/* playback file */
static enum {
	NONE, WRONG, BASE,
	LESS, SPACE, ZERO, X, N, SPACE2, D1, D2
} pstate = NONE;
static char	*hostname = (char *) NULL;
static unsigned char	myopts[256], hisopts[256];	/* telnet option flags */
static unsigned char	ibuf[4*BUFSZ], *ibptr;		/* 3270 input buffer */
static unsigned char	netrbuf[BUFSZ];			/* network input buffer */
static unsigned char	sbbuf[1024], *sbptr;		/* telnet sub-option buffer */
static unsigned char	telnet_state;
static char	trace_msg[256];
static int	ansi_data = 0;
static int	syncing;
static int	lnext = 0;
static int	backslashed = 0;
static int	t_valid = 0;

static char	vintr;
static char	vquit;
static char	verase;
static char	vkill;
static char	veof;
static char	vwerase;
static char	vrprnt;
static char	vlnext;

static char	*nnn();				/* expand a number */
static char	*ctl_see();			/* expand a character */
static int	telnet_fsm();
static void	net_rawout();
static void	do_data();
static void	do_intr();
static void	do_quit();
static void	do_cerase();
static void	do_werase();
static void	do_kill();
static void	do_rprnt();
static void	do_eof();
static void	do_eol();
static void	do_lnext();
static void	check_in3270();
static void	check_linemode();
static void	trace_netdata();
static char	parse_ctlchar();
static void	net_interrupt();
static int	non_blocking();
static void	net_connected();
static void	trace_str();

/* telnet states */
#define TNS_DATA	0	/* receiving data */
#define TNS_IAC		1	/* got an IAC */
#define TNS_WILL	2	/* got an IAC WILL */
#define TNS_WONT	3	/* got an IAC WONT */
#define TNS_DO		4	/* got an IAC DO */
#define TNS_DONT	5	/* got an IAC DONT */
#define TNS_SB		6	/* got an IAC SB */
#define TNS_SB_IAC	7	/* got an IAC after an IAC SB */

/* telnet predefined messages */
static unsigned char	do_opt[]	= { 
	IAC, DO, '_' };
static unsigned char	dont_opt[]	= { 
	IAC, DONT, '_' };
static unsigned char	will_opt[]	= { 
	IAC, WILL, '_' };
static unsigned char	wont_opt[]	= { 
	IAC, WONT, '_' };
static unsigned char	ttype_opt[]	= {
	IAC, SB, TELOPT_TTYPE, TELQUAL_IS } ;
static unsigned char	ttype_end[]	= {
	IAC, SE };

#if defined(TELCMD) && defined(TELCMD_OK)
#define cmd(c)	(TELCMD_OK(c) ? TELCMD(c) : nnn((int)c))
#else
#define cmd(c)	(((c) >= SE && (c) <= IAC) ? telcmds[(c) - SE] : nnn((int)c))
#endif

#if defined(TELOPT) && defined(TELOPT_OK)
#define opt(c)	(TELOPT_OK(c) ? TELOPT(c) : nnn((int)c))
#else
#define opt(c)	(((c) <= TELOPT_EOR) ? telopts[c] : nnn((int)c))
#endif

char *telquals[2] = { "IS", "SEND" };


/*
 * net_connect
 *	Establish a telnet socket to the given host passed as an argument.
 *	Called only once and is responsible for setting up the telnet
 *	variables.  Returns the file descriptor of the connected socket.
 */
int
net_connect(host, portname, pending)
char	*host;
char	*portname;
Boolean	*pending;
{
	struct servent	*sp;
	struct hostent	*hp;
	unsigned short		port;
	struct sockaddr_in	sin;
	int			on = 1;
#if defined(OMTU) /*[*/
	int			mtu = OMTU;
#endif /*]*/

#	define close_fail	{ (void) close(sock); sock = -1; return -1; }

	if (!t_valid) {
		vintr   = parse_ctlchar(appres.intr);
		vquit   = parse_ctlchar(appres.quit);
		verase  = parse_ctlchar(appres.erase);
		vkill   = parse_ctlchar(appres.kill);
		veof    = parse_ctlchar(appres.eof);
		vwerase = parse_ctlchar(appres.werase);
		vrprnt  = parse_ctlchar(appres.rprnt);
		vlnext  = parse_ctlchar(appres.lnext);
		t_valid = 1;
	}

	*pending = False;
	n_read = 0;

	if (hostname)
		XtFree(hostname);
	hostname = XtNewString(host);

	/* get the tcp/ip service (telnet) */
	port = (unsigned short) atoi(portname);
	if (port)
		port = htons(port);
	else {
		if (!(sp = getservbyname(portname, "tcp"))) {
			popup_an_error("Unknown service");
			return -1;
		}
		port = sp->s_port;
	}


	/* fill in the socket address of the given host */
	(void) memset((char *) &sin, 0, sizeof(sin));
	if ((hp = gethostbyname(host)) == (struct hostent *) 0) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(host);
		if (sin.sin_addr.s_addr == -1) {
			char *msg = xs_buffer("Unknown host:\n%s", hostname);

			popup_an_error(msg);
			XtFree(msg);
			return -1;
		}
	}
	else {
		sin.sin_family = hp->h_addrtype;
		(void) MEMORY_MOVE((char *) &sin.sin_addr,
			           (char *) hp->h_addr,
				   hp->h_length);
	}
	sin.sin_port = port;

	/* create the socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		popup_an_errno("socket", errno);
		return -1;
	}

	/* set options for inline out-of-band data and keepalives */
	if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0) {
		popup_an_errno("setsockopt(SO_OOBINLINE)", errno);
		close_fail;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0) {
		popup_an_errno("setsockopt(SO_KEEPALIVE)", errno);
		close_fail;
	}
#if defined(OMTU) /*[*/
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&mtu, sizeof(mtu)))
	{
		popup_an_errno("setsockopt(SO_SNDBUF)", errno);
		close_fail;
	}
#endif /*]*/

	/* set the socket to be non-delaying */
	if (non_blocking(True) < 0)
		close_fail;

	/* connect */
	if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		if (errno == EWOULDBLOCK
#if defined(EINPROGRESS) /*[*/
		 || errno == EINPROGRESS
#endif /*]*/
		                        ) {
			trace_str("Connection pending.\n");
			*pending = True;
		} else {
			char *msg = xs_buffer("Connect %s", hostname);

			popup_an_errno(msg, errno);
			XtFree(msg);
			close_fail;
		}
	} else {
		if (non_blocking(False) < 0)
			close_fail;
		net_connected();
	}

	/* all done */
	return sock;
}
#undef close_fail

static void
net_connected()
{
	(void) sprintf(trace_msg, "Connected to %s.\n", hostname);
	trace_str(trace_msg);

	/* set up telnet options */
	(void) memset((char *) myopts, 0, sizeof(myopts));
	(void) memset((char *) hisopts, 0, sizeof(hisopts));
	telnet_state = TNS_DATA;
	ibptr = &ibuf[0];
	sbptr = &sbbuf[0];

	/* clear statistics and flags */
	(void) time(&ns_time);
	ns_brcvd = 0;
	ns_rrcvd = 0;
	ns_bsent = 0;
	ns_rsent = 0;
	syncing = 0;

	check_linemode(True);
}

/*
 * net_disconnect
 *	Shut down the socket.
 */
void
net_disconnect()
{
	if (pstate != NONE) {
		(void) fclose(playbackf);
		XtFree(hostname);
		hostname = NULL;
		playbackf = (FILE *)0;
		pstate = NONE;
	} else {
		if (CONNECTED)
			(void) shutdown(sock, 2);
		(void) close(sock);
	}
	sock = -1;
	trace_str("SENT disconnect\n");
	return;
}


/*
 * net_input
 *	Called by the toolkit whenever there is input available on the
 *	socket.  Reads the data, processes the special telnet commands
 *	and calls process_ds to process the 3270 data stream.
 */
void
net_input()
{
	register unsigned char	*cp;
	int	nr;

	ansi_data = 0;

	nr = read(sock, (char *) netrbuf, BUFSZ);
	if (nr < 0) {
		if (errno == EWOULDBLOCK)
			return;
		if (HALF_CONNECTED && errno == EAGAIN) {
			if (non_blocking(False) < 0) {
				x_disconnect();
				return;
			}
			x_connected();
			net_connected();
			return;
		}
		(void) sprintf(trace_msg, "RCVD socket error %d\n", errno);
		trace_str(trace_msg);
		if (HALF_CONNECTED) {
			char *msg = xs_buffer("Connect %s", hostname);

			popup_an_errno(msg, errno);
			XtFree(msg);
		} else
			popup_an_errno("Socket read", errno);
		x_disconnect();
		return;
	} else if (nr == 0) {
		/* Host disconnected. */
		trace_str("RCVD disconnect\n");
		x_disconnect();
		return;
	}

	/* Process the data. */

	if (HALF_CONNECTED) {
		if (non_blocking(False) < 0) {
			x_disconnect();
			return;
		}
		x_connected();
		net_connected();
	}

	trace_netdata('<', netrbuf, nr);

	n_read += nr;
	ns_brcvd += nr;
	for (cp = netrbuf; cp < (netrbuf + nr); cp++)
		if (telnet_fsm(*cp)) {
			x_disconnect();
			return;
		}

	if (ansi_data) {
		trace_str("\n");
		ansi_data = 0;
	}
}


/*
 * telnet_fsm
 *	Telnet finite-state machine.
 *	Returns 0 for okay, -1 for errors.
 */
static int
telnet_fsm(c)
unsigned char c;
{
	char	*errmsg;
	char	*see_chr;
	int	sl;

	switch (telnet_state) {
	    case TNS_DATA:	/* normal data processing */
		if (c == IAC) {	/* got a telnet command */
			telnet_state = TNS_IAC;
			if (ansi_data) {
				trace_str("\n");
				ansi_data = 0;
			}
		} else if (IN_ANSI) {
			if (!ansi_data) {
				trace_str("<.. ");
				ansi_data = 4;
			}
			see_chr = ctl_see((int) c);
			ansi_data += (sl = strlen(see_chr));
			if (ansi_data >= TRACELINE) {
				trace_str(" ...\n... ");
				ansi_data = 4 + sl;
			}
			trace_str(see_chr);
			if (!syncing) {
				ansi_process((unsigned int) c);
				if (appres.scripted)
					script_store(c);
			}
		} else {
			*ibptr++ = c;
		}
		break;
	    case TNS_IAC:	/* process a telnet command */
		if (c != EOR) {
			(void) sprintf(trace_msg, "RCVD %s ", cmd(c));
			trace_str(trace_msg);
		}
		switch (c) {
		    case IAC:	/* escaped IAC, insert it */
			if (IN_ANSI) {
				if (!ansi_data) {
					trace_str("<.. ");
					ansi_data = 4;
				}
				see_chr = ctl_see((int) c);
				ansi_data += (sl = strlen(see_chr));
				if (ansi_data >= TRACELINE) {
					trace_str(" ...\n ...");
					ansi_data = 4 + sl;
				}
				trace_str(see_chr);
				ansi_process((unsigned int) c);
				if (appres.scripted)
					script_store(c);
			} else
				*ibptr++ = c;
			telnet_state = TNS_DATA;
			break;
		    case EOR:	/* eor, process accumulated input */
			if (IN_3270) {
				ns_rrcvd++;
				if (!syncing && (ibptr - ibuf))
					if (process_ds(ibuf, ibptr - ibuf))
						return -1;
			} else
				XtWarning("EOR received when not in 3270 mode, ignored.");
			trace_str("RCVD EOR\n");
			ibptr = ibuf;
			telnet_state = TNS_DATA;
			break;
		    case WILL:
			telnet_state = TNS_WILL;
			break;
		    case WONT:
			telnet_state = TNS_WONT;
			break;
		    case DO:
			telnet_state = TNS_DO;
			break;
		    case DONT:
			telnet_state = TNS_DONT;
			break;
		    case SB:
			telnet_state = TNS_SB;
			sbptr = sbbuf;
			break;
		    case DM:
			trace_str("\n");
			if (syncing) {
				syncing = 0;
				x_except_on();
			}
			telnet_state = TNS_DATA;
			break;
		    case GA:
			trace_str("\n");
			telnet_state = TNS_DATA;
			break;
		    default:
			trace_str("???\n");
			telnet_state = TNS_DATA;
			break;
		}
		break;
	    case TNS_WILL:	/* telnet WILL DO OPTION command */
		trace_str(opt(c));
		trace_str("\n");
		switch (c) {
		    case TELOPT_SGA:
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_ECHO: /* hack! hack! */
			if (!hisopts[c]) {
				hisopts[c] = 1;
				do_opt[2] = c;
				net_rawout(do_opt, sizeof(do_opt));
				(void) sprintf(trace_msg, "SENT %s %s\n",
				    cmd(DO), opt(c));
				trace_str(trace_msg);
			}
			check_in3270();
			break;
		    default:
			dont_opt[2] = c;
			net_rawout(dont_opt, sizeof(dont_opt));
			(void) sprintf(trace_msg, "SENT %s %s\n",
			    cmd(DONT), opt(c));
			trace_str(trace_msg);
			break;
		}
		telnet_state = TNS_DATA;
		check_linemode(False);
		break;
	    case TNS_WONT:	/* telnet WONT DO OPTION command */
		trace_str(opt(c));
		trace_str("\n");
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			if (!ansi_host) {
				errmsg = xs_buffer("Host won't DO option %s\nNot an IBM?", opt(c));
				popup_an_error(errmsg);
				XtFree(errmsg);
				return -1;
			}
			if (hisopts[c]) {
				dont_opt[2] = c;
				net_rawout(dont_opt, sizeof(dont_opt));
				(void) sprintf(trace_msg, "SENT %s %s\n",
				    cmd(DONT), opt(c));
				trace_str(trace_msg);
			}
			/* fall through */
		    default:
			hisopts[c] = 0;
			break;
		}
		telnet_state = TNS_DATA;
		check_in3270();
		check_linemode(False);
		break;
	    case TNS_DO:	/* telnet PLEASE DO OPTION command */
		trace_str(opt(c));
		trace_str("\n");
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_SGA:
			if (!myopts[c]) {
				myopts[c] = 1;
				will_opt[2] = c;
				net_rawout(will_opt, sizeof(will_opt));
				(void) sprintf(trace_msg, "SENT %s %s\n",
				    cmd(WILL), opt(c));
				trace_str(trace_msg);
			}
			check_in3270();
			break;
		    default:
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			(void) sprintf(trace_msg, "SENT %s %s\n",
			    cmd(WONT), opt(c));
			trace_str(trace_msg);
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_DONT:	/* telnet PLEASE DON'T DO OPTION command */
		trace_str(opt(c));
		trace_str("\n");
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			if (!ansi_host) {
				errmsg = xs_buffer("Host says DONT %s\nNot an IBM?", opt(c));
				popup_an_error(errmsg);
				XtFree(errmsg);
				return -1;
			}
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			(void) sprintf(trace_msg, "SENT %s %s\n",
			    cmd(WONT), opt(c));
			trace_str(trace_msg);
			/* fall through */
		    default:
			myopts[c] = 0;
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_SB:	/* telnet sub-option string command */
		if (c == IAC)
			telnet_state = TNS_SB_IAC;
		else
			*sbptr++ = c;
		break;
	    case TNS_SB_IAC:	/* telnet sub-option string command */
		if (c == SE) {
			telnet_state = TNS_DATA;
			if (sbbuf[0] == TELOPT_TTYPE && sbbuf[1] == TELQUAL_SEND) {
				unsigned char *ttype = (unsigned char *)
				    (appres.termname ?
				     appres.termname : ttype_val);

				(void) sprintf(trace_msg, "%s %s\n",
				    opt(sbbuf[0]), telquals[sbbuf[1]]);
				trace_str(trace_msg);
				net_rawout(ttype_opt, sizeof(ttype_opt));
				net_rawout(ttype, strlen((char *) ttype));
				net_rawout(ttype_end, sizeof(ttype_end));

				(void) sprintf(trace_msg,
				    "SENT %s %s %s %s %s\n",
				    cmd(SB), opt(TELOPT_TTYPE),
				    telquals[TELQUAL_IS], appres.termname ? appres.termname : ttype_val,
				    cmd(SE));
				trace_str(trace_msg);
				check_in3270();
				/* tell the remote host to DO EOR */
				/* perhaps this will root out non-IBM hosts */
				do_opt[2] = TELOPT_EOR;
				net_rawout(do_opt, sizeof(do_opt));
				(void) sprintf(trace_msg, "SENT %s %s\n",
				    cmd(DO), opt(TELOPT_EOR));
				trace_str(trace_msg);
			}
		}
		else {
			*sbptr = c;	/* just stuff it */
			telnet_state = TNS_SB;
		}
		break;
	}
	return 0;
}


/*
 * net_exception
 *	Called when there is an exceptional condition on the socket.
 */
void
net_exception()
{
	trace_str("RCVD urgent data indication\n");
	if (!syncing) {
		syncing = 1;
		x_except_off();
	}
}

/*
 * Flavors of Network Output:
 *
 *   3270 mode
 *	net_output	send a 3270 record
 *
 *   ANSI mode; call each other in turn
 *	net_sendc	net_cookout for 1 byte
 *	net_sends	net_cookout for a null-terminated string
 *	net_cookout	send user data with cooked-mode processing, ANSI mode
 *	net_cookedout	send user data, ANSI mode, already cooked
 *	net_rawout	send telnet protocol data, ANSI mode
 *
 */


/*
 * net_rawout
 *	Send out raw telnet data.  We assume that there will always be enough
 *	space to buffer what we want to transmit, so we don't handle EAGAIN or
 *	EWOULDBLOCK.
 */
static void
net_rawout(buf, len)
unsigned char	*buf;
int	len;
{
	int	nw;

	trace_netdata('>', buf, len);

	while (len) {
#if defined(OMTU) /*[*/
		int n2w = len;
		int pause = 0;

		if (n2w > OMTU) {
			n2w = OMTU;
			pause = 1;
		}
#else
#define n2w len
#endif
		if (pstate == NONE) {
			nw = write(sock, (char *) buf, n2w);
			if (nw < 0) {
				if (errno == EPIPE) {
					x_disconnect();
					return;
				} else if (errno == EINTR) {
					goto bot;
				} else {
					popup_an_errno("write", errno);
					x_disconnect();
					return;
				}
			}
		} else
			nw = len;
		ns_bsent += nw;
		len -= nw;
		buf += nw;
	    bot:
#if defined(OMTU) /*[*/
		if (pause)
			sleep(1);
#endif /*]*/
		;
	}
}


/*
 * net_cookedout
 *	Send user data out in ANSI mode, without cooked-mode processing.
 */
static void
net_cookedout(buf, len)
char	*buf;
int	len;
{
	int i;

	if (toggled(TRACE)) {
		(void) fprintf(tracef, ">");
		for (i = 0; i < len; i++)
			(void) fprintf(tracef, " %s", ctl_see((int) *(buf+i)));
		(void) fprintf(tracef, "\n");
	}
	net_rawout((unsigned char *) buf, len);
}


/*
 * net_cookout
 *	Send output in ANSI mode, including cooked-mode processing if
 *	appropriate.
 */
static void
net_cookout(buf, len)
char	*buf;
int	len;
{

	if (!IN_ANSI)
		return;

	if (linemode) {
		register int	i;
		char	c;

		for (i = 0; i < len; i++) {
			c = buf[i];

			/* Input conversions. */
			if (!lnext && c == '\r' && appres.icrnl)
				c = '\n';
			else if (!lnext && c == '\n' && appres.inlcr)
				c = '\r';

			/* Backslashes. */
			if (c == '\\' && !backslashed)
				backslashed = 1;
			else
				backslashed = 0;

			/* Control chars. */
			if (c == '\n')
				do_eol(c);
			else if (c == vintr)
				do_intr(c);
			else if (c == vquit)
				do_quit(c);
			else if (c == verase)
				do_cerase(c);
			else if (c == vkill)
				do_kill(c);
			else if (c == vwerase)
				do_werase(c);
			else if (c == vrprnt)
				do_rprnt(c);
			else if (c == veof)
				do_eof(c);
			else if (c == vlnext)
				do_lnext(c);
			else
				do_data(c);
		}
		return;
	} else
		net_cookedout(buf, len);
}


/*
 * Cooked mode input processing.
 */

static void
cooked_init()
{
	obptr = obuf;
	lnext = 0;
	backslashed = 0;
}

static void
ansi_process_s(data)
char *data;
{
	while (*data)
		ansi_process((unsigned int) *data++);
}

static void
forward_data()
{
	net_cookedout((char *) obuf, obptr - obuf);
	cooked_init();
}

static void
do_data(c)
char c;
{
	if (obptr+1 < obuf + sizeof(obuf)) {
		*obptr++ = c;
		if (c == '\r')
			*obptr++ = '\0';
		if (c == '\t')
			ansi_process((unsigned int) c);
		else
			ansi_process_s(ctl_see((int) c));
	} else
		ansi_process_s("\007");
	lnext = 0;
	backslashed = 0;
}

static void
do_intr(c)
char c;
{
	if (lnext) {
		do_data(c);
		return;
	}
	ansi_process_s(ctl_see((int) c));
	cooked_init();
	net_interrupt();
}

static void
do_quit(c)
char c;
{
	if (lnext) {
		do_data(c);
		return;
	}
	ansi_process_s(ctl_see((int) c));
	cooked_init();
	net_break();
}

static void
do_cerase(c)
char c;
{
	int len;

	if (backslashed) {
		obptr--;
		ansi_process_s("\b");
		do_data(c);
		return;
	}
	if (lnext) {
		do_data(c);
		return;
	}
	if (obptr > obuf) {
		len = strlen(ctl_see((int) *--obptr));

		while (len--)
			ansi_process_s("\b \b");
	}
}

static void
do_werase(c)
char c;
{
	int any = 0;
	int len;

	if (lnext) {
		do_data(c);
		return;
	}
	while (obptr > obuf) {
		char ch = *--obptr;

		if (ch == ' ' || ch == '\t') {
			if (any) {
				++obptr;
				break;
			}
		} else
			any = 1;
		len = strlen(ctl_see((int) ch));

		while (len--)
			ansi_process_s("\b \b");
	}
}

static void
do_kill(c)
char c;
{
	int i, len;

	if (backslashed) {
		obptr--;
		ansi_process_s("\b");
		do_data(c);
		return;
	}
	if (lnext) {
		do_data(c);
		return;
	}
	while (obptr > obuf) {
		len = strlen(ctl_see((int) *--obptr));

		for (i = 0; i < len; i++)
			ansi_process_s("\b \b");
	}
}

static void
do_rprnt(c)
char c;
{
	unsigned char *p;

	if (lnext) {
		do_data(c);
		return;
	}
	ansi_process_s(ctl_see((int) c));
	ansi_process_s("\r\n");
	for (p = obuf; p < obptr; p++)
		ansi_process_s(ctl_see((int) *p));
}

static void
do_eof(c)
char c;
{
	if (backslashed) {
		obptr--;
		ansi_process_s("\b");
		do_data(c);
		return;
	}
	if (lnext) {
		do_data(c);
		return;
	}
	do_data(c);
	forward_data();
}

static void
do_eol(c)
char c;
{
	if (lnext) {
		do_data(c);
		return;
	}
	if (obptr+2 >= obuf + sizeof(obuf)) {
		ansi_process_s("\007");
		return;
	}
	*obptr++ = '\r';
	*obptr++ = '\n';
	ansi_process_s("\r\n");
	forward_data();
}

static void
do_lnext(c)
char c;
{
	if (lnext) {
		do_data(c);
		return;
	}
	lnext = 1;
	ansi_process_s("^\b");
}



/*
 * check_in3270
 *	Check for mode switches between ANSI and 3270 mode.
 */
static void
check_in3270()
{
	Boolean now3270 = myopts[TELOPT_BINARY] &&
	    myopts[TELOPT_EOR] &&
	    myopts[TELOPT_TTYPE] &&
	    hisopts[TELOPT_BINARY] &&
	    hisopts[TELOPT_EOR];

	if (now3270 != IN_3270) {
		(void) sprintf(trace_msg, "No%c operating in 3270 mode.\n",
		    now3270 ? 'w' : 't');
		trace_str(trace_msg);
		x_in3270(now3270);
	}
}


/*
 * check_linemode
 *	Set the global variable 'linemode', which says whether we are in
 *	character-by-character mode or line mode.
 */
static void
check_linemode(init)
Boolean init;
{
	int wasline = linemode;

	/*
	 * The next line is a deliberate kluge to effectively ignore the SGA
	 * option.  If the host will echo for us, we assume
	 * character-at-a-time; otherwise we assume fully cooked by us.
	 *
	 * This allows certain IBM hosts which volunteer SGA but refuse
	 * ECHO to operate more-or-less normally, at the expense of
	 * implementing the (hopefully useless) "character-at-a-time, local
	 * echo" mode.
	 *
	 * We still implement "switch to line mode" and "switch to character
	 * mode" properly by asking for both SGA and ECHO to be off or on, but
	 * we basically ignore the reply for SGA.
	 */
	linemode = !hisopts[TELOPT_ECHO] /* && !hisopts[TELOPT_SGA] */;

	if (init || linemode != wasline) {
		menubar_newmode();
		if (!init) {
			(void) sprintf(trace_msg, "Operating in %s mode.\n",
			    linemode ? "line" : "character-at-a-time");
			trace_str(trace_msg);
		}
		if (linemode)
			cooked_init();
	}
}


/*
 * nnn
 *	Expands a number to a character string, for displaying unknown telnet
 *	commands and options.
 */
static char *
nnn(c)
int	c;
{
	static char	buf[64];

	(void) sprintf(buf, "%d", c);
	return buf;
}


/*
 * ctl_see
 *	Expands a character in the manner of "cat -v".
 */
static char *
ctl_see(c)
int	c;
{
	static char	buf[64];
	char	*p = buf;

	c &= 0xff;
	if (c & 0x80) {
		*p++ = 'M';
		*p++ = '-';
		c &= 0x7f;
	}
	if (c >= ' ' && c < 0x7f) {
		*p++ = c;
	} else {
		*p++ = '^';
		if (c == 0x7f) {
			*p++ = '?';
		} else {
			*p++ = c + '@';
		}
	}
	*p = '\0';
	return buf;
}

#define LINEDUMP_MAX	32

static void
trace_netdata(direction, buf, len)
char direction;
unsigned char *buf;
int len;
{
	int offset;

	if (!toggled(TRACE))
		return;
	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			(void) fprintf(tracef, "%s%c 0x%-3x ",
			    (offset ? "\n" : ""), direction, offset);
		(void) fprintf(tracef, "%02x", buf[offset]);
	}
	(void) fprintf(tracef, "\n");
}


/*
 * net_output
 *	Send 3270 output over the network, tacking on the necessary
 *	telnet end-of-record command.
 */
void
net_output(buf, len)
unsigned char	buf[];
int	len;
{
	buf[len++] = IAC;
	buf[len++] = EOR;
	net_rawout(buf, len);
	trace_str("SENT EOR\n");
	ns_rsent++;
	cooked_init();	/* in case we go back to ANSI mode */
}


/*
 * net_sendc
 *	Send a character of user data over the network in ANSI mode.
 */
void
net_sendc(c)
char	c;
{
	if (c == '\r' && !linemode)	/* CR must be quoted */
		net_cookout("\r\n", 2);
	else
		net_cookout(&c, 1);
}


/*
 * net_sends
 *	Send a null-terminated string of user data in ANSI mode.
 */
void
net_sends(s)
char	*s;
{
	net_cookout(s, strlen(s));
}


/*
 * net_send_erase
 *	Sends the KILL character in ANSI mode.
 */
void
net_send_erase()
{
	net_cookout(&verase, 1);
}


/*
 * net_send_kill
 *	Sends the KILL character in ANSI mode.
 */
void
net_send_kill()
{
	net_cookout(&vkill, 1);
}


/*
 * net_send_werase
 *	Sends the WERASE character in ANSI mode.
 */
void
net_send_werase()
{
	net_cookout(&vwerase, 1);
}


/*
 * External entry points to negotiate line or character mode.
 */
void
net_linemode()
{
	if (!CONNECTED)
		return;
	if (hisopts[TELOPT_ECHO]) {
		dont_opt[2] = TELOPT_ECHO;
		net_rawout(dont_opt, sizeof(dont_opt));
		(void) sprintf(trace_msg, "SENT %s %s\n",
		    cmd(DONT), opt(TELOPT_ECHO));
		trace_str(trace_msg);
	}
	if (hisopts[TELOPT_SGA]) {
		dont_opt[2] = TELOPT_SGA;
		net_rawout(dont_opt, sizeof(dont_opt));
		(void) sprintf(trace_msg, "SENT %s %s\n",
		    cmd(DONT), opt(TELOPT_SGA));
		trace_str(trace_msg);
	}
}

void
net_charmode()
{
	if (!CONNECTED)
		return;
	if (!hisopts[TELOPT_ECHO]) {
		do_opt[2] = TELOPT_ECHO;
		net_rawout(do_opt, sizeof(do_opt));
		(void) sprintf(trace_msg, "SENT %s %s\n", cmd(DO),
		    opt(TELOPT_ECHO));
		trace_str(trace_msg);
	}
	if (!hisopts[TELOPT_SGA]) {
		do_opt[2] = TELOPT_SGA;
		net_rawout(do_opt, sizeof(do_opt));
		(void) sprintf(trace_msg, "SENT %s %s\n", cmd(DO),
		    opt(TELOPT_SGA));
		trace_str(trace_msg);
	}
}


/*
 * net_break
 *	Send telnet break, which is used to implement 3270 ATTN.
 *
 */
void
net_break()
{
	static unsigned char buf[] = { IAC, BREAK };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	trace_str("SENT BREAK\n");
}

/*
 * net_interrupt
 *	Send telnet IP.
 *
 */
static void
net_interrupt()
{
	static unsigned char buf[] = { IAC, IP };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	trace_str("SENT IP\n");
}

/*
 * parse_ctlchar
 *	Parse an stty control-character specification.
 *	A cheap, non-complaining implementation.
 */
static char
parse_ctlchar(s)
char *s;
{
	if (!s || !*s)
		return 0;
	if ((int) strlen(s) > 1) {
		if (*s != '^')
			return 0;
		else if (*(s+1) == '?')
			return 0177;
		else
			return *(s+1) - '@';
	} else
		return *s;
}

/*
 * net_linemode_chars
 *	Report line-mode characters.
 */
struct ctl_char *
net_linemode_chars()
{
	static struct ctl_char c[9];

	c[0].name = "intr";	(void) strcpy(c[0].value, ctl_see(vintr));
	c[1].name = "quit";	(void) strcpy(c[1].value, ctl_see(vquit));
	c[2].name = "erase";	(void) strcpy(c[2].value, ctl_see(verase));
	c[3].name = "kill";	(void) strcpy(c[3].value, ctl_see(vkill));
	c[4].name = "eof";	(void) strcpy(c[4].value, ctl_see(veof));
	c[5].name = "werase";	(void) strcpy(c[5].value, ctl_see(vwerase));
	c[6].name = "rprnt";	(void) strcpy(c[6].value, ctl_see(vrprnt));
	c[7].name = "lnext";	(void) strcpy(c[7].value, ctl_see(vlnext));
	c[8].name = 0;

	return c;
}

/*
 * Set blocking/non-blocking mode on the socket.  On error, pops up an error
 * message, but does not close the socket.
 */
static int
non_blocking(on)
Boolean on;
{
#if defined(FIONBIO) /*[*/
	int i = on ? 1 : 0;

	if (ioctl(sock, FIONBIO, &i) < 0) {
		popup_an_errno("ioctl(FIONBIO)", errno);
		return -1;
	}
#else /*][*/
	int f;

	if ((f = fcntl(sock, F_GETFL, 0)) == -1) {
		popup_an_errno("fcntl(F_GETFL)", errno);
		return -1;
	}
	if (on)
		f |= O_NDELAY;
	else
		f &= ~O_NDELAY;
	if (fcntl(sock, F_SETFL, f) < 0) {
		popup_an_errno("fcntl(F_SETFL)", errno);
		return -1;
	}
#endif /*]*/
	return 0;
}

static void
trace_str(s)
char *s;
{
	if (!toggled(TRACE))
		return;
	(void) fprintf(tracef, "%s", s);
}

/*
 * Playback file support.
 */

int
net_playback_connect(f)
char *f;
{

	if (playbackf)
		(void) fclose(playbackf);
	if (!(playbackf = fopen(f, "r"))) {
		popup_an_errno(f, errno);
		return -1;
	}

	hostname = XtNewString(f);
	pstate = BASE;
	net_connected();

	return fileno(playbackf);
}

void
net_playback_step()
{
	int c = 0;
	static int d1;
	static char hexes[] = "0123456789abcdef";
#	define isxd(c) strchr(hexes, c)
	static int again = 0;

	if (!playbackf) {
		popup_an_error("PlaybackStep: not connected to playback file");
		return;
	}

	while (CONNECTED && (again || ((c = fgetc(playbackf)) != EOF))) {
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
			else if (c == ' ')
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
			} else if (c == ' ')
				pstate = SPACE2;
			else {
				pstate = WRONG;
				again = 1;
			}
			break;
		    case D1:
			if (isxd(c)) {
				int r = ns_rrcvd;

				if (telnet_fsm((unsigned char)((d1 * 16) + (strchr(hexes, c) - hexes)))) {
					popup_an_error("Playback file fsm error");
					goto close_playback;
				}
				pstate = D2;
				if (ns_rrcvd != r)
					return;
			} else {
				popup_an_error("Playback file parse error");
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
				if (ansi_data) {
					trace_str("\n");
					ansi_data = 0;
				}
				if (IN_ANSI)
					return;
			} else {
				popup_an_error("Playback file parse error");
				pstate = WRONG;
				again = 1;
			}
			break;
		}
	}
	if (ansi_data) {
		trace_str("\n");
		ansi_data = 0;
	}
	if (c == EOF)
		trace_str("Playback file EOF.\n");

    close_playback:
	x_disconnect();
}
