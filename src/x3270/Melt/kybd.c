/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994, 1995 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	kybd.c
 *		This module handles the keyboard for the 3270 emulator.
 */

#include "globals.h"
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include "3270ds.h"
#include "ctlr.h"
#include "cg.h"
#include "resources.h"

/* Statics */
#define NP	5
static Atom	paste_atom[NP];
static int	n_pasting = 0;
static int	pix = 0;
static Time	paste_time;
static enum	{ NONE, COMPOSE, FIRST } composing = NONE;
static char *ia_name[] = {
	"String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
	"Macro", "Script", "Peek", "Typeahead"
};
static unsigned char pf_xlate[] = { 
	AID_PF1,  AID_PF2,  AID_PF3,  AID_PF4,  AID_PF5,  AID_PF6,
	AID_PF7,  AID_PF8,  AID_PF9,  AID_PF10, AID_PF11, AID_PF12,
	AID_PF13, AID_PF14, AID_PF15, AID_PF16, AID_PF17, AID_PF18,
	AID_PF19, AID_PF20, AID_PF21, AID_PF22, AID_PF23, AID_PF24
};
static unsigned char pa_xlate[] = { 
	AID_PA1, AID_PA2, AID_PA3
};
#define PF_SZ	(sizeof(pf_xlate)/sizeof(pf_xlate[0]))
#define PA_SZ	(sizeof(pa_xlate)/sizeof(pa_xlate[0]))
static XtIntervalId unlock_id;
#define UNLOCK_MS	350
static Boolean	key_Character();

static int nxk = 0;
static struct xks {
	KeySym key;
	KeySym assoc;
} *xk;
static struct trans_list *tkm_last;

static Boolean		insert = False;		/* insert mode */
static Boolean		reverse = False;	/* reverse-input mode */

/* Globals */
unsigned int	kybdlock;
unsigned char	aid = AID_NO;		/* current attention ID */
enum iaction	ia_cause;
struct trans_list *temp_keymaps;	/* temporary keymap list */

/* Composite key mappings. */

enum keytype { LATIN, APL };
struct akeysym {
	KeySym keysym;
	enum keytype keytype;
};
static struct akeysym cc_first;
static struct composite {
	struct akeysym k1, k2;
	KeySym translation;
} *composites = NULL;
static int n_composites = 0;

#define ak_eq(k1, k2)	(((k1).keysym  == (k2).keysym) && \
			 ((k1).keytype == (k2).keytype))

static struct ta {
	struct ta *next;
	void (*fn)();
	char *parm1;
	char *parm2;
} *ta_head = (struct ta *) NULL,
  *ta_tail = (struct ta *) NULL;


/*
 * Put an action on the typeahead queue.
 */
void
enq_ta(fn, parm1, parm2)
void (*fn)();
char *parm1;
char *parm2;
{
	struct ta *ta;

	/* If no connection, forget it. */
	if (!CONNECTED) {
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, "  dropped (not connected)\n");
		return;
	}

	/* If operator error, complain and drop it. */
	if (kybdlock & KL_OERR_MASK) {
		ring_bell();
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, "  dropped (operator error)\n");
		return;
	}

	/* If scroll lock, complain and drop it. */
	if (kybdlock & KL_SCROLLED) {
		ring_bell();
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, "  dropped (scrolled)\n");
		return;
	}

	/* If typeahead disabled, complain and drop it. */
	if (!appres.typeahead) {
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, "  dropped (no typeahead)\n");
		return;
	}

	ta = (struct ta *) XtMalloc(sizeof(*ta));
	ta->next = (struct ta *) NULL;
	ta->fn = fn;
	ta->parm1 = ta->parm2 = CN;
	if (parm1) {
		ta->parm1 = XtNewString(parm1);
		if (parm2)
			ta->parm2 = XtNewString(parm2);
	}
	if (ta_head)
		ta_tail->next = ta;
	else {
		ta_head = ta;
		status_typeahead(True);
	}
	ta_tail = ta;

	if (toggled(EVENT_TRACE))
		(void) fprintf(tracef, "  action queued (kybdlock 0x%x)\n",
		    kybdlock);
}

/*
 * Execute an action from the typeahead queue.
 */
Boolean
run_ta()
{
	struct ta *ta;

	if (kybdlock || !(ta = ta_head))
		return False;

	if (!(ta_head = ta->next)) {
		ta_tail = (struct ta *) NULL;
		status_typeahead(False);
	}

	internal_action(ta->fn, IA_TYPEAHEAD, ta->parm1, ta->parm2);
	if (ta->parm1)
		XtFree(ta->parm1);
	if (ta->parm2)
		XtFree(ta->parm2);

	return True;
}

/*
 * Flush the typeahead queue.
 * Returns whether or not anything was flushed.
 */
Boolean
flush_ta()
{
	struct ta *ta, *next;
	Boolean any = False;

	for (ta = ta_head; ta != (struct ta *) NULL; ta = next) {
		if (ta->parm1)
			XtFree(ta->parm1);
		if (ta->parm2)
			XtFree(ta->parm2);
		next = ta->next;
		XtFree((XtPointer)ta);
		any = True;
	}
	ta_head = ta_tail = (struct ta *) NULL;
	status_typeahead(False);
	return any;
}

/*
 * Translation table cache.
 */
XtTranslations
lookup_tt(name, table)
char *name;
char *table;
{
	struct tt_cache {
		char *name;
		XtTranslations trans;
		struct tt_cache *next;
	};
#	define TTN (struct tt_cache *)NULL
	static struct tt_cache *tt_cache = TTN;
	struct tt_cache *t;

	/* Look for an old one. */
	for (t = tt_cache; t != TTN; t = t->next)
		if (!strcmp(name, t->name))
			return t->trans;

	/* Allocate and translate a new one. */
	t = (struct tt_cache *)XtMalloc(sizeof(*t));
	t->name = XtNewString(name);
	t->trans = XtParseTranslationTable(table);
	t->next = tt_cache;
	tt_cache = t;

	return t->trans;
}
#undef TTN

/* Set bits in the keyboard lock. */
/*ARGSUSED*/
void
kybdlock_set(bits, cause)
unsigned int bits;
char *cause;
{
	unsigned int n;

	n = kybdlock | bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef,
			    "  %s: kybdlock |= 0x%04x, 0x%04x -> 0x%04x\n",
			    cause, bits, kybdlock, n);
#endif /*]*/
		kybdlock = n;
		status_kybdlock();
	}
}

/* Clear bits in the keyboard lock. */
/*ARGSUSED*/
void
kybdlock_clr(bits, cause)
unsigned int bits;
char *cause;
{
	unsigned int n;

	n = kybdlock & ~bits;
	if (n != kybdlock) {
#if defined(KYBDLOCK_TRACE) /*[*/
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef,
			    "  %s: kybdlock &= ~0x%04x, 0x%04x -> 0x%04x\n",
			    cause, bits, kybdlock, n);
#endif /*]*/
		kybdlock = n;
		status_kybdlock();
	}
}

/*
 * Set or clear enter-inhibit mode.
 */
void
kybd_inhibit(inhibit)
Boolean inhibit;
{
	if (inhibit) {
		kybdlock_set(KL_ENTER_INHIBIT, "kybd_inhibit");
		if (kybdlock == KL_ENTER_INHIBIT)
			status_reset();
	} else {
		kybdlock_clr(KL_ENTER_INHIBIT, "kybd_inhibit");
		if (!kybdlock)
			status_reset();
	}
}

/*
 * Called when a host connects or disconnects.
 */
void
kybd_connect()
{
	if (kybdlock & KL_DEFERRED_UNLOCK)
		XtRemoveTimeOut(unlock_id);
	kybdlock_clr(-1, "kybd_connect");

	if (CONNECTED) {
		/* Wait for any output or a WCC(restore) from the host */
		kybdlock_set(KL_AWAITING_FIRST, "kybd_connect");
	} else {
		kybdlock_set(KL_NOT_CONNECTED, "kybd_connect");
		(void) flush_ta();
	}
}

/*
 * Toggle insert mode.
 */
static void
insert_mode(on)
Boolean on;
{
	insert = on;
	status_insert_mode(on);
}

/*
 * Toggle reverse mode.
 */
static void
reverse_mode(on)
Boolean on;
{
	reverse = on;
	status_reverse_mode(on);
}

/*
 * Lock the keyboard because of an operator error.
 */
static void
operator_error(error_type)
int error_type;
{
	if (scripting())
		popup_an_error("Keyboard locked");
	if (appres.oerr_lock || scripting()) {
		status_oerr(error_type);
		mcursor_locked();
		kybdlock_set((unsigned int)error_type, "operator_error");
		(void) flush_ta();
	} else
		ring_bell();
}


static char *
key_state(state)
unsigned int state;
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

/*
 * Return a name for an action.
 */
char *
action_name(action)
XtActionProc action;
{
	register int i;

	for (i = 0; i < actioncount; i++)
		if (actions[i].proc == (XtActionProc)action)
			return actions[i].string;
	return "(unknown)";
}

