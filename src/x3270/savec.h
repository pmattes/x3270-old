/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	savec.h
 *		Global declarations for save.c.
 */

extern void merge_profile(XrmDatabase *d);
extern void save_args(int argc, char *argv[]);
extern void save_init(int argc, char *hostname, char *port);
extern int save_options(char *n);
extern void save_yourself(void);
