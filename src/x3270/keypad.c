/*
 * Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	keypad.c
 *		This module handles the keypad with buttons for each of the
 *		3270 function keys.
 */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include "globals.h"
#include "keypad.bm"

#define NUM_ROWS	4
#define NUM_VROWS	14
#define BORDER		1
#define TOP_MARGIN	6
#define BOTTOM_MARGIN	6

#define SPACING		2
#define FAT_SPACING	3
#define VGAP		4
#define HGAP		4
#define SIDE_MARGIN	4

#define HORIZ_WIDTH \
	(SIDE_MARGIN + \
	 12*(pf_width+2*BORDER) + \
	 11*SPACING + \
	 HGAP + \
	 3*(pa_width+2*BORDER) + \
	 2*SPACING + \
	 SIDE_MARGIN)

#define VERT_WIDTH \
	(SIDE_MARGIN + \
	 3*(pa_width+2*BORDER) + \
	 2*SPACING + \
	 SIDE_MARGIN)

/*
 * Table of 3278 key labels and actions
 */
struct button_list {
	char *label;
	char *name;
	char *bits;
	int width;
	int height;
	void (*fn)();
};

static struct button_list pf_list[] = {
	{ "PF13",	"large", (char *)0, 0, 0,	key_PF13 },
	{ "PF14",	"large", (char *)0, 0, 0,	key_PF14 },
	{ "PF15",	"large", (char *)0, 0, 0,	key_PF15 },
	{ "PF16",	"large", (char *)0, 0, 0,	key_PF16 },
	{ "PF17",	"large", (char *)0, 0, 0,	key_PF17 },
	{ "PF18",	"large", (char *)0, 0, 0,	key_PF18 },
	{ "PF19",	"large", (char *)0, 0, 0,	key_PF19 },
	{ "PF20",	"large", (char *)0, 0, 0,	key_PF20 },
	{ "PF21",	"large", (char *)0, 0, 0,	key_PF21 },
	{ "PF22",	"large", (char *)0, 0, 0,	key_PF22 },
	{ "PF23",	"large", (char *)0, 0, 0,	key_PF23 },
	{ "PF24",	"large", (char *)0, 0, 0,	key_PF24 },
	{ "PF1",	"large", (char *)0, 0, 0,	key_PF1 },
	{ "PF2",	"large", (char *)0, 0, 0,	key_PF2 },
	{ "PF3",	"large", (char *)0, 0, 0,	key_PF3 },
	{ "PF4",	"large", (char *)0, 0, 0,	key_PF4 },
	{ "PF5",	"large", (char *)0, 0, 0,	key_PF5 },
	{ "PF6",	"large", (char *)0, 0, 0,	key_PF6 },
	{ "PF7",	"large", (char *)0, 0, 0,	key_PF7 },
	{ "PF8",	"large", (char *)0, 0, 0,	key_PF8 },
	{ "PF9",	"large", (char *)0, 0, 0,	key_PF9 },
	{ "PF10",	"large", (char *)0, 0, 0,	key_PF10 },
	{ "PF11",	"large", (char *)0, 0, 0,	key_PF11 },
	{ "PF12",	"large", (char *)0, 0, 0,	key_PF12 }
};
#define PF_SZ (sizeof(pf_list)/sizeof(pf_list[0]))

static struct button_list pad_list[] = {
	{ "PA1",	"large", (char *)0, 0, 0,	key_PA1 },
	{ "PA2",	"large", (char *)0, 0, 0,	key_PA2 },
	{ "PA3",	"large", (char *)0, 0, 0,	key_PA3 },
	{ 0, 0, 0 },
	{ "Up" ,	"bm", (char *)up_bits, up_width, up_height,	key_Up },
	{ 0, 0, 0 },
	{ "Left" ,	"bm", (char *)left_bits, left_width, left_height,	key_Left },
	{ "Home",	"bm", (char *)home_bits, home_width, home_height,	key_Home },
	{ "Right" ,	"bm", (char *)right_bits, right_width, right_height,	key_Right },
	{ 0, 0, 0 },
	{ "Down" ,	"bm", (char *)down_bits, down_width, down_height,	key_Down },
	{ 0, 0, 0 },
};
#define PAD_SZ (sizeof(pad_list)/sizeof(pad_list[0]))

static struct button_list lower_list[] = {
	{ "Clear",	"small", (char *)0, 0, 0,	key_Clear },
	{ "Reset",	"small", (char *)0, 0, 0,	key_Reset },

