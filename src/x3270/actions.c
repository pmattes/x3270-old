/*
 * Modifications Copyright 1993, 1994, 1995, 1999, 2000, 2001 by Paul Mattes.
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

#if defined(X3270_FT) /*[*/
#include "ftc.h"
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
#include "keypadc.h"
#include "menubarc.h"
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
#include "screenc.h"
#endif /*]*/

#if defined(X3270_DISPLAY) /*[*/
#include <X11/keysym.h>

#define MODMAP_SIZE	8
#define MAP_SIZE	13
#define MAX_MODS_PER	4
static struct {
        const char *name[MAX_MODS_PER];
        unsigned int mask;
	Boolean is_meta;
} skeymask[MAP_SIZE] = { 
	{ { "Shift" }, ShiftMask, False },
	{ { (char *)NULL } /* Lock */, LockMask, False },
	{ { "Ctrl" }, ControlMask, False },
	{ { (char *)NULL }, Mod1Mask, False },
	{ { (char *)NULL }, Mod2Mask, False },
	{ { (char *)NULL }, Mod3Mask, False },
	{ { (char *)NULL }, Mod4Mask, False },
	{ { (char *)NULL }, Mod5Mask, False },
	{ { "Button1" }, Button1Mask, False },
	{ { "Button2" }, Button2Mask, False },
	{ { "Button3" }, Button3Mask, False },
	{ { "Button4" }, Button4Mask, False },
	{ { "Button5" }, Button5Mask, False }
};
static Boolean know_mods = False;
#endif /*]*/

XtActionsRec actions[] = {
#if defined(C3270) /*[*/
	{ "Abort",		Abort_action },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	{ "AltCursor",  	AltCursor_action },
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
	{ "Compose",		Compose_action },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	{ "Cut",		Cut_action },
	{ "Default",		Default_action },
	{ "HandleMenu",		HandleMenu_action },
	{ "HardPrint",		PrintText_action },
	{ "HexString",		HexString_action },
	{ "Info",		Info_action },
	{ "Keymap",		TemporaryKeymap_action },
	{ PA_PFX "ConfigureNotify", PA_ConfigureNotify_action },
	{ PA_END,		PA_End_action },
#endif /*]*/
#if defined(C3270) /*[*/
	{ "Escape",		Escape_action },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
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
	{ "PrintWindow",	PrintWindow_action },
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
	{ "PrintText",		PrintText_action },
	{ "Flip",		Flip_action },
	{ "Redraw",		Redraw_action },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	{ "SetFont",		SetFont_action },
	{ "TemporaryKeymap",	TemporaryKeymap_action },
# if defined(X3270_FT) && defined(X3270_MENUS) /*[*/
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
#if defined(X3270_SCRIPT) /*[*/
	{ "AnsiText",		AnsiText_action },
#endif /*]*/
	{ "Ascii",		Ascii_action },
	{ "AsciiField",		AsciiField_action },
	{ "Attn",		Attn_action },
	{ "BackSpace",		BackSpace_action },
	{ "BackTab",		BackTab_action },
	{ "CircumNot",		CircumNot_action },
	{ "Clear",		Clear_action },
#if defined(C3270) /*[*/
	{ "Close",		Disconnect_action },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ "CloseScript",	CloseScript_action },
#endif /*]*/
	{ "Connect",		Connect_action },
#if defined(X3270_SCRIPT) /*[*/
	{ "ContinueScript",	ContinueScript_action },
#endif /*]*/
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
#if defined(X3270_SCRIPT) /*[*/
	{ "Execute",		Execute_action },
	{ "Expect",		Expect_action },
#endif /*]*/
	{ "FieldEnd",		FieldEnd_action },
	{ "FieldMark",		FieldMark_action },
	{ "FieldExit",		FieldExit_action },
	{ "HexString",		HexString_action},
#if defined(C3270) /*[*/
	{ "Help",		Help_action},
#endif/*]*/
	{ "Home",		Home_action },
	{ "Insert",		Insert_action },
	{ "Interrupt",		Interrupt_action },
	{ "Key",		Key_action },
#if defined(X3270_DISPLAY) /*[*/
	{ "KybdSelect",		KybdSelect_action },
