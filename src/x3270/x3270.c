/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 * Copyright 1990 Jeff Sparkes.
 * Copyright 1993 Paul Mattes.
 *
 *                         All Rights Reserved
 */

/*
 *	x3270.c
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Shell.h>
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
int		root_window;
int		depth;
Widget		toplevel;
Widget		icon_shell;
XtAppContext	appcontext;
Atom		a_delete_me, a_spacing, a_c, a_family_name, a_3270,
		a_registry, a_iso8859, a_encoding, a_1;
int		net_sock = -1;
char		*current_host;
char		*full_current_host;
int		background, foreground;
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

enum cstate	cstate = NOT_CONNECTED;
Boolean		ansi_host = False;
Boolean		ever_3270 = False;
Boolean		exiting = False;

/* Statics */
static XtInputId	ns_read_id;
static XtInputId	ns_exception_id;
static Boolean		reading = False;
static Boolean		excepting = False;
static Pixmap		inv_icon;
static Boolean		icon_inverted = False;
static char		*user_title = (char *) NULL;
static char		*user_icon_name = (char *) NULL;
static void		find_keymaps();
static void		add_trans();
static void		peek_at_xevent();
static void		x_throttle();

static XrmOptionDescRec options[]= {
	{ "-activeicon",".activeIcon",	XrmoptionNoArg,		"true" },
	{ "-apl",	".aplMode",	XrmoptionNoArg,		"true" },
	{ "-bfont",	".boldFont",	XrmoptionSepArg,	NULL },
	{ "-charset",	".charset",	XrmoptionSepArg,	NULL },
	{ "-efont",	".emulatorFont",XrmoptionSepArg,	NULL },
	{ "-keymap",	".keymap",	XrmoptionSepArg,	NULL },
	{ "-model",	".model",	XrmoptionSepArg,	NULL },
	{ "-mono",	".mono",	XrmoptionNoArg,		"true" },
	{ "-once",	".once",	XrmoptionNoArg,		"true" },
	{ "-port",	".port",	XrmoptionSepArg,	NULL },
	{ "-tn",	".termName",	XrmoptionSepArg,	NULL },
};