	{ "Ins",	"bm", (char *)ins_bits, ins_width, ins_height,	key_Insert },
	{ "Del",	"bm", (char *)del_bits, del_width, del_height,	key_Delete },

	{ "Erase\nEOF",	"small", (char *)0, 0, 0,	key_EraseEOF },
	{ "Erase\nInput",	"small", (char *)0, 0, 0,	key_EraseInput },

	{ "Dup",	"small", (char *)0, 0, 0,	key_Dup },
	{ "Field\nMark",	"small", (char *)0, 0, 0,	key_FieldMark },

	{ "Sys\nReq",	"small", (char *)0, 0, 0,	key_SysReq },
	{ "Cursor\nSelect",	"small", (char *)0, 0, 0,	key_CursorSelect },

	{ "Attn",	"small", (char *)0, 0, 0,	key_Attn },
	{ "Mono\nCase",	"small", (char *)0, 0, 0,	key_MonoCase },

	{ "Btab",	"bm", (char *)btab_bits, btab_width, btab_height,	key_BTab },
	{ "Tab",	"bm", (char *)tab_bits, tab_width, tab_height,	key_FTab },

	{ 0, 0, 0 },
	{ "Enter",	"small", (char *)0, 0, 0,	key_Enter }
};
#define LOWER_SZ (sizeof(lower_list)/sizeof(lower_list[0]))

static struct button_list vpf_list[] = {
	{ "PF1",	"large", (char *)0, 0, 0,	key_PF1 },
	{ "PF2",	"large", (char *)0, 0, 0,	key_PF2 },
	{ "PF3",	"large", (char *)0, 0, 0,	key_PF3 },
	{ "PF4",	"large", (char *)0, 0, 0,	key_PF4 },
	{ "PF5",	"large", (char *)0, 0, 0,	key_PF5 },
	{ "PF6",	"large", (char *)0, 0, 0,	key_PF6 },
	{ "PF7",	"large", (char *)0, 0, 0,	key_PF7 },
	{ "PF8",	"large", (char *)0, 0, 0,	key_PF8 },
	{ "PF9",	"large", (char *)0, 0, 0,	key_PF9 },
	{ "PF10",	"large", (char *)0, 0, 0,	key_PF10 },
	{ "PF11",	"large", (char *)0, 0, 0,	key_PF11 },
	{ "PF12",	"large", (char *)0, 0, 0,	key_PF12 },
};
#define VPF_SZ (sizeof(vpf_list)/sizeof(vpf_list[0]))

static struct button_list vspf_list[] = {
	{ "PF13",	"large", (char *)0, 0, 0,	key_PF13 },
	{ "PF14",	"large", (char *)0, 0, 0,	key_PF14 },
	{ "PF15",	"large", (char *)0, 0, 0,	key_PF15 },
	{ "PF16",	"large", (char *)0, 0, 0,	key_PF16 },
	{ "PF17",	"large", (char *)0, 0, 0,	key_PF17 },
	{ "PF18",	"large", (char *)0, 0, 0,	key_PF18 },
	{ "PF19",	"large", (char *)0, 0, 0,	key_PF19 },
	{ "PF20",	"large", (char *)0, 0, 0,	key_PF20 },
	{ "PF21",	"large", (char *)0, 0, 0,	key_PF21 },
	{ "PF22",	"large", (char *)0, 0, 0,	key_PF22 },
	{ "PF23",	"large", (char *)0, 0, 0,	key_PF23 },
	{ "PF24",	"large", (char *)0, 0, 0,	key_PF24 },
};
static Widget vpf_w[2][VPF_SZ];

static struct button_list vpad_list[] = {
	{ 0, 0, 0 },
	{ "Up" ,	"bm", (char *)up_bits, up_width, up_height,	key_Up },
	{ 0, 0, 0 },
	{ "Left" ,	"bm", (char *)left_bits, left_width, left_height,	key_Left },
	{ "Home",	"bm", (char *)home_bits, home_width, home_height,	key_Home },
	{ "Right" ,	"bm", (char *)right_bits, right_width, right_height,	key_Right },
	{ "Ins",	"bm", (char *)ins_bits, ins_width, ins_height,	key_Insert },
	{ "Down" ,	"bm", (char *)down_bits, down_width, down_height,	key_Down },
	{ "Del",	"bm", (char *)del_bits, del_width, del_height,	key_Delete },
	{ "PA1",	"large", (char *)0, 0, 0,	key_PA1 },
	{ "PA2",	"large", (char *)0, 0, 0,	key_PA2 },
	{ "PA3",	"large", (char *)0, 0, 0,	key_PA3 },
};
#define VPAD_SZ (sizeof(vpad_list)/sizeof(vpad_list[0]))