#endif /*]*/
	{ "Left",		Left_action },
	{ "Left2", 		Left2_action },
#if defined(X3270_SCRIPT) /*[*/
	{ "Macro", 		Macro_action },
#endif /*]*/
	{ "MonoCase",		MonoCase_action },
#if defined(X3270_DISPLAY) /*[*/
	{ "MouseSelect",	MouseSelect_action },
#endif /*]*/
	{ "MoveCursor",		MoveCursor_action },
	{ "Newline",		Newline_action },
	{ "NextWord",		NextWord_action },
#if defined(C3270) /*[*/
	{ "Open",		Connect_action },
#endif /*]*/
	{ "PA",			PA_action },
	{ "PF",			PF_action },
#if defined(X3270_SCRIPT) /*[*/
	{ "PauseScript",	PauseScript_action },
#endif /*]*/
	{ "PreviousWord",	PreviousWord_action },
#if defined(X3270_SCRIPT) && defined(X3270_PRINTER) /*[*/
	{ "Printer",		Printer_action },
#endif /*]*/
	{ "Quit",		Quit_action },
#if defined(X3270_MENUS) /*[*/
	{ "Reconnect",		Reconnect_action },
#endif /*]*/
	{ "Reset",		Reset_action },
	{ "Right",		Right_action },
	{ "Right2",		Right2_action },
#if defined(X3270_SCRIPT) /*[*/
	{ "Script",		Script_action },
#endif /*]*/
#if defined(C3270) /*[*/
	{ "Show",		Show_action },
#endif/*]*/
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	{ "Snap",		Snap_action },
#endif /*]*/
#if defined(TCL3270) /*[*/
	{ "Status",		Status_action },
#endif /*]*/
	{ "String",		String_action },
	{ "SysReq",		SysReq_action },
	{ "Tab",		Tab_action },
	{ "ToggleInsert",	ToggleInsert_action },
	{ "ToggleReverse",	ToggleReverse_action },
#if defined(C3270) && defined(X3270_TRACE) /*[*/
	{ "Trace",		Trace_action },
#endif/*]*/
#if defined(X3270_FT) /*[*/
	{ "Transfer",		Transfer_action },
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	{ "Unselect",		Unselect_action },
#endif /*]*/
	{ "Up",			Up_action },
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	{ "Wait",		Wait_action },
#endif /*]*/
	{ "ignore",		ignore_action }
};

int actioncount = XtNumber(actions);

enum iaction ia_cause;
const char *ia_name[] = {
	"String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
	"Macro", "Script", "Peek", "Typeahead", "File transfer", "Command",
	"Keymap"
};

/*
 * Return a name for an action.
 */
