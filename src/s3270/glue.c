/*
 * Modifications Copyright 1993, 1994, 1995, 1996 by Paul Mattes.
 * Original X11 Port Copyright 1990 by Jeff Sparkes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *   All Rights Reserved.  GTRC hereby grants public use of this software.
 *   Derivative works based on this software must incorporate this copyright
 *   notice.
 */

/*
 *	glue.c
 *		A displayless 3270 Terminal Emulator
 *		Glue for missing parts.
 */

#include "globals.h"
#include <sys/wait.h>
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
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "utilc.h"

extern void usage(char *);

/* Statics */
static void no_minus(char *arg);
static void parse_options(int *argcp, char **argv);
static void parse_set_clear(int *argcp, char **argv);

/* Globals */
char           *programname;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
AppRes          appres;
int		children = 0;
Boolean		exiting = False;
char	       *command_string = CN;

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


int
parse_command_line(int argc, char **argv, char **cl_hostname)
{
	int cl, i;
	int ovc, ovr;
	char junk;
	int sl;

	/* Figure out who we are */
	programname = strrchr(argv[0], '/');
	if (programname)
		++programname;
	else
		programname = argv[0];

	/* Save the command string. */
	cl = strlen(programname);
	for (i = 0; i < argc; i++) {
		cl += 1 + strlen(argv[i]);
	}
	cl++;
	command_string = Malloc(cl);
	(void) strcpy(command_string, programname);
	for (i = 0; i < argc; i++) {
		(void) strcat(strcat(command_string, " "), argv[i]);
	}

	/* Parse command-line options. */
	parse_options(&argc, argv);

	/* Pick out the remaining -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Verify command-line syntax. */
	switch (argc) {
	    case 1:
		if (!appres.scripted)
			usage(CN);
		break;
	    case 2:
		no_minus(argv[1]);
		*cl_hostname = argv[1];
		break;
	    case 3:
		no_minus(argv[1]);
		no_minus(argv[2]);
		*cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
		break;
	    default:
		usage(CN);
		break;
	}

	/* Sort out model, color and extended data stream modes. */
	appres.model = NewString(appres.model);
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
		appres.model = "3";
#else /*][*/
		appres.model = "4";
#endif /*]*/
	if (appres.m3279)
		appres.extended = True;
	if (appres.mono) {
		appres.m3279 = False;
	}
	if (!appres.extended)
		appres.oversize = CN;

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

	if (appres.apl_mode) {
		appres.charset = Apl;
	}

	return argc;
}

static void
no_minus(char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
}

/*
 * Pick out command-line options and set up appres.
 */
static void
parse_options(int *argcp, char **argv)
{
	int i, j;
	int argc_out = 0;
	char **argv_out = (char **) Malloc((*argcp + 1) * sizeof(char *));
#       define offset(n) (void *)&appres.n
	static struct {
		char *name;
		enum { OPT_BOOLEAN, OPT_STRING, OPT_SKIP2 } type;
		Boolean flag;
		void *aoff;
	} opts[] = {
		{ OptAplMode,  OPT_BOOLEAN, True,  offset(apl_mode) },
		{ OptCharset,  OPT_STRING,  False, offset(charset) },
		{ OptClear,    OPT_SKIP2,   False, NULL },
		{ OptModel,    OPT_STRING,  False, offset(model) },
		{ OptMono,     OPT_BOOLEAN, True,  offset(mono) },
		{ OptOversize, OPT_STRING,  False, offset(oversize) },
		{ OptPort,     OPT_STRING,  False, offset(port) },
		{ OptSet,      OPT_SKIP2,   False, NULL },
		{ OptTermName, OPT_STRING,  False, offset(termname) },
		{ CN,          OPT_SKIP2,   False, NULL }
	};
#	undef offset

	appres.mono = False;
	appres.extended = True;
	appres.m3279 = True;
	appres.modified_sel = False;
	appres.apl_mode = False;
	appres.scripted = True;
	appres.numeric_lock = False;
	appres.secure = False;
	appres.oerr_lock = False;
	appres.typeahead = True;
	appres.debug_tracing = True;
	appres.attn_lock = False;

	appres.model = "4";
	appres.hostsfile = CN;
	appres.port = "telnet";
	appres.charset = "bracket";
	appres.termname = CN;
	appres.macros = CN;
	appres.trace_dir = "/tmp";
	appres.oversize = CN;

	appres.icrnl = False;
	appres.inlcr = False;
	appres.erase = "^H";
	appres.kill = "^U";
	appres.werase = "^W";
	appres.rprnt = "^R";
	appres.lnext = "^V";
	appres.intr = "^C";
	appres.quit = "^\\";
	appres.eof = "^D";

	argv_out[argc_out++] = argv[0];

	for (i = 1; i < *argcp; i++) {
		for (j = 0; opts[j].name != CN; j++) {
			if (!strcmp(argv[i], opts[j].name))
				break;
		}
		if (opts[j].name == CN) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		switch (opts[j].type) {
		    case OPT_BOOLEAN:
			*(Boolean *)opts[j].aoff = opts[j].flag;
			break;
		    case OPT_STRING:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			*(char **)opts[j].aoff = argv[++i];
			break;
		    case OPT_SKIP2:
			argv_out[argc_out++] = argv[i++];
			if (i < *argcp)
				argv_out[argc_out++] = argv[i];
			continue;
		}
	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free(argv_out);
}

/*
 * Pick out -set and -clear toggle options.
 */
static void
parse_set_clear(int *argcp, char **argv)
{
	int i, j;
	int argc_out = 0;
	char **argv_out = (char **) Malloc((*argcp + 1) * sizeof(char *));

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
	Free(argv_out);
}

/* Screen globals. */

static int cw = 7;
int *char_width = &cw;

static int ch = 7;
int *char_height = &ch;

static Boolean f = False;
Boolean *debugging_font = &f;

Boolean flipped = False;

/* Replacements for functions in popups.c. */

#include <stdarg.h>

Boolean error_popup_visible = False;

static char vmsgbuf[4096];

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	if (sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	}
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
	va_list args;
	char *s;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	s = NewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error(s);
	Free(s);
}
