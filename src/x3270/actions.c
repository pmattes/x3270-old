/*
 * Modifications Copyright 1993, 1994, 1995, 1999 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 */

/*
 *	actions.c
 *		The X actions table and action debugging code.
 */

#include "globals.h"
#include "appres.h"

#include "actionsc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "printc.h"
#include "selectc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "xioc.h"

#if defined(X3270_DISPLAY) /*[*/
#include "ftc.h"
#include "keypadc.h"
#include "menubarc.h"
#include "screenc.h"
#endif /*]*/

XtActionsRec actions[] = {
#if defined(X3270_DISPLAY) /*[*/
	{ "AltCursor",  	AltCursor_action },
	{ "Compose",		Compose_action },
	{ "Cut",		Cut_action },
	{ "Default",		Default_action },
	{ "Flip",		Flip_action },
	{ "HandleMenu",		HandleMenu_action },
	{ "HardPrint",		PrintText_action },
	{ "HexString",		HexString_action },
	{ "Info",		Info_action },
	{ "Keymap",		TemporaryKeymap_action },
	{ PA_PFX "ConfigureNotify", PA_ConfigureNotify_action },
	{ PA_END,		PA_End_action },
	{ PA_PFX "EnterLeave",	PA_EnterLeave_action },
	{ PA_PFX "Expose",	PA_Expose_action },
	{ PA_PFX "Focus",	PA_Focus_action },
	{ PA_PFX "GraphicsExpose", PA_GraphicsExpose_action },
	{ PA_PFX "KeymapNotify", PA_KeymapNotify_action },
# if defined(X3270_TRACE) /*[*/
	{ PA_KEYMAP_TRACE,	PA_KeymapTrace_action },
# endif /*]*/
# if defined(X3270_KEYPAD) /*[*/
	{ PA_PFX "ReparentNotify", PA_ReparentNotify_action },
# endif /*]*/
	{ PA_PFX "Shift",	PA_Shift_action },
	{ PA_PFX "StateChanged", PA_StateChanged_action },
	{ PA_PFX "VisibilityNotify",PA_VisibilityNotify_action },
	{ PA_PFX "WMProtocols",	PA_WMProtocols_action },
	{ PA_PFX "confirm",	PA_confirm_action },
	{ "PrintText",		PrintText_action },
	{ "PrintWindow",	PrintWindow_action },
	{ "Redraw",		Redraw_action },
	{ "SetFont",		SetFont_action },
	{ "TemporaryKeymap",	TemporaryKeymap_action },
# if defined(X3270_FT) /*[*/
	{ PA_PFX "dialog-next",	PA_dialog_next_action },
	{ PA_PFX "dialog-focus", PA_dialog_focus_action },
# endif /*]*/
	{ "insert-selection",	insert_selection_action },
	{ "move-select",	move_select_action },
	{ "select-end",		select_end_action },
	{ "select-extend",	select_extend_action },
	{ "select-start",	select_start_action },
	{ "set-select",		set_select_action },
	{ "start-extend",	start_extend_action },
#endif /*]*/
	{ "AnsiText",		AnsiText_action },
	{ "Ascii",		Ascii_action },
	{ "AsciiField",		AsciiField_action },
	{ "Attn",		Attn_action },
	{ "BackSpace",		BackSpace_action },
	{ "BackTab",		BackTab_action },
	{ "CircumNot",		CircumNot_action },
	{ "Clear",		Clear_action },
	{ "CloseScript",	CloseScript_action },
	{ "Connect",		Connect_action },
	{ "ContinueScript",	ContinueScript_action },
	{ "CursorSelect",	CursorSelect_action },
	{ "Delete", 		Delete_action },
	{ "DeleteField",	DeleteField_action },
	{ "DeleteWord",		DeleteWord_action },
	{ "Disconnect",		Disconnect_action },
	{ "Down",		Down_action },
	{ "Dup",		Dup_action },
	{ "Ebcdic",		Ebcdic_action },
	{ "EbcdicField",	EbcdicField_action },
	{ "Enter",		Enter_action },
	{ "Erase",		Erase_action },
	{ "EraseEOF",		EraseEOF_action },
	{ "EraseInput",		EraseInput_action },
	{ "Execute",		Execute_action },
	{ "Expect",		Expect_action },
	{ "FieldEnd",		FieldEnd_action },
	{ "FieldMark",		FieldMark_action },
	{ "FieldExit",		FieldExit_action },
	{ "HexString",		HexString_action},
	{ "Home",		Home_action },
	{ "Insert",		Insert_action },
	{ "Interrupt",		Interrupt_action },
	{ "Key",		Key_action },
	{ "Left",		Left_action },
	{ "Left2", 		Left2_action },
	{ "Macro", 		Macro_action },
	{ "MonoCase",		MonoCase_action },
	{ "MoveCursor",		MoveCursor_action },
	{ "Newline",		Newline_action },
	{ "NextWord",		NextWord_action },
	{ "PA",			PA_action },
	{ "PF",			PF_action },
	{ "PauseScript",	PauseScript_action },
	{ "PreviousWord",	PreviousWord_action },
	{ "Quit",		Quit_action },
#if defined(X3270_MENUS) /*[*/
	{ "Reconnect",		Reconnect_action },
#endif /*]*/
	{ "Reset",		Reset_action },
	{ "Right",		Right_action },
	{ "Right2",		Right2_action },
	{ "Script",		Script_action },
	{ "String",		String_action },
	{ "SysReq",		SysReq_action },
	{ "Tab",		Tab_action },
	{ "ToggleInsert",	ToggleInsert_action },
	{ "ToggleReverse",	ToggleReverse_action },
	{ "Up",			Up_action },
	{ "Wait",		Wait_action },
	{ "ignore",		ignore_action }
};

