/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994, 1995 by Paul Mattes.
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

#include "globals.h"
#include <sys/types.h>
#include <sys/socket.h>
#if !defined(NO_SYS_TIME_H) /*[*/
#include <sys/time.h>
#endif /*]*/
#include <sys/ioctl.h>
#include <netinet/in.h>
#define TELCMDS 1
#define TELOPTS 1
#if !defined(LOCAL_TELNET_H) /*[*/
#include <arpa/telnet.h>
#else /*][*/
#include "telnet.h"
#endif /*]*/
#include <errno.h>
#include <fcntl.h>
#if !defined(NO_MEMORY_H) /*[*/
#include <memory.h>
#endif /*]*/
#include <netdb.h>
#include <string.h>

#define BUFSZ		4096
#define TRACELINE	72

#define N_OPTS		256

/* Globals */
int		n_read = 0;			/* number of characters read */
time_t		ns_time;
int		ns_brcvd;
int		ns_rrcvd;
int		ns_bsent;
int		ns_rsent;
unsigned char	obuf[8*BUFSZ], *obptr;		/* 3270 output buffer */
int		linemode = 1;
char		ttype_val[] = "IBM-3278-4-E";
char		*ttype_color = &ttype_val[7];
char		*ttype_model = &ttype_val[9];
char		*ttype_extension = &ttype_val[10];
char		*termtype = ttype_val;
void		trace_netdata();

/* Externals */
extern unsigned long	inet_addr();
extern FILE		*tracef;
extern struct timeval	ds_ts;

/* Statics */
static int	sock = -1;			/* active socket */
static char	*hostname = CN;
static unsigned char	myopts[N_OPTS], hisopts[N_OPTS]; /* telnet option flags */
static unsigned char	ibuf[8*BUFSZ], *ibptr;		/* 3270 input buffer */
static unsigned char	netrbuf[BUFSZ];			/* network input buffer */
static unsigned char	sbbuf[1024], *sbptr;		/* telnet sub-option buffer */
static unsigned char	lbuf[BUFSZ], *lbptr;		/* line-mode input buffer */
static unsigned char	telnet_state;
static char	trace_msg[256];
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

#if defined(TELCMD) && defined(TELCMD_OK)
#define cmd(c)	(TELCMD_OK(c) ? TELCMD(c) : nnn((int)c))
#define ccmd(c)	cmd(c)
#else
#define ccmd(c)	(telcmds[(c) - SE])
#define cmd(c)	(((c) >= SE && (c) <= IAC) ? ccmd(c) : nnn((int)c))
#endif

#if defined(TELOPT) && defined(TELOPT_OK)
#define opt(c)	(TELOPT_OK(c) ? TELOPT(c) : nnn((int)c))
#define copt(c)	opt(c)
#else
#define copt(c)	(telopts[c])
#define opt(c)	(((c) <= TELOPT_EOR) ? copt(c) : nnn((int)c))
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

	/* get the port number */
	port = (unsigned short) atoi(portname);
	if (port)
		port = htons(port);
	else {
		if (!(sp = getservbyname(portname, "tcp"))) {
			popup_an_error("Unknown port number or service: %s",
			    portname);
			return -1;
		}
		port = sp->s_port;
	}
	current_port = ntohs(port);


	/* fill in the socket address of the given host */
	(void) memset((char *) &sin, 0, sizeof(sin));
	if ((hp = gethostbyname(host)) == (struct hostent *) 0) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(host);
		if (sin.sin_addr.s_addr == -1) {
			popup_an_error("Unknown host:\n%s", hostname);
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
		popup_an_errno(errno, "socket");
		return -1;
	}

	/* set options for inline out-of-band data and keepalives */
	if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0) {
		popup_an_errno(errno, "setsockopt(SO_OOBINLINE)");
		close_fail;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0) {
		popup_an_errno(errno, "setsockopt(SO_KEEPALIVE)");
		close_fail;
	}
#if defined(OMTU) /*[*/
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&mtu, sizeof(mtu)) < 0) {
		popup_an_errno(errno, "setsockopt(SO_SNDBUF)");
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
			popup_an_errno(errno, "Connect to %s, port %d",
			    hostname, current_port);
			close_fail;
		}
	} else {
		if (non_blocking(False) < 0)
			close_fail;
		net_connected();
	}

	/* set up termtype */
	if (appres.termname)
		termtype = appres.termname;
	else {
		if (appres.extended && extended_ds_host)
			termtype = ttype_val;
		else {
			termtype = XtNewString(ttype_val);
			termtype[10] = '\0';
		}
	}

	/* all done */
	return sock;
}
#undef close_fail

