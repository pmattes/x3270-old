/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 *
 *                         All Rights Reserved
 */

/*
 *	keypanel.c
 *		This module handles the special 3270 keys using a panel
 *		of buttons.  All the PF keys as well as the PA1, PA2,
 *		Clear, Enter and SysReq keys are on the panel.  Only
 *		12 PF keys are displayed at a time; the other 12 are
 *		accessed by hitting the button labelled "Shift" (which
 *		is NOT the same as the Shift-key on the keyboard).
 */

#include <suntool/sunview.h>
#include <suntool/panel.h>
#include <suntool/canvas.h>
#include <stdio.h>

#define SPACING		2

#define ITEM_COORD(n)	(((n) * 32) + ((n) * SPACING) + SPACING)

static unsigned short	BtnClear_image[] = {
#include "BtnClear"
};
mpr_static (BtnClear_pixrect, 32, 32, 1, BtnClear_image);

static unsigned short	BtnEnter_image[] = {
#include "BtnEnter"
};
mpr_static (BtnEnter_pixrect, 32, 32, 1, BtnEnter_image);

static unsigned short	BtnPA1_image[] = {
#include "BtnPA1"
};
mpr_static (BtnPA1_pixrect, 32, 32, 1, BtnPA1_image);

static unsigned short	BtnPA2_image[] = {
#include "BtnPA2"
};
mpr_static (BtnPA2_pixrect, 32, 32, 1, BtnPA2_image);

static unsigned short	BtnPF01_image[] = {
#include "BtnPF01"
};
mpr_static (BtnPF01_pixrect, 32, 32, 1, BtnPF01_image);

static unsigned short	BtnPF02_image[] = {
#include "BtnPF02"
};
mpr_static (BtnPF02_pixrect, 32, 32, 1, BtnPF02_image);

static unsigned short	BtnPF03_image[] = {
#include "BtnPF03"
};
mpr_static (BtnPF03_pixrect, 32, 32, 1, BtnPF03_image);

static unsigned short	BtnPF04_image[] = {
#include "BtnPF04"
};
mpr_static (BtnPF04_pixrect, 32, 32, 1, BtnPF04_image);

static unsigned short	BtnPF05_image[] = {
#include "BtnPF05"
};
mpr_static (BtnPF05_pixrect, 32, 32, 1, BtnPF05_image);

static unsigned short	BtnPF06_image[] = {
#include "BtnPF06"
};
mpr_static (BtnPF06_pixrect, 32, 32, 1, BtnPF06_image);

static unsigned short	BtnPF07_image[] = {
#include "BtnPF07"
};
mpr_static (BtnPF07_pixrect, 32, 32, 1, BtnPF07_image);

static unsigned short	BtnPF08_image[] = {
#include "BtnPF08"
};
mpr_static (BtnPF08_pixrect, 32, 32, 1, BtnPF08_image);

static unsigned short	BtnPF09_image[] = {
#include "BtnPF09"
};
mpr_static (BtnPF09_pixrect, 32, 32, 1, BtnPF09_image);

static unsigned short	BtnPF10_image[] = {
#include "BtnPF10"
};
mpr_static (BtnPF10_pixrect, 32, 32, 1, BtnPF10_image);

static unsigned short	BtnPF11_image[] = {
#include "BtnPF11"
};
mpr_static (BtnPF11_pixrect, 32, 32, 1, BtnPF11_image);

static unsigned short	BtnPF12_image[] = {
#include "BtnPF12"
};
mpr_static (BtnPF12_pixrect, 32, 32, 1, BtnPF12_image);

static unsigned short	BtnPF13_image[] = {
#include "BtnPF13"
};
mpr_static (BtnPF13_pixrect, 32, 32, 1, BtnPF13_image);

static unsigned short	BtnPF14_image[] = {
#include "BtnPF14"
};
mpr_static (BtnPF14_pixrect, 32, 32, 1, BtnPF14_image);

static unsigned short	BtnPF15_image[] = {
#include "BtnPF15"
};
mpr_static (BtnPF15_pixrect, 32, 32, 1, BtnPF15_image);

static unsigned short	BtnPF16_image[] = {
#include "BtnPF16"
};
mpr_static (BtnPF16_pixrect, 32, 32, 1, BtnPF16_image);

static unsigned short	BtnPF17_image[] = {
#include "BtnPF17"
};
mpr_static (BtnPF17_pixrect, 32, 32, 1, BtnPF17_image);

static unsigned short	BtnPF18_image[] = {
#include "BtnPF18"
};
mpr_static (BtnPF18_pixrect, 32, 32, 1, BtnPF18_image);

static unsigned short	BtnPF19_image[] = {
#include "BtnPF19"
};
mpr_static (BtnPF19_pixrect, 32, 32, 1, BtnPF19_image);

static unsigned short	BtnPF20_image[] = {
#include "BtnPF20"
};
mpr_static (BtnPF20_pixrect, 32, 32, 1, BtnPF20_image);

static unsigned short	BtnPF21_image[] = {
#include "BtnPF21"
};
mpr_static (BtnPF21_pixrect, 32, 32, 1, BtnPF21_image);

