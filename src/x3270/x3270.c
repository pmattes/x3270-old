/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	x3270.c
 *		A 3270 Terminal Emulator for X11
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "globals.h"

#include "x3270.bm"

#define MAX_READ	1024
#define THROTTLE_MS	250	/* .25 sec */

/* Externals */
extern int	n_read;
extern char	*app_defaults_version;

/* Globals */
Display		*display;
int		default_screen;
Window		root_window;
int		depth;
Widget		toplevel;
Widget		icon_shell;
XtAppContext	appcontext;
Atom		a_delete_me, a_save_yourself, a_spacing, a_c, a_family_name,
		a_3270, a_registry, a_iso8859, a_encoding, a_1, a_command,
		a_state;
int		net_sock = -1;
char		*current_host;
char		*full_current_host;
char		*efontname;
char		*bfontname;
int		model_num;
char		*programname;
char		*pending_string = (char *) NULL;
Pixmap		gray;
XrmDatabase	rdb;
Boolean		keypad = False;
Boolean		shifted = False;
struct trans_list *trans_list = NULL;
static struct trans_list **last_trans = &trans_list;
AppRes		appres;
char		*charset = (char *) NULL;
enum kp_placement	kp_placement;
Pixmap		icon;
char		*user_title = (char *) NULL;
char		*user_icon_name = (char *) NULL;
struct font_list *font_list = (struct font_list *)NULL;
int		font_count = 0;

enum cstate	cstate = NOT_CONNECTED;
Boolean		ansi_host = False;
Boolean		playback_host = False;
Boolean		ever_3270 = False;
Boolean		exiting = False;

/* Statics */
static XtInputId	ns_read_id;
static XtInputId	ns_exception_id;
static Boolean		reading = False;
static Boolean		excepting = False;
static Pixmap		inv_icon;
static Boolean		icon_inverted = False;
static struct font_list *font_last = (struct font_list *)NULL;
static void		find_keymaps();
static void		add_trans();
static void		peek_at_xevent();
static void		x_throttle();
static void		parse_font_list();
static XtErrorMsgHandler old_emh;
static void		trap_colormaps();
static Boolean		colormap_failure = False;

static XrmOptionDescRec options[]= {
	{ "-activeicon",".activeIcon",	XrmoptionNoArg,		"true" },
	{ "-apl",	".aplMode",	XrmoptionNoArg,		"true" },
	{ "-aw",	".autoWrap",	XrmoptionNoArg,		"true" },
	{ "+aw",	".autoWrap",	XrmoptionNoArg,		"false" },
	{ "-bfont",	".boldFont",	XrmoptionSepArg,	NULL },
	{ "-charset",	".charset",	XrmoptionSepArg,	NULL },
	{ "-efont",	".emulatorFont",XrmoptionSepArg,	NULL },
	{ "-iconname",	".iconName",	XrmoptionSepArg,	NULL },
	{ "-keymap",	".keymap",	XrmoptionSepArg,	NULL },
	{ "-model",	".model",	XrmoptionSepArg,	NULL },
	{ "-mono",	".mono",	XrmoptionNoArg,		"true" },
	{ "-once",	".once",	XrmoptionNoArg,		"true" },
	{ "-script",	".scripted",	XrmoptionNoArg,		"true" },
	{ "-port",	".port",	XrmoptionSepArg,	NULL },
	{ "-sb",	".scrollBar",	XrmoptionNoArg,		"true" },
	{ "+sb",	".scrollBar",	XrmoptionNoArg,		"false" },
	{ "-sl",	".saveLines",	XrmoptionSepArg,	NULL },
	{ "-tn",	".termName",	XrmoptionSepArg,	NULL },
	{ "-trace",	".trace",	XrmoptionNoArg,		"true" },
};