static struct button_list vfn_list[] = {
	{ "Btab",	"bm", (char *)btab_bits, btab_width, btab_height,	key_BTab },
	{ "Tab",	"bm", (char *)tab_bits, tab_width, tab_height,	key_FTab },
	{ "Clear",	"small", (char *)0, 0, 0,	key_Clear },
	{ "Reset",	"small", (char *)0, 0, 0,	key_Reset },
	{ "Erase\nEOF",	"small", (char *)0, 0, 0,	key_EraseEOF },
	{ "Erase\nInput",	"small", (char *)0, 0, 0,	key_EraseInput },
	{ "Dup",	"small", (char *)0, 0, 0,	key_Dup },
	{ "Field\nMark",	"small", (char *)0, 0, 0,	key_FieldMark },
	{ "Sys\nReq",	"small", (char *)0, 0, 0,	key_SysReq },
	{ "Cursor\nSelect",	"small", (char *)0, 0, 0,	key_CursorSelect },
	{ "Attn",	"small", (char *)0, 0, 0,	key_Attn },
	{ "Mono\nCase",	"small", (char *)0, 0, 0,	key_MonoCase },
};
#define VFN_SZ (sizeof(vfn_list)/sizeof(vfn_list[0]))

static Dimension pf_width;
static Dimension key_height;
static Dimension pa_width;
static Dimension key_width;
static Dimension large_key_width;


/*
 * Callback for keypad buttons.  Simply calls the function pointed to by the
 * client data.
 */
/*ARGSUSED*/
static void
callfn(w, client_data, call_data)
	Widget w;
	XtPointer client_data;
	XtPointer call_data;
{
	struct button_list *keyd = (struct button_list *) client_data;

	(*keyd->fn)();
}

/*
 * Create a button.
 */
static Widget
make_a_button(container, x, y, w, h, keyd)
	Widget container;
	Position x, y;
	Dimension w, h;
	struct button_list *keyd;
{
	Widget command;
	Pixmap pixmap;

	if (!keyd->label)
		return (Widget) 0;

	command = XtVaCreateManagedWidget(
	    keyd->name, commandWidgetClass, container,
	    XtNx, x,
	    XtNy, y,
	    XtNwidth, w,
	    XtNheight, h,
	    XtNresize, False,
	    NULL);
	XtAddCallback(command, XtNcallback, callfn, (XtPointer)keyd);
	if (keyd->bits) {
		pixmap = XCreateBitmapFromData(display, root_window,
		    keyd->bits, keyd->width, keyd->height);
		XtVaSetValues(command, XtNbitmap, pixmap, NULL);
	} else
		XtVaSetValues(command, XtNlabel, keyd->label, NULL);
	return command;
}

/*
 * Create the keys for a horizontal keypad
 */
static void
keypad_keys_horiz(container)
	Widget container;
{
	int i;
	Position row, col;
	Position x0, y0;

	/* PF Keys */
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN;
	for (i = 0; i < PF_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pf_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pf_width,
		    key_height,
		    &pf_list[i]);
		if (++col >= 12) {
			col = 0;
			row++;
		}
	}

	/* Keypad */
	row = col = 0;
	x0 = SIDE_MARGIN + 12*(pf_width+2*BORDER+SPACING) + HGAP;
	y0 = TOP_MARGIN;
	for (i = 0; i < PAD_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &pad_list[i]);
		if (++col >= 3) {
			col = 0;
			if (++row == 1)
				y0 += VGAP;
		}
	}

	/* Bottom */
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN + 2*(key_height+2*BORDER+SPACING) + VGAP;

	for (i = 0; i < LOWER_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(key_width+2*BORDER+FAT_SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    key_width,
		    key_height,
		    &lower_list[i]);
		if (++row >= 2) {
			++col;
			row = 0;
		}
		if (row == 1 && col == 7)
			x0 += HGAP;
	}
}

static Boolean vert_keypad = False;
static Widget spf_container;

/*
 * Create the keys for a vertical keypad.
 */
