/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	popupsc.h
 *		Global declarations for popups.c.
 */

/* window placement enumeration */
enum placement { Center, Bottom, Left, Right };
extern enum placement *CenterP;
extern enum placement *BottomP;
extern enum placement *LeftP;
extern enum placement *RightP;

extern Widget create_form_popup(char *name, XtCallbackProc callback,
    XtCallbackProc callback2, Boolean no_spaces);
extern void error_popup_init(void);
extern void Info_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void info_popup_init(void);
extern void PA_confirm_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void place_popup(Widget w, XtPointer client_data, XtPointer call_data);
extern void popup_an_errno(int errn, char *fmt, ...);
extern void popup_an_error(char *fmt, ...);
extern void popup_an_info(char *fmt, ...);
extern void popup_popup(Widget shell, XtGrabKind grab);
extern void toplevel_geometry(Dimension *x, Dimension *y, Dimension *width,
    Dimension *height);
