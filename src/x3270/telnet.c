/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 * Copyright 1990 Jeff Sparkes.
 * Copyright 1993 Paul Mattes.
 *
 *                         All Rights Reserved
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
#ifndef hpux
#include <arpa/telnet.h>
#else
#include "telnet.h"
#endif
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include "globals.h"

#define BUFSZ	4096

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

/* Statics */
static int	sock = -1;			/* active socket */
static char	*hostname = (char *) NULL;
static unsigned char	myopts[256], hisopts[256];	/* telnet option flags */
static unsigned char	ibuf[4*BUFSZ], *ibptr;		/* 3270 input buffer */
static unsigned char	netrbuf[BUFSZ];			/* network input buffer */
static unsigned char	sbbuf[1024], *sbptr;		/* telnet sub-option buffer */
static unsigned char	telnet_state;
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

#define trace	if (toggled(TRACETN)) (void) printf
#define cmd(c)	(((c) >= SE && (c) <= IAC) ? telcmds[(c) - SE] : nnn((int)c))
#define opt(c)	(((c) <= TELOPT_EOR) ? telopts[c] : nnn((int)c))
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
	int			port;
	struct sockaddr_in	sin;
	int			i;
	int			on = 1;

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

	/* initialize the telnet variables */
	(void) memset((char *) myopts, 0, sizeof(myopts));
	(void) memset((char *) hisopts, 0, sizeof(hisopts));
	telnet_state = TNS_DATA;
	ibptr = &ibuf[0];
	sbptr = &sbbuf[0];

	/* get the tcp/ip service (telnet) */
	if (sp = getservbyname(portname, "tcp"))
		port = sp->s_port;
	else if (!(port = htons(atoi(portname)))) {
		popup_an_error("unknown service");
		return -1;
	}

	/* fill in the socket address of the given host */
	(void) memset((char *) &sin, 0, sizeof(sin));
	if ((hp = gethostbyname(host)) == (struct hostent *) 0) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(host);
		if (sin.sin_addr.s_addr == -1) {
			char *msg = xs_buffer("unknown host:\n%s", hostname);

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

	/* set the socket to be non-delaying */
	if (non_blocking(True) < 0)
		close_fail;

	/* connect */
	if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		if (errno == EWOULDBLOCK
#ifdef EINPROGRESS
		 || errno == EINPROGRESS
#endif
		                        ) {
			trace("connect pending\n");
			*pending = True;
		} else {
			char *msg = xs_buffer("connect %s", hostname);

			popup_an_errno(msg, errno);
			XtFree(msg);
			close_fail;
		}
	} else {
		if (non_blocking(False) < 0)
			close_fail;
		trace("connected\n");
	}

	/* clear statistics and flags */
	(void) time(&ns_time);
	ns_brcvd = 0;
	ns_rrcvd = 0;
	ns_bsent = 0;
	ns_rsent = 0;
	syncing = 0;
	check_linemode(True);

	/* all done */
	return sock;
}
#undef close_fail

/*
 * net_disconnect
 *	Shut down the socket.
 */
