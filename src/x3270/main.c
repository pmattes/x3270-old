/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999 by Paul Mattes.
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
 *	main.c
 *		A 3270 Terminal Emulator for X11
 *		Main proceudre.
 */

#include "globals.h"
#include <sys/wait.h>
#include <X11/StringDefs.h>
#include <X11/Core.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "resourcesc.h"
#include "savec.h"
#include "screenc.h"
#include "selectc.h"
#include "statusc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

/* Externals */
#if defined(USE_APP_DEFAULTS) /*[*/
extern const char *app_defaults_version;
#else /*][*/
extern String	color_fallbacks[];
extern String	mono_fallbacks[];
#endif /*]*/

/* Globals */
char           *programname;
Display        *display;
int             default_screen;
Window          root_window;
int             screen_depth;
Widget          toplevel;
XtAppContext    appcontext;
Atom            a_delete_me, a_save_yourself, a_3270, a_registry, a_iso8859,
		a_ISO8859, a_encoding, a_1, a_state;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
Pixmap          gray;
XrmDatabase     rdb;
AppRes          appres;
int		children = 0;
Boolean		exiting = False;

/* Statics */
static void	peek_at_xevent(XEvent *);
static XtErrorMsgHandler old_emh;
static void	trap_colormaps(String, String, String, String, String *,
			Cardinal *);
static Boolean  colormap_failure = False;
#if defined(LOCAL_PROCESS) /*[*/
static void	parse_local_process(int *argcp, char **argv, char **cmds);
#endif /*]*/
static void	parse_set_clear(int *, char **);
static void	label_init(void);
static char    *user_title = CN;
static char    *user_icon_name = CN;

XrmOptionDescRec options[]= {
	{ OptActiveIcon,DotActiveIcon,	XrmoptionNoArg,		ResTrue },
	{ OptAplMode,	DotAplMode,	XrmoptionNoArg,		ResTrue },
	{ OptCharClass,	DotCharClass,	XrmoptionSepArg,	NULL },
	{ OptCharset,	DotCharset,	XrmoptionSepArg,	NULL },
	{ OptClear,	".xxx",		XrmoptionSkipArg,	NULL },
	{ OptColorScheme,DotColorScheme,XrmoptionSepArg,	NULL },
	{ OptDsTrace,	DotDsTrace,	XrmoptionNoArg,		ResTrue },
	{ OptEmulatorFont,DotEmulatorFont,XrmoptionSepArg,	NULL },
	{ OptExtended,	DotExtended,	XrmoptionNoArg,		ResTrue },
	{ OptIconName,	".iconName",	XrmoptionSepArg,	NULL },
	{ OptIconX,	".iconX",	XrmoptionSepArg,	NULL },
	{ OptIconY,	".iconY",	XrmoptionSepArg,	NULL },
	{ OptKeymap,	DotKeymap,	XrmoptionSepArg,	NULL },
	{ OptKeypadOn,	DotKeypadOn,	XrmoptionNoArg,		ResTrue },
	{ OptM3279,	DotM3279,	XrmoptionNoArg,		ResTrue },
	{ OptModel,	DotModel,	XrmoptionSepArg,	NULL },
	{ OptMono,	DotMono,	XrmoptionNoArg,		ResTrue },
	{ OptNoScrollBar,DotScrollBar,	XrmoptionNoArg,		ResFalse },
	{ OptOnce,	DotOnce,	XrmoptionNoArg,		ResTrue },
	{ OptOversize,	DotOversize,	XrmoptionSepArg,	NULL },
	{ OptPort,	DotPort,	XrmoptionSepArg,	NULL },
	{ OptReconnect,	DotReconnect,	XrmoptionNoArg,		ResTrue },
	{ OptSaveLines,	DotSaveLines,	XrmoptionSepArg,	NULL },
	{ OptScripted,	DotScripted,	XrmoptionNoArg,		ResTrue },
	{ OptScrollBar,	DotScrollBar,	XrmoptionNoArg,		ResTrue },
	{ OptSet,	".xxx",		XrmoptionSkipArg,	NULL },
	{ OptTermName,	DotTermName,	XrmoptionSepArg,	NULL },
};
int num_options = XtNumber(options);

/* Fallback resources. */
#if defined(USE_APP_DEFAULTS) /*[*/
static String fallbacks[] = {
	"*adVersion:	fallback",
	NULL
};
#else /*][*/
static String *fallbacks = color_fallbacks;
#endif /*]*/