#define offset(field) XtOffset(AppResptr, field)
static XtResource resources[] = {
	{ XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	  offset(foreground), XtRString, "XtDefaultForeground" },
	{ XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	  offset(background), XtRString, "XtDefaultBackground" },
	{ "colorBackground", "ColorBackground", XtRPixel, sizeof(Pixel),
	  offset(colorbg), XtRString, "black" },
	{ "selectBackground", "SelectBackground", XtRPixel, sizeof(Pixel),
	  offset(selbg), XtRString, "dim gray" },
	{ "normalColor", "NormalColor", XtRPixel, sizeof(Pixel),
	  offset(normal), XtRString, "green" },
	{ "inputColor", "InputColor", XtRPixel, sizeof(Pixel),
	  offset(select), XtRString, "orange" },
	{ "boldColor", "BoldColor", XtRPixel, sizeof(Pixel),
	  offset(bold), XtRString, "cyan" },
	{ "mono", "Mono", XtRBoolean, sizeof(Boolean),
	  offset(mono), XtRString, "false" },
	{ "keypad", "Keypad", XtRString, sizeof(String),
	  offset(keypad), XtRString, "right" },
	{ "keypadOn", "KeypadOn", XtRBoolean, sizeof(Boolean),
	  offset(keypad_on), XtRString, "false" },
	{ "invertKeypadShift", "InvertKeypadShift", XtRBoolean, sizeof(Boolean),
	  offset(invert_kpshift), XtRString, "false" },
	{ "saveLines", "SaveLines", XtRInt, sizeof(int),
	  offset(save_lines), XtRString, "64" },
	{ "menuBar", "MenuBar", XtRBoolean, sizeof(Boolean),
	  offset(menubar), XtRString, "true" },
	{ "activeIcon", "ActiveIcon", XtRBoolean, sizeof(Boolean),
	  offset(active_icon), XtRString, "false" },
	{ "labelIcon", "LabelIcon", XtRBoolean, sizeof(Boolean),
	  offset(label_icon), XtRString, "false" },
	{ "keypadBackground", "KeypadBackground", XtRPixel, sizeof(Pixel),
	  offset(keypadbg), XtRString, "grey70" },
	{ "emulatorFont", "EmulatorFont", XtRString, sizeof(char *),
	  offset(efontname), XtRString, 0 },
	{ "boldFont", "boldFont", XtRString, sizeof(char *),
	  offset(bfontname), XtRString, 0 },
	{ "visualBell", "VisualBell", XtRBoolean, sizeof(Boolean),
	  offset(visual_bell), XtRString, "false" },
	{ "aplMode", "AplMode", XtRBoolean, sizeof(Boolean),
	  offset(apl_mode), XtRString, "false" },
	{ "once", "Once", XtRBoolean, sizeof(Boolean),
	  offset(once), XtRString, "false" },
	{ "scripted", "Scripted", XtRBoolean, sizeof(Boolean),
	  offset(scripted), XtRString, "false" },
	{ "model", "Model", XtRString, sizeof(char *),
	  offset(model), XtRString, "4" },
	{ "keymap", "Keymap", XtRString, sizeof(char *),
	  offset(key_map), XtRString, 0 },
	{ "composeMap", "ComposeMap", XtRString, sizeof(char *),
	  offset(compose_map), XtRString, "latin1" },
	{ "hostsFile", "HostsFile", XtRString, sizeof(char *),
	  offset(hostsfile), XtRString, "/usr/lib/X11/x3270/ibm_hosts" },
	{ "port", "Port", XtRString, sizeof(char *),
	  offset(port), XtRString, "telnet" },
	{ "charset", "Charset", XtRString, sizeof(char *),
	  offset(charset), XtRString, "us" },
	{ "termName", "TermName", XtRString, sizeof(char *),
	  offset(termname), XtRString, 0 },
	{ "debugFont", "DebugFont", XtRString, sizeof(char *),
	  offset(debug_font), XtRString, "3270d" },
	{ "fontList", "FontList", XtRString, sizeof(char *),
	  offset(font_list), XtRString, 0 },
	{ "iconFont", "IconFont", XtRString, sizeof(char *),
	  offset(icon_font), XtRString, "nil2" },
	{ "iconLabelFont", "IconLabelFont", XtRString, sizeof(char *),
	  offset(icon_label_font), XtRString, "8x13" },
	{ "defaultTranslations", "DefaultTranslations", XtRTranslationTable,
	    sizeof(XtTranslations),
	  offset(default_translations), XtRTranslationTable, NULL },
	{ "waitCursor", "WaitCursor", XtRCursor, sizeof(Cursor),
	  offset(wait_mcursor), XtRString, "watch" },
	{ "lockedCursor", "LockedCursor", XtRCursor, sizeof(Cursor),
	  offset(locked_mcursor), XtRString, "X_cursor" },
	{ "adVersion", "AdVersion", XtRString, sizeof(char *),
	  offset(ad_version), XtRString, 0 },
	{ "macros", "Macros", XtRString, sizeof(char *),
	  offset(macros), XtRString, 0 },
	{ "traceDir", "TraceDir", XtRString, sizeof(char *),
	  offset(trace_dir), XtRString, "/usr/tmp" },

	{ "monoCase", "MonoCase", XtRBoolean, sizeof(Boolean),
	  offset(toggle[MONOCASE].value), XtRString, "false" },
	{ "altCursor", "AltCursor", XtRBoolean, sizeof(Boolean),
	  offset(toggle[ALTCURSOR].value), XtRString, "false" },
	{ "blink", "Blink", XtRBoolean, sizeof(Boolean),
	  offset(toggle[BLINK].value), XtRString, "false" },
	{ "timing", "Timing", XtRBoolean, sizeof(Boolean),
	  offset(toggle[TIMING].value), XtRString, "false" },
	{ "cursorPos", "CursorPos", XtRBoolean, sizeof(Boolean),
	  offset(toggle[CURSORP].value), XtRString, "true" },
	{ "trace", "Trace", XtRBoolean, sizeof(Boolean),
	  offset(toggle[TRACE].value), XtRString, "false" },
	{ "scrollBar", "ScrollBar", XtRBoolean, sizeof(Boolean),
	  offset(toggle[SCROLLBAR].value), XtRString, "false" },
	{ "autoWrap", "AutoWrap", XtRBoolean, sizeof(Boolean),
	  offset(toggle[LINEWRAP].value), XtRString, "true" },
	{ "blankFill", "BlankFill", XtRBoolean, sizeof(Boolean),
	  offset(toggle[BLANKFILL].value), XtRString, "false" },
	{ "screenTrace", "ScreenTrace", XtRBoolean, sizeof(Boolean),
	  offset(toggle[SCREENTRACE].value), XtRString, "false" },

	{ "icrnl", "Icrnl", XtRBoolean, sizeof(Boolean),
	  offset(icrnl), XtRString, "true" },
	{ "inlcr", "Inlcr", XtRBoolean, sizeof(Boolean),
	  offset(inlcr), XtRString, "false" },
	{ "erase", "Erase", XtRString, sizeof(char *),
	  offset(erase), XtRString, "^?" },
	{ "kill", "Kill", XtRString, sizeof(char *),
	  offset(kill), XtRString, "^U" },
	{ "werase", "Werase", XtRString, sizeof(char *),
	  offset(werase), XtRString, "^W" },
	{ "rprnt", "Rprnt", XtRString, sizeof(char *),
	  offset(rprnt), XtRString, "^R" },
	{ "lnext", "Lnext", XtRString, sizeof(char *),
	  offset(lnext), XtRString, "^V" },
	{ "intr", "Intr", XtRString, sizeof(char *),
	  offset(intr), XtRString, "^C" },
	{ "quit", "Quit", XtRString, sizeof(char *),
	  offset(quit), XtRString, "^\\" },
	{ "eof", "Eof", XtRString, sizeof(char *),
	  offset(eof), XtRString, "^D" },

};
#undef offset

