/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	utilc.h
 *		Global declarations for util.c.
 */

extern char *ctl_see(int c);
extern char *do_subst(char *s, Boolean do_vars, Boolean do_tilde);
extern void fcatv(FILE *f, char *s);
extern char *get_message(char *key);
extern char *get_resource(char *name);
extern int split_dresource(char **st, char **left, char **right);
extern int split_lresource(char **st, char **value);
extern char *xs_buffer(char *fmt, ...);
extern void xs_error(char *fmt, ...);
extern void xs_warning(char *fmt, ...);