void
net_disconnect()
{
	(void) shutdown(sock, 2);
	(void) close(sock);
	sock = -1;
	trace("SENT disconnect\n");
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
			trace("connected\n");
			return;
		}
		trace("RCVD socket error %d\n", errno);
		if (HALF_CONNECTED) {
			char *msg = xs_buffer("connect %s", hostname);

			popup_an_errno(msg, errno);
			XtFree(msg);
		} else
			popup_an_errno("socket read", errno);
		x_disconnect();
		return;
	} else if (nr == 0) {
		/* Host disconnected. */
		trace("RCVD disconnect\n");
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
		trace("connected\n");
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
		trace("\n");
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

	switch (telnet_state) {
	    case TNS_DATA:	/* normal data processing */
		if (c == IAC) {	/* got a telnet command */
			telnet_state = TNS_IAC;
			if (ansi_data) {
				trace("\n");
				ansi_data = 0;
			}
		} else if (IN_ANSI) {
			if (!ansi_data++) {
				trace("<");
			}
			trace(" %s", ctl_see((int) c));
			if (!syncing)
				ansi_process((unsigned int) c);
		} else {
			*ibptr++ = c;
		}
		break;
	    case TNS_IAC:	/* process a telnet command */
		if (c != EOR)
			trace("RCVD %s ", cmd(c));
		switch (c) {
		    case IAC:	/* escaped IAC, insert it */
			if (IN_ANSI) {
				if (!ansi_data++) {
					trace("<");
				}
				trace(" %s", ctl_see((int) c));
				ansi_process((unsigned int) c);
			} else
				*ibptr++ = c;
			telnet_state = TNS_DATA;
			break;
		    case EOR:	/* eor, process accumulated input */
			if (IN_3270) {
				ns_rrcvd++;
				if (!syncing)
					if (process_ds(ibuf, ibptr - ibuf))
						return -1;
			} else
				(void) fprintf(stderr, "EOR received when not in 3270 mode, ignored.\n");
			trace("RCVD EOR\n");
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
			trace("\n");
			if (syncing) {
				syncing = 0;
				x_except_on();
			}
			telnet_state = TNS_DATA;
			break;
		    case GA:
			trace("\n");
			telnet_state = TNS_DATA;
			break;
		    default:
			trace("???\n");
			telnet_state = TNS_DATA;
			break;
		}
		break;
	    case TNS_WILL:	/* telnet WILL DO OPTION command */
		trace("%s\n", opt(c));
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
				trace("SENT %s %s\n", cmd(DO), opt(c));
			}
			check_in3270();
			break;
		    default:
			dont_opt[2] = c;
			net_rawout(dont_opt, sizeof(dont_opt));
			trace("SENT %s %s\n", cmd(DONT), opt(c));
			break;
		}
		telnet_state = TNS_DATA;
		check_linemode(False);
		break;
	    case TNS_WONT:	/* telnet WONT DO OPTION command */
		trace("%s\n", opt(c));
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			if (!ansi_host) {
				errmsg = xs_buffer("Host won't DO option %s\nNot an IBM?", opt(c));
				popup_an_error(errmsg);
				XtFree(errmsg);
				return -1;
			} /* else fall through */
		    default:
			hisopts[c] = 0;
			break;
		}
		telnet_state = TNS_DATA;
		check_in3270();
		check_linemode(False);
		break;
	    case TNS_DO:	/* telnet PLEASE DO OPTION command */
		trace("%s\n", opt(c));
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_SGA:
			if (!myopts[c]) {
				myopts[c] = 1;
				will_opt[2] = c;
				net_rawout(will_opt, sizeof(will_opt));
				trace("SENT %s %s\n", cmd(WILL), opt(c));
			}
			check_in3270();
			break;
		    default:
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			trace("SENT %s %s\n", cmd(WONT), opt(c));
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_DONT:	/* telnet PLEASE DON'T DO OPTION command */
		trace("%s\n", opt(c));
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
			if (!ansi_host) {
				errmsg = xs_buffer("Host says DONT %s\nNot an IBM?", opt(c));
				popup_an_error(errmsg);
				XtFree(errmsg);
				return -1;
			} /* else fall through */
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

				trace("%s %s\n", opt(sbbuf[0]),
				    telquals[sbbuf[1]]);
				net_rawout(ttype_opt, sizeof(ttype_opt));
				net_rawout(ttype, strlen((char *) ttype));
				net_rawout(ttype_end, sizeof(ttype_end));

				trace("SENT %s %s %s %s %s\n",
				    cmd(SB), opt(TELOPT_TTYPE),
				    telquals[TELQUAL_IS], appres.termname ? appres.termname : ttype_val,
				    cmd(SE));
				check_in3270();
				/* tell the remote host to DO EOR */
				/* perhaps this will root out non-IBM hosts */
				do_opt[2] = TELOPT_EOR;
				net_rawout(do_opt, sizeof(do_opt));
				trace("SENT %s %s\n", cmd(DO), opt(TELOPT_EOR));
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
	trace("RCVD urgent data indication\n");
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
		nw = write(sock, (char *) buf, len);
		if (nw < 0) {
			if (errno == EPIPE) {
				x_disconnect();
				return;
			} else if (errno == EINTR) {
				continue;
			} else {
				popup_an_errno("write", errno);
				x_disconnect();
				return;
			}
		}
		ns_bsent += len;
		len -= nw;
		buf += nw;
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

	if (toggled(TRACETN)) {
		(void) printf(">");
		for (i = 0; i < len; i++)
			(void) printf(" %s", ctl_see((int) *(buf+i)));
		(void) printf("\n");
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
	if (obptr+1 >= obuf + sizeof(obuf)) {
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
		trace("No%c operating in 3270 mode.\n", now3270 ? 'w' : 't');
		if (now3270)
			x_in3270();
		else	/* host un3270'd, punt for now */
			x_disconnect();
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
		if (!init)
			trace("Operating in %s mode.\n",
			    linemode ? "line" : "character-at-a-time");
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

	if (!toggled(TRACETN))
		return;
	for (offset = 0; offset < len; offset++) {
		if (!(offset % LINEDUMP_MAX))
			(void) printf("%s%c 0x%-3x ", (offset ? "\n" : ""),
			    direction, offset);
		(void) printf("%02x", buf[offset]);
	}
	(void) printf("\n");
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
	trace("SENT EOR\n");
	ns_rsent++;
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
		trace("SENT %s %s\n", cmd(DONT), opt(TELOPT_ECHO));
	}
	if (hisopts[TELOPT_SGA]) {
		dont_opt[2] = TELOPT_SGA;
		net_rawout(dont_opt, sizeof(dont_opt));
		trace("SENT %s %s\n", cmd(DONT), opt(TELOPT_SGA));
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
		trace("SENT %s %s\n", cmd(DO), opt(TELOPT_ECHO));
	}
	if (!hisopts[TELOPT_SGA]) {
		do_opt[2] = TELOPT_SGA;
		net_rawout(do_opt, sizeof(do_opt));
		trace("SENT %s %s\n", cmd(DO), opt(TELOPT_SGA));
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
	trace("SENT BREAK\n");
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
	trace("SENT IP\n");
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

	c[0].name = "intr";	strcpy(c[0].value, ctl_see(vintr));
	c[1].name = "quit";	strcpy(c[1].value, ctl_see(vquit));
	c[2].name = "erase";	strcpy(c[2].value, ctl_see(verase));
	c[3].name = "kill";	strcpy(c[3].value, ctl_see(vkill));
	c[4].name = "eof";	strcpy(c[4].value, ctl_see(veof));
	c[5].name = "werase";	strcpy(c[5].value, ctl_see(vwerase));
	c[6].name = "rprnt";	strcpy(c[6].value, ctl_see(vrprnt));
	c[7].name = "lnext";	strcpy(c[7].value, ctl_see(vlnext));
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
#ifdef FIONBIO
	int i = on ? 1 : 0;

	if (ioctl(sock, FIONBIO, &i) < 0) {
		popup_an_errno("ioctl(FIONBIO)", errno);
		return -1;
	}
#else
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
#endif
	return 0;
}