/* Fallback resources, adequate if app-defaults is missing */
static String fallbacks[] = {
	"*adVersion:	fallback",
	NULL
};


void
main(argc, argv)
int	argc;
char	*argv[];
{
	char	*buf;
	Boolean	default_bold = False;
	char	*ef, *bf = NULL, *emsg;
	int	i;
	Boolean saw_apl_keymod = False;
	Atom	protocols[2];

	/* Figure out who we are */
	programname = strrchr(argv[0], '/');
	if (programname)
		++programname;
	else
		programname = argv[0];

	toplevel = XtVaAppInitialize(
	    &appcontext,
	    "X3270",
	    options, XtNumber(options),
	    &argc, argv,
	    fallbacks,
	    XtNinput, True,
	    XtNallowShellResize, False,
	    NULL);

	old_emh = XtAppSetWarningMsgHandler(appcontext,
	    (XtErrorMsgHandler)trap_colormaps);
	XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    XtNumber(resources), 0, 0);
	(void) XtAppSetWarningMsgHandler(appcontext, old_emh);
	if (!appres.ad_version)
		XtError("Outdated app-defaults file");
	else if (!strcmp(appres.ad_version, "fallback"))
		XtError("No app-defaults file");
	else if (strcmp(appres.ad_version, app_defaults_version)) {
		char *msg = xs_buffer2("app-defaults version mismatch: want %s, got %s",
		    app_defaults_version, appres.ad_version);

		XtError(msg);
	}

	display = XtDisplay(toplevel);
	default_screen = DefaultScreen(display);
	root_window = RootWindow(display, default_screen);
	depth = DefaultDepthOfScreen(XtScreen(toplevel));
	if (depth <= 1 || colormap_failure)
		appres.mono = True;
	rdb = XtDatabase(display);

	a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
	a_save_yourself = XInternAtom(display, "WM_SAVE_YOURSELF", False);
	a_spacing = XInternAtom(display, "SPACING", False);
	a_c = XInternAtom(display, "C", False);
	a_family_name = XInternAtom(display, "FAMILY_NAME", False);
	a_3270 = XInternAtom(display, "3270", False);
	a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
	a_iso8859 = XInternAtom(display, "ISO8859", False);
	a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
	a_1 = XInternAtom(display, "1", False);
	a_command = XInternAtom(display, "WM_COMMAND", False);
	a_state = XInternAtom(display, "WM_STATE", False);

	XtAppAddActions(appcontext, actions, actioncount);

	if (argc > 2) {
		buf = xs_buffer("Usage: %s [[a:]hostname]", programname);
		XtError(buf);
	}

	/*
	 * Font stuff.  Try the "emulatorFont" resource; if that isn't right,
	 * complain but keep going.  Try the "font" resource next, but don't
	 * say anything if it's wrong.  Finally, fall back to "fixed", and
	 * die if that won't work.
	 */
	parse_font_list();
	if (appres.apl_mode) {
		if ((appres.efontname = get_resource("aplFont")) == NULL)
			XtWarning("Can't find aplFont resource");
		appres.bfontname = NULL;
	}
	if (!appres.efontname)
		appres.efontname = font_list->font;
	ef = appres.efontname;
	if (appres.mono)
		bf = appres.bfontname;
	if (emsg = load_fixed_font(ef, efontinfo)) {
		bf = NULL;
		xs_warning2("emulatorFont '%s' %s, using default font",
		    ef, emsg);
		if (ef = get_resource("font"))
			(void) load_fixed_font(ef, efontinfo);
	}
	if (*efontinfo == NULL) {
		ef = "fixed";
		if (emsg = load_fixed_font(ef, efontinfo)) {
			buf = xs_buffer2("default emulatorFont '%s' %s", ef,
			    emsg);
			XtError(buf);
		}
	}

	if (appres.mono) {
		if (!bf) {
			/* construct something suitable */
			char *bfn = XtCalloc(sizeof(char), strlen(ef) + 5);

			bfn = xs_buffer("%sbold", ef);
			bf = bfn;
			default_bold = True;
		}
		(void) load_fixed_font(bf, bfontinfo);
		if (!*bfontinfo && !default_bold)
			xs_warning("Can't load boldFont '%s'", bf);
	}

	set_font_globals(*efontinfo, ef, bf);

	set_rows_cols(appres.model);
	hostfile_init();
	macros_init();

	if (appres.key_map == NULL)
		appres.key_map = (char *)getenv("KEYMAP");
	if (appres.key_map == NULL)
		appres.key_map = (char *)getenv("KEYBD");
	if (appres.key_map != NULL) {
		char *ns = XtNewString(appres.key_map);
		char *comma;

		do {
			comma = strchr(ns, ',');
			if (comma)
				*comma = '\0';
			if (!strcmp(ns, "apl"))
				saw_apl_keymod = True;
			find_keymaps(ns);
			if (comma)
				ns = comma + 1;
			else
				ns = NULL;
		} while (ns);
	}
	if (appres.apl_mode && !saw_apl_keymod) {
		find_keymaps("apl");
		if (appres.key_map)
			appres.key_map = xs_buffer("%s,apl", appres.key_map);
		else
			appres.key_map = "apl";
	}
	if (appres.apl_mode) {
		appres.compose_map = "apl";
		appres.charset = "apl";
	}
	if (appres.charset != NULL) {
		buf = xs_buffer("charset.%s", appres.charset);
		charset = get_resource(buf);
		XtFree(buf);
		if (charset == NULL)
			xs_warning("Can't find charset '%s'", appres.charset);
	}

	if (!strcmp(appres.keypad, "right"))
		kp_placement = kp_right;
	else if (!strcmp(appres.keypad, "bottom"))
		kp_placement = kp_bottom;
	else if (!strcmp(appres.keypad, "integral"))
		kp_placement = kp_integral;
	else
		XtError("Unknown value for keypad");
	if (appres.keypad_on)
		keypad = True;

	icon = XCreateBitmapFromData(display, root_window,
	    (char *) x3270_bits, x3270_width, x3270_height);
	for (i = 0; i < sizeof(x3270_bits); i++)
		x3270_bits[i] = ~x3270_bits[i];
	inv_icon = XCreateBitmapFromData(display, root_window,
	    (char *) x3270_bits, x3270_width, x3270_height);

	aicon_font_init();
	if (appres.active_icon) {
		Dimension iw, ih;

		aicon_size(&iw, &ih);
		icon_shell =  XtVaAppCreateShell(
		    "x3270icon",
		    "X3270",
		    overrideShellWidgetClass,
		    display,
		    XtNwidth, iw,
		    XtNheight, ih,
		    XtNmappedWhenManaged, False,
		    XtNbackground, appres.mono ? appres.background : appres.colorbg,
		    NULL);
		XtRealizeWidget(icon_shell);
		XtVaSetValues(toplevel,
		    XtNiconWindow, XtWindow(icon_shell),
		    NULL);
	} else
		XtVaSetValues(toplevel,
		    XtNiconPixmap, icon,
		    XtNiconMask, icon,
		    NULL);

	screen_init(False, True, True);
	keypad_at_startup();
	protocols[0] = a_delete_me;
	protocols[1] = a_save_yourself;
	XSetWMProtocols(display, XtWindow(toplevel), protocols, 2);

	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Respect the user's title wishes. */
	user_title = get_resource("title");
	user_icon_name = get_resource("iconName");
	if (user_icon_name)
		set_aicon_label(user_icon_name);

	/* Handle initial toggle settings. */
	initialize_toggles();

	/* Connect to the host. */
	if (argc <= 1 || x_connect(argv[1]) < 0)
		x_disconnect();

	/* Prepare the tty. */
	script_init();

	/* Process X events forever. */
	while (1) {
		XEvent		event;

		while (XtAppPending(appcontext) == XtIMXEvent) {
			XtAppNextEvent(appcontext, &event);
			peek_at_xevent(&event);
			XtDispatchEvent(&event);
		}
		screen_disp();
		XtAppProcessEvent(appcontext, XtIMAll);
		if (n_read >= MAX_READ) {
			x_throttle();
			n_read = 0;
		}
	}
}

