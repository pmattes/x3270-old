/*
 * Copyright 1993, 1994, 1995 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	toggle.c
 *		Basic support for toggles.
 */

#include "globals.h"
#include "resources.h"

struct toggle_name toggle_names[N_TOGGLES] = {
	{ ResMonoCase,        MONOCASE },
	{ ResAltCursor,       ALT_CURSOR },
	{ ResCursorBlink,     CURSOR_BLINK },
	{ ResShowTiming,      SHOW_TIMING },
	{ ResCursorPos,       CURSOR_POS },
	{ ResDsTrace,         DS_TRACE },
	{ ResScrollBar,       SCROLL_BAR },
	{ ResLineWrap,        LINE_WRAP },
	{ ResBlankFill,       BLANK_FILL },
	{ ResScreenTrace,     SCREEN_TRACE },
	{ ResEventTrace,      EVENT_TRACE },
	{ ResMarginedPaste,   MARGINED_PASTE },
	{ ResRectangleSelect, RECTANGLE_SELECT }
};

/*
 * Called from system initialization code to handle initial toggle settings.
 */
void
initialize_toggles()
{
	/* Define the upcalls. */
	appres.toggle[MONOCASE].upcall =         toggle_monocase;
	appres.toggle[ALT_CURSOR].upcall =       toggle_altCursor;
	appres.toggle[CURSOR_BLINK].upcall =     toggle_cursorBlink;
	appres.toggle[SHOW_TIMING].upcall =      toggle_showTiming;
	appres.toggle[CURSOR_POS].upcall =       toggle_cursorPos;
	appres.toggle[DS_TRACE].upcall =         toggle_dsTrace;
	appres.toggle[SCROLL_BAR].upcall =       toggle_scrollBar;
	appres.toggle[LINE_WRAP].upcall =        toggle_lineWrap;
	appres.toggle[BLANK_FILL].upcall =       toggle_nop;
	appres.toggle[SCREEN_TRACE].upcall =     toggle_screenTrace;
	appres.toggle[EVENT_TRACE].upcall =      toggle_eventTrace;
	appres.toggle[MARGINED_PASTE].upcall =   toggle_nop;
	appres.toggle[RECTANGLE_SELECT].upcall = toggle_nop;

	/* Call the initial upcalls. */
	if (toggled(DS_TRACE))
		appres.toggle[DS_TRACE].upcall(&appres.toggle[DS_TRACE],
		    TT_INITIAL);
	if (toggled(EVENT_TRACE))
		appres.toggle[EVENT_TRACE].upcall(&appres.toggle[EVENT_TRACE],
		    TT_INITIAL);
	if (toggled(SCREEN_TRACE))
		appres.toggle[SCREEN_TRACE].upcall(&appres.toggle[SCREEN_TRACE],
		    TT_INITIAL);
}

/*
 * Called from system exit code to handle toggles.
 */
void
shutdown_toggles()
{
	/* Clean up the data stream trace monitor window. */
	if (toggled(DS_TRACE)) {
		appres.toggle[DS_TRACE].value = False;
		toggle_dsTrace(&appres.toggle[DS_TRACE], TT_FINAL);
	}
	if (toggled(EVENT_TRACE)) {
		appres.toggle[EVENT_TRACE].value = False;
		toggle_dsTrace(&appres.toggle[EVENT_TRACE], TT_FINAL);
	}

	/* Clean up the screen trace file. */
	if (toggled(SCREEN_TRACE)) {
		appres.toggle[SCREEN_TRACE].value = False;
		toggle_screenTrace(&appres.toggle[SCREEN_TRACE], TT_FINAL);
	}
}

void
do_toggle(index)
int index;
{
	struct toggle *t = &appres.toggle[index];

	/*
	 * Change the value, call the internal update routine, and reset the
	 * menu label(s).
	 */
	toggle_toggle(t);
	t->upcall(t, TT_TOGGLE);
	menubar_retoggle(t);
}

/*
 * No-op toggle upcall.
 */
/*ARGSUSED*/
void
toggle_nop(t, tt)
struct toggle *t;
enum toggle_type tt;
{
}