int actioncount = XtNumber(actions);

enum iaction ia_cause;
char *ia_name[] = {
	"String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
	"Macro", "Script", "Peek", "Typeahead", "File transfer"
};

/*
 * Return a name for an action.
 */
char *
action_name(XtActionProc action)
{
	register int i;

	for (i = 0; i < actioncount; i++)
		if (actions[i].proc == action)
			return actions[i].string;
	return "(unknown)";
}

#if defined(X3270_DISPLAY) && defined(X3270_TRACE) /*[*/
static char *
key_state(unsigned int state)
{
	static char rs[64];
	char *comma = "";
	static struct {
		char *name;
		unsigned int mask;
	} keymask[] = {
		{ "Shift", ShiftMask },
		{ "Lock", LockMask },
		{ "Control", ControlMask },
		{ "Mod1", Mod1Mask },
		{ "Mod2", Mod2Mask },
		{ "Mod3", Mod3Mask },
		{ "Mod4", Mod4Mask },
		{ "Mod5", Mod5Mask },
		{ "Button1", Button1Mask },
		{ "Button2", Button2Mask },
		{ "Button3", Button3Mask },
		{ "Button4", Button4Mask },
		{ "Button5", Button5Mask },
		{ CN, 0 },
	};
	int i;

	rs[0] = '\0';
	for (i = 0; keymask[i].name; i++)
		if (state & keymask[i].mask) {
			(void) strcat(rs, comma);
			(void) strcat(rs, keymask[i].name);
			comma = "|";
			state &= ~keymask[i].mask;
		}
	if (!rs[0])
		(void) sprintf(rs, "%d", state);
	else if (state)
		(void) sprintf(strchr(rs, '\0'), "|%d", state);
	return rs;
}
#endif /*]*/

/*
 * Check the number of argument to an action, and possibly pop up a usage
 * message.
 *
 * Returns 0 if the argument count is correct, -1 otherwise.
 */
int
check_usage(XtActionProc action, Cardinal nargs, Cardinal nargs_min,
    Cardinal nargs_max)
{
	if (nargs >= nargs_min && nargs <= nargs_max)
		return 0;
	if (nargs_min == nargs_max)
		popup_an_error("%s() requires %d argument%s",
		    action_name(action), nargs_min, nargs_min == 1 ? "" : "s");
	else
		popup_an_error("%s() requires %d or %d arguments",
		    action_name(action), nargs_min, nargs_max);
	return -1;
}

/*
 * Display an action debug message
 */
#if defined(X3270_TRACE) /*[*/

