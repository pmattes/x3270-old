/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	actionsc.h
 *		Global declarations for actions.c.
 */

/* types of internal actions */
enum iaction {
	IA_STRING, IA_PASTE, IA_REDRAW,
	IA_KEYPAD, IA_DEFAULT, IA_KEY,
	IA_MACRO, IA_SCRIPT, IA_PEEK,
	IA_TYPEAHEAD, IA_FT, IA_COMMAND, IA_KEYMAP
};
extern enum iaction ia_cause;

extern int              actioncount;
extern XtActionsRec     actions[];

extern const char       *ia_name[];

#if defined(X3270_TRACE) /*[*/
extern void action_debug(XtActionProc action, XEvent *event, String *params,
    Cardinal *num_params);
#else /*][*/
#define action_debug(a, e, p, n)
#endif /*]*/
extern void action_internal(XtActionProc action, enum iaction cause,
    const char *parm1, const char *parm2);
extern const char *action_name(XtActionProc action);
extern int check_usage(XtActionProc action, Cardinal nargs, Cardinal nargs_min,
    Cardinal nargs_max);
extern Boolean event_is_meta(int state);
