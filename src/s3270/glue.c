/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 2000 by Paul Mattes.
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
#include "gluec.h"
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

#define LAST_ARG	"--"

#if defined(C3270) /*[*/
extern void merge_profile(void); /* XXX */
#endif /*]*/

/* Statics */
static void no_minus(char *arg);
#if defined(LOCAL_PROCESS) /*[*/
static void parse_local_process(int *argcp, char **argv, char **cmds);
#endif /*]*/
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
static Boolean	sfont = False;
Boolean	       *standard_font = &sfont;

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
	int hn_argc;

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

#if defined(LOCAL_PROCESS) /*[*/ 
        /* Pick out the -e option. */
        parse_local_process(&argc, argv, cl_hostname);
#endif /*]*/    

	/* Parse command-line options. */
	parse_options(&argc, argv);

	/* Pick out the remaining -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Now figure out if there's a hostname. */
	for (hn_argc = 1; hn_argc < argc; hn_argc++) {
		if (!strcmp(argv[hn_argc], LAST_ARG))
			break;
	}

	/* Verify command-line syntax. */
	switch (hn_argc) {
	    case 1:
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

	/* Delete the host name and any "--". */
	if (argv[hn_argc] != CN && !strcmp(argv[hn_argc], LAST_ARG))
		hn_argc++;
	if (hn_argc > 1) {
		for (i = 1; i < argc - hn_argc + 2; i++) {
			argv[i] = argv[i + hn_argc - 1];
		}
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

	if (appres.apl_mode)
		appres.charset = Apl;
	if (*cl_hostname == CN)
		appres.once = False;

	return argc;
}

static void
no_minus(char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
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
		*cmds = Malloc(e_len);
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
 * Pick out command-line options and set up appres.
 */
static void
parse_options(int *argcp, char **argv)
{
	int i, j;
	int argc_out = 0;
	char **argv_out = (char **) Malloc((*argcp + 1) * sizeof(char *));
#       define offset(n) (void *)&appres.n
#       define toggle_offset(index) offset(toggle[index].value)
	static struct {
		const char *name;
		enum {
		    OPT_BOOLEAN, OPT_STRING, OPT_XRM, OPT_SKIP2, OPT_DONE
		} type;
		Boolean flag;
		void *aoff;
	} opts[] = {
		{ OptAplMode,  OPT_BOOLEAN, True,  offset(apl_mode) },
		{ OptCharset,  OPT_STRING,  False, offset(charset) },
		{ OptClear,    OPT_SKIP2,   False, NULL },
		{ OptDsTrace,  OPT_BOOLEAN, True,  toggle_offset(DS_TRACE) },
		{ OptHostsFile,OPT_STRING,  False, offset(hostsfile) },
#if defined(C3270) /*[*/
		{ OptKeymap,   OPT_STRING,  False, offset(key_map) },
#endif /*]*/
		{ OptModel,    OPT_STRING,  False, offset(model) },
		{ OptMono,     OPT_BOOLEAN, True,  offset(mono) },
		{ OptOnce,     OPT_BOOLEAN, True,  offset(once) },
		{ OptOversize, OPT_STRING,  False, offset(oversize) },
		{ OptPort,     OPT_STRING,  False, offset(port) },
		{ OptSet,      OPT_SKIP2,   False, NULL },
		{ OptTermName, OPT_STRING,  False, offset(termname) },
		{ OptTraceFile,OPT_STRING,  False, offset(trace_file) },
		{ "-xrm",      OPT_XRM,     False, NULL },
		{ LAST_ARG,    OPT_DONE,    False, NULL },
		{ CN,          OPT_SKIP2,   False, NULL }
	};

	/* Set the defaults. */
	appres.mono = False;
	appres.extended = True;
	appres.m3279 = True;
	appres.modified_sel = False;
	appres.apl_mode = False;
#if defined(C3270) || defined(TCL3270) /*[*/
	appres.scripted = False;
#else /*][*/
	appres.scripted = True;
#endif /*]*/
	appres.numeric_lock = False;
	appres.secure = False;
#if defined(C3270) /*[*/
	appres.oerr_lock = True;
#else /*][*/
	appres.oerr_lock = False;
#endif /*]*/
	appres.typeahead = True;
	appres.debug_tracing = True;
#if defined(C3270) /*[*/
	appres.compose_map = "latin1";
#endif /*]*/

	appres.model = "4";
	appres.hostsfile = CN;
	appres.port = "telnet";
	appres.charset = "bracket";
	appres.termname = CN;
	appres.macros = CN;
	appres.trace_dir = "/tmp";
	appres.oversize = CN;
	appres.ft_command = "ind$file";

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

#if defined(C3270) /*[*/
	/* Merge in the profile. */
	merge_profile();
#endif /*]*/

	/* Parse the command-line options. */
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
		    case OPT_XRM:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			parse_xrm(argv[++i], "-xrm");
			break;
		    case OPT_SKIP2:
			argv_out[argc_out++] = argv[i++];
			if (i < *argcp)
				argv_out[argc_out++] = argv[i];
			continue;
		    case OPT_DONE:
			while (i < *argcp)
				argv_out[argc_out++] = argv[i++];
			break;
		}
	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free(argv_out);

	/* One isn't very useful without the other. */
	if (appres.toggle[DS_TRACE].value)
		appres.toggle[EVENT_TRACE].value = True;
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

/*
 * Parse '-xrm' options.
 * Understands only:
 *   {c,s,tcl}3270.<resourcename>: value
 * Asterisks and class names need not apply.
 */

static struct {
	const char *name;
	void *address;
	enum resource_type { XRM_STRING, XRM_BOOLEAN } type;
} resources[] = {
	{ ResCharset,	offset(charset),	XRM_STRING },
	{ ResEof,	offset(eof),		XRM_STRING },
	{ ResErase,	offset(erase),		XRM_STRING },
	{ ResExtended,	offset(extended),	XRM_BOOLEAN },
	{ ResFtCommand,	offset(ft_command),	XRM_STRING },
	{ ResHostsFile,	offset(hostsfile),	XRM_STRING },
	{ ResIcrnl,	offset(icrnl),		XRM_BOOLEAN },
	{ ResInlcr,	offset(inlcr),		XRM_BOOLEAN },
	{ ResIntr,	offset(intr),		XRM_STRING },
#if defined(C3270) /*[*/
	{ ResKeymap,	offset(key_map),	XRM_STRING },
#endif /*]*/
	{ ResKill,	offset(kill),		XRM_STRING },
	{ ResLnext,	offset(lnext),		XRM_STRING },
	{ ResM3279,	offset(m3279),		XRM_BOOLEAN },
	{ ResModel,	offset(model),		XRM_STRING },
	{ ResModifiedSel, offset(modified_sel),	XRM_BOOLEAN },
	{ ResMono,	offset(mono),		XRM_BOOLEAN },
	{ ResNumericLock, offset(numeric_lock),	XRM_BOOLEAN },
	{ ResOerrLock,	offset(oerr_lock),	XRM_BOOLEAN },
	{ ResOversize,	offset(oversize),	XRM_STRING },
	{ ResPort,	offset(port),		XRM_STRING },
	{ ResQuit,	offset(quit),		XRM_STRING },
	{ ResRprnt,	offset(rprnt),		XRM_STRING },
	{ ResSecure,	offset(secure),		XRM_BOOLEAN },
	{ ResTermName,	offset(termname),	XRM_STRING },
	{ ResTraceDir,	offset(trace_dir),	XRM_STRING },
	{ ResTraceFile,	offset(trace_file),	XRM_STRING },
	{ ResTypeahead,	offset(typeahead),	XRM_BOOLEAN },
	{ ResWerase,	offset(werase),		XRM_STRING },

	{ CN,		0,			XRM_STRING }
};

/*
 * Compare two strings, allowing the second to differ by uppercasing the
 * first character of the second.
 */
static int
strncapcmp(const char *known, const char *unknown, unsigned unk_len)
{
	if (unk_len != strlen(known))
		return -1;
	if (!strncmp(known, unknown, unk_len))
		return 0;
	if (unk_len > 1 &&
	    unknown[0] == toupper(known[0]) &&
	    !strncmp(known + 1, unknown + 1, unk_len - 1))
		return 0;
	return -1;
}


#if defined(C3270) /*[*/
#define ME	"c3270"
#elif defined(TCL3270) /*][*/
#define ME	"tcl3270"
#else /*][*/
#define ME	"s3270"
#endif /*]*/

void
parse_xrm(const char *arg, const char *where)
{
	static char me_dot[] = ME ".";
	static char me_star[] = ME "*";
	unsigned match_len;
	const char *s;
	unsigned rnlen;
	int i;
	char *t;
	void *address = NULL;
	enum resource_type type = XRM_STRING;
#if defined(C3270) /*[*/
	char *add_buf = CN;
#endif /*]*/

	/* Enforce "-3270." or "-3270*" or "*". */
	if (!strncmp(arg, me_dot, sizeof(me_dot)-1))
		match_len = sizeof(me_dot)-1;
	else if (!strncmp(arg, me_star, sizeof(me_star)-1))
		match_len = sizeof(me_star)-1;
	else if (arg[0] == '*')
		match_len = 1;
	else {
		xs_warning("%s: Invalid resource syntax '%.*s', name must "
		    "begin with '%s'",
		    where, sizeof(me_dot)-1, arg, me_dot);
		return;
	}

	/* Separate the parts. */
	s = arg + match_len;
	while (*s && *s != ':' && !isspace(*s))
		s++;
	rnlen = s - (arg + match_len);
	if (!rnlen) {
		xs_warning("%s: Invalid resource syntax, missing resource "
		    "name", where);
		return;
	}
	while (isspace(*s))
		s++;
	if (*s != ':') {
		xs_warning("%s: Invalid resource syntax, missing ':'", where);
		return;
	}
	s++;
	while (isspace(*s))
		s++;

	/* Look up the name. */
	for (i = 0; resources[i].name != CN; i++) {
		if (!strncapcmp(resources[i].name, arg + match_len, rnlen)) {
			address = resources[i].address;
			type = resources[i].type;
			break;
		}
	}
	if (address == NULL) {
		for (i = 0; i < N_TOGGLES; i++) {
			if (!strncapcmp(toggle_names[i].name, arg + match_len,
			    rnlen)) {
				address =
				    &appres.toggle[toggle_names[i].index].value;
				type = XRM_BOOLEAN;
				break;
			}
		}
	}
#if defined(C3270) /*[*/
	if (address == NULL) {
		if (!strncasecmp(ResKeymap ".", arg + match_len,
		                 strlen(ResKeymap ".")) ||
		    !strncasecmp(ResCharset ".", arg + match_len,
		                 strlen(ResCharset ".")) ||
		    !strncasecmp("printer.", arg + match_len, 8)) {
			add_buf = Malloc(strlen(s) + 1);
			address = add_buf;
			type = XRM_STRING;
		}
	}
#endif /*]*/
	if (address == NULL) {
		xs_warning("%s: Unknown resource name: %.*s",
		    where, (int)rnlen, arg + match_len);
		return;
	}
	switch (type) {
	case XRM_BOOLEAN:
		if (!strcasecmp(s, "true") ||
		    !strcasecmp(s, "t") ||
		    !strcmp(s, "1")) {
			*(Boolean *)address = True;
		} else if (!strcasecmp(s, "false") ||
		    !strcasecmp(s, "f") ||
		    !strcmp(s, "0")) {
			*(Boolean *)address = False;
		} else {
			xs_warning("%s: Invalid Boolean value: %s", where, s);
		}
		break;
	case XRM_STRING:
		t = Malloc(strlen(s) + 1);
		*(char **)address = t;
		if (*s == '"') {
			Boolean quoted = False;
			char c;

			s++;
			while ((c = *s++) != '\0') {
				if (quoted) {
					switch (c) {
					case 'n':
						*t++ = '\n';
						break;
					case 'r':
						*t++ = '\r';
						break;
					case 'b':
						*t++ = '\b';
						break;
					default:
						*t++ = c;
						break;
					}
					quoted = False;
				} else if (c == '\\') {
					quoted = True;
				} else if (c == '"') {
					break;
				} else {
					*t++ = c;
				}
			}
			*t = '\0';
		} else {
			(void) strcpy(t, s);
		}
		break;
	}

#if defined(C3270) /*[*/
	/* Add a new, arbitrarily-named resource. */
	if (add_buf != CN) {
		char *rsname;

		rsname = Malloc(rnlen);
		(void) strncpy(rsname, arg + match_len, rnlen);
		rsname[rnlen] = '\0';
		add_resource(rsname, NewString(s));
	}
#endif /*]*/
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
	va_end(args);
	if (sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	} else {
#if defined(C3270) /*[*/
		screen_suspend();
#endif /*]*/
		(void) fprintf(stderr, "%s\n", vmsgbuf);
		macro_output = True;
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
	va_end(args);
	s = NewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error(s);
	Free(s);
}

void
action_output(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	if (sms_redirect()) {
		sms_info(vmsgbuf);
		return;
	} else {
		FILE *aout;

#if defined(C3270) /*[*/
		screen_suspend();
		aout = start_pager();
#else /*][*/
		aout = stdout;
#endif /*]*/
		(void) fprintf(aout, "%s\n", vmsgbuf);
		macro_output = True;
	}
}
