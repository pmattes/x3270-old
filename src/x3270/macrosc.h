/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	macrosc.h
 *		Global declarations for macros.c.
 */

/* macro definition */
struct macro_def {
	char			*name;
	char			*action;
	struct macro_def	*next;
};
extern struct macro_def *macro_defs;

extern void abort_script(void);
extern void AnsiText_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void AsciiField_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Ascii_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void CloseScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ContinueScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void EbcdicField_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Ebcdic_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Execute_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void execute_action_option(Widget w, XtPointer client_data,
    XtPointer call_data);
extern void Expect_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void login_macro(char *s);
extern void macros_init(void);
extern void Macro_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void macro_command(struct macro_def *m);
extern void PauseScript_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void peer_script_init(void);
extern void ps_set(char *s, Boolean is_hex);
extern void Script_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern Boolean sms_active(void);
extern void sms_connect_wait(void);
extern void sms_continue(void);
extern void sms_error(char *msg);
extern void sms_init(void);
extern Boolean sms_redirect(void);
extern void sms_store(unsigned char c);
extern void Wait_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);