struct toggle_name toggle_names[N_TOGGLES] = {
	{ ResMonoCase,        MONOCASE },
	{ ResAltCursor,       ALT_CURSOR },
	{ ResCursorBlink,     CURSOR_BLINK },
	{ ResShowTiming,      SHOW_TIMING },
	{ ResCursorPos,       CURSOR_POS },
#if defined(X3270_TRACE) /*[*/
	{ ResDsTrace,         DS_TRACE },
#endif /*]*/
	{ ResScrollBar,       SCROLL_BAR },
#if defined(X3270_ANSI) /*[*/
	{ ResLineWrap,        LINE_WRAP },
#endif /*]*/
	{ ResBlankFill,       BLANK_FILL },
#if defined(X3270_TRACE) /*[*/
	{ ResScreenTrace,     SCREEN_TRACE },
	{ ResEventTrace,      EVENT_TRACE },
#endif /*]*/
	{ ResMarginedPaste,   MARGINED_PASTE },
	{ ResRectangleSelect, RECTANGLE_SELECT }
};


static void
usage(const char *msg)
{
	if (msg != CN)
		XtWarning(msg);
#if defined(X3270_MENUS) /*[*/
	xs_error("Usage: %s [options] [[ps:][LUname@]hostname[:port]]", programname);
#else /*][*/
	xs_error("Usage: %s [options] [ps:][LUname@]hostname[:port]", programname);
#endif /*]*/
}

static void
no_minus(char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
}

int
main(int argc, char *argv[])
{
	char	*dname;
	int	i;
	Atom	protocols[2];
	char	*cl_hostname = CN;
	int	ovc, ovr;
	char	junk;
	int	sl;

	/* Figure out who we are */
	programname = strrchr(argv[0], '/');
	if (programname)
		++programname;
	else
		programname = argv[0];

	/* Save a copy of the command-line args for merging later. */
	save_args(argc, argv);

#if !defined(USE_APP_DEFAULTS) /*[*/
	/*
	 * Figure out which fallbacks to use, based on the "-mono"
	 * switch on the command line, and the depth of the display.
	 */
	dname = CN;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-mono"))
			fallbacks = mono_fallbacks;
		else if (!strcmp(argv[i], "-display") && argc > i)
			dname = argv[i+1];
	}
	display = XOpenDisplay(dname);
	if (display == (Display *)NULL)
		XtError("Can't open display");
	if (DefaultDepthOfScreen(XDefaultScreenOfDisplay(display)) == 1)
		fallbacks = mono_fallbacks;
	XCloseDisplay(display);
#endif /*]*/

	/* Initialize. */
	toplevel = XtVaAppInitialize(
	    &appcontext,
#if defined(USE_APP_DEFAULTS) /*[*/
	    "X3270",
#else /*][*/
	    "X3270xad",	/* explicitly _not_ X3270 */
#endif /*]*/
	    options, num_options,
	    &argc, argv,
	    fallbacks,
	    XtNinput, True,
	    XtNallowShellResize, False,
	    NULL);
	display = XtDisplay(toplevel);
	rdb = XtDatabase(display);

	/* Merge in the profile. */
	merge_profile(&rdb);

	old_emh = XtAppSetWarningMsgHandler(appcontext,
	    (XtErrorMsgHandler)trap_colormaps);
	XtGetApplicationResources(toplevel, (XtPointer)&appres, resources,
	    num_resources, 0, 0);
	(void) XtAppSetWarningMsgHandler(appcontext, old_emh);

#if defined(USE_APP_DEFAULTS) /*[*/
	/* Check the app-defaults version. */
	if (!appres.ad_version)
		XtError("Outdated app-defaults file");
	else if (!strcmp(appres.ad_version, "fallback"))
		XtError("No app-defaults file");
	else if (strcmp(appres.ad_version, app_defaults_version))
		xs_error("app-defaults version mismatch: want %s, got %s",
		    app_defaults_version, appres.ad_version);
#endif /*]*/

#if defined(LOCAL_PROCESS) /*[*/
	/* Pick out the -e option. */
	parse_local_process(&argc, argv, &cl_hostname);
#endif /*]*/

	/* Pick out -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Verify command-line syntax. */
	switch (argc) {
	    case 1:
#if !defined(X3270_MENUS) /*[*/
		if (cl_hostname == CN)
			usage(CN);
#endif /*]*/
		break;
	    case 2:
		if (cl_hostname != CN)
			usage(CN);
		no_minus(argv[1]);
		cl_hostname = argv[1];
		break;
	    case 3:
		if (cl_hostname != CN)
			usage(CN);
		no_minus(argv[1]);
		no_minus(argv[2]);
		cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
		break;
	    default:
		usage(CN);
		break;
	}

	default_screen = DefaultScreen(display);
	root_window = RootWindow(display, default_screen);
	screen_depth = DefaultDepthOfScreen(XtScreen(toplevel));

	/* Sort out model, color and extended data stream modes. */
	appres.model = XtNewString(appres.model);
	if (!strncmp(appres.model, "3278", 4)) {
		appres.m3279 = False;
		appres.model = appres.model + 4;
		if (appres.model[0] == '-')
			++appres.model;
	} else if (!strncmp(appres.model, "3279", 4)) {
		appres.m3279 = True;
		appres.model = appres.model + 4;
		if (appres.model[0] == '-')
			++appres.model;
	}
	sl = strlen(appres.model);
	if (sl && (appres.model[sl-1] == 'E' || appres.model[sl-1] == 'e')) {
		appres.extended = True;
		appres.model[sl-1] = '\0';
		sl--;
		if (sl && appres.model[sl-1] == '-')
			appres.model[sl-1] = '\0';
	}
	if (!appres.model[0])
