/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	statusc.h
 *		Global declarations for status.c.
 */

extern void status_compose(Boolean on, unsigned char c, enum keytype keytype);
extern void status_ctlr_done(void);
extern void status_cursor_pos(int ca);
extern void status_disp(void);
extern void status_init(void);
extern void status_insert_mode(Boolean on);
extern void status_kmap(Boolean on);
extern void status_kybdlock(void);
extern void status_oerr(int error_type);
extern void status_reinit(unsigned cmask);
extern void status_reset(void);
extern void status_reverse_mode(Boolean on);
extern void status_script(Boolean on);
extern void status_scrolled(int n);
extern void status_shift_mode(int state);
extern void status_syswait(void);
extern void status_timing(struct timeval *t0, struct timeval *t1);
extern void status_touch(void);
extern void status_twait(void);
extern void status_typeahead(Boolean on);
extern void status_uncursor_pos(void);
extern void status_untiming(void);