#define KSBUF	256
void
action_debug(void (*action)(), XEvent *event, String *params,
    Cardinal *num_params)
{
	int i;
#if defined(X3270_DISPLAY) /*[*/
	XKeyEvent *kevent;
	KeySym ks;
	XButtonEvent *bevent;
	XMotionEvent *mevent;
	XConfigureEvent *cevent;
	XClientMessageEvent *cmevent;
	XExposeEvent *exevent;
	char *press = "Press";
	char dummystr[KSBUF+1];
	char *atom_name;
#endif /*]*/

	if (!toggled(EVENT_TRACE))
		return;
	if (event == (XEvent *)NULL) {
		(void) fprintf(tracef, " %s", ia_name[(int)ia_cause]);
	}
#if defined(X3270_DISPLAY) /*[*/
	else switch (event->type) {
	    case KeyRelease:
		press = "Release";
	    case KeyPress:
		kevent = (XKeyEvent *)event;
		(void) XLookupString(kevent, dummystr, KSBUF, &ks, NULL);
		(void) fprintf(tracef,
		    "Key%s [state %s, keycode %d, keysym 0x%lx \"%s\"]",
			    press, key_state(kevent->state),
			    kevent->keycode, ks,
			    ks == NoSymbol ? "NoSymbol" : XKeysymToString(ks));
		break;
	    case ButtonRelease:
		press = "Release";
	    case ButtonPress:
		bevent = (XButtonEvent *)event;
		(void) fprintf(tracef,
		    "Button%s [state %s, button %d]",
		    press, key_state(bevent->state),
		    bevent->button);
		break;
	    case MotionNotify:
		mevent = (XMotionEvent *)event;
		(void) fprintf(tracef,
		    "MotionNotify [state %s]", key_state(mevent->state));
		break;
	    case EnterNotify:
		(void) fprintf(tracef, "EnterNotify");
		break;
	    case LeaveNotify:
		(void) fprintf(tracef, "LeaveNotify");
		break;
	    case FocusIn:
		(void) fprintf(tracef, "FocusIn");
		break;
	    case FocusOut:
		(void) fprintf(tracef, "FocusOut");
		break;
	    case KeymapNotify:
		(void) fprintf(tracef, "KeymapNotify");
		break;
	    case Expose:
		exevent = (XExposeEvent *)event;
		(void) fprintf(tracef, "Expose [%dx%d+%d+%d]",
		    exevent->width, exevent->height, exevent->x, exevent->y);
		break;
	    case PropertyNotify:
		(void) fprintf(tracef, "PropertyNotify");
		break;
	    case ClientMessage:
		cmevent = (XClientMessageEvent *)event;
		atom_name = XGetAtomName(display, (Atom)cmevent->data.l[0]);
		(void) fprintf(tracef, "ClientMessage [%s]",
		    (atom_name == CN) ? "(unknown)" : atom_name);
		break;
	    case ConfigureNotify:
		cevent = (XConfigureEvent *)event;
		(void) fprintf(tracef, "ConfigureNotify [%dx%d+%d+%d]",
		    cevent->width, cevent->height, cevent->x, cevent->y);
		break;
	    default:
		(void) fprintf(tracef, "Event %d", event->type);
		break;
	}
	if (keymap_trace != CN)
		(void) fprintf(tracef, " via %s -> %s(", keymap_trace,
		    action_name(action));
	else
#endif /*]*/
		(void) fprintf(tracef, " -> %s(",
		    action_name(action));
	for (i = 0; i < *num_params; i++) {
		(void) fprintf(tracef, "%s\"", i ? ", " : "");
		fcatv(tracef, params[i]);
		fputc('"', tracef);
	}
	(void) fprintf(tracef, ")\n");
}

#endif /*]*/

/*
 * Wrapper for calling an action internally.
 */
void
action_internal(XtActionProc action, enum iaction cause, char *parm1,
    char *parm2)
{
	Cardinal count = 0;
	String parms[2];

	parms[0] = parm1;
	parms[1] = parm2;
	if (parm1 != CN) {
		if (parm2 != CN)
			count = 2;
		else
			count = 1;
	} else
		count = 0;

	ia_cause = cause;
	(*action)((Widget) NULL, (XEvent *) NULL,
	    count ? parms : (String *) NULL,
	    &count);
}
