/*
 * Modifications Copyright 1993, 1994, 1995, 1999, 2000 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 */

/*
 *	telnet.c
 *		This module initializes and manages a telnet socket to
 *		the given IBM host.
 */

#include "globals.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#define TELCMDS 1
#define TELOPTS 1
#if !defined(LOCAL_TELNET_H) /*[*/
#include <arpa/telnet.h>
#else /*][*/
#include "telnet.h"
#endif /*]*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include "tn3270e.h"

#include "appres.h"

#include "ansic.h"
#include "ctlrc.h"
#include "hostc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "statusc.h"
#include "telnetc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#if !defined(TELOPT_NAWS) /*[*/
#define TELOPT_NAWS	31
#endif /*]*/

#define BUFSZ		4096
#define TRACELINE	72

#define N_OPTS		256

/* Globals */
char    	*hostname = CN;
time_t          ns_time;
int             ns_brcvd;
int             ns_rrcvd;
int             ns_bsent;
int             ns_rsent;
unsigned char  *obuf;		/* 3270 output buffer */
int             obuf_size = 0;
unsigned char  *obptr = (unsigned char *) NULL;
int             linemode = 1;
#if defined(LOCAL_PROCESS) /*[*/
Boolean		local_process = False;
#endif /*]*/
char           *termtype;

/* Externals */
extern struct timeval ds_ts;

/* Statics */
static int      sock = -1;	/* active socket */
static unsigned char myopts[N_OPTS], hisopts[N_OPTS];
			/* telnet option flags */
static unsigned char *ibuf = (unsigned char *) NULL;
			/* 3270 input buffer */
static unsigned char *ibptr;
static int      ibuf_size = 0;	/* size of ibuf */
static unsigned char *obuf_base = (unsigned char *)NULL;
static unsigned char *netrbuf = (unsigned char *)NULL;
			/* network input buffer */
static unsigned char *sbbuf = (unsigned char *)NULL;
			/* telnet sub-option buffer */
static unsigned char *sbptr;
static unsigned char telnet_state;
static int      syncing;
static char     ttype_tmpval[13];

#if defined(X3270_TN3270E) /*[*/
static unsigned long e_funcs;	/* negotiated TN3270E functions */
#define E_OPT(n)	(1 << (n))
static unsigned short e_xmit_seq; /* transmit sequence number */
static int response_required;
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
static int      ansi_data = 0;
static unsigned char *lbuf = (unsigned char *)NULL;
			/* line-mode input buffer */
static unsigned char *lbptr;
static int      lnext = 0;
static int      backslashed = 0;
static int      t_valid = 0;
static char     vintr;
static char     vquit;
static char     verase;
static char     vkill;
static char     veof;
static char     vwerase;
static char     vrprnt;
static char     vlnext;
#endif /*]*/

static int	tn3270e_negotiated = 0;
static enum { E_NONE, E_3270, E_NVT, E_SSCP } tn3270e_submode = E_NONE;
static int	tn3270e_bound = 0;
static char	**lus = (char **)NULL;
static char	**curr_lu = (char **)NULL;
static char	*try_lu = CN;

static int telnet_fsm(unsigned char c);
static void net_rawout(unsigned const char *buf, int len);
static void check_in3270(void);
static void store3270in(unsigned char c);
static void check_linemode(Boolean init);
static int non_blocking(Boolean on);
static void net_connected(void);
#if defined(X3270_TN3270E) /*[*/
static int tn3270e_negotiate(void);
#endif /*]*/
static int process_eor(void);
#if defined(X3270_TN3270E) /*[*/
static const char *tn3270e_function_names(const unsigned char *, int);
static void tn3270e_subneg_send(unsigned char, unsigned long);
static unsigned long tn3270e_fdecode(const unsigned char *, int);
static void tn3270e_ack(void);
static void tn3270e_nak(enum pds);
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
static void do_data(char c);
static void do_intr(char c);
static void do_quit(char c);
static void do_cerase(char c);
static void do_werase(char c);
static void do_kill(char c);
static void do_rprnt(char c);
static void do_eof(char c);
static void do_eol(char c);
static void do_lnext(char c);
static char parse_ctlchar(char *s);
static void cooked_init(void);
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
static void trace_str(const char *s);
static void vtrace_str(const char *fmt, ...);
static const char *cmd(unsigned char c);
static const char *opt(unsigned char c);
static const char *nnn(int c);
#else /*][*/
#define trace_str 0 &&
#define vtrace_str 0 &&
#define cmd(x) 0
#define opt(x) 0
#define nnn(x) 0
#endif /*]*/

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
#if defined(X3270_TN3270E) /*[*/
static unsigned char	functions_req[] = {
	IAC, SB, TELOPT_TN3270E, TN3270E_OP_FUNCTIONS };
#endif /*]*/

const char *telquals[2] = { "IS", "SEND" };
#if defined(X3270_TN3270E) /*[*/
const char *reason_code[8] = { "CONN-PARTNER", "DEVICE-IN-USE", "INV-ASSOCIATE",
	"INV-NAME", "INV-DEVICE-TYPE", "TYPE-NAME-ERROR", "UNKNOWN-ERROR",
	"UNSUPPORTED-REQ" };
#define rsn(n)	(((n) <= TN3270E_REASON_UNSUPPORTED_REQ) ? \
			reason_code[(n)] : "??")
const char *function_name[5] = { "BIND-IMAGE", "DATA-STREAM-CTL", "RESPONSES",
	"SCS-CTL-CODES", "SYSREQ" };
#define fnn(n)	(((n) <= TN3270E_FUNC_SYSREQ) ? \
			function_name[(n)] : "??")
const char *data_type[9] = { "3270-DATA", "SCS-DATA", "RESPONSE", "BIND-IMAGE",
	"UNBIND", "NVT-DATA", "REQUEST", "SSCP-LU-DATA", "PRINT-EOJ" };
#define e_dt(n)	(((n) <= TN3270E_DT_PRINT_EOJ) ? \
			data_type[(n)] : "??")
const char *req_flag[1] = { " ERR-COND-CLEARED" };
#define e_rq(fn, n) (((fn) == TN3270E_DT_REQUEST) ? \
			(((n) <= TN3270E_RQF_ERR_COND_CLEARED) ? \
			req_flag[(n)] : " ??") : "")
const char *hrsp_flag[3] = { "NO-RESPONSE", "ERROR-RESPONSE",
	"ALWAYS-RESPONSE" };
#define e_hrsp(n) (((n) <= TN3270E_RSF_ALWAYS_RESPONSE) ? \
			hrsp_flag[(n)] : "??")
const char *trsp_flag[2] = { "POSITIVE-RESPONSE", "NEGATIVE-RESPONSE" };
#define e_trsp(n) (((n) <= TN3270E_RSF_NEGATIVE_RESPONSE) ? \
			trsp_flag[(n)] : "??")
#define e_rsp(fn, n) (((fn) == TN3270E_DT_RESPONSE) ? e_trsp(n) : e_hrsp(n))
#endif /*]*/


/*
 * net_connect
 *	Establish a telnet socket to the given host passed as an argument.
 *	Called only once and is responsible for setting up the telnet
 *	variables.  Returns the file descriptor of the connected socket.
 */
