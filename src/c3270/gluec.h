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
 *		Declarations for glue.c
 */

extern int parse_command_line(int argc, char **argv, char **cl_hostname);
extern void parse_xrm(const char *arg, const char *where);
extern Boolean process_events(Boolean block);