/*
 * A way to work around bugs with Xt resources.  It seems to be impossible
 * to get arbitrarily named resources.  Someday this should be hacked to
 * add classes too.
 */
char *
get_resource(name)
char	*name;
{
	XrmValue value;
	char *type[20];
	char *str;
	char *r = (char *) NULL;

	str = xs_buffer2("%s.%s", XtName(toplevel), name);
	if ((XrmGetResource(rdb, str, 0, type, &value) == True) && *value.addr)
		r = XtNewString(value.addr);
	XtFree(str);
	return r;
}

char *
get_message(key)
char *key;
{
	static char namebuf[128];
	char *r;

	(void) sprintf(namebuf, "message.%s", key);
	if (r = get_resource(namebuf))
		return r;
	else {
		(void) sprintf(namebuf, "[missing '%s' message]", key);
		return namebuf;
	}
}


/*
 * Network connect/disconnect operations, combined with X input operations
 *
 * Returns 0 for success, -1 for error.
 */
#define A_COLON(s)	(!strncmp(s, "a:", 2) || !strncmp(s, "A:", 2))
#define P_COLON(t)	(!strncmp(t, "p:", 2) || !strncmp(t, "P:", 2))
int
x_connect(pseudonym)
char	*pseudonym;
{
	char *raw_name;
	char *real_name;
	char *ps;
	Boolean pending;

	if (CONNECTED)
		return 0;

	/* Remember exactly what they typed. */
	raw_name = pseudonym;

	/* Strip off 'a:' and remember it. */
	ansi_host = False;
	playback_host = False;
	if (A_COLON(pseudonym))
		ansi_host = True;
	else if (P_COLON(pseudonym))
		playback_host = True;

	/* Look up the name in the hosts file and record any pending string. */
	if (hostfile_lookup(pseudonym, &real_name, &ps)) {
		ps_set(XtNewString(ps), True);
	} else {
		real_name = pseudonym;
		ps_set((char *) NULL, False);
	}

	/* Maybe the translated name in the hosts file had an 'a:'. */
	if (A_COLON(real_name)) {
		ansi_host = True;
		real_name += 2;
	}
	if (P_COLON(real_name)) {
		playback_host = True;
		real_name += 2;
	}

	/* Null name? */
	if (!*real_name)
		return -1;

	/* Attempt contact. */
	ever_3270 = False;
	if (playback_host) {
		pending = False;
		net_sock = net_playback_connect(real_name);
	} else
		net_sock = net_connect(real_name, appres.port, &pending);
	if (net_sock < 0)
		return -1;

	if (pending) {
		cstate = PENDING;
		ticking_start(True);
	} else
		cstate = CONNECTED_INITIAL;

	/* Store the name they typed and the cleaned-up version in globals. */
	if (full_current_host)
		XtFree(full_current_host);
	full_current_host = XtNewString(raw_name);
	current_host = full_current_host;
	if (A_COLON(current_host) || P_COLON(current_host))
		current_host += 2;

	/* Prepare for I/O. */
	if (!playback_host) {
		ns_exception_id = XtAppAddInput(appcontext, net_sock,
		    (XtPointer)XtInputExceptMask, net_exception,
		    (XtPointer)NULL);
		excepting = True;
		ns_read_id = XtAppAddInput(appcontext, net_sock,
		    (XtPointer) XtInputReadMask, net_input, (XtPointer) NULL);
		reading = True;
	}

	/* Tell the world. */
	menubar_connect();
	screen_connect();
	kybd_connect();
	relabel();
	ansi_init();
	scroll_reset();

	return 0;
}
#undef A_COLON
#undef P_COLON