/*
 * Check the number of argument to an action, and possibly pop up a usage
 * message.
 *
 * Returns 0 if the argument count is correct, -1 otherwise.
 */
int
check_usage(action, nargs, nargs_min, nargs_max)
XtActionProc action;
Cardinal nargs;
Cardinal nargs_min;
Cardinal nargs_max;
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
#define KSBUF	256
void
debug_action(action, event, params, num_params)
void (*action)();
XEvent *event;
String *params;
Cardinal *num_params;
{
	int i;
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

	if (!toggled(EVENT_TRACE))
		return;
	if (!event) {
		(void) fprintf(tracef, " %s", ia_name[(int)ia_cause]);
	} else switch (event->type) {
	    case KeyRelease:
		press = "Release";
	    case KeyPress:
		kevent = (XKeyEvent *)event;
		(void) XLookupString(kevent, dummystr, KSBUF, &ks, NULL);
		(void) fprintf(tracef, "Key%s [state %s, keycode %d, keysym 0x%lx \"%s\"]",
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
	(void) fprintf(tracef, " -> %s(", action_name((XtActionProc)action));
	for (i = 0; i < *num_params; i++) {
		Boolean quoted;

		quoted = (strchr(params[i], ' ')  != CN ||
			  strchr(params[i], '\t') != CN);
		(void) fprintf(tracef, "%s%s%s%s",
		    i ? ", " : "",
		    quoted ? "\"" : "",
		    params[i],
		    quoted ? "\"" : "");
	}
	(void) fprintf(tracef, ")\n");
}

/*
 * Wrapper for calling an action internally.
 */
void
internal_action(fn, cause, parm1, parm2)
void (*fn)();
enum iaction cause;
char *parm1;
char *parm2;
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
	(*fn)((Widget) NULL, (XEvent *) NULL, count ? parms : (String *) NULL,
	    &count);
}


/*
 * Handle an AID (Attention IDentifier) key.  This is the common stuff that
 * gets executed for all AID keys (PFs, PAs, Clear and etc).
 */
void
key_AID(aid_code)
unsigned char	aid_code;
{
	if (IN_ANSI) {
		register int	i;

		if (aid_code == AID_ENTER) {
			net_sendc('\r');
			return;
		}
		for (i = 0; i < PF_SZ; i++)
			if (aid_code == pf_xlate[i]) {
				ansi_send_pf(i+1);
				return;
			}
		for (i = 0; i < PA_SZ; i++)
			if (aid_code == pa_xlate[i]) {
				ansi_send_pa(i+1);
				return;
			}
		return;
	}
	status_twait();
	mcursor_waiting();
	insert_mode(False);
	kybdlock_set(KL_OIA_TWAIT | KL_OIA_LOCKED, "key_AID");
	aid = aid_code;
	ctlr_read_modified(aid, False);
	ticking_start(False);
	status_ctlr_done();
}

/*ARGSUSED*/
void
key_PF(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int k;

	debug_action(key_PF, event, params, num_params);
	if (check_usage(key_PF, *num_params, 1, 1) < 0)
		return;
	k = atoi(params[0]);
	if (k < 0 || k > PF_SZ) {
		popup_an_error("%s: invalid argument", action_name(key_PF));
		return;
	}
	if (kybdlock)
		enq_ta(key_PF, params[0], CN);
	else
		key_AID(pf_xlate[k-1]);
}

/*ARGSUSED*/
void
key_PA(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int k;

	debug_action(key_PA, event, params, num_params);
	if (check_usage(key_PA, *num_params, 1, 1) < 0)
		return;
	k = atoi(params[0]);
	if (k < 0 || k > PA_SZ) {
		popup_an_error("%s: invalid argument %d", action_name(key_PA),
		    k);
		return;
	}
	if (kybdlock)
		enq_ta(key_PA, params[0], CN);
	else
		key_AID(pa_xlate[k-1]);
}


/*
 * ATTN key, similar to an AID key but without the read_modified call.
 */
/*ARGSUSED*/
void
key_Attn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Attn, event, params, num_params);
	if (!IN_3270)
		return;
	if (kybdlock) {
		enq_ta(key_Attn, CN, CN);
		return;
	}
	status_twait();
	mcursor_waiting();
	insert_mode(False);
	kybdlock_set(KL_OIA_TWAIT | KL_OIA_LOCKED, "key_Attn");
	net_break();
	ticking_start(False);
	status_ctlr_done();
}



/*ARGSUSED*/
static void
key_Character_wrapper(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int cgcode;
	Boolean with_ge = False;

	cgcode = atoi(params[0]);
	if (cgcode & 0x100) {
		with_ge = True;
		cgcode &= 0xff;
	}
	if (toggled(EVENT_TRACE)) {
		(void) fprintf(tracef, " %s -> Key(%s\"%s\")\n",
		    ia_name[(int) ia_cause],
		    with_ge ? "GE " : "",
		    ctl_see((int) cg2asc[cgcode]));
	}
	(void) key_Character(cgcode, with_ge);
}

/*
 * Handle an ordinary displayable character key.  Lots of stuff to handle
 * insert-mode, protected fields and etc.
 */
static Boolean
key_Character(cgcode, with_ge)
int	cgcode;
Boolean	with_ge;
{
	register int	baddr, end_baddr;
	register unsigned char	*fa;
	Boolean no_room = False;

	if (kybdlock) {
		char code[64];

		(void) sprintf(code, "%d", cgcode | (with_ge ? 0x100 : 0));
		enq_ta(key_Character_wrapper, code, CN);
		return False;
	}
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (IS_FA(screen_buf[baddr]) || FA_IS_PROTECTED(*fa)) {
		operator_error(KL_OERR_PROTECTED);
		return False;
	}
	if (appres.numeric_lock && FA_IS_NUMERIC(*fa) &&
	    !((cgcode >= CG_0 && cgcode <= CG_9) ||
	      cgcode == CG_minus || cgcode == CG_period)) {
		operator_error(KL_OERR_NUMERIC);
		return False;
	}
	if (reverse || (insert && screen_buf[baddr])) {
		int last_blank = -1;

		/* Find next null, next fa, or last blank */
		end_baddr = baddr;
		if (screen_buf[end_baddr] == CG_space)
			last_blank = end_baddr;
		do {
			INC_BA(end_baddr);
			if (screen_buf[end_baddr] == CG_space)
				last_blank = end_baddr;
			if (screen_buf[end_baddr] == CG_null
			    ||  IS_FA(screen_buf[end_baddr]))
				break;
		} while (end_baddr != baddr);

		/* Pretend a trailing blank is a null, if desired. */
		if (toggled(BLANK_FILL) && last_blank != -1) {
			INC_BA(last_blank);
			if (last_blank == end_baddr) {
				DEC_BA(end_baddr);
				ctlr_add(end_baddr, CG_null, 0);
			}
		}

		/* Check for field overflow. */
		if (screen_buf[end_baddr] != CG_null) {
			if (insert) {
				operator_error(KL_OERR_OVERFLOW);
				return False;
			} else {	/* reverse */
				no_room = True;
			}
		} else {
			/* Shift data over. */
			if (end_baddr > baddr) {
				ctlr_bcopy(baddr, baddr+1, end_baddr - baddr, 0);
			} else {
				ctlr_bcopy(0, 1, end_baddr, 0);
				ctlr_add(0, screen_buf[(ROWS * COLS) - 1], 0);
				ctlr_bcopy(baddr, baddr+1, ((ROWS * COLS) - 1) - baddr, 0);
			}
		}

	}

	/* Replace leading nulls with blanks, if desired. */
	if (formatted && toggled(BLANK_FILL)) {
		int		baddr_sof = fa - screen_buf;
		register int	baddr_fill = baddr;

		DEC_BA(baddr_fill);
		while (baddr_fill != baddr_sof) {

			/* Check for backward line wrap. */
			if ((baddr_fill % COLS) == COLS - 1) {
				Boolean aborted = True;
				register int baddr_scan = baddr_fill;

				/*
				 * Check the field within the preceeding line
				 * for NULLs.
				 */
				while (baddr_scan != baddr_sof) {
					if (screen_buf[baddr_scan] != CG_null) {
						aborted = False;
						break;
					}
					if (!(baddr_scan % COLS))
						break;
					DEC_BA(baddr_scan);
				}
				if (aborted)
					break;
			}

			if (screen_buf[baddr_fill] == CG_null)
				ctlr_add(baddr_fill, CG_space, 0);
			DEC_BA(baddr_fill);
		}
	}

	/* Add the character. */
	if (no_room) {
		do {
			INC_BA(baddr);
		} while (!IS_FA(screen_buf[baddr]));
	} else {
		ctlr_add(baddr, (unsigned char)cgcode,
		    (unsigned char)(with_ge ? 1 : 0));
		ctlr_add_fg(baddr, 0);
		ctlr_add_gr(baddr, 0);
		if (!reverse)
			INC_BA(baddr);
	}

	/* Implement auto-skip, and don't land on attribute bytes. */
	if (IS_FA(screen_buf[baddr]) &&
	    FA_IS_SKIP(screen_buf[baddr]))
		baddr = next_unprotected(baddr);
	else while (IS_FA(screen_buf[baddr]))
		INC_BA(baddr);

	cursor_move(baddr);
	mdt_set(fa);
	return True;
}

