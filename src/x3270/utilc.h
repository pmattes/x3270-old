/*
 * Copyright 1995, 1999, 2000 by Paul Mattes.
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

extern void add_resource(const char *name, const char *value);
extern char *ctl_see(int c);
extern char *do_subst(const char *s, Boolean do_vars, Boolean do_tilde);
extern void fcatv(FILE *f, char *s);
extern const char *get_message(const char *key);
extern char *get_resource(const char *name);
extern int split_dresource(char **st, char **left, char **right);
extern int split_lresource(char **st, char **value);
extern char *xs_buffer(const char *fmt, ...);
extern void xs_error(const char *fmt, ...);
extern void xs_warning(const char *fmt, ...);

extern unsigned long AddInput(int, void (*)(void));
extern unsigned long AddExcept(int, void (*)(void));
extern void RemoveInput(unsigned long);
extern unsigned long AddTimeOut(unsigned long msec, void (*fn)(void));
extern void RemoveTimeOut(unsigned long cookie);
extern KeySym StringToKeysym(char *s);
extern char *KeysymToString(KeySym k);
