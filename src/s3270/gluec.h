/*
 * Copyright 2000 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	gluec.h
 *		Declarations for glue.c and XtGlue.c
 */

/* glue.c */
extern int parse_command_line(int argc, const char **argv,
    const char **cl_hostname);
extern void parse_xrm(const char *arg, const char *where);
extern Boolean process_events(Boolean block);

/* XtGlue.c */
extern void (*Warning_redirect)(const char *);
extern int select_setup(int *nfds, fd_set *readfds, fd_set *writefds,
    fd_set *exceptfds, struct timeval **timeout, struct timeval *timebuf);