static void
keypad_keys_vert(container)
	Widget container;
{
	int i;
	Position row, col;
	Position x0, y0;
	Widget c1, c2;

	vert_keypad = True;

	/* Container for shifted PF keys */
	spf_container = XtVaCreateManagedWidget(
	    "shift", compositeWidgetClass, container,
	    XtNmappedWhenManaged, False,
	    XtNborderWidth, 0,
	    XtNwidth, VERT_WIDTH,
	    XtNheight, TOP_MARGIN+4*(key_height+2*BORDER)+3*SPACING,
	    NULL);
	if (appres.mono)
		XtVaSetValues(spf_container, XtNbackgroundPixmap, gray, NULL);
	else
		XtVaSetValues(spf_container, XtNbackground, appres.keypadbg, NULL);

	/* PF keys */
	if (appres.invert_kpshift) {
		c1 = spf_container;
		c2 = container;
	} else {
		c1 = container;
		c2 = spf_container;
	}
	row = col = 0;
	x0 = SIDE_MARGIN;
	y0 = TOP_MARGIN;
	for (i = 0; i < VPF_SZ; i++) {
		vpf_w[0][i] = make_a_button(c1,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vpf_list[i]);
		vpf_w[1][i] = make_a_button(c2,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vspf_list[i]);
		if (++col >= 3) {
			col = 0;
			row++;
		}
	}

	/* Cursor and PA keys */
	for (i = 0; i < VPAD_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(pa_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    pa_width,
		    key_height,
		    &vpad_list[i]);
		if (++col >= 3) {
			col = 0;
			row++;
		}
	}

	/* Other keys */
	for (i = 0; i < VFN_SZ; i++) {
		(void) make_a_button(container,
		    (Position)(x0 + (col*(large_key_width+2*BORDER+SPACING))),
		    (Position)(y0 + (row*(key_height+2*BORDER+SPACING))),
		    large_key_width,
		    key_height,
		    &vfn_list[i]);
		if (++col >= 2) {
			col = 0;
			row++;
		}
	}
}

static Dimension
get_keypad_dimension(name)
char *name;
{
	static char rname[64];
	char *d;
	long v;

	(void) sprintf(rname, "keypad.%s", name);
	if (!(d = get_resource(rname)))
		XtError("Can't find keypad dimension resource");
	if ((v = strtol(d, (char **)0, 0)) <= 0)
		XtError("Illegal keypad dimension resource");
	XtFree(d);
	return (Dimension)v;
}

static void
init_keypad_dimensions()
{
	static Boolean done = False;

	if (done)
		return;
	key_height = get_keypad_dimension("keyHeight");
	key_width = get_keypad_dimension("keyWidth");
	pf_width = get_keypad_dimension("pfWidth");
	pa_width = get_keypad_dimension("paWidth");
	large_key_width = get_keypad_dimension("largeKeyWidth");
	done = True;
}

Dimension
min_keypad_width()
{
	init_keypad_dimensions();
	return HORIZ_WIDTH;
}

/*
 * Create a keypad.
 */
Widget
keypad_init(container, voffset, screen_width, floating, vert)
	Widget container;
	Dimension voffset;
	Dimension screen_width;
	Boolean floating;
	Boolean vert;
{
	Widget key_pad;
	Dimension height;
	Dimension width = screen_width;
	Dimension hoffset;

	init_keypad_dimensions();

	/* Figure out what dimensions to use */
	if (vert)
		width = VERT_WIDTH;
	else
		width = HORIZ_WIDTH;

	if (vert)
		height = TOP_MARGIN +
		   (NUM_VROWS*(key_height+2*BORDER)) + (NUM_VROWS-1)*SPACING +
		   BOTTOM_MARGIN;
	else
		height = TOP_MARGIN +
		   (NUM_ROWS*(key_height+2*BORDER)) + (NUM_ROWS-1)*SPACING + VGAP +
		   BOTTOM_MARGIN;

	/* Create a container */
	if (screen_width > width)
		hoffset = ((screen_width - width) / (unsigned) 2) & ~1;
	else
		hoffset = 0;
	if (voffset & 1)
		voffset++;
	key_pad = XtVaCreateManagedWidget(
		"keyPad", compositeWidgetClass, container,
		XtNx, hoffset,
		XtNy, voffset,
		XtNborderWidth, (Dimension) (floating ? 1 : 0),
		XtNwidth, width,
		XtNheight, height,
		NULL);
	if (appres.mono)
		XtVaSetValues(key_pad, XtNbackgroundPixmap, gray, NULL);
	else
		XtVaSetValues(key_pad, XtNbackground, appres.keypadbg, NULL);

	/* Create the keys */
	if (vert)
		keypad_keys_vert(key_pad);
	else
		keypad_keys_horiz(key_pad);

	return key_pad;
}