static unsigned short	BtnPF22_image[] = {
#include "BtnPF22"
};
mpr_static (BtnPF22_pixrect, 32, 32, 1, BtnPF22_image);

static unsigned short	BtnPF23_image[] = {
#include "BtnPF23"
};
mpr_static (BtnPF23_pixrect, 32, 32, 1, BtnPF23_image);

static unsigned short	BtnPF24_image[] = {
#include "BtnPF24"
};
mpr_static (BtnPF24_pixrect, 32, 32, 1, BtnPF24_image);

static unsigned short	BtnSysReq_image[] = {
#include "BtnSysReq"
};
mpr_static (BtnSysReq_pixrect, 32, 32, 1, BtnSysReq_image);

static unsigned short	BtnShift_image[] = {
#include "BtnShift"
};
mpr_static (BtnShift_pixrect, 32, 32, 1, BtnShift_image);

extern Frame	frame;
extern Canvas	canvas;
Frame		key_frame;
Panel		key_panel;
Panel_item	BtnPA1, BtnPA2, BtnClear,
		BtnPF01, BtnPF02, BtnPF03,
		BtnPF04, BtnPF05, BtnPF06,
		BtnPF07, BtnPF08, BtnPF09,
		BtnPF10, BtnPF11, BtnPF12,
		BtnPF13, BtnPF14, BtnPF15,
		BtnPF16, BtnPF17, BtnPF18,
		BtnPF19, BtnPF20, BtnPF21,
		BtnPF22, BtnPF23, BtnPF24,
		BtnSysReq, BtnEnter, BtnShift;
bool		panel_shifted = FALSE, panel_showing = FALSE;

extern int	key_PA1 (), key_PA2 (), key_Clear (),
		key_PF1 (), key_PF2 (), key_PF3 (),
		key_PF4 (), key_PF5 (), key_PF6 (),
		key_PF7 (), key_PF8 (), key_PF9 (),
		key_PF10 (), key_PF11 (), key_PF12 (),
		key_PF13 (), key_PF14 (), key_PF15 (),
		key_PF16 (), key_PF17 (), key_PF18 (),
		key_PF19 (), key_PF20 (), key_PF21 (),
		key_PF22 (), key_PF23 (), key_PF24 (),
		key_SysReq (), key_Enter ();

/*
 * Shift button hit, hide the currently showing 12 PF keys and show the
 * 12 that are hiding.
 */
key_PFShift ()
{
    if (panel_shifted) {
	panel_shifted = FALSE;
	panel_set (BtnPF13, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF14, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF15, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF16, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF17, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF18, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF19, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF20, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF21, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF22, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF23, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF24, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF01, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF02, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF03, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF04, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF05, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF06, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF07, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF08, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF09, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF10, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF11, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF12, PANEL_SHOW_ITEM, TRUE, 0);
    }
    else {
	panel_shifted = TRUE;
	panel_set (BtnPF01, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF02, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF03, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF04, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF05, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF06, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF07, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF08, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF09, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF10, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF11, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF12, PANEL_SHOW_ITEM, FALSE, 0);
	panel_set (BtnPF13, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF14, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF15, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF16, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF17, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF18, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF19, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF20, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF21, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF22, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF23, PANEL_SHOW_ITEM, TRUE, 0);
	panel_set (BtnPF24, PANEL_SHOW_ITEM, TRUE, 0);
    }
}


/*
 * Toggle whether the key panel is showing or not.
 */
key_panel_toggle ()
{
    panel_showing = !((bool) window_get (key_frame, WIN_SHOW));
    window_set (key_frame, WIN_SHOW, panel_showing, 0);
}


/*
 * Catch open/close events on the main frame so that if the key panel frame
 * is visible, we can hide and display it as necessary.  This makes the
 * key panel frame seem to track the open/close events on the main frame.
 */
static Notify_value
frame_interposer (frame, event, arg, type)
Frame			frame;
Event			*event;
Notify_arg		arg;
Notify_event_type	type;
{
    int			init_closed, current_closed;
    Notify_value	value;

    init_closed = (int) window_get (frame, FRAME_CLOSED);
    value = notify_next_event_func (frame, event, arg, type);
    current_closed = (int) window_get (frame, FRAME_CLOSED);
    if (init_closed != current_closed && panel_showing)
	window_set (key_frame, WIN_SHOW, !current_closed, 0);
    return (value);
}


/*
 * Build the key panel from ground up.  Should only be called once.
 */