#define offset(field) XtOffset(AppResptr, field)
static XtResource resources[] = {
	{ XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	  offset(foreground), XtRString, "XtDefaultForeground", },
	{ XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	  offset(background), XtRString, "XtDefaultBackground", },
	{ "colorBackground", "ColorBackground", XtRPixel, sizeof(Pixel),
	  offset(colorbg), XtRString, "black", },
	{ "selectBackground", "SelectBackground", XtRPixel, sizeof(Pixel),
	  offset(selbg), XtRString, "dim gray", },
	{ "normalColor", "NormalColor", XtRPixel, sizeof(Pixel),
	  offset(normal), XtRString, "green", },
	{ "inputColor", "InputColor", XtRPixel, sizeof(Pixel),
	  offset(select), XtRString, "orange", },
	{ "boldColor", "BoldColor", XtRPixel, sizeof(Pixel),
	  offset(bold), XtRString, "cyan", },
	{ "mono", "Mono", XtRBoolean, sizeof(Boolean),
	  offset(mono), XtRString, "false" },
	{ "keypad", "Keypad", XtRString, sizeof(String),
	  offset(keypad), XtRString, "right" },
	{ "keypadOn", "KeypadOn", XtRBoolean, sizeof(Boolean),
	  offset(keypad_on), XtRString, "false" },
	{ "menuBar", "MenuBar", XtRBoolean, sizeof(Boolean),
	  offset(menubar), XtRString, "true" },
	{ "activeIcon", "ActiveIcon", XtRBoolean, sizeof(Boolean),
	  offset(active_icon), XtRString, "false" },
	{ "labelIcon", "LabelIcon", XtRBoolean, sizeof(Boolean),
	  offset(label_icon), XtRString, "false" },
	{ "keypadBackground", "KeypadBackground", XtRPixel, sizeof(Pixel),
	  offset(keypadbg), XtRString, "grey70", },
	{ "emulatorFont", "EmulatorFont", XtRString, sizeof(char *),
	  offset(efontname), XtRString, 0, },
	{ "boldFont", "boldFont", XtRString, sizeof(char *),
	  offset(bfontname), XtRString, 0, },
	{ "visualBell", "VisualBell", XtRBoolean, sizeof(Boolean),
	  offset(visual_bell), XtRString, "false" },
	{ "aplMode", "AplMode", XtRBoolean, sizeof(Boolean),
	  offset(apl_mode), XtRString, "false" },
	{ "once", "once", XtRBoolean, sizeof(Boolean),
	  offset(once), XtRString, "false" },
	{ "model", "Model", XtRString, sizeof(char *),
	  offset(model), XtRString, "4", },
	{ "keymap", "Keymap", XtRString, sizeof(char *),
	  offset(key_map), XtRString, 0, },
	{ "composeMap", "ComposeMap", XtRString, sizeof(char *),
	  offset(compose_map), XtRString, "latin1", },
	{ "hostsFile", "HostsFile", XtRString, sizeof(char *),
	  offset(hostsfile), XtRString, "/usr/local/pub/ibm_hosts", },
	{ "port", "Port", XtRString, sizeof(char *),
	  offset(port), XtRString, "telnet", },
	{ "charset", "Charset", XtRString, sizeof(char *),
	  offset(charset), XtRString, "us", },
	{ "termName", "TermName", XtRString, sizeof(char *),
	  offset(termname), XtRString, 0, },
	{ "largeFont", "LargeFont", XtRString, sizeof(char *),
	  offset(large_font), XtRString, "3270", },
	{ "smallFont", "smallFont", XtRString, sizeof(char *),
	  offset(small_font), XtRString, "3270-12", },
	{ "iconFont", "IconFont", XtRString, sizeof(char *),
	  offset(icon_font), XtRString, "nil2", },
	{ "iconLabelFont", "IconLabelFont", XtRString, sizeof(char *),
	  offset(icon_label_font), XtRString, "8x13", },
	{ "defaultTranslations", "DefaultTranslations", XtRTranslationTable, sizeof(XtTranslations),
	  offset(default_translations), XtRTranslationTable, NULL, },
	{ "waitCursor", "WaitCursor", XtRCursor, sizeof(Cursor),
	  offset(wait_mcursor), XtRString, "watch", },
	{ "lockedCursor", "LockedCursor", XtRCursor, sizeof(Cursor),
	  offset(locked_mcursor), XtRString, "X_cursor", },
	{ "adVersion", "AdVersion", XtRString, sizeof(char *),
	  offset(ad_version), XtRString, 0, },

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
	{ "trace3270", "Trace3270", XtRBoolean, sizeof(Boolean),
	  offset(toggle[TRACE3270].value), XtRString, "false" },
	{ "traceTelnet", "TraceTelnet", XtRBoolean, sizeof(Boolean),
	  offset(toggle[TRACETN].value), XtRString, "false" },

	{ "icrnl", "Icrnl", XtRBoolean, sizeof(Boolean),
	  offset(icrnl), XtRString, "true" },
	{ "inlcr", "Inlcr", XtRBoolean, sizeof(Boolean),
	  offset(inlcr), XtRString, "false" },
	{ "erase", "Erase", XtRString, sizeof(char *),
	  offset(erase), XtRString, "^?", },
	{ "kill", "Kill", XtRString, sizeof(char *),
	  offset(kill), XtRString, "^U", },
	{ "werase", "Werase", XtRString, sizeof(char *),
	  offset(werase), XtRString, "^W", },
	{ "rprnt", "Rprnt", XtRString, sizeof(char *),
	  offset(rprnt), XtRString, "^R", },
	{ "lnext", "Lnext", XtRString, sizeof(char *),
	  offset(lnext), XtRString, "^V", },
	{ "intr", "Intr", XtRString, sizeof(char *),
	  offset(intr), XtRString, "^C", },
	{ "quit", "Quit", XtRString, sizeof(char *),
	  offset(quit), XtRString, "^\\", },
	{ "eof", "Eof", XtRString, sizeof(char *),
	  offset(eof), XtRString, "^D", },

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

	XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    XtNumber(resources), 0, 0);
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
	if (depth <= 1)
		appres.mono = True;
	rdb = XtDatabase(display);

	a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
	a_spacing = XInternAtom(display, "SPACING", False);
	a_c = XInternAtom(display, "C", False);
	a_family_name = XInternAtom(display, "FAMILY_NAME", False);
	a_3270 = XInternAtom(display, "3270", False);
	a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
	a_iso8859 = XInternAtom(display, "ISO8859", False);
	a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
	a_1 = XInternAtom(display, "1", False);

	XtAppAddActions(appcontext, actions, actioncount);

	if (argc > 2) {
		buf = xs_buffer("Usage: %s [[a:]hostname]", programname);
		XtError(buf);
	}
	foreground = appres.foreground;
	background = appres.background;

	/*
	 * Font stuff.  Try the "emulatorFont" resource; if that isn't right,
	 * complain but keep going.  Try the "font" resource next, but don't
	 * say anything if it's wrong.  Finally, fall back to "fixed", and
	 * die if that won't work.
	 */
	if (appres.apl_mode) {
		if ((appres.efontname = get_resource("aplFont")) == NULL)
			XtWarning("Can't find aplFont resource");
		appres.bfontname = NULL;
	}
	if (!appres.efontname)
		appres.efontname = appres.large_font;
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
	XSetWMProtocols(display, XtWindow(toplevel), &a_delete_me, 1);

	/* Make sure we don't fall over any SIGPIPEs. */
	signal(SIGPIPE, SIG_IGN);

	/* Respect the user's title wishes. */
	user_title = get_resource("title");
	user_icon_name = get_resource("iconName");
	if (user_icon_name)
		set_aicon_label(user_icon_name);

	/* Connect to the host. */
	if (argc > 1)
		(void) x_connect(argv[1]);
	else
		x_disconnect();

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

	sprintf(namebuf, "message.%s", key);
	if (r = get_resource(namebuf))
		return r;
	else {
		sprintf(namebuf, "[missing '%s' message]", key);
		return namebuf;
	}
}


