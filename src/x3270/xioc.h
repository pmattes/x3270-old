/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	xioc.h
 *		Global declarations for xio.c.
 */

extern void Quit_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void x3270_exit(int n);
extern void x_add_input(int net_sock);
extern void x_except_off(void);
extern void x_except_on(int net_sock);
extern void x_remove_input(void);