/*
 * Handle an ordinary character key, given an ASCII code.
 */
static void
key_ACharacter(c, keytype, cause)
unsigned char	c;
enum keytype	keytype;
enum iaction	cause;
{
	register int i;
	struct akeysym ak;

	ak.keysym = c;
	ak.keytype = keytype;

	switch (composing) {
	    case NONE:
		break;
	    case COMPOSE:
		for (i = 0; i < n_composites; i++)
			if (ak_eq(composites[i].k1, ak) ||
			    ak_eq(composites[i].k2, ak))
				break;
		if (i < n_composites) {
			cc_first.keysym = c;
			cc_first.keytype = keytype;
			composing = FIRST;
			status_compose(True, c);
		} else {
			ring_bell();
			composing = NONE;
			status_compose(False, 0);
		}
		return;
	    case FIRST:
		composing = NONE;
		status_compose(False, 0);
		for (i = 0; i < n_composites; i++)
			if ((ak_eq(composites[i].k1, cc_first) &&
			     ak_eq(composites[i].k2, ak)) ||
			    (ak_eq(composites[i].k1, ak) &&
			     ak_eq(composites[i].k2, cc_first)))
				break;
		if (i < n_composites)
			c = composites[i].translation;
		else {
			ring_bell();
			return;
		}
		break;
	}

	if (toggled(EVENT_TRACE)) {
		(void) fprintf(tracef, " %s -> Key(\"%s\")\n",
		    ia_name[(int) cause], ctl_see((int) c));
	}
	if (IN_3270) {
		if (c < ' ') {
			if (toggled(EVENT_TRACE))
				(void) fprintf(tracef,
					    "  dropped (control char)\n");
			return;
		}
		(void) key_Character((int) asc2cg[c], keytype == APL);
	} else if (IN_ANSI) {
		net_sendc((char) c);
	} else {
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, "  dropped (not connected)\n");
	}
}


/*
 * Simple toggles.
 */
/*ARGSUSED*/
static void
key_AltCr(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_AltCr, event, params, num_params);
	do_toggle(ALT_CURSOR);
}

/*ARGSUSED*/
void
key_MonoCase(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_MonoCase, event, params, num_params);
	do_toggle(MONOCASE);
}

/*
 * Flip the display left-to-right
 */
/*ARGSUSED*/
void
key_Flip(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Flip, event, params, num_params);
	screen_flip();
}



/*
 * Tab forward to next field.
 */
/*ARGSUSED*/
void
key_FTab(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_FTab, event, params, num_params);
	if (IN_ANSI) {
		net_sendc('\t');
		return;
	}
	if (kybdlock) {
		enq_ta(key_FTab, CN, CN);
		return;
	}
	cursor_move(next_unprotected(cursor_addr));
}


/*
 * Tab backward to previous field.
 */
/*ARGSUSED*/
void
key_BTab(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr, nbaddr;
	int		sbaddr;

	debug_action(key_BTab, event, params, num_params);
	if (!IN_3270)
		return;
	if (kybdlock) {
		enq_ta(key_BTab, CN, CN);
		return;
	}
	baddr = cursor_addr;
	DEC_BA(baddr);
	if (IS_FA(screen_buf[baddr]))	/* at bof */
		DEC_BA(baddr);
	sbaddr = baddr;
	while (True) {
		nbaddr = baddr;
		INC_BA(nbaddr);
		if (IS_FA(screen_buf[baddr])
		    &&  !FA_IS_PROTECTED(screen_buf[baddr])
		    &&  !IS_FA(screen_buf[nbaddr]))
			break;
		DEC_BA(baddr);
		if (baddr == sbaddr) {
			cursor_move(0);
			return;
		}
	}
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Deferred keyboard unlock.
 */

/*ARGSUSED*/
static void
defer_unlock(closure, id)
XtPointer closure;
XtIntervalId *id;
{
	kybdlock_clr(KL_DEFERRED_UNLOCK, "defer_unlock");
	status_reset();
	if (CONNECTED)
		ps_process();
}

/*
 * Reset keyboard lock.
 */
void
do_reset(explicit)
Boolean explicit;
{
	/*
	 * If explicit (from the keyboard) and there is typeahead,
	 * simply flush it.
	 */
	if (explicit && flush_ta())
		return;

	/* Always clear insert and reverse modes. */
	insert_mode(False);
	/* reverse_mode(False); */

	/* Otherwise, if not connect, reset is a no-op. */
	if (!CONNECTED)
		return;

	/*
	 * Remove any deferred keyboard unlock.  We will either unlock the
	 * keyboard now, or want to defer further into the future.
	 */
	if (kybdlock & KL_DEFERRED_UNLOCK)
		XtRemoveTimeOut(unlock_id);

	/*
	 * If explicit (from the keyboard), unlock the keyboard now.
	 * Otherwise (from the host), schedule a deferred keyboard unlock.
	 */
	if (explicit) {
		kybdlock_clr(-1, "do_reset");
	} else if (kybdlock &
  (KL_DEFERRED_UNLOCK | KL_OIA_TWAIT | KL_OIA_LOCKED | KL_AWAITING_FIRST)) {
		kybdlock_clr(~KL_DEFERRED_UNLOCK, "do_reset");
		kybdlock_set(KL_DEFERRED_UNLOCK, "do_reset");
		unlock_id = XtAppAddTimeOut(appcontext, UNLOCK_MS,
		    defer_unlock, 0);
	}

	/* Clean up other modes. */
	status_reset();
	mcursor_normal();
	composing = NONE;
	status_compose(False, 0);
}

/*ARGSUSED*/
void
key_Reset(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Reset, event, params, num_params);
	do_reset(True);
}


/*
 * Move to first unprotected field on screen.
 */
/*ARGSUSED*/
void
key_Home(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Home, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_home();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Home, CN, CN);
		return;
	}
	if (!formatted) {
		cursor_move(0);
		return;
	}
	cursor_move(next_unprotected(ROWS*COLS-1));
}


/*
 * Cursor left 1 position.
 */
static void
do_left()
{
	register int	baddr;

	baddr = cursor_addr;
	DEC_BA(baddr);
	cursor_move(baddr);
}

/*ARGSUSED*/
void
key_Left(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Left, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_left();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Left, CN, CN);
		return;
	}
	if (!flipped)
		do_left();
	else {
		register int	baddr;

		baddr = cursor_addr;
		INC_BA(baddr);
		cursor_move(baddr);
	}
}


/*
 * Delete char key.
 * Returns "True" if succeeds, "False" otherwise.
 */
static Boolean
do_delete()
{
	register int	baddr, end_baddr;
	register unsigned char	*fa;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(KL_OERR_PROTECTED);
		return False;
	}
	/* find next fa */
	end_baddr = baddr;
	do {
		INC_BA(end_baddr);
		if (IS_FA(screen_buf[end_baddr]))
			break;
	} while (end_baddr != baddr);
	DEC_BA(end_baddr);
	if (end_baddr > baddr) {
		ctlr_bcopy(baddr+1, baddr, end_baddr - baddr, 0);
	} else if (end_baddr != baddr) {
		ctlr_bcopy(baddr+1, baddr, ((ROWS * COLS) - 1) - baddr, 0);
		ctlr_add((ROWS * COLS) - 1, screen_buf[0], 0);
		ctlr_bcopy(1, 0, end_baddr, 0);
	}
	ctlr_add(end_baddr, CG_null, 0);
	mdt_set(fa);
	return True;
}

/*ARGSUSED*/
void
key_Delete(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Delete, event, params, num_params);
	if (IN_ANSI) {
		net_sendc('\177');
		return;
	}
	if (kybdlock) {
		enq_ta(key_Delete, CN, CN);
		return;
	}
	if (!do_delete())
		return;
	if (reverse) {
		int baddr = cursor_addr;

		DEC_BA(baddr);
		if (!IS_FA(screen_buf[baddr]))
			cursor_move(baddr);
	}
}


/*
 * Backspace.
 */
/*ARGSUSED*/
void
key_BackSpace(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_BackSpace, event, params, num_params);
	if (IN_ANSI) {
		net_send_erase();
	} else {
		if (kybdlock) {
			enq_ta(key_BackSpace, CN, CN);
			return;
		}
		if (reverse)
			(void) do_delete();
		else if (!flipped)
			do_left();
		else {
			register int	baddr;

			baddr = cursor_addr;
			INC_BA(baddr);
			cursor_move(baddr);
		}
	}
}


/*
 * Destructive backspace, like Unix "erase".
 */