#if defined(RESTRICT_3279) /*[*/
		appres.model = XtNewString("3");
#else /*][*/
		appres.model = XtNewString("4");
#endif /*]*/
	if (appres.m3279)
		appres.extended = True;
	if (screen_depth <= 1 || colormap_failure)
		appres.mono = True;
	if (appres.mono) {
		appres.use_cursor_color = False;
		appres.m3279 = False;
	}
	if (!appres.extended)
		appres.oversize = CN;

	a_delete_me = XInternAtom(display, "WM_DELETE_WINDOW", False);
	a_save_yourself = XInternAtom(display, "WM_SAVE_YOURSELF", False);
	a_3270 = XInternAtom(display, "3270", False);
	a_registry = XInternAtom(display, "CHARSET_REGISTRY", False);
	a_ISO8859 = XInternAtom(display, "ISO8859", False);
	a_iso8859 = XInternAtom(display, "iso8859", False);
	a_encoding = XInternAtom(display, "CHARSET_ENCODING", False);
	a_1 = XInternAtom(display, "1", False);
	a_state = XInternAtom(display, "WM_STATE", False);

	XtAppAddActions(appcontext, actions, actioncount);

	/* Initialize fonts. */
	font_init();

#if defined(RESTRICT_3279) /*[*/
	if (appres.m3279 && !strcmp(appres.model, "4"))
		appres.model = "3";
#endif /*]*/
	if (!appres.extended || appres.oversize == CN ||
	    sscanf(appres.oversize, "%dx%d%c", &ovc, &ovr, &junk) != 2) {
		ovc = 0;
		ovr = 0;
	}
	set_rows_cols(atoi(appres.model), ovc, ovr);
	if (appres.termname != CN)
		termtype = appres.termname;
	else
		termtype = full_model_name;

	hostfile_init();

	keymap_init(appres.key_map);

	if (appres.apl_mode) {
		appres.compose_map = XtNewString(Apl);
		appres.charset = XtNewString(Apl);
	}
	if (!charset_init(appres.charset))
		xs_warning("Cannot find charset \"%s\"", appres.charset);

	/* Initialize the icon. */
	icon_init();

	/*
	 * If no hostname is specified on the command line, ignore certain
	 * options.
	 */
	if (argc <= 1) {
#if defined(LOCAL_PROCESS) /*[*/
		if (cl_hostname == CN)
#endif /*]*/
			appres.once = False;
		appres.reconnect = False;
	}

#if !defined(X3270_MENUS) /*[*/
	/*
	 * If there are no menus, then -once is the default; let -reconnect
	 * override it.
	 */
	if (appres.reconnect)
		appres.once = False;
#endif /*]*/

	if (appres.char_class != CN)
		reclass(appres.char_class);

	screen_init();
	kybd_init();
	ansi_init();
	sms_init();
	error_popup_init();
	info_popup_init();

	protocols[0] = a_delete_me;
	protocols[1] = a_save_yourself;
	XSetWMProtocols(display, XtWindow(toplevel), protocols, 2);

	/* Save the command line. */
	save_init(argc, argv[1], argv[2]);

	/* Make sure we don't fall over any SIGPIPEs. */
	(void) signal(SIGPIPE, SIG_IGN);

	/* Set up the window and icon labels. */
	label_init();

	/* Handle initial toggle settings. */
#if defined(X3270_TRACE) /*[*/
	if (!appres.debug_tracing) {
		appres.toggle[DS_TRACE].value = False;
		appres.toggle[EVENT_TRACE].value = False;
	}
#endif /*]*/
	initialize_toggles();

	/* Connect to the host. */
	if (cl_hostname != CN)
		(void) host_connect(cl_hostname);

	/* Prepare to run a peer script. */
	peer_script_init();

	/* Process X events forever. */
	while (1) {
		XEvent		event;

		while (XtAppPending(appcontext) & (XtIMXEvent | XtIMTimer)) {
			if (XtAppPeekEvent(appcontext, &event))
				peek_at_xevent(&event);
			XtAppProcessEvent(appcontext,
			    XtIMXEvent | XtIMTimer);
		}
		screen_disp();
		XtAppProcessEvent(appcontext, XtIMAll);

		if (children && waitpid(0, (int *)0, WNOHANG) > 0)
			--children;
	}
}