void
x_disconnect()
{
	ps_set((char *) NULL, True);
	ticking_stop();
	if (HALF_CONNECTED)
		status_untiming();
	if (CONNECTED || HALF_CONNECTED) {
		if (reading) {
			XtRemoveInput(ns_read_id);
			reading = False;
		}
		if (excepting) {
			XtRemoveInput(ns_exception_id);
			excepting = False;
		}
		net_disconnect();
		net_sock = -1;
		if (CONNECTED && appres.once) {
			if (error_popup_visible)
				exiting = True;
			else
				x3270_exit(0);
		}
		cstate = NOT_CONNECTED;
	}
	menubar_connect();
	screen_connect();
	kybd_connect();
	script_continue();	/* in case of Wait pending */
	relabel();
}

void
x_in3270(now3270)
Boolean now3270;
{
	if (now3270) {		/* ANSI -> 3270 */
		cstate = CONNECTED_3270;
		ever_3270 = True;
		menubar_newmode();
		screen_connect();
		kybd_connect();
		relabel();
		scroll_reset();
		script_continue();	/* in case of Wait pending */
	} else if (ansi_host) {	/* 3270 -> ANSI */
		cstate = CONNECTED_INITIAL;
		ever_3270 = False;
		ticking_stop();
		menubar_newmode();
		screen_connect();
		kybd_connect();
		relabel();
		ansi_init();
		scroll_reset();
	} else			/* 3270 -> nothing */
		x_disconnect();
}