int
net_connect(const char *host, char *portname, Boolean ls, Boolean *pending)
{
	struct servent	*sp;
	struct hostent	*hp;
	unsigned short		port;
	char	        	passthru_haddr[8];
	int			passthru_len = 0;
	unsigned short		passthru_port = 0;
	struct sockaddr_in	haddr;
	int			on = 1;
#if defined(OMTU) /*[*/
	int			mtu = OMTU;
#endif /*]*/

#	define close_fail	{ (void) close(sock); sock = -1; return -1; }

	if (netrbuf == (unsigned char *)NULL)
		netrbuf = (unsigned char *)Malloc(BUFSZ);

#if defined(X3270_ANSI) /*[*/
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
#endif /*]*/

	*pending = False;

	if (hostname)
		Free(hostname);
	hostname = NewString(host);

	/* get the passthru host and port number */
	if (passthru_host) {
		const char *hn;

		hn = getenv("INTERNET_HOST");
		if (hn == CN)
			hn = "internet-gateway";

		hp = gethostbyname(hn);
		if (hp == (struct hostent *) 0) {
			popup_an_error("Unknown passthru host: %s", hn);
			return -1;
		}
		(void) memmove(passthru_haddr, hp->h_addr, hp->h_length);
		passthru_len = hp->h_length;

		sp = getservbyname("telnet-passthru","tcp");
		if (sp != (struct servent *)NULL)
			passthru_port = sp->s_port;
		else
			passthru_port = htons(3514);
	}

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
	(void) memset((char *) &haddr, 0, sizeof(haddr));
	if (passthru_host) {
		haddr.sin_family = AF_INET;
		(void) memmove(&haddr.sin_addr, passthru_haddr, passthru_len);
		haddr.sin_port = passthru_port;
	} else {
#if defined(LOCAL_PROCESS) /*[*/
		if (ls) {
			local_process = True;
		} else {
			local_process = False;
#endif /*]*/
			hp = gethostbyname(host);
			if (hp == (struct hostent *) 0) {
				haddr.sin_family = AF_INET;
				haddr.sin_addr.s_addr = inet_addr(host);
				if (haddr.sin_addr.s_addr == (unsigned long)-1) {
					popup_an_error("Unknown host:\n%s",
					    hostname);
					return -1;
				}
			}
			else {
				haddr.sin_family = hp->h_addrtype;
				(void) memmove(&haddr.sin_addr, hp->h_addr,
						   hp->h_length);
			}
			haddr.sin_port = port;
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/

	}

#if defined(LOCAL_PROCESS) /*[*/
	if (local_process) {
		int amaster;
		struct winsize w;

		w.ws_row = maxROWS;
		w.ws_col = maxCOLS;
		w.ws_xpixel = 0;
		w.ws_ypixel = 0;

		switch (forkpty(&amaster, NULL, NULL, &w)) {
		    case -1:	/* failed */
			popup_an_errno(errno, "forkpty");
			close_fail;
		    case 0:	/* child */
			putenv("TERM=xterm");
			if (strchr(host, ' ') != CN) {
				(void) execlp("/bin/sh", "sh", "-c", host,
				    NULL);
			} else {
				char *arg1;

				arg1 = strrchr(host, '/');
				(void) execlp(host,
					(arg1 == CN) ? host : arg1 + 1,
					NULL);
			}
			perror(host);
			_exit(1);
			break;
		    default:	/* parent */
			sock = amaster;
			(void) fcntl(sock, F_SETFD, 1);
			net_connected();
			host_in3270(CONNECTED_ANSI);
			break;
		}
	} else {
#endif /*]*/
		/* create the socket */
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			popup_an_errno(errno, "socket");
			return -1;
		}

		/* set options for inline out-of-band data and keepalives */
		if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *)&on,
			    sizeof(on)) < 0) {
			popup_an_errno(errno, "setsockopt(SO_OOBINLINE)");
			close_fail;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
			    sizeof(on)) < 0) {
			popup_an_errno(errno, "setsockopt(SO_KEEPALIVE)");
			close_fail;
		}
#if defined(OMTU) /*[*/
		if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&mtu,
			    sizeof(mtu)) < 0) {
			popup_an_errno(errno, "setsockopt(SO_SNDBUF)");
			close_fail;
		}
#endif /*]*/

		/* set the socket to be non-delaying */
		if (non_blocking(True) < 0)
			close_fail;

		/* don't share the socket with our children */
		(void) fcntl(sock, F_SETFD, 1);

		/* connect */
		if (connect(sock, (struct sockaddr *) &haddr, sizeof(haddr)) == -1) {
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
#if defined(LOCAL_PROCESS) /*[*/
	}
#endif /*]*/

	/* set up temporary termtype */
	if (appres.termname == CN && std_ds_host) {
		(void) sprintf(ttype_tmpval, "IBM-327%c-%d",
		    appres.m3279 ? '9' : '8', model_num);
		termtype = ttype_tmpval;
	}

	/* all done */
	return sock;
}
#undef close_fail

/* Set up the LU list. */
static void
setup_lus(void)
{
	char *lu;
	char *comma;
	int n_lus = 1;
	int i;

	connected_lu = CN;
	connected_type = CN;

	if (!luname[0]) {
		if (lus) {
			Free(lus);
			lus = (char **)NULL;
		}
		curr_lu = (char **)NULL;
		try_lu = CN;
		return;
	}

	/*
	 * Count the commas in the LU name.  That plus one is the
	 * number of LUs to try. 
	 */
	lu = luname;
	while ((comma = strchr(lu, ',')) != CN) {
		n_lus++;
		lu++;
	}

	/*
	 * Allocate enough memory to construct an argv[] array for
	 * the LUs.
	 */
	lus = (char **)Malloc((n_lus+1) * sizeof(char *) + strlen(luname) + 1);

	/* Copy each LU into the array. */
	lu = (char *)(lus + n_lus + 1);
	(void) strcpy(lu, luname);
	i = 0;
	do {
		lus[i++] = lu;
		comma = strchr(lu, ',');
		if (comma != CN) {
			*comma = '\0';
			lu = comma + 1;
		}
	} while (comma != CN);
	lus[i] = CN;
	curr_lu = lus;
	try_lu = *curr_lu;
}

static void
net_connected(void)
{
	vtrace_str("Connected to %s, port %u.\n", hostname, current_port);

	/* set up telnet options */
	(void) memset((char *) myopts, 0, sizeof(myopts));
	(void) memset((char *) hisopts, 0, sizeof(hisopts));
#if defined(X3270_TN3270E) /*[*/
	e_funcs = E_OPT(TN3270E_FUNC_BIND_IMAGE) |
		  E_OPT(TN3270E_FUNC_RESPONSES) |
		  E_OPT(TN3270E_FUNC_SYSREQ);
	e_xmit_seq = 0;
	response_required = TN3270E_RSF_NO_RESPONSE;
#endif /*]*/
	telnet_state = TNS_DATA;
	ibptr = ibuf;

	/* clear statistics and flags */
	(void) time(&ns_time);
	ns_brcvd = 0;
	ns_rrcvd = 0;
	ns_bsent = 0;
	ns_rsent = 0;
	syncing = 0;
	tn3270e_negotiated = 0;
	tn3270e_submode = E_NONE;
	tn3270e_bound = 0;

	setup_lus();

	check_linemode(True);

	/* write out the passthru hostname and port nubmer */
	if (passthru_host) {
		char *buf;

		buf = Malloc(strlen(hostname) + 32);
		(void) sprintf(buf, "%s %d\r\n", hostname, current_port);
		(void) write(sock, buf, strlen(buf));
		Free(buf);
	}
}

/*
 * net_disconnect
 *	Shut down the socket.
 */
void
net_disconnect(void)
{
	if (CONNECTED)
		(void) shutdown(sock, 2);
	(void) close(sock);
	sock = -1;
	trace_str("SENT disconnect\n");

	/* Restore terminal type to its default. */
	if (appres.termname == CN)
		termtype = full_model_name;
}


/*
 * net_input
 *	Called by the toolkit whenever there is input available on the
 *	socket.  Reads the data, processes the special telnet commands
 *	and calls process_ds to process the 3270 data stream.
 */