/* Change the window and icon labels. */
static void
relabel(Boolean ignored unused)
{
	char *title;
	char icon_label[8];

	if (user_title != CN && user_icon_name != CN)
		return;
	title = XtMalloc(10 + ((PCONNECTED || appres.reconnect) ?
						strlen(current_host) : 0));
	if (PCONNECTED || appres.reconnect) {
		(void) sprintf(title, "x3270-%d%s %s", model_num,
		    (IN_ANSI ? "A" : ""), current_host);
		if (user_title == CN)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (user_icon_name == CN)
			XtVaSetValues(toplevel,
			    XtNiconName, current_host,
			    NULL);
		set_aicon_label(current_host);
	} else {
		(void) sprintf(title, "x3270-%d", model_num);
		(void) sprintf(icon_label, "x3270-%d", model_num);
		if (user_title == CN)
			XtVaSetValues(toplevel, XtNtitle, title, NULL);
		if (user_icon_name == CN)
			XtVaSetValues(toplevel, XtNiconName, icon_label, NULL);
		set_aicon_label(icon_label);
	}
	XtFree(title);
}

/* Respect the user's label/icon wishes and set up the label/icon callbacks. */
static void
label_init(void)
{
	user_title = get_resource(XtNtitle);
	user_icon_name = get_resource(XtNiconName);
	if (user_icon_name != CN)
		set_aicon_label(user_icon_name);

	register_schange(ST_HALF_CONNECT, relabel);
	register_schange(ST_CONNECT, relabel);
	register_schange(ST_3270_MODE, relabel);
	register_schange(ST_REMODEL, relabel);
}

/*
 * Peek at X events before Xt does, calling PA_KeymapNotify_action if we see a
 * KeymapEvent.  This is to get around an (apparent) server bug that causes
 * Keymap events to come in with a window id of 0, so Xt never calls our
 * event handler.
 *
 * If the bug is ever fixed, this code will be redundant but harmless.
 */
static void
peek_at_xevent(XEvent *e)
{
	static Cardinal zero = 0;

	if (e->type == KeymapNotify) {
		ia_cause = IA_PEEK;
		PA_KeymapNotify_action((Widget)NULL, e, (String *)NULL, &zero);
	}
}


/*
 * Warning message trap, for catching colormap failures.
 */
static void
trap_colormaps(String name, String type, String class, String defaultp,
    String *params, Cardinal *num_params)
{
    if (!strcmp(type, "cvtStringToPixel"))
	    colormap_failure = True;
    (*old_emh)(name, type, class, defaultp, params, num_params);
}

#if defined(LOCAL_PROCESS) /*[*/
/*
 * Pick out the -e option.
 */
static void
parse_local_process(int *argcp, char **argv, char **cmds)
{
	int i, j;
	int e_len = -1;

	for (i = 1; i < *argcp; i++) {
		if (strcmp(argv[i], OptLocalProcess))
			continue;

		/* Matched.  Copy 'em. */
		e_len = strlen(OptLocalProcess) + 1;
		for (j = i+1; j < *argcp; j++) {
			e_len += 1 + strlen(argv[j]);
		}
		e_len++;
		*cmds = XtMalloc(e_len);
		(void) strcpy(*cmds, OptLocalProcess);
		for (j = i+1; j < *argcp; j++) {
			(void) strcat(strcat(*cmds, " "), argv[j]);
		}

		/* Stamp out the remaining args. */
		*argcp = i;
		argv[i] = CN;
		break;
	}
}
#endif /*]*/

/*
 * Pick out -set and -clear toggle options.
 */
static void
parse_set_clear(int *argcp, char **argv)
{
	int i, j;
	int argc_out = 0;
	char **argv_out = (char **) XtMalloc((*argcp + 1) * sizeof(char *));

	argv_out[argc_out++] = argv[0];

	for (i = 1; i < *argcp; i++) {
		Boolean is_set = False;

		if (!strcmp(argv[i], OptSet))
			is_set = True;
		else if (strcmp(argv[i], OptClear)) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		if (i == *argcp - 1)	/* missing arg */
			continue;

		/* Delete the argument. */
		i++;

		for (j = 0; j < N_TOGGLES; j++)
			if (!strcmp(argv[i], toggle_names[j].name)) {
				appres.toggle[toggle_names[j].index].value =
				    is_set;
				break;
			}
		if (j >= N_TOGGLES)
			usage("Unknown toggle name");

	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	XtFree((XtPointer)argv_out);
}