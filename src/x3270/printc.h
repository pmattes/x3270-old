/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	printc.h
 *		Global declarations for print.c.
 */

extern Boolean fprint_screen(FILE *f, Boolean even_if_empty);
extern void PrintText_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PrintWindow_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void print_text_option(Widget w, XtPointer client_data,
    XtPointer call_data);
extern void print_window_option(Widget w, XtPointer client_data,
    XtPointer call_data);