/*ARGSUSED*/
void
key_Erase(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int	baddr;
	unsigned char	*fa;

	debug_action(key_Erase, event, params, num_params);
	if (IN_ANSI) {
		net_send_erase();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Erase, CN, CN);
		return;
	}
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (fa == &screen_buf[baddr] || FA_IS_PROTECTED(*fa)) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	if (baddr && fa == &screen_buf[baddr - 1])
		return;
	do_left();
	(void) do_delete();
}


/*
 * Cursor right 1 position.
 */
/*ARGSUSED*/
void
key_Right(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;

	debug_action(key_Right, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_right();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Right, CN, CN);
		return;
	}
	if (!flipped) {
		baddr = cursor_addr;
		INC_BA(baddr);
		cursor_move(baddr);
	} else
		do_left();
}


/*
 * Cursor left 2 positions.
 */
/*ARGSUSED*/
static void
key_Left2(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;

	debug_action(key_Left2, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_Left2, CN, CN);
		return;
	}
	baddr = cursor_addr;
	DEC_BA(baddr);
	DEC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor to previous word.
 */
/*ARGSUSED*/
static void
previous_word(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int baddr;
	int baddr0;
	unsigned char  c;
	Boolean prot;

	debug_action(previous_word, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(previous_word, CN, CN);
		return;
	}
	if (!formatted)
		return;

	baddr = cursor_addr;
	prot = FA_IS_PROTECTED(*get_field_attribute(baddr));

	/* Skip to before this word, if in one now. */
	if (!prot) {
		c = screen_buf[baddr];
		while (!IS_FA(c) && c != CG_space && c != CG_null) {
			DEC_BA(baddr);
			if (baddr == cursor_addr)
				return;
			c = screen_buf[baddr];
		}
	}
	baddr0 = baddr;

	/* Find the end of the preceding word. */
	do {
		c = screen_buf[baddr];
		if (IS_FA(c)) {
			DEC_BA(baddr);
			prot = FA_IS_PROTECTED(*get_field_attribute(baddr));
			continue;
		}
		if (!prot && c != CG_space && c != CG_null)
			break;
		DEC_BA(baddr);
	} while (baddr != baddr0);

	if (baddr == baddr0)
		return;

	/* Go it its front. */
	for (;;) {
		DEC_BA(baddr);
		c = screen_buf[baddr];
		if (IS_FA(c) || c == CG_space || c == CG_null) {
			break;
		}
	}
	INC_BA(baddr);
	cursor_move(baddr);
}


/*
 * Cursor right 2 positions.
 */
/*ARGSUSED*/
static void
key_Right2(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;

	debug_action(key_Right2, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_Right2, CN, CN);
		return;
	}
	baddr = cursor_addr;
	INC_BA(baddr);
	INC_BA(baddr);
	cursor_move(baddr);
}


/* Find the next unprotected word, or -1 */
static int
nu_word(baddr)
int baddr;
{
	int baddr0 = baddr;
	unsigned char c;
	Boolean prot;

	prot = FA_IS_PROTECTED(*get_field_attribute(baddr));

	do {
		c = screen_buf[baddr];
		if (IS_FA(c))
			prot = FA_IS_PROTECTED(c);
		else if (!prot && c != CG_space && c != CG_null)
			return baddr;
		INC_BA(baddr);
	} while (baddr != baddr0);

	return -1;
}

/* Find the next word in this field, or -1 */
static int
nt_word(baddr)
int baddr;
{
	int baddr0 = baddr;
	unsigned char c;
	Boolean in_word = True;

	do {
		c = screen_buf[baddr];
		if (IS_FA(c))
			return -1;
		if (in_word) {
			if (c == CG_space || c == CG_null)
				in_word = False;
		} else {
			if (c != CG_space && c != CG_null)
				return baddr;
		}
		INC_BA(baddr);
	} while (baddr != baddr0);

	return -1;
}


/*
 * Cursor to next unprotected word.
 */
/*ARGSUSED*/
static void
next_word(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;
	unsigned char c;

	debug_action(next_word, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(next_word, CN, CN);
		return;
	}
	if (!formatted)
		return;

	/* If not in an unprotected field, go to the next unprotected word. */
	if (IS_FA(screen_buf[cursor_addr]) ||
	    FA_IS_PROTECTED(*get_field_attribute(cursor_addr))) {
		baddr = nu_word(cursor_addr);
		if (baddr != -1)
			cursor_move(baddr);
		return;
	}

	/* If there's another word in this field, go to it. */
	baddr = nt_word(cursor_addr);
	if (baddr != -1) {
		cursor_move(baddr);
		return;
	}

	/* If in a word, go to just after its end. */
	c = screen_buf[cursor_addr];
	if (c != CG_space && c != CG_null) {
		baddr = cursor_addr;
		do {
			c = screen_buf[baddr];
			if (c == CG_space || c == CG_null) {
				cursor_move(baddr);
				return;
			} else if (IS_FA(c)) {
				baddr = nu_word(baddr);
				if (baddr != -1)
					cursor_move(baddr);
				return;
			}
			INC_BA(baddr);
		} while (baddr != cursor_addr);
	}
	/* Otherwise, go to the next unprotected word. */
	else {
		baddr = nu_word(cursor_addr);
		if (baddr != -1)
			cursor_move(baddr);
	}
}


/*
 * Cursor up 1 position.
 */
/*ARGSUSED*/
void
key_Up(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;

	debug_action(key_Up, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_up();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Up, CN, CN);
		return;
	}
	baddr = cursor_addr - COLS;
	if (baddr < 0)
		baddr = (cursor_addr + (ROWS * COLS)) - COLS;
	cursor_move(baddr);
}


/*
 * Cursor down 1 position.
 */
/*ARGSUSED*/
void
key_Down(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;

	debug_action(key_Down, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_down();
		return;
	}
	if (kybdlock) {
		enq_ta(key_Down, CN, CN);
		return;
	}
	baddr = (cursor_addr + COLS) % (COLS * ROWS);
	cursor_move(baddr);
}


/*
 * Cursor to first field on next line or any lines after that.
 */
/*ARGSUSED*/
void
key_Newline(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;
	register unsigned char	*fa;

	debug_action(key_Newline, event, params, num_params);
	if (IN_ANSI) {
		net_sendc('\n');
		return;
	}
	if (kybdlock) {
		enq_ta(key_Newline, CN, CN);
		return;
	}
	baddr = (cursor_addr + COLS) % (COLS * ROWS);	/* down */
	baddr = (baddr / COLS) * COLS;			/* 1st col */
	fa = get_field_attribute(baddr);
	if (fa != (&screen_buf[baddr]) && !FA_IS_PROTECTED(*fa))
		cursor_move(baddr);
	else
		cursor_move(next_unprotected(baddr));
}


/*
 * DUP key
 */
/*ARGSUSED*/
void
key_Dup(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Dup, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_Dup, CN, CN);
		return;
	}
	if (key_Character(CG_dup, False))
		cursor_move(next_unprotected(cursor_addr));
}


/*
 * FM key
 */
/*ARGSUSED*/
void
key_FieldMark(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_FieldMark, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_FieldMark, CN, CN);
		return;
	}
	(void) key_Character(CG_fm, False);
}


/*
 * Vanilla AID keys.
 */
/*ARGSUSED*/
void
key_Enter(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Enter, event, params, num_params);
	if (kybdlock)
		enq_ta(key_Enter, CN, CN);
	else
		key_AID(AID_ENTER);
}


/*ARGSUSED*/
void
key_SysReq(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_SysReq, event, params, num_params);
	if (kybdlock)
		enq_ta(key_SysReq, CN, CN);
	else
		key_AID(AID_SYSREQ);
}


/*
 * Clear AID key
 */
/*ARGSUSED*/
void
key_Clear(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Clear, event, params, num_params);
	if (IN_ANSI) {
		ansi_send_clear();
		return;
	}
	if (kybdlock && CONNECTED) {
		enq_ta(key_Clear, CN, CN);
		return;
	}
	buffer_addr = 0;
	ctlr_clear(True);
	cursor_move(0);
	if (CONNECTED)
		key_AID(AID_CLEAR);
}


/*
 * Cursor Select key (light pen simulator).
 */
/*ARGSUSED*/
void
key_CursorSelect(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register unsigned char	*fa, *sel;

	debug_action(key_CursorSelect, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_CursorSelect, CN, CN);
		return;
	}
	fa = get_field_attribute(cursor_addr);
	if (!FA_IS_SELECTABLE(*fa)) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	sel = fa + 1;
	switch (*sel) {
	    case CG_greater:		/* > */
		ctlr_add(cursor_addr, CG_question, 0); /* change to ? */
		mdt_clear(fa);
		break;
	    case CG_question:		/* ? */
		ctlr_add(cursor_addr, CG_greater, 0);	/* change to > */
		mdt_set(fa);
		break;
	    case CG_space:		/* space */
	    case CG_null:		/* null */
		key_AID(AID_SELECT);
		break;
	    case CG_ampersand:		/* & */
		key_AID(AID_ENTER);
		break;
	    default:
		operator_error(KL_OERR_PROTECTED);
	}
}