const char *
action_name(XtActionProc action)
{
	register int i;

	for (i = 0; i < actioncount; i++)
		if (actions[i].proc == action)
			return actions[i].string;
	return "(unknown)";
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * Search the modifier map to learn the modifier bits for Meta, Alt, Hyper and
 *  Super.
 */
static void
learn_modifiers(void)
{
	XModifierKeymap *mm;
	int i, j, k;

	mm = XGetModifierMapping(display);

	for (i = 0; i < MODMAP_SIZE; i++) {
		for (j = 0; j < mm->max_keypermod; j++) {
			KeyCode kc;
			const char *name = CN;
			Boolean is_meta = False;

			kc = mm->modifiermap[(i * mm->max_keypermod) + j];
			if (!kc)
				continue;

			switch(XKeycodeToKeysym(display, kc, 0)) {
			    case XK_Meta_L:
			    case XK_Meta_R:
				name = "Meta";
				is_meta = True;
				break;
			    case XK_Alt_L:
			    case XK_Alt_R:
				name = "Alt";
				break;
			    case XK_Super_L:
			    case XK_Super_R:
				name = "Super";
				break;
			    case XK_Hyper_L:
			    case XK_Hyper_R:
				name = "Hyper";
				break;
			    default:
				break;
			}
			if (name == CN)
				continue;
			if (is_meta)
				skeymask[i].is_meta = True;

			for (k = 0; k < MAX_MODS_PER; k++) {
				if (skeymask[i].name[k] == CN)
					break;
				else if (!strcmp(skeymask[i].name[k], name))
					k = MAX_MODS_PER;
			}
			if (k >= MAX_MODS_PER)
				continue;
			skeymask[i].name[k] = name;
		}
	}
}

#if defined(X3270_TRACE) /*[*/
/*
 * Return the symbolic name for the modifier combination (i.e., "Meta" instead
 * of "Mod2".  Note that because it is possible to map multiple keysyms to the
 * same modifier bit, the answer may be ambiguous; we return the combinations
 * iteratively.
 */
static char *
key_symbolic_state(unsigned int state, int *iteration)
{
	static char rs[64];
	static int ix[MAP_SIZE];
	static int ix_ix[MAP_SIZE];
	static int n_ix = 0;
	static int leftover = 0;
	const char *comma = "";
	int i;

	if (!know_mods) {
		learn_modifiers();
		know_mods = True;
	}

	if (*iteration == 0) {
		/* First time, build the table. */
		n_ix = 0;
		for (i = 0; i < MAP_SIZE; i++) {
			if (skeymask[i].name[0] != CN &&
			    (state & skeymask[i].mask)) {
				ix[i] = 0;
				state &= ~skeymask[i].mask;
				ix_ix[n_ix++] = i;
			} else
				ix[i] = MAX_MODS_PER;
		}
		leftover = state;
	}

	/* Construct this result. */
	rs[0] = '\0';
	for (i = 0; i < n_ix;  i++) {
		(void) strcat(rs, comma);
		(void) strcat(rs, skeymask[ix_ix[i]].name[ix[ix_ix[i]]]);
		comma = " ";
	}
	if (leftover)
		(void) sprintf(strchr(rs, '\0'), "%s?%d", comma, state);

	/*
	 * Iterate to the next.
	 * This involves treating each slot like an n-ary number, where n is
	 * the number of elements in the slot, iterating until the highest-
	 * ordered slot rolls back over to 0.
	 */
	if (n_ix) {
		i = n_ix - 1;
		ix[ix_ix[i]]++;
		while (i >= 0 &&
		       (ix[ix_ix[i]] >= MAX_MODS_PER ||
			skeymask[ix_ix[i]].name[ix[ix_ix[i]]] == CN)) {
			ix[ix_ix[i]] = 0;
			i = i - 1;
			if (i >= 0)
				ix[ix_ix[i]]++;
		}
		*iteration = i >= 0;
	} else
		*iteration = 0;

	return rs;
}
#endif /*]*/

/* Return whether or not an KeyPress event state includes the Meta key. */
Boolean
event_is_meta(int state)
{
	int i;

	/* Learn the modifier map. */
	if (!know_mods) {
		learn_modifiers();
		know_mods = True;
	}
	for (i = 0; i < MAP_SIZE; i++) {
		if (skeymask[i].name[0] != CN &&
		    skeymask[i].is_meta &&
		    (state & skeymask[i].mask)) {
			return True;
		}
	}
	return False;
}

#if defined(VERBOSE_EVENTS) /*[*/
static char *
key_state(unsigned int state)
{
	static char rs[64];
	const char *comma = "";
	static struct {
		const char *name;
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
	for (i = 0; keymask[i].name; i++) {
		if (state & keymask[i].mask) {
			(void) strcat(rs, comma);
			(void) strcat(rs, keymask[i].name);
			comma = "|";
			state &= ~keymask[i].mask;
		}
	}
	if (!rs[0])
		(void) sprintf(rs, "%d", state);
	else if (state)
		(void) sprintf(strchr(rs, '\0'), "%s?%d", comma, state);
	return rs;
}
#endif /*]*/
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
		popup_an_error("%s requires %d argument%s",
		    action_name(action), nargs_min, nargs_min == 1 ? "" : "s");
	else
		popup_an_error("%s requires %d or %d arguments",
		    action_name(action), nargs_min, nargs_max);
	return -1;
}

/*
 * Display an action debug message
 */
#if defined(X3270_TRACE) /*[*/

#define KSBUF	256
void
action_debug(XtActionProc action, XEvent *event, String *params,
    Cardinal *num_params)
{
	Cardinal i;
#if defined(X3270_DISPLAY) /*[*/
	XKeyEvent *kevent;
	KeySym ks;
	XButtonEvent *bevent;
	XMotionEvent *mevent;
	XConfigureEvent *cevent;
	XClientMessageEvent *cmevent;
	XExposeEvent *exevent;
	const char *press = "Press";
	const char *direction = "Down";
	char dummystr[KSBUF+1];
	char *atom_name;
	int ambiguous = 0;
	int state;
	const char *symname = "";
	char snbuf[11];
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
		state = kevent->state;
		/*
		 * If the keysym is a printable ASCII character, ignore the
		 * Shift key.
		 */
		if (ks != ' ' && !(ks & ~0xff) && isprint(ks))
			state &= ~ShiftMask;
		if (ks == NoSymbol)
			symname = "NoSymbol";
		else if ((symname = XKeysymToString(ks)) == CN) {
			(void) sprintf(snbuf, "0x%lx", (unsigned long)ks);
			symname = snbuf;
		}
		do {
			int was_ambiguous = ambiguous;

			(void) fprintf(tracef, "%s ':%s<Key%s>%s'",
				was_ambiguous? " or": "Event",
				key_symbolic_state(state, &ambiguous),
				press,
				symname);
		} while (ambiguous);
		/*
		 * If the keysym is an alphanumeric ASCII character, show the
		 * case-insensitive alternative, sans the colon.
		 */
		if (!(ks & ~0xff) && isalpha(ks)) {
			ambiguous = 0;
			do {
				int was_ambiguous = ambiguous;

				(void) fprintf(tracef, " %s '%s<Key%s>%s'",
					was_ambiguous? "or":
					    "(case-insensitive:",
					key_symbolic_state(state, &ambiguous),
					press,
					symname);
			} while (ambiguous);
			(void) fprintf(tracef, ")");
		}
#if defined(VERBOSE_EVENTS) /*[*/
		(void) fprintf(tracef,
		    "\nKey%s [state %s, keycode %d, keysym 0x%lx \"%s\"]",
			    press, key_state(kevent->state),
			    kevent->keycode, ks,
			    symname);
#endif /*]*/
		break;
	    case ButtonRelease:
		press = "Release";
		direction = "Up";
	    case ButtonPress:
		bevent = (XButtonEvent *)event;
		do {
			int was_ambiguous = ambiguous;

			(void) fprintf(tracef, "%s '%s<Btn%d%s>'",
				was_ambiguous? " or": "Event",
				key_symbolic_state(bevent->state, &ambiguous),
				bevent->button,
				direction);
		} while (ambiguous);
#if defined(VERBOSE_EVENTS) /*[*/
		(void) fprintf(tracef,
		    "\nButton%s [state %s, button %d]",
		    press, key_state(bevent->state),
		    bevent->button);
#endif /*]*/
		break;
	    case MotionNotify:
		mevent = (XMotionEvent *)event;
		do {
			int was_ambiguous = ambiguous;

			(void) fprintf(tracef, "%s '%s<Motion>'",
				was_ambiguous? " or": "Event",
				key_symbolic_state(mevent->state, &ambiguous));
		} while (ambiguous);
#if defined(VERBOSE_EVENTS) /*[*/
		(void) fprintf(tracef,
		    "\nMotionNotify [state %s]", key_state(mevent->state));
#endif /*]*/
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
action_internal(XtActionProc action, enum iaction cause, const char *parm1,
    const char *parm2)
{
	Cardinal count = 0;
	String parms[2];

	/* Duplicate the parms, because XtActionProc doesn't grok 'const'. */
	if (parm1 != CN) {
		parms[0] = NewString(parm1);
		count++;
		if (parm2 != CN) {
			parms[1] = NewString(parm2);
			count++;
		}
	}

	ia_cause = cause;
	(*action)((Widget) NULL, (XEvent *) NULL,
	    count ? parms : (String *) NULL,
	    &count);

	/* Free the parm copies. */
	switch (count) {
	    case 2:
		Free(parms[1]);
		/* fall through... */
	    case 1:
		Free(parms[0]);
		break;
	    default:
		break;
	}
}