void
net_input(void)
{
	register unsigned char	*cp;
	int	nr;

#if defined(X3270_ANSI) /*[*/
	ansi_data = 0;
#endif /*]*/

	nr = read(sock, (char *) netrbuf, BUFSZ);
	if (nr < 0) {
		if (errno == EWOULDBLOCK)
			return;
		if (HALF_CONNECTED && errno == EAGAIN) {
			if (non_blocking(False) < 0) {
				host_disconnect(True);
				return;
			}
			host_connected();
			net_connected();
			return;
		}
		vtrace_str("RCVD socket error %d\n", errno);
		if (HALF_CONNECTED)
			popup_an_errno(errno, "Connect to %s, port %d",
			    hostname, current_port);
		else if (errno != ECONNRESET)
			popup_an_errno(errno, "Socket read");
		host_disconnect(True);
		return;
	} else if (nr == 0) {
		/* Host disconnected. */
		trace_str("RCVD disconnect\n");
		host_disconnect(False);
		return;
	}

	/* Process the data. */

	if (HALF_CONNECTED) {
		if (non_blocking(False) < 0) {
			host_disconnect(True);
			return;
		}
		host_connected();
		net_connected();
	}

#if defined(X3270_TRACE) /*[*/
	trace_netdata('<', netrbuf, nr);
#endif /*]*/

	ns_brcvd += nr;
	for (cp = netrbuf; cp < (netrbuf + nr); cp++) {
#if defined(LOCAL_PROCESS) /*[*/
		if (local_process) {
			/* More to do here, probably. */
			if (IN_NEITHER) {	/* now can assume ANSI mode */
				host_in3270(CONNECTED_ANSI);
				hisopts[TELOPT_ECHO] = 1;
				check_linemode(False);
				kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
				status_reset();
				ps_process();
			}
			ansi_process((unsigned int) *cp);
		} else {
#endif /*]*/
			if (telnet_fsm(*cp)) {
				host_disconnect(True);
				return;
			}
#if defined(LOCAL_PROCESS) /*[*/
		}
#endif /*]*/
	}

#if defined(X3270_ANSI) /*[*/
	if (ansi_data) {
		trace_str("\n");
		ansi_data = 0;
	}
#endif /*]*/
}


/*
 * set16
 *	Put a 16-bit value in a buffer.
 *	Returns the number of bytes required.
 */
static int
set16(char *buf, int n)
{
	char *b0 = buf;

	n %= 256 * 256;
	if ((n / 256) == IAC)
		*(unsigned char *)buf++ = IAC;
	*buf++ = (n / 256);
	n %= 256;
	if (n == IAC)
		*(unsigned char *)buf++ = IAC;
	*buf++ = n;
	return buf - b0;
}

/*
 * send_naws
 *	Send a Telnet window size sub-option negotation.
 */
static void
send_naws(void)
{
	char naws_msg[14];
	int naws_len = 0;

	(void) sprintf(naws_msg, "%c%c%c", IAC, SB, TELOPT_NAWS);
	naws_len += 3;
	naws_len += set16(naws_msg + naws_len, maxCOLS);
	naws_len += set16(naws_msg + naws_len, maxROWS);
	(void) sprintf(naws_msg + naws_len, "%c%c", IAC, SE);
	naws_len += 2;
	net_rawout((unsigned char *)naws_msg, naws_len);
	vtrace_str("SENT %s NAWS %d %d %s\n", cmd(SB), maxCOLS,
	    maxROWS, cmd(SE));
}



/* Advance 'try_lu' to the next desired LU name. */
static void
next_lu(void)
{
	if (curr_lu != (char **)NULL && (try_lu = *++curr_lu) == CN)
		curr_lu = (char **)NULL;
}

/*
 * telnet_fsm
 *	Telnet finite-state machine.
 *	Returns 0 for okay, -1 for errors.
 */