void
x_connected()
{
	cstate = CONNECTED_INITIAL;
	ticking_stop();
	menubar_connect();
	screen_connect();
	kybd_connect();
	relabel();
	script_continue();	/* in case of Wait pending */
}

/*
 * Timeout callback to resume reading network data.
 */
/*ARGSUSED*/
static void
x_unthrottle(data, id)
XtPointer data;
XtIntervalId *id;
{
	if (CONNECTED && !reading) {
		ns_read_id = XtAppAddInput(appcontext, net_sock,
		    (XtPointer) XtInputReadMask, net_input,
		    (XtPointer) NULL);
		reading = True;
	}
}

/*
 * Called to shut off network input at some regular interval.
 */
static void
x_throttle()
{
	if (CONNECTED && reading) {
		XtRemoveInput(ns_read_id);
		reading = False;
		(void) XtAppAddTimeOut(appcontext, THROTTLE_MS, x_unthrottle,
		    (XtPointer)NULL);
	}
}

/*
 * Called when an exception is received to disable further exceptions.
 */
void
x_except_off()
{
	if (excepting) {
		XtRemoveInput(ns_exception_id);
		excepting = False;
	}
}

/*
 * Called when exception processing is complete to re-enable exceptions.
 * This includes removing and restoring reading, so the exceptions are always
 * processed first.
 */
