/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000 by Paul Mattes.
 * Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	appres.h
 *		Application resource definitions for x3270.
 */

/* Toggles */

enum toggle_type { TT_INITIAL, TT_TOGGLE, TT_FINAL };
struct toggle {
	Boolean value;		/* toggle value */
	Boolean changed;	/* has the value changed since init */
	Widget w[2];		/* the menu item widgets */
	const char *label[2];	/* labels */
	void (*upcall)(struct toggle *, enum toggle_type); /* change value */
};
#define MONOCASE	0
#define ALT_CURSOR	1
#define CURSOR_BLINK	2
#define SHOW_TIMING	3
#define CURSOR_POS	4

#if defined(X3270_TRACE) /*[*/
#define DS_TRACE	5
#endif /*]*/

#define SCROLL_BAR	6

#if defined(X3270_ANSI) /*[*/
#define LINE_WRAP	7
#endif /*]*/

#define BLANK_FILL	8

#if defined(X3270_TRACE) /*[*/
#define SCREEN_TRACE	9
#define EVENT_TRACE	10
#endif /*]*/

#define MARGINED_PASTE	11
#define RECTANGLE_SELECT 12

#define N_TOGGLES	13

#define toggled(ix)		(appres.toggle[ix].value)
#define toggle_toggle(t) \
	{ (t)->value = !(t)->value; (t)->changed = True; }

/* Application resources */

typedef struct {
	/* Basic colors */
#if defined(X3270_DISPLAY) /*[*/
	Pixel	foreground;
	Pixel	background;
#endif /*]*/

	/* Options (not toggles) */
	Boolean mono;
	Boolean extended;
	Boolean m3279;
	Boolean modified_sel;
	Boolean	once;
#if defined(X3270_DISPLAY) /*[*/
	Boolean visual_bell;
	Boolean menubar;
	Boolean active_icon;
	Boolean label_icon;
	Boolean invert_kpshift;
	Boolean use_cursor_color;
	Boolean allow_resize;
	Boolean no_other;
	Boolean do_confirms;
	Boolean reconnect;
	Boolean visual_select;
# if defined(X3270_KEYPAD) /*[*/
	Boolean	keypad_on;
# endif /*]*/
#endif /*]*/
	Boolean	apl_mode;
	Boolean scripted;
	Boolean numeric_lock;
	Boolean secure;
	Boolean oerr_lock;
	Boolean	typeahead;
	Boolean debug_tracing;
	Boolean disconnect_clear;
	Boolean highlight_bold;
	Boolean highlight_select;

	/* Named resources */
#if defined(X3270_KEYPAD) /*[*/
	char	*keypad;
#endif /*]*/
#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
	char	*key_map;
	char	*compose_map;
#endif /*]*/
#if defined(X3270_DISPLAY) /*[*/
	char	*efontname;
	char	*afontname;
	char	*font_list;
	char	*debug_font;
	char	*icon_font;
	char	*icon_label_font;
	int	save_lines;
	char	*normal_name;
	char	*select_name;
	char	*bold_name;
	char	*colorbg_name;
	char	*keypadbg_name;
	char	*selbg_name;
	char	*cursor_color_name;
	char    *color_scheme;
	int	bell_volume;
	char	*char_class;
	int	modified_sel_color;
	int	visual_select_color;
#endif /*]*/
	char	*model;
	char	*hostsfile;
	char	*port;
	char	*charset;
	char	*termname;
	char	*macros;
	char	*trace_dir;
#if defined(X3270_TRACE) /*[*/
	char	*trace_file;
	char	*screentrace_file;
#endif /*]*/
	char	*oversize;
#if defined(X3270_FT) /*[*/
	char	*ft_command;
#endif /*]*/
	char	*connectfile_name;

	/* Toggles */
	struct toggle toggle[N_TOGGLES];

#if defined(X3270_DISPLAY) /*[*/
	/* Simple widget resources */
	Cursor	normal_mcursor;
	Cursor	wait_mcursor;
	Cursor	locked_mcursor;
#endif /*]*/

#if defined(X3270_ANSI) /*[*/
	/* Line-mode TTY parameters */
	Boolean	icrnl;
	Boolean	inlcr;
	char	*erase;
	char	*kill;
	char	*werase;
	char	*rprnt;
	char	*lnext;
	char	*intr;
	char	*quit;
	char	*eof;
#endif /*]*/

#if defined(USE_APP_DEFAULTS) /*[*/
	/* App-defaults version */
	char	*ad_version;
#endif /*]*/

} AppRes, *AppResptr;

extern AppRes appres;
