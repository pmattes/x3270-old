/*
 * Copyright 1995, 1999, 2000 by Paul Mattes.
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

/* form input editing enumeration */
enum form_type { FORM_NO_WHITE, FORM_NO_CC, FORM_AS_IS };

/* abort callback */
typedef void abort_callback_t(void);

extern void action_output(const char *fmt, ...);
extern Widget create_form_popup(const char *name, XtCallbackProc callback,
    XtCallbackProc callback2, enum form_type form_type);
extern void child_popup_init(void);
extern void error_init(void);
extern void error_popup_init(void);
extern Boolean error_popup_visible(void);
extern void Info_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void info_popup_init(void);
extern void PA_confirm_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void place_popup(Widget w, XtPointer client_data, XtPointer call_data);
extern void popdown_an_error(void);
extern void popup_an_errno(int errn, const char *fmt, ...);
extern void popup_an_error(const char *fmt, ...);
extern void popup_an_info(const char *fmt, ...);
extern void popup_child_output(Boolean is_err, abort_callback_t *a,
    const char *fmt, ...);
extern void popup_popup(Widget shell, XtGrabKind grab);
extern void popup_printer_output(Boolean is_err, abort_callback_t *a,
    const char *fmt, ...);
extern void printer_popup_init(void);
extern void toplevel_geometry(Position *x, Position *y, Dimension *width,
    Dimension *height);