void
x_except_on()
{
	if (excepting)
		return;
	if (reading)
		XtRemoveInput(ns_read_id);
	ns_exception_id = XtAppAddInput(appcontext, net_sock,
	    (XtPointer) XtInputExceptMask, net_exception, (XtPointer) NULL);
	excepting = True;
	if (reading)
		ns_read_id = XtAppAddInput(appcontext, net_sock,
		    (XtPointer) XtInputReadMask, net_input,
		    (XtPointer) NULL);
}

void
relabel()
{
	char *title;
	char icon_label[8];

	if (user_title && user_icon_name)
		return;
	title = XtMalloc(10 + (current_host ? strlen(current_host) : 0));
	if (PCONNECTED) {
		(void) sprintf(title, "x3270-%d%s %s", model_num,
		    (IN_ANSI ? "A" : ""), current_host);
		if (!user_title)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (!user_icon_name)
			XtVaSetValues(toplevel, XtNiconName, current_host, NULL);
		set_aicon_label(current_host);
	} else {
		(void) sprintf(title, "x3270-%d", model_num);
		(void) sprintf(icon_label, "x3270-%d", model_num);
		if (!user_title)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (!user_icon_name)
			XtVaSetValues(toplevel, XtNiconName, icon_label, NULL);
		set_aicon_label(icon_label);
	}
	XtFree(title);
}

/*
 * Invert the icon.
 */
void
invert_icon(invert)
Boolean invert;
{
	if (appres.active_icon)
		return;
	if (invert != icon_inverted) {
		XtVaSetValues(toplevel,
		    XtNiconPixmap, invert ? inv_icon : icon,
		    XtNiconMask, invert ? inv_icon : icon,
		    NULL);
		icon_inverted = invert;
	}
}

/*
 * Add to the list of user-specified keymap translations.
 */

static void
find_keymaps(name)
char *name;
{
	char *translations;
	char *buf;
	int any = 0;

	buf = xs_buffer("keymap.%s", name);
	if (translations = get_resource(buf)) {
		add_trans(name, translations);
		any++;
	}
	XtFree(buf);
	buf = xs_buffer("keymap.%s.user", name);
	if (translations = get_resource(buf)) {
		add_trans(buf + 7, translations);
		any++;
	}
	XtFree(buf);
	if (!any)
		xs_warning("Can't find keymap '%s'", name);
}

static void
add_trans(name, translations)
char *name;
char *translations;
{
	struct trans_list *t;

	t = (struct trans_list *)XtMalloc(sizeof(*t));
	t->name = XtNewString(name);
	t->translations = translations;
	t->next = NULL;
	*last_trans = t;
	last_trans = &t->next;
}

/*
 * Peek at X events before Xt does, calling keymap_event if we see a
 * KeymapEvent.  This is to get around an (apparent) server bug that causes
 * Keymap events to come in with a window id of 0, so Xt never calls our
 * event handler.
 *
 * If the bug is ever fixed, this code will be redundant but harmless.
 */
static void
peek_at_xevent(e)
XEvent *e;
{
	if (e->type == KeymapNotify)
		keymap_event((Widget)NULL, e, (String *)NULL, (Cardinal *)NULL);
}

/*
 * Application exit, with cleanup.
 */