/*
 * Network connect/disconnect operations, combined with X input operations
 *
 * Returns 0 for success, -1 for error.
 */
#define A_COLON(s)	(!strncmp(s, "a:", 2) || !strncmp(s, "A:", 2))
int
x_connect(pseudonym)
char	*pseudonym;
{
	char *raw_name;
	Boolean manual_ansi = False;
	char *real_name;
	char *ps;
	Boolean pending;

	if (CONNECTED)
		return 0;

	/* Remember exactly what they typed. */
	raw_name = pseudonym;

	/* Strip off 'a:' and remember it. */
	ansi_host = False;
	if (A_COLON(pseudonym))
		ansi_host = True;

	/* Look up the name in the hosts file and record any pending string. */
	if (hostfile_lookup(pseudonym, &real_name, &ps)) {
		ps_set(XtNewString(ps));
	} else {
		real_name = pseudonym;
		ps_set((char *) NULL);
	}

	/* Maybe the translated name in the hosts file had an 'a:'. */
	if (A_COLON(real_name)) {
		ansi_host = True;
		real_name += 2;
	}

	/* Attempt contact. */
	ever_3270 = False;
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
	if (A_COLON(current_host))
		current_host += 2;

	/* Prepare for I/O. */
	ns_exception_id = XtAppAddInput(appcontext, net_sock,
	    (XtPointer) XtInputExceptMask, net_exception, (XtPointer) NULL);
	excepting = True;
	ns_read_id = XtAppAddInput(appcontext, net_sock,
	    (XtPointer) XtInputReadMask, net_input, (XtPointer) NULL);
	reading = True;

	/* Tell the world. */
	menubar_connect();
	screen_connect();
	kybd_connect();
	relabel();
	ansi_init();

	return 0;
}
#undef A_COLON

void
x_disconnect()
{
	ps_set((char *) NULL);
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
				exit(0);
		}
		cstate = NOT_CONNECTED;
	}
	menubar_connect();
	screen_connect();
	kybd_connect();
	relabel();
}

void
x_in3270()
{
	cstate = CONNECTED_3270;
	ever_3270 = True;
	menubar_newmode();
	relabel();
	kybd_connect();
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
	char icon[8];

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
		(void) sprintf(icon, "x3270-%d", model_num);
		if (!user_title)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (!user_icon_name)
			XtVaSetValues(toplevel, XtNiconName, icon, NULL);
		set_aicon_label(icon);
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
		keymap_event(NULL, e, NULL, NULL);
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

#ifndef MEMORY_MOVE
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
#endif