/*
 * Erase End Of Field Key.
 */
/*ARGSUSED*/
void
key_EraseEOF(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;
	register unsigned char	*fa;

	debug_action(key_EraseEOF, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_EraseEOF, CN, CN);
		return;
	}
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	if (formatted) {	/* erase to next field attribute */
		do {
			ctlr_add(baddr, CG_null, 0);
			INC_BA(baddr);
		} while (!IS_FA(screen_buf[baddr]));
		mdt_set(fa);
	} else {	/* erase to end of screen */
		do {
			ctlr_add(baddr, CG_null, 0);
			INC_BA(baddr);
		} while (baddr != 0);
	}
}


/*
 * Erase all Input Key.
 */
/*ARGSUSED*/
void
key_EraseInput(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

	debug_action(key_EraseInput, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_EraseInput, CN, CN);
		return;
	}
	if (formatted) {
		/* find first field attribute */
		baddr = 0;
		do {
			if (IS_FA(screen_buf[baddr]))
				break;
			INC_BA(baddr);
		} while (baddr != 0);
		sbaddr = baddr;
		f = False;
		do {
			fa = screen_buf[baddr];
			if (!FA_IS_PROTECTED(fa)) {
				mdt_clear(&screen_buf[baddr]);
				do {
					INC_BA(baddr);
					if (!f) {
						cursor_move(baddr);
						f = True;
					}
					if (!IS_FA(screen_buf[baddr])) {
						ctlr_add(baddr, CG_null, 0);
					}
				}		while (!IS_FA(screen_buf[baddr]));
			} else {	/* skip protected */
				do {
					INC_BA(baddr);
				} while (!IS_FA(screen_buf[baddr]));
			}
		} while (baddr != sbaddr);
		if (!f)
			cursor_move(0);
	} else {
		ctlr_clear(True);
		cursor_move(0);
	}
}



/*
 * Delete word key.  Backspaces the cursor until it hits the front of a word,
 * deletes characters until it hits a blank or null, and deletes all of these
 * but the last.
 *
 * Which is to say, does a ^W.
 */
/*ARGSUSED*/
void
key_DeleteWord(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr, baddr2, front_baddr, back_baddr, end_baddr;
	register unsigned char	*fa;

	debug_action(key_DeleteWord, event, params, num_params);
	if (IN_ANSI) {
		net_send_werase();
		return;
	}
	if (kybdlock) {
		enq_ta(key_DeleteWord, CN, CN);
		return;
	}
	if (!formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);

	/* Make sure we're on a modifiable field. */
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}

	/* Search backwards for a non-blank character. */
	front_baddr = baddr;
	while (screen_buf[front_baddr] == CG_space ||
	       screen_buf[front_baddr] == CG_null)
		DEC_BA(front_baddr);

	/* If we ran into the edge of the field without seeing any non-blanks,
	   there isn't any word to delete; just move the cursor. */
	if (IS_FA(screen_buf[front_baddr])) {
		cursor_move(front_baddr+1);
		return;
	}

	/* front_baddr is now pointing at a non-blank character.  Now search
	   for the first blank to the left of that (or the edge of the field),
	   leaving front_baddr pointing at the the beginning of the word. */
	while (!IS_FA(screen_buf[front_baddr]) &&
	       screen_buf[front_baddr] != CG_space &&
	       screen_buf[front_baddr] != CG_null)
		DEC_BA(front_baddr);
	INC_BA(front_baddr);

	/* Find the end of the word, searching forward for the edge of the
	   field or a non-blank. */
	back_baddr = front_baddr;
	while (!IS_FA(screen_buf[back_baddr]) &&
	       screen_buf[back_baddr] != CG_space &&
	       screen_buf[back_baddr] != CG_null)
		INC_BA(back_baddr);

	/* Find the start of the next word, leaving back_baddr pointing at it
	   or at the end of the field. */
	while (screen_buf[back_baddr] == CG_space ||
	       screen_buf[back_baddr] == CG_null)
		INC_BA(back_baddr);

	/* Find the end of the field, leaving end_baddr pointing at the field
	   attribute of the start of the next field. */
	end_baddr = back_baddr;
	while (!IS_FA(screen_buf[end_baddr]))
		INC_BA(end_baddr);

	/* Copy any text to the right of the word we are deleting. */
	baddr = front_baddr;
	baddr2 = back_baddr;
	while (baddr2 != end_baddr) {
		ctlr_add(baddr, screen_buf[baddr2], 0);
		INC_BA(baddr);
		INC_BA(baddr2);
	}

	/* Insert nulls to pad out the end of the field. */
	while (baddr != end_baddr) {
		ctlr_add(baddr, CG_null, 0);
		INC_BA(baddr);
	}

	/* Set the MDT and move the cursor. */
	mdt_set(fa);
	cursor_move(front_baddr);
}



/*
 * Delete field key.  Similar to EraseEOF, but it wipes out the entire field
 * rather than just to the right of the cursor, and it leaves the cursor at
 * the front of the field.
 *
 * Which is to say, does a ^U.
 */
/*ARGSUSED*/
void
key_DeleteField(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	register int	baddr;
	register unsigned char	*fa;

	debug_action(key_DeleteField, event, params, num_params);
	if (IN_ANSI) {
		net_send_kill();
		return;
	}
	if (kybdlock) {
		enq_ta(key_DeleteField, CN, CN);
		return;
	}
	if (!formatted)
		return;

	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (FA_IS_PROTECTED(*fa) || IS_FA(screen_buf[baddr])) {
		operator_error(KL_OERR_PROTECTED);
		return;
	}
	while (!IS_FA(screen_buf[baddr]))
		DEC_BA(baddr);
	INC_BA(baddr);
	cursor_move(baddr);
	while (!IS_FA(screen_buf[baddr])) {
		ctlr_add(baddr, CG_null, 0);
		INC_BA(baddr);
	}
	mdt_set(fa);
}



/*
 * Set insert mode key.
 */
/*ARGSUSED*/
void
key_Insert(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_Insert, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_Insert, CN, CN);
		return;
	}
	insert_mode(True);
}


/*
 * Toggle insert mode key.
 */
/*ARGSUSED*/
void
key_ToggleInsert(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_ToggleInsert, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_ToggleInsert, CN, CN);
		return;
	}
	if (insert)
		insert_mode(False);
	else
		insert_mode(True);
}


/*
 * Toggle reverse mode key.
 */
/*ARGSUSED*/
void
key_ToggleReverse(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(key_ToggleReverse, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_ToggleReverse, CN, CN);
		return;
	}
	reverse_mode(!reverse);
}


/*
 * Move the cursor to the first blank after the last nonblank in the
 * field, or if the field is full, to the last character in the field.
 */
/*ARGSUSED*/
void
key_FieldEnd(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int	baddr;
	unsigned char	*fa, c;
	int	last_nonblank = -1;

	debug_action(key_FieldEnd, event, params, num_params);
	if (IN_ANSI)
		return;
	if (kybdlock) {
		enq_ta(key_FieldEnd, CN, CN);
		return;
	}
	if (!formatted)
		return;
	baddr = cursor_addr;
	fa = get_field_attribute(baddr);
	if (fa == &screen_buf[baddr] || FA_IS_PROTECTED(*fa))
		return;

	baddr = fa - screen_buf;
	while (True) {
		INC_BA(baddr);
		c = screen_buf[baddr];
		if (IS_FA(c))
			break;
		if (c != CG_null && c != CG_space)
			last_nonblank = baddr;
	}

	if (last_nonblank == -1) {
		baddr = fa - screen_buf;
		INC_BA(baddr);
	} else {
		baddr = last_nonblank;
		INC_BA(baddr);
		if (IS_FA(screen_buf[baddr]))
			baddr = last_nonblank;
	}
	cursor_move(baddr);
}


/*
 * X-dependent code starts here.
 */

/*
 * Translate a keymap (from an XQueryKeymap or a KeymapNotify event) into
 * a bitmap of Shift, Meta or Alt keys pressed.
 */
#define key_is_down(kc, bitmap) (kc && ((bitmap)[(kc)/8] & (1<<((kc)%8))))
int
state_from_keymap(keymap)
char keymap[32];
{
	static Boolean	initted = False;
	static KeyCode	kc_Shift_L, kc_Shift_R;
	static KeyCode	kc_Meta_L, kc_Meta_R;
	static KeyCode	kc_Alt_L, kc_Alt_R;
	int	pseudo_state = 0;

	if (!initted) {
		kc_Shift_L = XKeysymToKeycode(display, XK_Shift_L);
		kc_Shift_R = XKeysymToKeycode(display, XK_Shift_R);
		kc_Meta_L  = XKeysymToKeycode(display, XK_Meta_L);
		kc_Meta_R  = XKeysymToKeycode(display, XK_Meta_R);
		kc_Alt_L   = XKeysymToKeycode(display, XK_Alt_L);
		kc_Alt_R   = XKeysymToKeycode(display, XK_Alt_R);
		initted = True;
	}
	if (key_is_down(kc_Shift_L, keymap) ||
	    key_is_down(kc_Shift_R, keymap))
		pseudo_state |= ShiftKeyDown;
	if (key_is_down(kc_Meta_L, keymap) ||
	    key_is_down(kc_Meta_R, keymap))
		pseudo_state |= MetaKeyDown;
	if (key_is_down(kc_Alt_L, keymap) ||
	    key_is_down(kc_Alt_R, keymap))
		pseudo_state |= AltKeyDown;
	return pseudo_state;
}
#undef key_is_down