void
x3270_exit(n)
int n;
{
	static Boolean exiting = 0;

	/* Handle unintentional recursion. */
	if (exiting)
		return;
	exiting = True;

	/* Turn off toggle-related activity. */
	shutdown_toggles();

	/* Shut down the socket gracefully. */
	x_disconnect();

	exit(n);
}

/*
 * Common helper functions to insert strings, through a template, into a new
 * buffer.
 * 'format' is assumed to be a printf format string with one '%s' in it.
 */
char *
xs_buffer(format, string)
char *format;
char *string;
{
	char *r;

	r = XtMalloc(strlen(format) - 1 + strlen(string));
	(void) sprintf(r, format, string);
	return r;
}

char *
xs_buffer2(format, string1, string2)
char *format;
char *string1;
char *string2;
{
	char *r;

	r = XtMalloc(strlen(format) - 3 + strlen(string1) + strlen(string2));
	(void) sprintf(r, format, string1, string2);
	return r;
}

/* Common uses of xs_buffer. */
void
xs_warning(format, string)
char *format;
char *string;
{
	char *r;

	r = xs_buffer(format, string);
	XtWarning(r);
	XtFree(r);
}

void
xs_warning2(format, string1, string2)
char *format;
char *string1;
char *string2;
{
	char *r;

	r = xs_buffer2(format, string1, string2);
	XtWarning(r);
	XtFree(r);
}

#if !defined(MEMORY_MOVE) /*[*/
/*
 * A version of memcpy that handles overlaps
 */
char *
MEMORY_MOVE(dst, src, cnt)
register char *dst;
register char *src;
register int cnt;
{
	char *r = dst;

	if (dst < src && dst + cnt - 1 >= src) {	/* overlap left */
		while (cnt--)
			*dst++ = *src++;
	} else if (src < dst && src + cnt - 1 >= dst) {	/* overlap right */
		dst += cnt;
		src += cnt;
		while (cnt--)
			*--dst = *--src;
	} else {					/* no overlap */ 
		(void) memcpy(dst, src, cnt);
	}
	return r;
}
#endif /*]*/

/*
 * Resource splitter.
 * Returns 1 (success), 0 (EOF), -1 (error).
 */
int
split_resource(st, left, right)
char **st;
char **left;
char **right;
{
	char *s = *st;
	char *t;
	Boolean quote;

	/* Skip leading white space. */
	while (isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* There must be a left-hand side. */
	if (*s == ':')
		return -1;

	/* Scan until an unquoted colon is found. */
	*left = s;
	for (; *s && *s != ':' && *s != '\n'; s++)
		if (*s == '\\' && *(s+1) == ':')
			s++;
	if (*s != ':')
		return -1;

	/* Stip white space before the colon. */
	for (t = s-1; isspace(*t); t--)
		*t = '\0';

	/* Terminate the left-hand side. */
	*(s++) = '\0';

	/* Skip white space after the colon. */
	while (*s != '\n' && isspace(*s))
		s++;

	/* There must be a right-hand side. */
	if (!*s || *s == '\n')
		return -1;

	/* Scan until an unquoted newline is found. */
	*right = s;
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (isspace(*t))
		*t-- = '\0';

	/* Done. */
	return 1;
}

/*
 * Font list parser.
 */
static void
parse_font_list()
{
	char *s, *label, *font;
	int ns;
	struct font_list *f;

	if (!appres.font_list)
		XtError("No fontList resource");
	s = XtNewString(appres.font_list);
	while ((ns = split_resource(&s, &label, &font)) == 1) {
		f = (struct font_list *)XtMalloc(sizeof(*f));
		f->label = label;
		f->font = font;
		f->next = (struct font_list *)NULL;
		if (font_list)
			font_last->next = f;
		else
			font_list = f;
		font_last = f;
		font_count++;
	}
	if (!font_count)
		XtError("Invalid fontList resource");
}

/*
 * Warning message trap, for catching colormap failures.
 */
static void
trap_colormaps(name, type, class, defaultp, params, num_params)
String name;
String type;
String class;
String defaultp;
String *params;
Cardinal *num_params;
{
    if (!strcmp(type, "cvtStringToPixel"))
	    colormap_failure = True;
    (*old_emh)(name, type, class, defaultp, params, num_params);
}
