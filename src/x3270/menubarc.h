/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	menubarc.h
 *		Global declarations for menubar.c.
 */

#if defined(X3270_MENUS) /*[*/

extern void do_newfont(Widget w, XtPointer userdata, XtPointer calldata);
extern void HandleMenu_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void menubar_as_set(Boolean sensitive);
extern void menubar_init(Widget container, Dimension overall_width,
    Dimension current_width);
extern void menubar_keypad_changed(void);
extern Dimension menubar_qheight(Dimension container_width);
extern void menubar_resize(Dimension width);
extern void menubar_retoggle(struct toggle *t);
extern void menubar_show_reconnect(void);
extern void toggle_callback(Widget w, XtPointer userdata, XtPointer calldata);

#else /*][*/

#define menubar_as_set(n)
#define menubar_init(a, b, c)
#define menubar_keypad_changed()
#define menubar_qheight(n)	0
#define menubar_resize(n)
#define menubar_retoggle(t)
#define menubar_show_reconnect()
#define HandleMenu_action ignore_action

#endif /*]*/