/*
 * Process shift keyboard events.  The code has to look for the raw Shift keys,
 * rather than using the handy "state" field in the event structure.  This is
 * because the event state is the state _before_ the key was pressed or
 * released.  This isn't enough information to distinguish between "left
 * shift released" and "left shift released, right shift still held down"
 * events, for example.
 *
 * This function is also called as part of Focus event processing.
 */
/*ARGSUSED*/
void
key_Shift(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	char	keys[32];

	XQueryKeymap(display, keys);
	shift_event(state_from_keymap(keys));
}

/* Add a key to the extended association table. */
void
add_xk(key, assoc)
KeySym key;
KeySym assoc;
{
	int i;

	for (i = 0; i < nxk; i++)
		if (xk[i].key == key) {
			xk[i].assoc = assoc;
			return;
		}
	xk = (struct xks *)XtRealloc((XtPointer)xk,
	    (nxk + 1) * sizeof(struct xks));
	xk[nxk].key = key;
	xk[nxk].assoc = assoc;
	nxk++;
}

/* Translate a keysym name to a keysym, including APL characters. */
static KeySym
StringToKeysym(s, keytypep)
char *s;
enum keytype *keytypep;
{
	KeySym k;

	if (!strncmp(s, "apl_", 4)) {
		k = APLStringToKeysym(s);
		*keytypep = APL;
	} else {
		k = XStringToKeysym(s);
		*keytypep = LATIN;
	}
	if (k == NoSymbol && strlen(s) == 1)
		k = s[0] & 0xff;
	if (k < ' ')
		k = NoSymbol;
	else if (k > 0xff) {
		int i;

		for (i = 0; i < nxk; i++)
			if (xk[i].key == k) {
				k = xk[i].assoc;
				break;
			}
		if (k > 0xff)
			k = NoSymbol;
	}
	return k;
}

static Boolean
build_composites()
{
	char *c;
	char *cn;
	char *ln;
	char ksname[3][64];
	char junk[2];
	KeySym k[3];
	enum keytype a[3];
	int i;
	struct composite *cp;

	if (!appres.compose_map) {
		xs_warning("Compose: No %s defined", ResComposeMap);
		return False;
	}
	cn = xs_buffer("%s.%s", ResComposeMap, appres.compose_map);
	if ((c = get_resource(cn)) == NULL) {
		xs_warning("Compose: Cannot find %s \"%s\"",
		    ResComposeMap, appres.compose_map);
		return False;
	}
	XtFree(cn);
	while ((ln = strtok(c, "\n"))) {
		Boolean okay = True;

		c = NULL;
		if (sscanf(ln, " %63[^+ \t] + %63[^= \t] =%63s%1s",
		    ksname[0], ksname[1], ksname[2], junk) != 3) {
			xs_warning("Compose: Invalid syntax: %s", ln);
			continue;
		}
		for (i = 0; i < 3; i++) {
			k[i] = StringToKeysym(ksname[i], &a[i]);
			if (k[i] == NoSymbol) {
				xs_warning("Compose: Invalid KeySym: \"%s\"",
				    ksname[i]);
				okay = False;
				break;
			}
		}
		if (!okay)
			continue;
		composites = (struct composite *) XtRealloc((char *)composites,
		    (n_composites + 1) * sizeof(struct composite));
		cp = composites + n_composites;
		cp->k1.keysym = k[0];
		cp->k1.keytype = a[0];
		cp->k2.keysym = k[1];
		cp->k2.keytype = a[1];
		cp->translation = k[2];
		n_composites++;
	}
	return True;
}

/*
 * Called by the toolkit when the "Compose" key is pressed.  "Compose" is
 * implemented by pressing and releasing three keys: "Compose" and two
 * data keys.  For example, "Compose" "s" "s" gives the German "ssharp"
 * character, and "Compose" "C", "," gives a capital "C" with a cedilla
 * (symbol Ccedilla).
 *
 * The mechanism breaks down a little when the user presses "Compose" and
 * then a non-data key.  Oh well.
 */
/*ARGSUSED*/
static void
Compose(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(Compose, event, params, num_params);

	if (!composites && !build_composites())
		return;

	if (composing != NONE) {
		composing = NONE;
		status_compose(False, 0);
		return;
	}
	composing = COMPOSE;
	status_compose(True, 0);
}

/*
 * Called by the toolkit for any key without special actions.
 */
/*ARGSUSED*/
static void
Default(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	XKeyEvent	*kevent = (XKeyEvent *)event;
	char		buf[32];
	KeySym		ks;
	int		ll;

	debug_action(Default, event, params, num_params);
	ll = XLookupString(kevent, buf, 32, &ks, (XComposeStatus *) 0);
	if (ll != 1) {
		if (toggled(EVENT_TRACE))
			(void) fprintf(tracef, " Default: Unknown keysym\n");
		return;
	}

	key_ACharacter((unsigned char) buf[0], LATIN, IA_DEFAULT);
}


/*
 * Key action.
 */
/*ARGSUSED*/
static void
key(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int i;
	KeySym k;
	enum keytype keytype;

	debug_action(key, event, params, num_params);
	for (i = 0; i < *num_params; i++) {
		char *s = params[i];

		k = StringToKeysym(s, &keytype);
		if (k == NoSymbol) {
			popup_an_error("%s: Nonexistent or invalid KeySym: %s",
			    action_name(key), s);
			continue;
		}
		if (k & ~0xff) {
			popup_an_error("%s: Invalid KeySym: %s",
			    action_name(key), s);
			continue;
		}
		key_ACharacter((unsigned char)(k & 0xff), keytype, IA_KEY);
	}
}

/*
 * String action.
 */
/*ARGSUSED*/
static void
string(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	int i;
	int len = 0;
	char *s;

	debug_action(string, event, params, num_params);

	/* Determine the total length of the strings. */
	for (i = 0; i < *num_params; i++)
		len += strlen(params[i]);
	if (!len)
		return;

	/* Allocate a block of memory and copy them in. */
	s = XtMalloc(len + 1);
	*s = '\0';
	for (i = 0; i < *num_params; i++)
		(void) strcat(s, params[i]);

	/* Set the pending string to that and process it. */
	ps_set(s, False);
	ps_process();
}

/*
 * Dual-mode action for the "asciicircum" ("^") key:
 *  If in ANSI mode, pass through untranslated.
 *  If in 3270 mode, translate to "notsign".
 */
/*ARGSUSED*/
static void
circum_not(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(circum_not, event, params, num_params);

	if (IN_3270 && composing == NONE)
		key_ACharacter(0xac, LATIN, IA_KEY);
	else
		key_ACharacter('^', LATIN, IA_KEY);
}

/* PA key action for String actions */
static void
do_pa(n)
int n;
{
	if (n < 1 || n > PA_SZ) {
		popup_an_error("Unknown PA key %d", n);
		return;
	}
	if (kybdlock) {
		char nn[3];

		(void) sprintf(nn, "%d", n);
		enq_ta(key_PA, nn, CN);
		return;
	}
	key_AID(pa_xlate[n-1]);
}

/* PF key action for String actions */
static void
do_pf(n)
int n;
{
	if (n < 1 || n > PF_SZ) {
		popup_an_error("Unknown PF key %d", n);
		return;
	}
	if (kybdlock) {
		char nn[3];

		(void) sprintf(nn, "%d", n);
		enq_ta(key_PF, nn, CN);
		return;
	}
	key_AID(pf_xlate[n-1]);
}

/*
 * Set or clear the keyboard scroll lock.
 */
void
kybd_scroll_lock(lock)
Boolean lock;
{
	if (!IN_3270)
		return;
	if (lock)
		kybdlock_set(KL_SCROLLED, "kybd_scroll_lock");
	else
		kybdlock_clr(KL_SCROLLED, "kybd_scroll_lock");
}

/*
 * Move the cursor back within the legal paste area.
 * Returns a Boolean indicating success.
 */
static Boolean
remargin(lmargin)
int lmargin;
{
	Boolean ever = False;
	int baddr, b0;
	unsigned char *fa;

	baddr = cursor_addr;
	while (BA_TO_COL(baddr) < lmargin) {
		baddr = ROWCOL_TO_BA(BA_TO_ROW(baddr), lmargin);
		if (!ever) {
			b0 = baddr;
			ever = True;
		}
		fa = get_field_attribute(baddr);
		if (fa == &screen_buf[baddr] || FA_IS_PROTECTED(*fa)) {
			baddr = next_unprotected(baddr);
			if (baddr <= b0)
				return False;
		}
	}

	cursor_move(baddr);
	return True;
}