static int
telnet_fsm(unsigned char c)
{
	char	*see_chr;
	int	sl;

	switch (telnet_state) {
	    case TNS_DATA:	/* normal data processing */
		if (c == IAC) {	/* got a telnet command */
			telnet_state = TNS_IAC;
#if defined(X3270_ANSI) /*[*/
			if (ansi_data) {
				trace_str("\n");
				ansi_data = 0;
			}
#endif /*]*/
			break;
		}
		if (IN_NEITHER) {	/* now can assume ANSI mode */
			host_in3270(CONNECTED_ANSI);
#if defined(X3270_ANSI)/*[*/
			if (linemode)
				cooked_init();
#endif /*]*/
			kybdlock_clr(KL_AWAITING_FIRST, "telnet_fsm");
			status_reset();
			ps_process();
		}
		if (IN_ANSI && !IN_E) {
#if defined(X3270_ANSI) /*[*/
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
				sms_store(c);
			}
#endif /*]*/
		} else {
			store3270in(c);
		}
		break;
	    case TNS_IAC:	/* process a telnet command */
		if (c != EOR && c != IAC) {
			vtrace_str("RCVD %s ", cmd(c));
		}
		switch (c) {
		    case IAC:	/* escaped IAC, insert it */
			if (IN_ANSI && !IN_E) {
#if defined(X3270_ANSI) /*[*/
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
				sms_store(c);
#endif /*]*/
			} else
				store3270in(c);
			telnet_state = TNS_DATA;
			break;
		    case EOR:	/* eor, process accumulated input */
			if (IN_3270 || (IN_E && tn3270e_negotiated)) {
				ns_rrcvd++;
				if (process_eor())
					return -1;
			} else
				Warning("EOR received when not in 3270 mode, "
				    "ignored.");
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
			if (sbbuf == (unsigned char *)NULL)
				sbbuf = (unsigned char *)Malloc(1024);
			sbptr = sbbuf;
			break;
		    case DM:
			trace_str("\n");
			if (syncing) {
				syncing = 0;
				x_except_on(sock);
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
		vtrace_str("%s\n", opt(c));
		switch (c) {
		    case TELOPT_SGA:
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_ECHO:
#if defined(X3270_TN3270E) /*[*/
		    case TELOPT_TN3270E:
#endif /*]*/
			if (c != TELOPT_TN3270E || !non_tn3270e_host) {
				if (!hisopts[c]) {
					hisopts[c] = 1;
					do_opt[2] = c;
					net_rawout(do_opt, sizeof(do_opt));
					vtrace_str("SENT %s %s\n", cmd(DO),
						opt(c));

					/*
					 * For UTS, volunteer to do EOR when
					 * they do.
					 */
					if (c == TELOPT_EOR && !myopts[c]) {
						myopts[c] = 1;
						will_opt[2] = c;
						net_rawout(will_opt,
							sizeof(will_opt));
						vtrace_str("SENT %s %s\n",
							cmd(WILL), opt(c));
					}

					check_in3270();
					check_linemode(False);
				}
				break;
			}
		    default:
			dont_opt[2] = c;
			net_rawout(dont_opt, sizeof(dont_opt));
			vtrace_str("SENT %s %s\n", cmd(DONT), opt(c));
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_WONT:	/* telnet WONT DO OPTION command */
		vtrace_str("%s\n", opt(c));
		if (hisopts[c]) {
			hisopts[c] = 0;
			dont_opt[2] = c;
			net_rawout(dont_opt, sizeof(dont_opt));
			vtrace_str("SENT %s %s\n", cmd(DONT), opt(c));
			check_in3270();
			check_linemode(False);
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_DO:	/* telnet PLEASE DO OPTION command */
		vtrace_str("%s\n", opt(c));
		switch (c) {
		    case TELOPT_BINARY:
		    case TELOPT_EOR:
		    case TELOPT_TTYPE:
		    case TELOPT_SGA:
		    case TELOPT_NAWS:
		    case TELOPT_TM:
#if defined(X3270_TN3270E) /*[*/
		    case TELOPT_TN3270E:
#endif /*]*/
			if (c != TELOPT_TN3270E || !non_tn3270e_host) {
				if (!myopts[c]) {
					if (c != TELOPT_TM)
						myopts[c] = 1;
					will_opt[2] = c;
					net_rawout(will_opt, sizeof(will_opt));
					vtrace_str("SENT %s %s\n", cmd(WILL),
						opt(c));
					check_in3270();
					check_linemode(False);
				}
				if (c == TELOPT_NAWS)
					send_naws();
				break;
			}
		    default:
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			vtrace_str("SENT %s %s\n", cmd(WONT), opt(c));
			break;
		}
		telnet_state = TNS_DATA;
		break;
	    case TNS_DONT:	/* telnet PLEASE DON'T DO OPTION command */
		vtrace_str("%s\n", opt(c));
		if (myopts[c]) {
			myopts[c] = 0;
			wont_opt[2] = c;
			net_rawout(wont_opt, sizeof(wont_opt));
			vtrace_str("SENT %s %s\n", cmd(WONT), opt(c));
			check_in3270();
			check_linemode(False);
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
		*sbptr++ = c;
		if (c == SE) {
			telnet_state = TNS_DATA;
			if (sbbuf[0] == TELOPT_TTYPE &&
			    sbbuf[1] == TELQUAL_SEND) {
				int tt_len, tb_len;
				char *tt_out;

				vtrace_str("%s %s\n", opt(sbbuf[0]),
				    telquals[sbbuf[1]]);
				if (lus != (char **)NULL && try_lu == CN) {
					/* None of the LUs worked. */
					popup_an_error("Cannot connect to "
						"specified LU");
					return -1;
				}

				tt_len = strlen(termtype);
				if (try_lu != CN && *try_lu) {
					tt_len += strlen(try_lu) + 1;
					connected_lu = try_lu;
				} else
					connected_lu = CN;

				tb_len = 4 + tt_len + 2;
				tt_out = Malloc(tb_len + 1);
				(void) sprintf(tt_out, "%c%c%c%c%s%s%s%c%c",
				    IAC, SB, TELOPT_TTYPE, TELQUAL_IS,
				    termtype,
				    (try_lu != CN && *try_lu) ? "@" : "",
				    (try_lu != CN && *try_lu) ? try_lu : "",
				    IAC, SE);
				net_rawout((unsigned char *)tt_out, tb_len);

				vtrace_str("SENT %s %s %s %.*s %s\n",
				    cmd(SB), opt(TELOPT_TTYPE),
				    telquals[TELQUAL_IS],
				    tt_len, tt_out + 4,
				    cmd(SE));
				Free(tt_out);

				/* Advance to the next LU name. */
				next_lu();
			}
#if defined(X3270_TN3270E) /*[*/
			else if (myopts[TELOPT_TN3270E] &&
				   sbbuf[0] == TELOPT_TN3270E) {
				if (tn3270e_negotiate())
					return -1;
			}
#endif /*]*/
		} else {
			telnet_state = TNS_SB;
		}
		break;
	}
	return 0;
}

#if defined(X3270_TN3270E) /*[*/
/* Send a TN3270E terminal type request. */
static void
tn3270e_request(void)
{
	int tt_len, tb_len;
	char *tt_out;
	char *t;

	tt_len = strlen(termtype);
	if (try_lu != CN && *try_lu)
		tt_len += strlen(try_lu) + 1;

	tb_len = 5 + tt_len + 2;
	tt_out = Malloc(tb_len + 1);
	t = tt_out;
	t += sprintf(tt_out, "%c%c%c%c%c%s",
	    IAC, SB, TELOPT_TN3270E, TN3270E_OP_DEVICE_TYPE,
	    TN3270E_OP_REQUEST, termtype);

	/* Convert 3279 to 3278, per the RFC. */
	if (tt_out[12] == '9')
		tt_out[12] = '8';

	if (try_lu != CN && *try_lu)
		t += sprintf(t, "%c%s", TN3270E_OP_CONNECT, try_lu);

	(void) sprintf(t, "%c%c", IAC, SE);

	net_rawout((unsigned char *)tt_out, tb_len);

	vtrace_str("SENT %s %s DEVICE-TYPE REQUEST %.*s%s%s "
		   "%s\n",
	    cmd(SB), opt(TELOPT_TN3270E), strlen(termtype), tt_out + 5,
	    (try_lu != CN && *try_lu) ? " CONNECT " : "",
	    (try_lu != CN && *try_lu) ? try_lu : "",
	    cmd(SE));

	Free(tt_out);
}

/*
 * Negotiation of TN3270E options.
 * Returns 0 if okay, -1 if we have to give up altogether.
 */
static int
tn3270e_negotiate(void)
{
#define LU_MAX	32
	static char reported_lu[LU_MAX+1];
	static char reported_type[LU_MAX+1];
	int sblen;
	unsigned long e_rcvd;

	/* Find out how long the subnegotiation buffer is. */
	for (sblen = 0; ; sblen++) {
		if (sbbuf[sblen] == SE)
			break;
	}

	vtrace_str("TN3270E ");

	switch (sbbuf[1]) {

	case TN3270E_OP_SEND:

		if (sbbuf[2] == TN3270E_OP_DEVICE_TYPE) {

			/* Host wants us to send our device type. */
			vtrace_str("SEND DEVICE-TYPE SE\n");

			tn3270e_request();
		} else {
			vtrace_str("SEND ??%u SE\n", sbbuf[2]);
		}
		break;

	case TN3270E_OP_DEVICE_TYPE:

		/* Device type negotiation. */
		vtrace_str("DEVICE-TYPE ");

		switch (sbbuf[2]) {
		case TN3270E_OP_IS: {
			int tnlen, snlen;

			/* Device type success. */

			/* Isolate the terminal type and session. */
			tnlen = 0;
			while (sbbuf[3+tnlen] != SE &&
			       sbbuf[3+tnlen] != TN3270E_OP_CONNECT)
				tnlen++;
			snlen = 0;
			if (sbbuf[3+tnlen] == TN3270E_OP_CONNECT) {
				while(sbbuf[3+tnlen+1+snlen] != SE)
					snlen++;
			}
			vtrace_str("IS %.*s CONNECT %.*s SE\n",
				tnlen, &sbbuf[3],
				snlen, &sbbuf[3+tnlen+1]);

			/* Remember the LU. */
			if (tnlen) {
				if (tnlen > LU_MAX)
					tnlen = LU_MAX;
				(void)strncpy(reported_type,
				    (char *)&sbbuf[3], tnlen);
				reported_type[tnlen] = '\0';
				connected_type = reported_type;
			}
			if (snlen) {
				if (snlen > LU_MAX)
					snlen = LU_MAX;
				(void)strncpy(reported_lu,
				    (char *)&sbbuf[3+tnlen+1], snlen);
				reported_lu[snlen] = '\0';
				connected_lu = reported_lu;
			}

			/* Tell them what we can do. */
			tn3270e_subneg_send(TN3270E_OP_REQUEST, e_funcs);
			break;
		}
		case TN3270E_OP_REJECT:

			/* Device type failure. */

			vtrace_str("REJECT REASON %s SE\n", rsn(sbbuf[4]));

			next_lu();
			if (try_lu != CN) {
				/* Try the next LU. */
				tn3270e_request();
			} else if (lus != (char **)NULL) {
				/* No more LUs to try.  Give up. */
				popup_an_error("Cannot connect to "
					"specified LU:\n%s", rsn(sbbuf[4]));
				return -1;
			} else {
				popup_an_error("Device type rejected:\n"
					"%s", rsn(sbbuf[4]));
				return -1;
			}

			break;
		default:
			vtrace_str("??%u SE\n", sbbuf[2]);
			break;
		}
		break;

	case TN3270E_OP_FUNCTIONS:

		/* Functions negotiation. */
		vtrace_str("FUNCTIONS ");

		switch (sbbuf[2]) {

		case TN3270E_OP_REQUEST:

			/* Host is telling us what functions they want. */
			vtrace_str("REQUEST %s SE\n",
			    tn3270e_function_names(sbbuf+3, sblen-3));

			e_rcvd = tn3270e_fdecode(sbbuf+3, sblen-3);
			if ((e_rcvd == e_funcs) || (e_funcs & ~e_rcvd)) {
				/* They want what we want, or less.  Done. */
				e_funcs = e_rcvd;
				tn3270e_subneg_send(TN3270E_OP_IS, e_funcs);
				tn3270e_negotiated = 1;
				check_in3270();
			} else {
				/*
				 * They want us to do something we can't.
				 * Request the common subset.
				 */
				e_funcs &= e_rcvd;
				tn3270e_subneg_send(TN3270E_OP_REQUEST,
				    e_funcs);
			}
			break;

		case TN3270E_OP_IS:

			/* They accept our last request, or a subset thereof. */
			vtrace_str("IS %s SE\n",
			    tn3270e_function_names(sbbuf+3, sblen-3));
			e_rcvd = tn3270e_fdecode(sbbuf+3, sblen-3);
			if (e_rcvd != e_funcs) {
				if (e_funcs & ~e_rcvd) {
					/*
					 * They've removed something.  This is
					 * technically illegal, but we can
					 * live with it.
					 */
					e_funcs = e_rcvd;
				} else {
					/*
					 * They've added something.  Abandon
					 * TN3270E, they're brain dead.
					 */
					vtrace_str("Host illegally added "
						"function(s), aborting "
						"TN3270E\n");
					wont_opt[2] = TELOPT_TN3270E;
					net_rawout(wont_opt, sizeof(wont_opt));
					vtrace_str("SENT %s %s\n", cmd(WONT),
						opt(TELOPT_TN3270E));
					myopts[TELOPT_TN3270E] = 0;
					check_in3270();
					break;
				}
			}
			tn3270e_negotiated = 1;
			check_in3270();
			break;

		default:
			vtrace_str("??%u SE\n", sbbuf[2]);
			break;
		}
		break;

	default:
		vtrace_str("??%u SE\n", sbbuf[1]);
	}

	/* Good enough for now. */
	return 0;
}

/* Expand a string of TN3270E function codes into text. */
static const char *
tn3270e_function_names(const unsigned char *buf, int len)
{
	int i;
	static char text_buf[1024];
	char *s = text_buf;

	if (!len)
		return("(null)");
	for (i = 0; i < len; i++) {
		s += sprintf(s, "%s%s", (s == text_buf) ? "" : " ",
		    fnn(buf[i]));
	}
	return text_buf;
}

/* Expand the current TN3270E function codes into text. */
const char *
tn3270e_current_opts(void)
{
	int i;
	static char text_buf[1024];
	char *s = text_buf;

	if (!e_funcs || !IN_E)
		return CN;
	for (i = 0; i < 32; i++) {
		if (e_funcs & E_OPT(i))
		s += sprintf(s, "%s%s", (s == text_buf) ? "" : " ",
		    fnn(i));
	}
	return text_buf;
}

/* Transmit a TN3270E FUNCTIONS REQUEST message. */
static void
tn3270e_subneg_send(unsigned char op, unsigned long funcs)
{
	unsigned char proto_buf[7 + 32];
	int proto_len;
	int i;

	/* Construct the buffers. */
	(void) memcpy(proto_buf, functions_req, 4);
	proto_buf[4] = op;
	proto_len = 5;
	for (i = 0; i < 32; i++) {
		if (funcs & E_OPT(i))
			proto_buf[proto_len++] = i;
	}

	/* Complete and send out the protocol message. */
	proto_buf[proto_len++] = IAC;
	proto_buf[proto_len++] = SE;
	net_rawout(proto_buf, proto_len);

	/* Complete and send out the trace text. */
	vtrace_str("SENT %s %s FUNCTIONS REQUEST %s %s\n",
	    cmd(SB), opt(TELOPT_TN3270E),
	    tn3270e_function_names(proto_buf + 5, proto_len - 7),
	    cmd(SE));
}

/* Translate a string of TN3270E functions into a bit-map. */
static unsigned long
tn3270e_fdecode(const unsigned char *buf, int len)
{
	unsigned long r = 0L;
	int i;

	/* Note that this code silently ignores options >= 32. */
	for (i = 0; i < len; i++) {
		if (buf[i] < 32)
			r |= E_OPT(buf[i]);
	}
	return r;
}
#endif /*]*/

static int
process_eor(void)
{
	if (syncing || !(ibptr - ibuf))
		return(0);

#if defined(X3270_TN3270E) /*[*/
	if (IN_E) {
		tn3270e_header *h = (tn3270e_header *)ibuf;
		unsigned char *s;
		enum pds rv;

		vtrace_str("RCVD TN3270E(%s%s %s %u)\n",
		    e_dt(h->data_type),
		    e_rq(h->data_type, h->request_flag),
		    e_rsp(h->data_type, h->response_flag),
		    h->seq_number[0] << 8 | h->seq_number[1]);

		switch (h->data_type) {
		case TN3270E_DT_3270_DATA:
			if ((e_funcs & E_OPT(TN3270E_FUNC_BIND_IMAGE)) &&
			    !tn3270e_bound)
				return 0;
			tn3270e_submode = E_3270;
			check_in3270();
			response_required = h->response_flag;
			rv = process_ds(ibuf + EH_SIZE,
			    (ibptr - ibuf) - EH_SIZE);
			if (rv < 0 &&
			    response_required != TN3270E_RSF_NO_RESPONSE)
				tn3270e_nak(rv);
			else if (rv == PDS_OKAY_NO_OUTPUT &&
			    response_required == TN3270E_RSF_ALWAYS_RESPONSE)
				tn3270e_ack();
			response_required = TN3270E_RSF_NO_RESPONSE;
			return 0;
		case TN3270E_DT_BIND_IMAGE:
			if (!(e_funcs & E_OPT(TN3270E_FUNC_BIND_IMAGE)))
				return 0;
			tn3270e_bound = 1;
			check_in3270();
			return 0;
		case TN3270E_DT_UNBIND:
			if (!(e_funcs & E_OPT(TN3270E_FUNC_BIND_IMAGE)))
				return 0;
			tn3270e_bound = 0;
			if (tn3270e_submode == E_3270)
				tn3270e_submode = E_NONE;
			check_in3270();
			return 0;
		case TN3270E_DT_NVT_DATA:
			/* In tn3270e NVT mode */
			tn3270e_submode = E_NVT;
			check_in3270();
			for (s = ibuf; s < ibptr; s++) {
				ansi_process(*s++);
			}
			return 0;
		case TN3270E_DT_SSCP_LU_DATA:
			if (!(e_funcs & E_OPT(TN3270E_FUNC_BIND_IMAGE)))
				return 0;
			tn3270e_submode = E_SSCP;
			check_in3270();
			ctlr_write_sscp_lu(ibuf + EH_SIZE,
			                   (ibptr - ibuf) - EH_SIZE);
			return 0;
		default:
			/* Should do something more extraordinary here. */
			return 0;
		}
	} else
#endif /*]*/
	{
		(void) process_ds(ibuf, ibptr - ibuf);
	}
	return 0;
}


/*
 * net_exception
 *	Called when there is an exceptional condition on the socket.
 */
void
net_exception(void)
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
net_rawout(unsigned const char *buf, int len)
{
	int	nw;

#if defined(X3270_TRACE) /*[*/
	trace_netdata('>', buf, len);
#endif /*]*/

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
		nw = write(sock, (const char *) buf, n2w);
		if (nw < 0) {
			vtrace_str("RCVD socket error %d\n", errno);
			if (errno == EPIPE || errno == ECONNRESET) {
				host_disconnect(False);
				return;
			} else if (errno == EINTR) {
				goto bot;
			} else {
				popup_an_errno(errno, "Socket write");
				host_disconnect(True);
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


#if defined(X3270_ANSI) /*[*/
/*
 * net_hexansi_out
 *	Send uncontrolled user data to the host in ANSI mode, performing IAC
 *	and CR quoting as necessary.
 */
void
net_hexansi_out(unsigned char *buf, int len)
{
	unsigned char *tbuf;
	unsigned char *xbuf;

	if (!len)
		return;

#if defined(X3270_TRACE) /*[*/
	/* Trace the data. */
	if (toggled(DS_TRACE)) {
		int i;

		(void) fprintf(tracef, ">");
		for (i = 0; i < len; i++)
			(void) fprintf(tracef, " %s", ctl_see((int) *(buf+i)));
		(void) fprintf(tracef, "\n");
	}
#endif /*]*/

	/* Expand it. */
	tbuf = xbuf = (unsigned char *)Malloc(2*len);
	while (len) {
		unsigned char c = *buf++;

		*tbuf++ = c;
		len--;
		if (c == IAC)
			*tbuf++ = IAC;
		else if (c == '\r' && (!len || *buf != '\n'))
			*tbuf++ = '\0';
	}

	/* Send it to the host. */
	net_rawout(xbuf, tbuf - xbuf);
	Free((XtPointer)xbuf);
}

/*
 * net_cookedout
 *	Send user data out in ANSI mode, without cooked-mode processing.
 */
static void
net_cookedout(const char *buf, int len)
{
#if defined(X3270_TRACE) /*[*/
	if (toggled(DS_TRACE)) {
		int i;

		(void) fprintf(tracef, ">");
		for (i = 0; i < len; i++)
			(void) fprintf(tracef, " %s", ctl_see((int) *(buf+i)));
		(void) fprintf(tracef, "\n");
	}
#endif /*]*/
	net_rawout((unsigned const char *) buf, len);
}


/*
 * net_cookout
 *	Send output in ANSI mode, including cooked-mode processing if
 *	appropriate.
 */
static void
net_cookout(const char *buf, int len)
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
cooked_init(void)
{
	if (lbuf == (unsigned char *)NULL)
		lbuf = (unsigned char *)Malloc(BUFSZ);
	lbptr = lbuf;
	lnext = 0;
	backslashed = 0;
}

static void
ansi_process_s(const char *data)
{
	while (*data)
		ansi_process((unsigned int) *data++);
}

static void
forward_data(void)
{
	net_cookedout((char *) lbuf, lbptr - lbuf);
	cooked_init();
}

static void
do_data(char c)
{
	if (lbptr+1 < lbuf + BUFSZ) {
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
do_intr(char c)
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
do_quit(char c)
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
do_cerase(char c)
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
do_werase(char c)
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
do_kill(char c)
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
do_rprnt(char c)
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
do_eof(char c)
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
do_eol(char c)
{
	if (lnext) {
		do_data(c);
		return;
	}
	if (lbptr+2 >= lbuf + BUFSZ) {
		ansi_process_s("\007");
		return;
	}
	*lbptr++ = '\r';
	*lbptr++ = '\n';
	ansi_process_s("\r\n");
	forward_data();
}

static void
do_lnext(char c)
{
	if (lnext) {
		do_data(c);
		return;
	}
	lnext = 1;
	ansi_process_s("^\b");
}
#endif /*]*/



/*
 * check_in3270
 *	Check for switches between NVT, SSCP-LU and 3270 modes.
 */
static void
check_in3270(void)
{
	enum cstate new_cstate = NOT_CONNECTED;
	static const char *state_name[] = {
		"unconnected",
		"pending",
		"connected initial",
		"TN3270 NVT",
		"TN3270 3270",
		"TN3270E",
		"TN3270E NVT",
		"TN3270E SSCP-LU",
		"TN3270E 3270"
	};

#if defined(X3270_TN3270E) /*[*/
	if (myopts[TELOPT_TN3270E]) {
		if (!tn3270e_negotiated)
			new_cstate = CONNECTED_INITIAL_E;
		else switch (tn3270e_submode) {
		case E_NONE:
			new_cstate = CONNECTED_INITIAL_E;
			break;
		case E_NVT:
			new_cstate = CONNECTED_NVT;
			break;
		case E_3270:
			new_cstate = CONNECTED_TN3270E;
			break;
		case E_SSCP:
			new_cstate = CONNECTED_SSCP;
			break;
		}
	} else
#endif /*]*/
	if (myopts[TELOPT_BINARY] &&
	           myopts[TELOPT_EOR] &&
	           myopts[TELOPT_TTYPE] &&
	           hisopts[TELOPT_BINARY] &&
	           hisopts[TELOPT_EOR]) {
		new_cstate = CONNECTED_3270;
	} else if (cstate == CONNECTED_INITIAL) {
		/* Nothing has happened, yet. */
		return;
	} else {
		new_cstate = CONNECTED_ANSI;
	}

	if (new_cstate != cstate) {
#if defined(X3270_TN3270E) /*[*/
		int was_in_e = IN_E;
#endif /*]*/

		vtrace_str("Now operating in %s mode.\n",
			state_name[new_cstate]);
		host_in3270(new_cstate);

#if defined(X3270_TN3270E) /*[*/
		/*
		 * If we've now switched between non-TN3270E mode and
		 * TN3270E mode, reset the LU list so we can try again
		 * in the new mode.
		 */
		if (lus != (char **)NULL && was_in_e != IN_E) {
			curr_lu = lus;
			try_lu = *curr_lu;
		}
#endif /*]*/

		/* Allocate the initial 3270 input buffer. */
		if (new_cstate >= CONNECTED_INITIAL && !ibuf_size) {
			ibuf = (unsigned char *)Malloc(BUFSIZ);
			ibuf_size = BUFSIZ;
			ibptr = ibuf;
		}

#if defined(X3270_ANSI) /*[*/
		/* Reinitialize line mode. */
		if ((new_cstate == CONNECTED_ANSI && linemode) ||
		    new_cstate == CONNECTED_NVT)
			cooked_init();
#endif /*]*/

#if defined(X3270_TN3270E) /*[*/
		/* If we fell out of TN3270E, remove the state. */
		if (!myopts[TELOPT_TN3270E]) {
			tn3270e_negotiated = 0;
			tn3270e_submode = E_NONE;
			tn3270e_bound = 0;
		}
#endif /*]*/
	}
}

/*
 * store3270in
 *	Store a character in the 3270 input buffer, checking for buffer
 *	overflow and reallocating ibuf if necessary.
 */
static void
store3270in(unsigned char c)
{
	if (ibptr - ibuf >= ibuf_size) {
		ibuf_size += BUFSIZ;
		ibuf = (unsigned char *)Realloc((char *)ibuf, ibuf_size);
		ibptr = ibuf + ibuf_size - BUFSIZ;
	}
	*ibptr++ = c;
}

/*
 * space3270out
 *	Ensure that <n> more characters will fit in the 3270 output buffer.
 *	Allocates the buffer in BUFSIZ chunks.
 *	Allocates hidden space at the front of the buffer for TN3270E.
 */
void
space3270out(int n)
{
	unsigned nc = 0;	/* amount of data currently in obuf */
	unsigned more = 0;

	if (obuf_size)
		nc = obptr - obuf;

	while ((nc + n + EH_SIZE) > (obuf_size + more)) {
		more += BUFSIZ;
	}

	if (more) {
		obuf_size += more;
		obuf_base = (unsigned char *)Realloc((char *)obuf_base,
			obuf_size);
		obuf = obuf_base + EH_SIZE;
		obptr = obuf + nc;
	}
}


/*
 * check_linemode
 *	Set the global variable 'linemode', which says whether we are in
 *	character-by-character mode or line mode.
 */
static void
check_linemode(Boolean init)
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
		st_changed(ST_LINE_MODE, linemode);
		if (!init) {
			vtrace_str("Operating in %s mode.\n",
			    linemode ? "line" : "character-at-a-time");
		}
#if defined(X3270_ANSI) /*[*/
		if (IN_ANSI && linemode)
			cooked_init();
#endif /*]*/
	}
}


#if defined(X3270_TRACE) /*[*/

/*
 * nnn
 *	Expands a number to a character string, for displaying unknown telnet
 *	commands and options.
 */
static const char *
nnn(int c)
{
	static char	buf[64];

	(void) sprintf(buf, "%d", c);
	return buf;
}

#if !defined(TELCMD) || !defined(TELCMD_OK) /*[*/
#undef TELCMD
#define TELCMD(x)	telcmds[(x) - SE]
#undef TELCMD_OK
#define TELCMD_OK(x)	((x) >= SE && (x) <= IAC)
#endif /*]*/

/*
 * cmd
 *	Expands a TELNET command into a character string.
 */
static const char *
cmd(unsigned char c)
{
	if (TELCMD_OK(c))
		return TELCMD(c);
	else
		return nnn((int)c);
}

#if !defined(TELOPT) || !defined(TELOPT_OK) /*[*/
#undef TELOPT
#define TELOPT(x)	telopts[(x)]
#undef TELOPT_OK
#define TELOPT_OK(x)	((x) >= TELOPT_BINARY && (x) <= TELOPT_EOR)
#endif /*]*/

/*
 * opt
 *	Expands a TELNET option into a character string.
 */
static const char *
opt(unsigned char c)
{
	if (TELOPT_OK(c))
		return TELOPT(c);
	else if (c == TELOPT_TN3270E)
		return "TN3270E";
	else
		return nnn((int)c);
}


#define LINEDUMP_MAX	32

void
trace_netdata(char direction, unsigned const char *buf, int len)
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
#endif /*]*/


/*
 * net_output
 *	Send 3270 output over the network, prepending TN3270E headers and
 *	tacking on the necessary telnet end-of-record command.
 */
void
net_output(void)
{
#if defined(X3270_TN3270E) /*[*/
#define BSTART	((IN_TN3270E || IN_SSCP) ? obuf_base : obuf)
#else /*][*/
#define BSTART	obuf
#endif /*]*/

#if defined(X3270_TN3270E) /*[*/
	/* Set the TN3720E header. */
	if (IN_TN3270E || IN_SSCP) {
		tn3270e_header *h = (tn3270e_header *)obuf_base;

		/* Check for sending a TN3270E response. */
		if (response_required == TN3270E_RSF_ALWAYS_RESPONSE) {
			tn3270e_ack();
			response_required = TN3270E_RSF_NO_RESPONSE;
		}

		/* Set the outbound TN3270E header. */
		h->data_type = IN_TN3270E ?
			TN3270E_DT_3270_DATA : TN3270E_DT_SSCP_LU_DATA;
		h->request_flag = 0;
		h->response_flag = 0;
		h->seq_number[0] = (e_xmit_seq >> 8) & 0xff;
		h->seq_number[1] = e_xmit_seq & 0xff;
	}
#endif /*]*/

	/* Count the number of IACs in the message. */
	{
		char *buf = (char *)BSTART;
		int len = obptr - BSTART;
		char *iac;
		int cnt = 0;

		while (len && (iac = memchr(buf, IAC, len)) != CN) {
			cnt++;
			len -= iac - buf + 1;
			buf = iac + 1;
		}
		if (cnt) {
			space3270out(cnt);
			len = obptr - BSTART;
			buf = (char *)BSTART;

			/* Now quote them. */
			while (len && (iac = memchr(buf, IAC, len)) != CN) {
				int ml = len - (iac - buf);

				(void) memmove(iac + 1, iac, ml);
				len -= iac - buf + 1;
				buf = iac + 2;
				obptr++;
			}
		}
	}

	/* Add IAC EOR to the end and send it. */
	space3270out(2);
	*obptr++ = IAC;
	*obptr++ = EOR;
#if defined(X3270_TN3270E) /*[*/
	if (IN_TN3270E || IN_SSCP) {
		vtrace_str("SENT TN3270E(%s NO-RESPONSE %u)\n",
			IN_TN3270E ? "3270-DATA" : "SSCP-LU-DATA", e_xmit_seq);
		if (e_funcs & E_OPT(TN3270E_FUNC_RESPONSES))
			e_xmit_seq = (e_xmit_seq + 1) & 0x7fff;
	}
#endif /*]*/
	net_rawout(BSTART, obptr - BSTART);

	trace_str("SENT EOR\n");
	ns_rsent++;
#undef BSTART
}

#if defined(X3270_TN3270E) /*[*/
/* Send a TN3270E positive response to the server. */
static void
tn3270e_ack(void)
{
	unsigned char rsp_buf[9];
	tn3270e_header *h, *h_in;
	int rsp_len = EH_SIZE;

	h = (tn3270e_header *)rsp_buf;
	h_in = (tn3270e_header *)ibuf;

	h->data_type = TN3270E_DT_RESPONSE;
	h->request_flag = 0;
	h->response_flag = TN3270E_RSF_POSITIVE_RESPONSE;
	h->seq_number[0] = h_in->seq_number[0];
	h->seq_number[1] = h_in->seq_number[1];
	if (h->seq_number[1] == IAC)
		rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = TN3270E_POS_DEVICE_END;
	rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = EOR;
	vtrace_str("SENT TN3270E(RESPONSE POSITIVE-RESPONSE "
		"%u) DEVICE-END\n",
		h_in->seq_number[0] << 8 | h_in->seq_number[1]);
	net_rawout(rsp_buf, rsp_len);
}

/* Send a TN3270E negative response to the server. */
static void
tn3270e_nak(enum pds rv unused)
{
	unsigned char rsp_buf[9];
	tn3270e_header *h, *h_in;
	int rsp_len = EH_SIZE;

	h = (tn3270e_header *)rsp_buf;
	h_in = (tn3270e_header *)ibuf;

	h->data_type = TN3270E_DT_RESPONSE;
	h->request_flag = 0;
	h->response_flag = TN3270E_RSF_NEGATIVE_RESPONSE;
	h->seq_number[0] = h_in->seq_number[0];
	h->seq_number[1] = h_in->seq_number[1];
	if (h->seq_number[1] == IAC)
		rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = TN3270E_NEG_COMMAND_REJECT;
	rsp_buf[rsp_len++] = IAC;
	rsp_buf[rsp_len++] = EOR;
	vtrace_str("SENT TN3270E(RESPONSE NEGATIVE-RESPONSE %u) "
		"COMMAND-REJECT\n",
		h_in->seq_number[0] << 8 | h_in->seq_number[1]);
	net_rawout(rsp_buf, rsp_len);
}

#if defined(X3270_TRACE) /*[*/
/* Add a dummy TN3270E header to the output buffer. */
Boolean
net_add_dummy_tn3270e(void)
{
	tn3270e_header *h;

	if (!IN_E || tn3270e_submode == E_NONE)
		return False;

	space3270out(EH_SIZE);
	h = (tn3270e_header *)obptr;

	switch (tn3270e_submode) {
	case E_NONE:
		break;
	case E_NVT:
		h->data_type = TN3270E_DT_NVT_DATA;
		break;
	case E_SSCP:
		h->data_type = TN3270E_DT_SSCP_LU_DATA;
		break;
	case E_3270:
		h->data_type = TN3270E_DT_3270_DATA;
		break;
	}
	h->request_flag = 0;
	h->response_flag = TN3270E_RSF_NO_RESPONSE;
	h->seq_number[0] = 0;
	h->seq_number[1] = 0;
	obptr += EH_SIZE;
	return True;
}
#endif /*]*/
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
/*
 * Add IAC EOR to a buffer.
 */
void
net_add_eor(unsigned char *buf, int len)
{
	buf[len++] = IAC;
	buf[len++] = EOR;
}
#endif /*]*/


#if defined(X3270_ANSI) /*[*/
/*
 * net_sendc
 *	Send a character of user data over the network in ANSI mode.
 */
void
net_sendc(char c)
{
	if (c == '\r' && !linemode
#if defined(LOCAL_PROCESS) /*[*/
				   && !local_process
#endif /*]*/
						    ) {
		/* CR must be quoted */
		net_cookout("\r\0", 2);
	} else {
		net_cookout(&c, 1);
	}
}


/*
 * net_sends
 *	Send a null-terminated string of user data in ANSI mode.
 */
void
net_sends(const char *s)
{
	net_cookout(s, strlen(s));
}


/*
 * net_send_erase
 *	Sends the KILL character in ANSI mode.
 */
void
net_send_erase(void)
{
	net_cookout(&verase, 1);
}


/*
 * net_send_kill
 *	Sends the KILL character in ANSI mode.
 */
void
net_send_kill(void)
{
	net_cookout(&vkill, 1);
}


/*
 * net_send_werase
 *	Sends the WERASE character in ANSI mode.
 */
void
net_send_werase(void)
{
	net_cookout(&vwerase, 1);
}
#endif /*]*/


#if defined(X3270_MENUS) /*[*/
/*
 * External entry points to negotiate line or character mode.
 */
void
net_linemode(void)
{
	if (!CONNECTED)
		return;
	if (hisopts[TELOPT_ECHO]) {
		dont_opt[2] = TELOPT_ECHO;
		net_rawout(dont_opt, sizeof(dont_opt));
		vtrace_str("SENT %s %s\n", cmd(DONT), opt(TELOPT_ECHO));
	}
	if (hisopts[TELOPT_SGA]) {
		dont_opt[2] = TELOPT_SGA;
		net_rawout(dont_opt, sizeof(dont_opt));
		vtrace_str("SENT %s %s\n", cmd(DONT), opt(TELOPT_SGA));
	}
}

void
net_charmode(void)
{
	if (!CONNECTED)
		return;
	if (!hisopts[TELOPT_ECHO]) {
		do_opt[2] = TELOPT_ECHO;
		net_rawout(do_opt, sizeof(do_opt));
		vtrace_str("SENT %s %s\n", cmd(DO), opt(TELOPT_ECHO));
	}
	if (!hisopts[TELOPT_SGA]) {
		do_opt[2] = TELOPT_SGA;
		net_rawout(do_opt, sizeof(do_opt));
		vtrace_str("SENT %s %s\n", cmd(DO), opt(TELOPT_SGA));
	}
}
#endif /*]*/


/*
 * net_break
 *	Send telnet break, which is used to implement 3270 ATTN.
 *
 */
void
net_break(void)
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
void
net_interrupt(void)
{
	static unsigned char buf[] = { IAC, IP };

	/* I don't know if we should first send TELNET synch ? */
	net_rawout(buf, sizeof(buf));
	trace_str("SENT IP\n");
}

/*
 * net_abort
 *	Send telnet AO.
 *
 */
#if defined(X3270_TN3270E) /*[*/
void
net_abort(void)
{
	static unsigned char buf[] = { IAC, AO };

	if (e_funcs & E_OPT(TN3270E_FUNC_SYSREQ)) {
		/*
		 * I'm not sure yet what to do here.  Should the host respond
		 * to the AO by sending us SSCP-LU data (and putting us into
		 * SSCP-LU mode), or should we put ourselves in it?
		 * Time, and testers, will tell.
		 */
		switch (tn3270e_submode) {
		case E_NONE:
		case E_NVT:
			break;
		case E_SSCP:
			net_rawout(buf, sizeof(buf));
			trace_str("SENT AO\n");
			if (tn3270e_bound ||
			    !(e_funcs & E_OPT(TN3270E_FUNC_BIND_IMAGE))) {
				tn3270e_submode = E_3270;
				check_in3270();
			}
			break;
		case E_3270:
			net_rawout(buf, sizeof(buf));
			trace_str("SENT AO\n");
			tn3270e_submode = E_SSCP;
			check_in3270();
			break;
		}
	}
}
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
/*
 * parse_ctlchar
 *	Parse an stty control-character specification.
 *	A cheap, non-complaining implementation.
 */
static char
parse_ctlchar(char *s)
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
#endif /*]*/

#if defined(X3270_MENUS) || defined(C3270) /*[*/
/*
 * net_linemode_chars
 *	Report line-mode characters.
 */
struct ctl_char *
net_linemode_chars(void)
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
#endif /*]*/

#if defined(X3270_TRACE) /*[*/
/*
 * Construct a string to reproduce the current TELNET options.
 * Returns a Boolean indicating whether it is necessary.
 */
Boolean
net_snap_options(void)
{
	Boolean any = False;
	int i;
	static unsigned char ttype_str[] = {
		IAC, DO, TELOPT_TTYPE,
		IAC, SB, TELOPT_TTYPE, TELQUAL_SEND, IAC, SE
	};

	if (!CONNECTED)
		return False;

	obptr = obuf;

	/* Do TTYPE first. */
	if (myopts[TELOPT_TTYPE]) {
		unsigned j;

		space3270out(sizeof(ttype_str));
		for (j = 0; j < sizeof(ttype_str); j++)
			*obptr++ = ttype_str[j];
	}

	/* Do the other options. */
	for (i = 0; i < N_OPTS; i++) {
		space3270out(6);
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

#if defined(X3270_TN3270E) /*[*/
	/* If we're in TN3270E mode, snap the subnegotations as well. */
	if (myopts[TELOPT_TN3270E]) {
		any = True;

		space3270out(5 +
			((connected_type != CN) ? strlen(connected_type) : 0) +
			((connected_lu != CN) ? + strlen(connected_lu) : 0) +
			2);
		*obptr++ = IAC;
		*obptr++ = SB;
		*obptr++ = TELOPT_TN3270E;
		*obptr++ = TN3270E_OP_DEVICE_TYPE;
		*obptr++ = TN3270E_OP_IS;
		if (connected_type != CN) {
			(void) memcpy(obptr, connected_type,
					strlen(connected_type));
			obptr += strlen(connected_type);
		}
		if (connected_lu != CN) {
			*obptr++ = TN3270E_OP_CONNECT;
			(void) memcpy(obptr, connected_lu,
					strlen(connected_lu));
			obptr += strlen(connected_lu);
		}
		*obptr++ = IAC;
		*obptr++ = SE;

		space3270out(38);
		(void) memcpy(obptr, functions_req, 4);
		obptr += 4;
		*obptr++ = TN3270E_OP_IS;
		for (i = 0; i < 32; i++) {
			if (e_funcs & E_OPT(i))
				*obptr++ = i;
		}
		*obptr++ = IAC;
		*obptr++ = SE;

		if (tn3270e_bound) {
			tn3270e_header *h;

			space3270out(EH_SIZE + 3);
			h = (tn3270e_header *)obptr;
			h->data_type = TN3270E_DT_BIND_IMAGE;
			h->request_flag = 0;
			h->response_flag = 0;
			h->seq_number[0] = 0;
			h->seq_number[1] = 0;
			obptr += EH_SIZE;
			*obptr++ = 1; /* dummy */
			*obptr++ = IAC;
			*obptr++ = EOR;
		}
	}
#endif /*]*/
	return any;
}
#endif /*]*/

/*
 * Set blocking/non-blocking mode on the socket.  On error, pops up an error
 * message, but does not close the socket.
 */
static int
non_blocking(Boolean on)
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

#if defined(X3270_TRACE) /*[*/

static void
trace_str(const char *s)
{
	if (!toggled(DS_TRACE))
		return;
	(void) fprintf(tracef, "%s", s);
}

static void
vtrace_str(const char *fmt, ...)
{
	static char trace_msg[256];
	va_list args;

	if (!toggled(DS_TRACE))
		return;

	va_start(args, fmt);
	(void) vsprintf(trace_msg, fmt, args);
	trace_str(trace_msg);
}

#endif /*]*/
