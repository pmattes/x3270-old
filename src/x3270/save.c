/*
 * Copyright 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	save.c
 *		Implements the response to the WM_SAVE_YOURSELF message.
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include "globals.h"


struct value {
	char *start;
	int len;
};

static char d_xrm[] = "-xrm";

/*
 * Add a string to a value.
 */
static void
add_string(v, s)
struct value *v;
char *s;
{
	int xlen = strlen(s) + 1;

	v->start = XtRealloc(v->start, v->len + xlen);
	(void) strcpy(v->start + v->len, s);
	v->len += xlen;
}

static void
save_xy(v)
struct value *v;
{
	char tbuf[64];
	Window window, frame, child;
	XWindowAttributes wa;
	int x, y;

	window = XtWindow(toplevel);
	if (!XGetWindowAttributes(display, window, &wa))
		return;
	(void) XTranslateCoordinates(display, window, wa.root, 
		-wa.border_width, -wa.border_width,
		&x, &y, &child);

	frame = XtWindow(toplevel);
	while (True) {
		Window root, parent;
		Window *children;
		unsigned int nchildren;

		int status = XQueryTree(display, frame, &root, &parent,
		    &children, &nchildren);
		if (parent == root || !parent || !status)
			break;
		frame = parent;
		if (status && children)
			XFree((char *)children);
	}
	if (frame != window) {
		if (!XGetWindowAttributes(display, frame, &wa))
			return;
		x = wa.x;
		y = wa.y;
	}

	add_string(v, "-geometry");
	(void) sprintf(tbuf, "+%d+%d", x, y);
	add_string(v, tbuf);
}

static void
save_icon(v)
struct value *v;
{
	unsigned char *data;
	int iconX, iconY;
	char tbuf[64];

	{
		Atom actual_type;
		int actual_format;
		unsigned long nitems, leftover;

		if (XGetWindowProperty(display, XtWindow(toplevel), a_state,
		    0L, 4L, False, a_state, &actual_type, &actual_format,
		    &nitems, &leftover, &data) != Success)
			return;
	}

	if (*(unsigned long *)data == IconicState)
		add_string(v, "-iconic");

	{
		int icon_window;
		XWindowAttributes wa;
		Window child;

		icon_window = *(unsigned long *)(data + sizeof(unsigned long));
		if (!XGetWindowAttributes(display, icon_window, &wa))
			return;
		(void) XTranslateCoordinates(display, icon_window, wa.root,
		    -wa.border_width, -wa.border_width, &iconX, &iconY,
		    &child);
		if (!iconX && !iconY)
			return;
	}

	add_string(v, d_xrm);
	(void) sprintf(tbuf, "*iconX: %d", iconX);
	add_string(v, tbuf);

	add_string(v, d_xrm);
	(void) sprintf(tbuf, "*iconY: %d", iconY);
	add_string(v, tbuf);
}

static void
save_titles(v)
struct value *v;
{
	if (user_title) {
		add_string(v, "-title");
		add_string(v, user_title);
	}
	if (user_icon_name) {
		add_string(v, "-iconname");
		add_string(v, user_icon_name);
	}
}

/*ARGSUSED*/
static void
save_keymap(v)
struct value *v;
{
       /* Note: keymap propogation is deliberately disabled, because it
	  may vary from workstation to workstation.  The recommended
	  way of specifying keymaps is through your .Xdefaults or the
	  KEYMAP or KEYBD environment variables, which can be easily set
	  in your .login or .profile to machine-specific values; the
	  -keymap switch is really for debugging or testing keymaps.

	  I'm sure I'll regret this.  */

#if defined(notdef) /*[*/
	if (appres.keymap) {
		add_string(v, "-keymap");
		add_string(v, appres.keymap);
	}
#endif /*]*/
}

static void
save_charset(v)
struct value *v;
{
	if (!appres.apl_mode && appres.charset) {
		add_string(v, "-charset");
		add_string(v, appres.charset);
	}
}

static void
save_model(v)
struct value *v;
{
	char tbuf[2];

	add_string(v, "-model");
	(void) sprintf(tbuf, "%d", model_num);
	add_string(v, tbuf);
}

static void
save_port(v)
struct value *v;
{
	if (appres.port && strcmp(appres.port, "telnet")) {
		add_string(v, "-port");
		add_string(v, appres.port);
	}
}

static void
save_tn(v)
struct value *v;
{
	if (appres.termname) {
		add_string(v, "-tn");
		add_string(v, appres.termname);
	}
}

static void
save_keypad(v)
struct value *v;
{
	extern Boolean keypad_popped;

	if (keypad_popped) {
		add_string(v, d_xrm);
		add_string(v, "*keypadOn: true");
	}
}

static void
save_scrollbar(v)
struct value *v;
{
	char tbuf[32];

	if (toggled(SCROLLBAR))
		add_string(v, "-sb");
	else
		add_string(v, "+sb");
	add_string(v, "-sl");
	(void) sprintf(tbuf, "%d", appres.save_lines);
	add_string(v, tbuf);
}

static void
save_host(v)
struct value *v;
{
	if (CONNECTED || HALF_CONNECTED)
		add_string(v, full_current_host);
}

void
save_yourself()
{
	struct value v;

	v.start = (char *)0;
	v.len = 0;

	/* Program name. */
	add_string(&v, programname);
	if (strcmp(programname, XtName(toplevel))) {
		add_string(&v, "-name");
		add_string(&v, XtName(toplevel));
	}

	/* Options. */
	save_xy(&v);
	save_icon(&v);
	save_titles(&v);
	save_keymap(&v);
	save_charset(&v);
	save_model(&v);
	save_port(&v);
	save_tn(&v);
	save_keypad(&v);
	save_scrollbar(&v);
	if (appres.apl_mode)
		add_string(&v, "-apl");
	if (appres.once)
		add_string(&v, "-once");
	if (appres.active_icon)
		add_string(&v, "-activeicon");
	save_host(&v);

	/* Do it. */
	XChangeProperty(display, XtWindow(toplevel), a_command,
	    XA_STRING, 8, PropModeReplace, (unsigned char *)v.start, v.len);
	XtFree(v.start);
}