/*
 * Pretend that a sequence of keys was entered at the keyboard.
 *
 * "Pasting" means that the sequence came from the X clipboard.  Returns are
 * ignored; newlines mean "move to beginning of next line"; tabs and formfeeds
 * become spaces.  Backslashes are not special, but ASCII ESC characters are
 * used to signify 3270 Graphic Escapes.
 *
 * "Not pasting" means that the sequence is a login string specified in the
 * hosts file, or a parameter to the String action.  Returns are "move to
 * beginning of next line"; newlines mean "Enter AID" and the termination of
 * processing the string.  Backslashes are processed as in C.
 *
 * Returns the number of unprocessed characters.
 */
int
emulate_input(s, len, pasting)
char *s;
int len;
Boolean pasting;
{
	char c;
	enum { BASE, BACKSLASH, BACKX, BACKP, BACKPA, BACKPF, OCTAL, HEX,
	       XGE } state = BASE;
	int literal;
	int nc;
	enum iaction ia = pasting ? IA_PASTE : IA_STRING;
	int orig_addr = cursor_addr;
	int orig_col = BA_TO_COL(cursor_addr);
	static char dxl[] = "0123456789abcdef";

	/*
	 * In the switch statements below, "break" generally means "consume
	 * this character," while "continue" means "rescan this character."
	 */
	while (len) {

		/*
		 * It isn't possible to unlock the keyboard from a string,
		 * so if the keyboard is locked, it's fatal
		 */
		if (kybdlock) {
			if (toggled(EVENT_TRACE))
				(void) fprintf(tracef,
				    "  keyboard locked, string dropped\n");
			return 0;
		}

		if (pasting && IN_3270) {

			/* Check for cursor wrap to top of screen. */
			if (cursor_addr < orig_addr)
				return len-1;		/* wrapped */

			/* Jump cursor over left margin. */
			if (toggled(MARGINED_PASTE) &&
			    BA_TO_COL(cursor_addr) < orig_col) {
				if (!remargin(orig_col))
					return len-1;
			}
		}

		c = *s;
		switch (state) {
		    case BASE:
			switch (c) {
			    case '\b':
				internal_action(key_Left, ia, CN, CN);
				continue;
			    case '\f':
				if (pasting) {
					key_ACharacter((unsigned char) ' ',
					    LATIN, ia);
				} else {
					internal_action(key_Clear, ia, CN, CN);
					if (IN_3270)
						return len-1;
					else
						break;
				}
			    case '\n':
				if (pasting)
					internal_action(key_Newline, ia, CN, CN);
				else {
					internal_action(key_Enter, ia, CN, CN);
					if (IN_3270)
						return len-1;
				}
				break;
			    case '\r':	/* ignored */
				break;
			    case '\t':
				if (pasting)
					key_ACharacter((unsigned char) ' ',
					    LATIN, ia);
				else
					internal_action(key_FTab, ia, CN, CN);
				break;
			    case '\\':	/* backslashes are NOT special when pasting */
				if (!pasting)
					state = BACKSLASH;
				else
					key_ACharacter((unsigned char) c,
					    LATIN, ia);
				break;
			    case '\033': /* ESC is special only when pasting */
				if (pasting)
					state = XGE;
				break;
			    case '[':	/* APL left bracket */
				if (pasting && appres.apl_mode)
					key_ACharacter(
					    (unsigned char) XK_Yacute,
					    APL, ia);
				else
					key_ACharacter((unsigned char) c,
					    LATIN, ia);
				break;
			    case ']':	/* APL right bracket */
				if (pasting && appres.apl_mode)
					key_ACharacter(
					    (unsigned char) XK_diaeresis,
					    APL, ia);
				else
					key_ACharacter((unsigned char) c,
					    LATIN, ia);
				break;
			default:
				key_ACharacter((unsigned char) c, LATIN,
				    ia);
				break;
			}
			break;
		    case BACKSLASH:	/* last character was a backslash */
			switch (c) {
			    case 'a':
				popup_an_error("%s: bell not supported",
				    action_name(string));
				state = BASE;
				break;
			    case 'b':
				internal_action(key_Left, ia, CN, CN);
				state = BASE;
				break;
			    case 'f':
				internal_action(key_Clear, ia, CN, CN);
				state = BASE;
				if (IN_3270)
					return len-1;
				else
					break;
			    case 'n':
				internal_action(key_Enter, ia, CN, CN);
				state = BASE;
				if (IN_3270)
					return len-1;
				else
					break;
			    case 'p':
				state = BACKP;
				break;
			    case 'r':
				internal_action(key_Newline, ia, CN, CN);
				state = BASE;
				break;
			    case 't':
				internal_action(key_FTab, ia, CN, CN);
				state = BASE;
				break;
			    case 'v':
				popup_an_error("%s: vertical tab not supported",
				    action_name(string));
				state = BASE;
				break;
			    case 'x':
				state = BACKX;
				break;
			    case '0': 
			    case '1': 
			    case '2': 
			    case '3':
			    case '4': 
			    case '5': 
			    case '6': 
			    case '7':
				state = OCTAL;
				literal = 0;
				nc = 0;
				continue;
			default:
				state = BASE;
				continue;
			}
			break;
		    case BACKP:	/* last two characters were "\p" */
			switch (c) {
			    case 'a':
				literal = 0;
				nc = 0;
				state = BACKPA;
				break;
			    case 'f':
				literal = 0;
				nc = 0;
				state = BACKPF;
				break;
			    default:
				popup_an_error("%s: unknown character after \\p",
				    action_name(string));
				state = BASE;
				break;
			}
			break;
		    case BACKPF: /* last three characters were "\pf" */
			if (nc < 2 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("%s: unknown character after \\pf",
				    action_name(string));
				state = BASE;
			} else {
				do_pf(literal);
				if (IN_3270)
					return len-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKPA: /* last three characters were "\pa" */
			if (nc < 1 && isdigit(c)) {
				literal = (literal * 10) + (c - '0');
				nc++;
			} else if (!nc) {
				popup_an_error("%s: unknown character after \\pa",
				    action_name(string));
				state = BASE;
			} else {
				do_pa(literal);
				if (IN_3270)
					return len-1;
				state = BASE;
				continue;
			}
			break;
		    case BACKX:	/* last two characters were "\x" */
			if (isxdigit(c)) {
				state = HEX;
				literal = 0;
				nc = 0;
				continue;
			} else {
				popup_an_error("%s: missing hex digits after \\x",
				    action_name(string));
				state = BASE;
				continue;
			}
		    case OCTAL:	/* have seen \ and one or more octal digits */
			if (nc < 3 && isdigit(c) && c < '8') {
				literal = (literal * 8) + (strchr(dxl, c) - dxl);
				nc++;
				break;
			} else {
				key_ACharacter((unsigned char) literal, LATIN,
				    ia);
				state = BASE;
				continue;
			}
		    case HEX:	/* have seen \ and one or more hex digits */
			if (nc < 2 && isxdigit(c)) {
				literal = (literal * 16) + (strchr(dxl, tolower(c)) - dxl);
				nc++;
				break;
			} else {
				key_ACharacter((unsigned char) literal, LATIN,
				    ia);
				state = BASE;
				continue;
			}
		    case XGE:	/* have seen ESC */
			key_ACharacter((unsigned char) c, APL, ia);
			state = BASE;
			break;
		}
		s++;
		len--;
	}

	switch (state) {
	    case OCTAL:
	    case HEX:
		key_ACharacter((unsigned char) literal, LATIN, ia);
		state = BASE;
		break;
	    case BACKPF:
		if (nc > 0) {
			do_pf(literal);
			state = BASE;
		}
		break;
	    case BACKPA:
		if (nc > 0) {
			do_pa(literal);
			state = BASE;
		}
		break;
	    default:
		break;
	}

	if (state != BASE)
		popup_an_error("%s: missing data after \\",
		    action_name(string));

	return len;
}

/*ARGSUSED*/
static void
paste_callback(w, client_data, selection, type, value, length, format)
Widget w;
XtPointer client_data;
Atom *selection;
Atom *type;
XtPointer value;
unsigned long *length;
int *format;
{
	char *s;
	unsigned long len;

	if ((value == NULL) || (*length == 0)) {
		XtFree(value);

		/* Try the next one. */
		if (n_pasting > pix)
			XtGetSelectionValue(w, paste_atom[pix++], XA_STRING,
			    paste_callback, NULL, paste_time);
		return;
	}

	s = (char *)value;
	len = *length;
	(void) emulate_input(s, (int) len, True);
	n_pasting = 0;

	XtFree(value);
}

static void
insert_selection(w, event, params, num_params)
Widget w;
XButtonEvent *event;
String *params;
Cardinal *num_params;
{
	int	i;
	Atom	a;

	debug_action(insert_selection, (XEvent *)event, params, num_params);
	n_pasting = 0;
	for (i = 0; i < *num_params; i++) {
		a = XInternAtom(display, params[i], True);
		if (a == None) {
			popup_an_error("%s: no atom for selection",
			    action_name(insert_selection));
			continue;
		}
		if (n_pasting < NP)
			paste_atom[n_pasting++] = a;
	}
	pix = 0;
	if (n_pasting > pix) {
		paste_time = event->time;
		XtGetSelectionValue(w, paste_atom[pix++], XA_STRING,
		    paste_callback, NULL, paste_time);
	}
}

/*ARGSUSED*/
static void
ignore(w, event, params, num_params)
Widget w;
XButtonEvent *event;
String *params;
Cardinal *num_params;
{
	debug_action(ignore, (XEvent *)event, params, num_params);
}

/*
 * Set or clear a temporary keymap.
 *
 *   Keymap(x)		toggle keymap "x" (add "x" to the keymap, or if "x"
 *			 was already added, remove it)
 *   Keymap()		removes the previous keymap, if any
 *   Keymap(None)	removes the previous keymap, if any
 */
/*ARGSUSED*/
static void
toggle_keymap(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
	char *k;
	char *kmname, *km;
	XtTranslations trans;
	struct trans_list *t, *prev;
#	define TN (struct trans_list *)NULL

	debug_action(toggle_keymap, event, params, num_params);

	if (check_usage(toggle_keymap, *num_params, 0, 1) < 0)
		return;

	if (*num_params == 0 || !strcmp(params[0], "None")) {
		struct trans_list *next;

		/* Delete all temporary keymaps. */
		for (t = temp_keymaps; t != TN; t = next) {
			XtFree((XtPointer)t->name);
			next = t->next;
			XtFree((XtPointer)t);
		}
		tkm_last = temp_keymaps = TN;
		screen_set_keymap((XtTranslations)NULL);
		keypad_set_keymap((XtTranslations)NULL);
		status_kmap(False);
		return;
	}

	k = params[0];

	/* Check for deleting one keymap. */
	for (prev = TN, t = temp_keymaps; t != TN; prev = t, t = t->next)
		if (!strcmp(k, t->name))
			break;
	if (t != TN) {

		/* Delete the keymap from the list. */
		if (prev != TN)
			prev->next = t->next;
		else
			temp_keymaps = t->next;
		if (tkm_last == t)
			tkm_last = prev;
		XtFree((XtPointer)t->name);
		XtFree((XtPointer)t);

		/* Rebuild the translation tables from the remaining ones. */
		screen_set_keymap((XtTranslations)NULL);
		keypad_set_keymap((XtTranslations)NULL);
		for (t = temp_keymaps; t != TN; t = t->next) {
			trans = lookup_tt(t->name, CN);
			screen_set_keymap(trans);
			keypad_set_keymap(trans);
		}

		/* Update the status line. */
		if (temp_keymaps == TN)
			status_kmap(False);
		return;
	}

	/* Add a keymap. */

	/* Look up the resource. */
	kmname = xs_buffer("%s.%s", ResKeymap, k);
	km = get_resource(kmname);
	XtFree(kmname);
	if (km == CN) {
		popup_an_error("%s: can't find %s.%s",
		    action_name(toggle_keymap), ResKeymap, k);
		return;
	}

	/* Update the translation tables. */
	trans = lookup_tt(k, km);
	XtFree(km);
	screen_set_keymap(trans);
	keypad_set_keymap(trans);

	/* Add it to the list. */
	t = (struct trans_list *)XtMalloc(sizeof(*t));
	t->name = XtNewString(k);
	t->next = TN;
	if (tkm_last != TN)
		tkm_last->next = t;
	else
		temp_keymaps = t;
	tkm_last = t;

	/* Update the status line. */
	status_kmap(True);
}
#undef TN

extern void melt();

XtActionsRec actions[] = {
	{ "AltCursor",  	(XtActionProc)key_AltCr },
	{ "AnsiText",		(XtActionProc)ansi_text_fn },
	{ "Ascii",		(XtActionProc)ascii_fn },
	{ "AsciiField",		(XtActionProc)ascii_field_fn },
	{ "Attn",		(XtActionProc)key_Attn },
	{ "BackSpace",		(XtActionProc)key_BackSpace },
	{ "BackTab",		(XtActionProc)key_BTab },
	{ "CircumNot",		(XtActionProc)circum_not },
	{ "Clear",		(XtActionProc)key_Clear },
	{ "CloseScript",	(XtActionProc)close_script_fn },
	{ "Compose",		(XtActionProc)Compose },
	{ "Configure",		(XtActionProc)configure },
	{ "Connect",		(XtActionProc)Connect },
	{ "ContinueScript",	(XtActionProc)continue_script_fn },
	{ "CursorSelect",	(XtActionProc)key_CursorSelect },
	{ "Cut",		(XtActionProc)Cut },
	{ "Default",		(XtActionProc)Default },
	{ "Delete", 		(XtActionProc)key_Delete },
	{ "DeleteField",	(XtActionProc)key_DeleteField },
	{ "DeleteWord",		(XtActionProc)key_DeleteWord },
	{ "Disconnect",		(XtActionProc)Disconnect },
	{ "Down",		(XtActionProc)key_Down },
	{ "Dup",		(XtActionProc)key_Dup },
	{ "Ebcdic",		(XtActionProc)ebcdic_fn },
	{ "EbcdicField",	(XtActionProc)ebcdic_field_fn },
	{ "Enter",		(XtActionProc)key_Enter },
	{ "EnterLeave",		(XtActionProc)enter_leave },
	{ "Erase",		(XtActionProc)key_Erase },
	{ "EraseEOF",		(XtActionProc)key_EraseEOF },
	{ "EraseInput",		(XtActionProc)key_EraseInput },
	{ "Execute",		(XtActionProc)execute_fn },
	{ "Expect",		(XtActionProc)expect_fn },
	{ "FieldEnd",		(XtActionProc)key_FieldEnd },
	{ "FieldMark",		(XtActionProc)key_FieldMark },
	{ "Flip",		(XtActionProc)key_Flip },
	{ "FocusEvent",		(XtActionProc)focus_change },
	{ "GraphicsExpose",	(XtActionProc)graphics_expose },
	{ "HandleMenu",		(XtActionProc)handle_menu },
	{ "HardPrint",		(XtActionProc)print_text },
	{ "Home",		(XtActionProc)key_Home },
	{ "Info",		(XtActionProc)Info },
	{ "Insert",		(XtActionProc)key_Insert },
	{ "Key",		(XtActionProc)key },
	{ "Keymap",		(XtActionProc)toggle_keymap },
	{ "KeymapEvent",	(XtActionProc)keymap_event },
	{ "Left",		(XtActionProc)key_Left },
	{ "Left2", 		(XtActionProc)key_Left2 },
	{ "Melt",		(XtActionProc)melt },
	{ "MonoCase",		(XtActionProc)key_MonoCase },
	{ "MoveCursor",		(XtActionProc)MoveCursor },
	{ "Newline",		(XtActionProc)key_Newline },
	{ "NextWord",		(XtActionProc)next_word },
	{ "PauseScript",	(XtActionProc)pause_script_fn },
	{ "PA",			(XtActionProc)key_PA },
	{ "PF",			(XtActionProc)key_PF },
	{ "PreviousWord",	(XtActionProc)previous_word },
	{ "PrintText",		(XtActionProc)print_text },
	{ "PrintWindow",	(XtActionProc)print_window },
	{ "Quit",		(XtActionProc)quit_event },
	{ "Reconnect",		(XtActionProc)Reconnect },
	{ "Redraw",		(XtActionProc)redraw },
	{ "Reset",		(XtActionProc)key_Reset },
	{ "Right",		(XtActionProc)key_Right },
	{ "Right2",		(XtActionProc)key_Right2 },
	{ "SetFont",		(XtActionProc)set_font },
	{ "Shift",		(XtActionProc)key_Shift },
	{ "StateChanged",	(XtActionProc)state_event },
	{ "String",		(XtActionProc)string },
	{ "SysReq",		(XtActionProc)key_SysReq },
	{ "Tab",		(XtActionProc)key_FTab },
	{ "ToggleInsert",	(XtActionProc)key_ToggleInsert },
	{ "ToggleReverse",	(XtActionProc)key_ToggleReverse },
	{ "Up",			(XtActionProc)key_Up },
	{ "Visible",		(XtActionProc)visible },
	{ "WMProtocols",	(XtActionProc)wm_protocols },
	{ "Wait",		(XtActionProc)wait_fn },
	{ "confirm",		(XtActionProc)Confirm },
	{ "ignore",		(XtActionProc)ignore },
	{ "insert-selection",	(XtActionProc)insert_selection },
	{ "move-select",	(XtActionProc)move_select },
	{ "select-end",		(XtActionProc)select_end },
	{ "select-extend",	(XtActionProc)select_extend },
	{ "select-start",	(XtActionProc)select_start },
	{ "set-select",		(XtActionProc)set_select },
	{ "start-extend",	(XtActionProc)start_extend },
};

int actioncount = XtNumber(actions);