key_panel_init ()
{
    int			input_designee;

    input_designee = (int) window_get (canvas, WIN_DEVICE_NUMBER);
    key_frame = window_create (
	frame, FRAME,
	FRAME_SHOW_LABEL,	FALSE,
	FRAME_SHOW_SHADOW,	FALSE,	/* SunOS 4.0 only, ok to delete */
	WIN_SHOW,		FALSE,
	0
    );
    panel_showing = FALSE;
    key_panel = window_create (
	key_frame, PANEL,
	PANEL_ITEM_X_GAP,	SPACING,
	PANEL_ITEM_Y_GAP,	SPACING,
	WIN_TOP_MARGIN,		SPACING,
	WIN_BOTTOM_MARGIN,	SPACING,
	WIN_LEFT_MARGIN,	SPACING,
	WIN_RIGHT_MARGIN,	SPACING,
	WIN_ROW_GAP,		SPACING,
	WIN_COLUMN_GAP,		SPACING,
	WIN_IGNORE_KBD_EVENTS,	WIN_ASCII_EVENTS,
				WIN_LEFT_KEYS,
				WIN_RIGHT_KEYS,
				WIN_TOP_KEYS,
				0,
	WIN_INPUT_DESIGNEE,	input_designee,
	0
    );
    BtnPA1 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (0),
	PANEL_LABEL_IMAGE,	&BtnPA1_pixrect,
	PANEL_NOTIFY_PROC,	key_PA1,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPA2 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (0),
	PANEL_LABEL_IMAGE,	&BtnPA2_pixrect,
	PANEL_NOTIFY_PROC,	key_PA2,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnClear = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (0),
	PANEL_LABEL_IMAGE,	&BtnClear_pixrect,
	PANEL_NOTIFY_PROC,	key_Clear,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF01 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF01_pixrect,
	PANEL_NOTIFY_PROC,	key_PF1,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF02 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF02_pixrect,
	PANEL_NOTIFY_PROC,	key_PF2,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF03 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF03_pixrect,
	PANEL_NOTIFY_PROC,	key_PF3,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF04 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF04_pixrect,
	PANEL_NOTIFY_PROC,	key_PF4,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF05 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF05_pixrect,
	PANEL_NOTIFY_PROC,	key_PF5,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF06 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF06_pixrect,
	PANEL_NOTIFY_PROC,	key_PF6,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF07 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF07_pixrect,
	PANEL_NOTIFY_PROC,	key_PF7,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF08 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF08_pixrect,
	PANEL_NOTIFY_PROC,	key_PF8,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF09 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF09_pixrect,
	PANEL_NOTIFY_PROC,	key_PF9,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF10 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF10_pixrect,
	PANEL_NOTIFY_PROC,	key_PF10,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF11 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF11_pixrect,
	PANEL_NOTIFY_PROC,	key_PF11,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF12 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF12_pixrect,
	PANEL_NOTIFY_PROC,	key_PF12,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnPF13 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF13_pixrect,
	PANEL_NOTIFY_PROC,	key_PF13,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF14 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF14_pixrect,
	PANEL_NOTIFY_PROC,	key_PF14,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF15 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (1),
	PANEL_LABEL_IMAGE,	&BtnPF15_pixrect,
	PANEL_NOTIFY_PROC,	key_PF15,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF16 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF16_pixrect,
	PANEL_NOTIFY_PROC,	key_PF16,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF17 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF17_pixrect,
	PANEL_NOTIFY_PROC,	key_PF17,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF18 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (2),
	PANEL_LABEL_IMAGE,	&BtnPF18_pixrect,
	PANEL_NOTIFY_PROC,	key_PF18,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF19 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF19_pixrect,
	PANEL_NOTIFY_PROC,	key_PF19,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF20 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF20_pixrect,
	PANEL_NOTIFY_PROC,	key_PF20,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF21 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (3),
	PANEL_LABEL_IMAGE,	&BtnPF21_pixrect,
	PANEL_NOTIFY_PROC,	key_PF21,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF22 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF22_pixrect,
	PANEL_NOTIFY_PROC,	key_PF22,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF23 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF23_pixrect,
	PANEL_NOTIFY_PROC,	key_PF23,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnPF24 = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (4),
	PANEL_LABEL_IMAGE,	&BtnPF24_pixrect,
	PANEL_NOTIFY_PROC,	key_PF24,
	PANEL_SHOW_ITEM,	FALSE,
	0
    );
    BtnSysReq = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (0),
	PANEL_ITEM_Y,		ITEM_COORD (5),
	PANEL_LABEL_IMAGE,	&BtnSysReq_pixrect,
	PANEL_NOTIFY_PROC,	key_SysReq,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnEnter = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (1),
	PANEL_ITEM_Y,		ITEM_COORD (5),
	PANEL_LABEL_IMAGE,	&BtnEnter_pixrect,
	PANEL_NOTIFY_PROC,	key_Enter,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    BtnShift = panel_create_item (
	key_panel, PANEL_BUTTON,
	PANEL_ITEM_X,		ITEM_COORD (2),
	PANEL_ITEM_Y,		ITEM_COORD (5),
	PANEL_LABEL_IMAGE,	&BtnShift_pixrect,
	PANEL_NOTIFY_PROC,	key_PFShift,
	PANEL_SHOW_ITEM,	TRUE,
	0
    );
    panel_shifted = FALSE;
    window_fit (key_panel);
    window_fit (key_frame);

    /* interpose base frame on opens and close so that we can mimic them on
     * the key panel frame.
     */
    (void) notify_interpose_event_func (frame, frame_interposer, NOTIFY_SAFE);
}
