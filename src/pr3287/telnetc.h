/*
 * Copyright 1995, 1999, 2000 by Paul Mattes.
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
 *	telnetc.h
 *		Global declarations for telnet.c.
 */

/* Output buffer. */
extern unsigned char *obuf, *obptr;

/* Spelled-out tty control character. */
struct ctl_char {
	const char *name;
	char value[3];
};

extern void net_abort(void);
extern Boolean net_add_dummy_tn3270e(void);
extern void net_add_eor(unsigned char *buf, int len);
extern void net_break(void);
extern void net_charmode(void);
extern int net_connect(const char *, char *, Boolean, Boolean *);
extern void net_disconnect(void);
extern void net_exception(void);
extern void net_hexansi_out(unsigned char *buf, int len);
extern int net_input(int s);
extern void net_interrupt(void);
extern void net_linemode(void);
extern struct ctl_char *net_linemode_chars(void);
extern void net_output(void);
extern void net_sendc(char c);
extern void net_sends(const char *s);
extern void net_send_erase(void);
extern void net_send_kill(void);
extern void net_send_werase(void);
extern Boolean net_snap_options(void);
extern void space3270out(int n);
extern const char *tn3270e_current_opts(void);
extern void trace_netdata(char direction, unsigned const char *buf, int len);