static void
net_connected()
{
	(void) sprintf(trace_msg, "Connected to %s, port %u.\n",
	    hostname, current_port);
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
	if (CONNECTED)
		(void) shutdown(sock, 2);
	(void) close(sock);
	sock = -1;
	trace_str("SENT disconnect\n");

	/* Restore terminal type to its default. */
	if (!appres.termname && termtype != ttype_val) {
		XtFree((XtPointer)termtype);
		termtype = ttype_val;
	}
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

	nr = read(sock, (char *) netrbuf, BUFSZ);
	if (nr < 0) {
		if (errno == EWOULDBLOCK)
			return;
		if (HALF_CONNECTED && errno == EAGAIN) {
			if (non_blocking(False) < 0) {
				x_disconnect(True);
				return;
			}
			x_connected();
			net_connected();
			return;
		}
		(void) sprintf(trace_msg, "RCVD socket error %d\n", errno);
		trace_str(trace_msg);
		if (HALF_CONNECTED)
			popup_an_errno(errno, "Connect to %s, port %d",
			    hostname, current_port);
		else
			popup_an_errno(errno, "Socket read");
		x_disconnect(True);
		return;
	} else if (nr == 0) {
		/* Host disconnected. */
		trace_str("RCVD disconnect\n");
		x_disconnect(False);
		return;
	}

	/* Process the data. */

	if (HALF_CONNECTED) {
		if (non_blocking(False) < 0) {
			x_disconnect(True);
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
			x_disconnect(True);
			return;
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
	int	sl;

	switch (telnet_state) {
	    case TNS_DATA:	/* normal data processing */
		if (c == IAC) {	/* got a telnet command */
			telnet_state = TNS_IAC;
		} else if (IN_ANSI) {
			if (kybdlock & KL_AWAITING_FIRST) {
				kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
				status_reset();
				ps_process();
				script_continue();
			}
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
		switch (c) {
		    case IAC:	/* escaped IAC, insert it */
			if (IN_ANSI) {
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
			if (syncing) {
				syncing = 0;
				x_except_on();
			}
			telnet_state = TNS_DATA;
			break;
		    case GA:
			telnet_state = TNS_DATA;
			break;
		    default:
			telnet_state = TNS_DATA;
			break;
		}
		break;
	    case TNS_WILL:	/* telnet WILL DO OPTION command */
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
			}
			check_in3270();
			break;
		    default:
			dont_opt[2] = c;
			net_rawout(dont_opt, sizeof(dont_opt));
			break;
		}
		telnet_state = TNS_DATA;
		check_linemode(False);
		break;
	    case TNS_WONT:	/* telnet WONT DO OPTION command */
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			if (hisopts[c]) {
				dont_opt[2] = c;
				net_rawout(dont_opt, sizeof(dont_opt));
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
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_SGA:
			if (!myopts[c]) {
				myopts[c] = 1;
				will_opt[2] = c;
				net_rawout(will_opt, sizeof(will_opt));
			}
			check_in3270();
			break;
		    default:
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_DONT:	/* telnet PLEASE DON'T DO OPTION command */
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
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
				char *tt_out;

				tt_out = XtMalloc(4 + strlen(termtype) + 2 + 1);
				(void) sprintf(tt_out, "%c%c%c%c%s%c%c",
				    IAC, SB, TELOPT_TTYPE, TELQUAL_IS,
				    termtype,
				    IAC, SE);
				net_rawout((unsigned char *)tt_out,
					   4 + strlen(termtype) + 2);
				XtFree(tt_out);

				check_in3270();
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
#		define n2w len
#endif
		nw = write(sock, (char *) buf, n2w);
		if (nw < 0) {
			if (errno == EPIPE) {
				x_disconnect(False);
				return;
			} else if (errno == EINTR) {
				goto bot;
			} else {
				popup_an_errno(errno, "write");
				x_disconnect(True);
				return;
			}
		}
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

	if (toggled(DS_TRACE)) {
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

	if (!IN_ANSI || (kybdlock & KL_AWAITING_FIRST))
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
	lbptr = lbuf;
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
	net_cookedout((char *) lbuf, lbptr - lbuf);
	cooked_init();
}

static void
do_data(c)
char c;
{
	if (lbptr+1 < lbuf + sizeof(lbuf)) {
		*lbptr++ = c;
		if (c == '\r')
			*lbptr++ = '\0';
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
		lbptr--;
		ansi_process_s("\b");
		do_data(c);
		return;
	}
	if (lnext) {
		do_data(c);
		return;
	}
	if (lbptr > lbuf) {
		len = strlen(ctl_see((int) *--lbptr));

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
	while (lbptr > lbuf) {
		char ch = *--lbptr;

		if (ch == ' ' || ch == '\t') {
			if (any) {
				++lbptr;
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
		lbptr--;
		ansi_process_s("\b");
		do_data(c);
		return;
	}
	if (lnext) {
		do_data(c);
		return;
	}
	while (lbptr > lbuf) {
		len = strlen(ctl_see((int) *--lbptr));

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
	for (p = lbuf; p < lbptr; p++)
		ansi_process_s(ctl_see((int) *p));
}

static void
do_eof(c)
char c;
{
	if (backslashed) {
		lbptr--;
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
	if (lbptr+2 >= lbuf + sizeof(lbuf)) {
		ansi_process_s("\007");
		return;
	}
	*lbptr++ = '\r';
	*lbptr++ = '\n';
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


#define LINEDUMP_MAX	32

void
trace_netdata(direction, buf, len)
char direction;
unsigned char *buf;
int len;
{
	int offset;
	struct timeval ts;
	double tdiff;

	if (!toggled(DS_TRACE))
		return;
	(void) gettimeofday(&ts, (struct timezone *)NULL);
	if (IN_3270) {
		tdiff = ((1.0e6 * (double)(ts.tv_sec - ds_ts.tv_sec)) +
			(double)(ts.tv_usec - ds_ts.tv_usec)) / 1.0e6;
		(void) fprintf(tracef, "%c +%gs\n", direction, tdiff);
	}
	ds_ts = ts;
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
	char *iac;
	char *b = (char *)buf;
	int l = len;

	/* Quote any lurking IAC characters in the message. */
	while (l && (iac = memchr(b, IAC, l))) {
		int ml = l - (iac - b);

		MEMORY_MOVE(iac + 1, iac, ml);
		l -= iac - b + 1;
		b = iac + 2;
		len++;
	}

	/* Add IAC EOR to the end and send it. */
	buf[len++] = IAC;
	buf[len++] = EOR;
	net_rawout(buf, len);

	ns_rsent++;
	cooked_init();	/* in case we go back to ANSI mode */
}

/*
 * Add IAC EOR to a buffer.
 */
void
net_add_eor(buf, len)
unsigned char *buf;
int len;
{
	buf[len++] = IAC;
	buf[len++] = EOR;
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
	}
	if (hisopts[TELOPT_SGA]) {
		dont_opt[2] = TELOPT_SGA;
		net_rawout(dont_opt, sizeof(dont_opt));
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
	}
	if (!hisopts[TELOPT_SGA]) {
		do_opt[2] = TELOPT_SGA;
		net_rawout(do_opt, sizeof(do_opt));
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
 * Construct a string to reproduce the current TELNET options.
 * Returns a Boolean indicating whether it is necessary.
 */
Boolean
net_snap_options()
{
	Boolean any = False;
	int i;
	static unsigned char ttype_str[] = {
		IAC, DO, TELOPT_TTYPE,
		IAC, SB, TELOPT_TTYPE, TELQUAL_SEND, IAC, SE
	};

	if (!CONNECTED)
		return False;

	obptr = &obuf[0];

	/* Do TTYPE first. */
	if (myopts[TELOPT_TTYPE])
		for (i = 0; i < sizeof(ttype_str); i++)
			*obptr++ = ttype_str[i];

	/* Do the other options. */
	for (i = 0; i < N_OPTS; i++) {
		if (i == TELOPT_TTYPE)
			continue;
		if (hisopts[i]) {
			*obptr++ = IAC;
			*obptr++ = WILL;
			*obptr++ = (unsigned char)i;
			any = True;
		}
		if (myopts[i]) {
			*obptr++ = IAC;
			*obptr++ = DO;
			*obptr++ = (unsigned char)i;
			any = True;
		}
	}
	return any;
}

/*
 * Set blocking/non-blocking mode on the socket.  On error, pops up an error
 * message, but does not close the socket.
 */
static int
non_blocking(on)
Boolean on;
{
#if !defined(BLOCKING_CONNECT_ONLY) /*[*/
# if defined(FIONBIO) /*[*/
	int i = on ? 1 : 0;

	if (ioctl(sock, FIONBIO, &i) < 0) {
		popup_an_errno(errno, "ioctl(FIONBIO)");
		return -1;
	}
# else /*][*/
	int f;

	if ((f = fcntl(sock, F_GETFL, 0)) == -1) {
		popup_an_errno(errno, "fcntl(F_GETFL)");
		return -1;
	}
	if (on)
		f |= O_NDELAY;
	else
		f &= ~O_NDELAY;
	if (fcntl(sock, F_SETFL, f) < 0) {
		popup_an_errno(errno, "fcntl(F_SETFL)");
		return -1;
	}
# endif /*]*/
#endif /*]*/
	return 0;
}

static void
trace_str(s)
char *s;
{
	if (!toggled(DS_TRACE))
		return;
	(void) fprintf(tracef, "%s", s);
}