/*
 * Swap PF1-12 and PF13-24 on the vertical popup keypad, by mapping or
 * unmapping the window containing the shifted keys.
 */
void
keypad_shift()
{
	if (!vert_keypad || !XtIsRealized(spf_container))
		return;

	if (shifted)
		XtMapWidget(spf_container);
	else
		XtUnmapWidget(spf_container);
}


/*
 * Keypad popup
 */
Widget keypad_shell = NULL;
Boolean keypad_popped = False;
Boolean up_once = False;

/* Called from main when a popup keypad is initially on */
void
keypad_at_startup()
{
	Boolean v;

	if (!keypad || kp_placement == kp_integral)
		return;
	 keypad_popup_init();

	 /*
	  * Normally, we could just pop up the window here.  However, there
	  * is a bug in olwm that allows the pop-up to appear even if its
	  * main window is iconified.  This only happens at start-up; after
	  * that, the pop-ups disappear and reappear as the main window is
	  * iconified and restored, without help.
	  *
	  * The following if(), and the keypad_first_up() function below
	  * (called when the main window state changes to un-iconified) are a
	  * workaround.
	  */
	 XtVaGetValues(toplevel, XtNiconic, &v, NULL);
	 if (!v)
		 popup_popup(keypad_shell, XtGrabNone);
}

/* Called when we become un-iconified, to pop up the keypad, once */
void
keypad_first_up()
{
	if (!appres.keypad_on || kp_placement == kp_integral || up_once)
		return;
	popup_popup(keypad_shell, XtGrabNone);
}

/* Called when the keypad popup pops up or down */
/*ARGSUSED*/
static void
keypad_changed(w, client_data, call_data)
Widget w;
XtPointer client_data;
XtPointer call_data;
{
	keypad_popped = *(Boolean *)client_data;
	if (keypad_popped)
		up_once = True;
	menubar_keypad_changed();
}

static Boolean TrueD = True;
static Boolean *TrueP = &TrueD;
static Boolean FalseD = False;
static Boolean *FalseP = &FalseD;

/* Create the pop-up keypad */
void
keypad_popup_init()
{
	Widget w;
	Widget keypad_container;
	Dimension height, width, border;
	Boolean vert = (kp_placement == kp_right);
	extern enum placement *RightP, *BottomP;

	if (keypad_shell != NULL)
		return;

	/* Create a popup shell */

	keypad_shell = XtVaCreatePopupShell(
	    "keypadPopup", transientShellWidgetClass, toplevel,
	    NULL);
	XtAddCallback(keypad_shell, XtNpopupCallback, place_popup,
	    (XtPointer)(vert ? RightP : BottomP));
	XtAddCallback(keypad_shell, XtNpopupCallback, keypad_changed,
	    (XtPointer) TrueP);
	XtAddCallback(keypad_shell, XtNpopdownCallback, keypad_changed,
	    (XtPointer) FalseP);

	/* Create a keypad in the popup */

	keypad_container = XtVaCreateManagedWidget(
	    "container", compositeWidgetClass, keypad_shell,
	    XtNborderWidth, 0,
	    XtNheight, 10,
	    XtNwidth, 10,
	    NULL);
	w = keypad_init(keypad_container, 0, 0, True, vert);

	/* Fix the window size */

	XtVaGetValues(w,
	    XtNheight, &height,
	    XtNwidth, &width,
	    XtNborderWidth, &border,
	    NULL);
	height += 2*border;
	width += 2*border;
	XtVaSetValues(keypad_container,
	    XtNheight, height,
	    XtNwidth, width,
	    NULL);
	XtVaSetValues(keypad_shell,
	    XtNheight, height,
	    XtNwidth, width,
	    XtNbaseHeight, height,
	    XtNbaseWidth, width,
	    XtNminHeight, height,
	    XtNminWidth, width,
	    XtNmaxHeight, height,
	    XtNmaxWidth, width,
	    NULL);

	/* Make keystrokes in the popup apply to the main window */

	set_translations(keypad_container);
}
