/*
 * Copyright 2000 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	keymap.c
 *		A curses-based 3270 Terminal Emulator
 *		Keyboard mapping
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "hostc.h"
#include "keymapc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "statusc.h"
#include "trace_dsc.h"
#include "utilc.h"

#undef COLS
extern int cCOLS;

#undef COLOR_BLACK
#undef COLOR_RED    
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE   
#undef COLOR_WHITE
#if defined(HAVE_NCURSES_H) /*[*/
#include <ncurses.h>
#else /*][*/
#include <curses.h>
#endif /*]*/

#define KM_CTRL		0x0001
#define KM_META		0x0002
#define KM_KEYMAP	0x8000

struct keymap {
	struct keymap *next;
	struct keymap *successor;
	int ncodes;		/* number of key codes */
	int *codes;		/* key codes (ASCII or ncurses symbols) */
	int *hints;		/* hints (flags) */
	char *file;		/* file or resource name */
	int line;		/* keymap line number */
	char *action;		/* actions to perform */
};

static struct keymap *master_keymap = NULL;
static struct keymap *lastk = NULL;

static Boolean last_nvt = False;

static int lookup_ccode(const char *s);
static void keymap_3270_mode(Boolean);

#define codecmp(k1, k2, len)	\
	memcmp((k1)->codes, (k2)->codes, (len)*sizeof(int))

/*
 * Parse a key definition.
 * Returns <0 for error, 1 for key found and parsed, 0 for nothing found.
 * Returns the balance of the string and the character code.
 * Is destructive.
 */
static int
parse_keydef(char **str, int *ccode, int *hint)
{
	char *s = *str;
	char *t;
	char *ks;
	int flags = 0;
	KeySym Ks;

	/* Check for nothing. */
	while (isspace(*s))
		s++;
	if (!*s)
		return 0;
	*str = s;

	s = strstr(s, "<Key>");
	if (s == CN)
		return -1;
	ks = s + 5;
	*s = '\0';
	s = *str;
	while (*s) {
		while (isspace(*s))
			s++;
		if (!*s)
			break;
		if (!strncmp(s, "Meta", 4)) {
			flags |= KM_META;
			s += 4;
		} else if (!strncmp(s, "Ctrl", 4)) {
			flags |= KM_CTRL;
			s += 4;
		} else
			return -2;
	}
	s = ks;
	while (isspace(*s))
		s++;
	if (!*s)
		return -3;

	t = s;
	while (*t && !isspace(*t))
		t++;
	if (*t)
		*t++ = '\0';
	Ks = StringToKeysym(s);
	if (Ks == NoSymbol) {
		*ccode = lookup_ccode(s);
		if (*ccode == -1)
			return -4;
		if (flags)
			return -5;
	} else
		*ccode = (int)Ks;

	if (flags & KM_CTRL)
		*ccode &= 0x1f;
	if (flags & KM_META)
		*ccode |= 0x80;

	/* Return the remaining string, and success. */
	*str = t;
	*hint = flags;
	return 1;
}

static char *pk_errmsg[] = {
	"Missing <Key>",
	"Unknown modifier",
	"Missing keysym",
	"Unknown keysym",
	"Can't use modifier with curses symbol"
};

/*
 * Locate a keymap resource or file.
 * Returns 0 for do-nothing, 1 for success, -1 for error.
 * On success, returns the full name of the resource or file (which must be
 *  freed) in '*fullname'.
 * On success, returns a resource string (which must be closed) or NULL
 *  (indicating a file name to open is in *fullname) in '*r'.
 */
static int
locate_keymap(const char *name, char **fullname, char **r)
{
	char *fn = CN;			/* resource or file name */
	char *fn_nvt = CN;		/* .nvt version of fn */
	char *rs;			/* resource value */
	char *rs_nvt;			/* .nvt version of rs */

	char *fnx;			/* expanded file name */
	char *fnx_nvt;			/* .nvt version of fnx */
	int a;				/* access(fnx) */
	int a_nvt;			/* access(fnx_nvt) */

	/* Return nothing, to begin with. */
	*fullname = CN;
	*r = CN;

	/* See if it's a resource. */
	fn = xs_buffer(ResKeymap ".%s", name);
	rs = get_resource(fn);
	Free(fn);
	fn = CN;

	fn_nvt = xs_buffer(ResKeymap ".%s.%s", name, ResNvt);
	rs_nvt = get_resource(fn_nvt);
	Free(fn_nvt);
	fn_nvt = CN;

	/* In ANSI mode, the NVT version supercedes the plain version. */
	if (IN_ANSI && rs_nvt != CN) {
		*fullname = xs_buffer("%s.%s", name, ResNvt);
		*r = NewString(rs_nvt);
		return 1;
	}

	/*
	 * If there's an NVT version and no plain version, but we're not
	 * in ANSI mode, do nothing (successfully).
	 */
	if (!IN_ANSI && rs_nvt != CN && rs == CN)
		return 0;

	/* If there's a plain version, return it. */
	if (rs != CN) {
		*fullname = NewString(name);
		*r = NewString(rs);
		return 1;
	}

	/* See if it's a file. */
	fnx = do_subst(name, True, True);
	a = access(fnx, R_OK);

	fn_nvt = xs_buffer("%s.%s", name, ResNvt);
	fnx_nvt = do_subst(fn_nvt, True, True);
	Free(fn_nvt);
	a_nvt = access(fnx_nvt, R_OK);

	/* In ANSI mode, the NVT version supercedes the plain version. */
	if (IN_ANSI && a_nvt == 0) {
		Free(fnx);
		*fullname = fnx_nvt;
		return 1;
	}

	/*
	 * If there's an NVT version and no plain version, but we're not
	 * in ANSI mode, do nothing (successfully).
	 */
	if (!IN_ANSI && a_nvt == 0 && a < 0) {
		Free(fnx);
		Free(fnx_nvt);
		return 0;
	}

	/* If there's a plain version, return it. */
	if (a == 0) {
		Free(fnx_nvt);
		*fullname = fnx;
		return 1;
	}

	/* No dice. */
	Free(fnx);
	Free(fnx_nvt);
	return -1;
}

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but of course, they
 * are different.
 */
static int
read_keymap(const char *name)
{
	char *r0, *r;			/* resource value */
	char *fn = CN;			/* full resource or file name */
	int rc = 0;			/* function return code */
	FILE *f = NULL;			/* resource file */
	char buf[1024];			/* file read buffer */
	int line = 0;			/* line number */
	char *left, *right;		/* chunks of line */
	struct keymap *j;		/* compare keymap */
	struct keymap *k;		/* new keymap entry */

	/* Find the resource or file. */
	switch (rc = locate_keymap(name, &fn, &r0)) {
		case 0:
			return 0;
		case -1:
			xs_warning("No such keymap resource or file: %s",
			    name);
			return -1;
		default:
			break;
	}
	if (r0 != CN)
		r = r0;
	else {
		f = fopen(fn, "r");
		if (f == NULL) {
			xs_warning("Cannot open file: %s", fn);
			Free(fn);
			return -1;
		}
	}

	while ((r != CN)? (rc = split_dresource(&r, &left, &right)):
		          fgets(buf, sizeof(buf), f) != CN) {
		char *s;
		int ccode;
		int pkr;
		int hint;

		line++;

		/* Skip empty lines and comments. */
		if (r == CN) {
			s = buf;
			while (isspace(*s))
				s++;
			if (!*s || *s == '!' || *s == '#')
				continue;
		}

		/* Split. */
		if (rc < 0 ||
		    (r == CN && split_dresource(&s, &left, &right) < 0)) {
			popup_an_error("%s, line %d: syntax error",
			    fn, line);
			rc = -1;
			goto done;
		}

		pkr = parse_keydef(&left, &ccode, &hint);
		if (pkr == 0) {
			popup_an_error("%s, line %d: Missing <Key>",
			    fn, line);
			rc = -1;
			goto done;
		}
		if (pkr < 0) {
			popup_an_error("%s, line %d: %s",
			    fn, line, pk_errmsg[-1 - pkr]);
			rc = -1;
			goto done;
		}

		/* Add a new one at the end of the list. */
		k = Malloc(sizeof(struct keymap));
		k->next = NULL;
		k->successor = NULL;
		k->ncodes = 1;
		k->codes = Malloc(sizeof(int));
		k->codes[0] = ccode;
		k->hints = Malloc(sizeof(int));
		k->hints[0] = hint;
		k->file = NewString(fn);
		k->line = line;
		k->action = NewString(right);
		if (lastk != NULL)
			lastk->next = k;
		else
			master_keymap = k;
		lastk = k;

		while ((pkr = parse_keydef(&left, &ccode, &hint)) != 0) {
			if (pkr < 0) {
				popup_an_error("%s, line %d: %s",
				    fn, line, pk_errmsg[-1 - pkr]);
				rc = -1;
				goto done;
			}
			k->ncodes++;
			k->codes = Realloc(k->codes, k->ncodes*sizeof(int));
			k->codes[k->ncodes - 1] = ccode;
			k->hints = Realloc(k->hints, k->ncodes*sizeof(int));
			k->hints[k->ncodes - 1] = hint;
		}

		/* Later definitions supercede older ones. */
		for (j = master_keymap; j != NULL; j = j->next) {
			if (j != k &&
			    j->ncodes == k->ncodes &&
			    !codecmp(j, k, k->ncodes)) {
				j->successor = k;
			}
		}
	}

    done:
	if (r0 != CN)
		Free(r0);
	if (f != NULL)
		fclose(f);
	if (fn != CN)
		Free(fn);
	return rc;
}

/* Multi-key keymap support. */
static struct keymap *current_match = NULL;
static int consumed = 0;
static char *ignore = "[ignore]";

/* Find the shortest keymap with a longer match than k. */
static struct keymap *
longer_match(struct keymap *k, int nc)
{
	struct keymap *j;
	struct keymap *shortest = NULL;

	for (j = master_keymap; j != NULL; j = j->next) {
		if (j != k && j->ncodes > nc && !codecmp(j, k, nc)) {
			if (j->ncodes == nc+1)
				return j;
			if (shortest == NULL || j->ncodes < shortest->ncodes)
				shortest = j;
		}
	}
	return shortest;
}

/*
 * Helper function that returns a keymap action, sets the status line, and
 * traces the result.  
 *
 * If s is NULL, then this is a failed initial lookup.
 * If s is 'ignore', then this is a lookup in progress (k non-NULL) or a
 *  failed multi-key lookup (k NULL).
 * Otherwise, this is a successful lookup.
 */
static char *
status_ret(char *s, struct keymap *k)
{
	/* Set the compose indicator based on the new value of current_match. */
	if (k != NULL)
		status_compose(True, ' ', KT_STD);
	else
		status_compose(False, 0, KT_STD);

	if (s != NULL && s != ignore)
		trace_event(" %s:%d -> %s\n", current_match->file,
		    current_match->line, s);
	if ((current_match = k) == NULL)
		consumed = 0;
	return s;
}

/* Timeout for ambiguous keymaps. */
static struct keymap *timeout_match = NULL;
static unsigned long kto = 0L;

static void
key_timeout(void)
{
	trace_event("Keymap timeout\n");
	kto = 0L;
	current_match = timeout_match;
	push_keymap_action(status_ret(timeout_match->action, NULL));
	timeout_match = NULL;
}

static struct keymap *
ambiguous(struct keymap *k, int nc)
{
	struct keymap *j;

	if ((j = longer_match(k, nc)) != NULL) {
		trace_event(" ambiguous, setting timer\n");
		timeout_match = k;
		kto = AddTimeOut(500L, key_timeout);
	}
	return j;
}

/*
 * Look up an key in the keymap, return the matching action if there is one.
 *
 * This code implements the mutli-key lookup, by returning dummy actions for
 * partial matches.  It does not handle substrings, i.e., if there is a
 * definition for "Ctrl<Key>a" and a definition for "Ctrl<Key>a <Key>F1",
 * the first will either override the second (if it appears first) or
 * be disabled.  I suppose we could implement timeouts, but that would be
 * tremendously ugly, and would be of use only for funky terminals that aren't
 * properly defined in termcap (vs. human multi-key sequences).
 */
char *
lookup_key(int code)
{
	struct keymap *j, *k;

	/* If there's a timeout pending, cancel it. */
	if (kto) {
		RemoveTimeOut(kto);
		kto = 0L;
		timeout_match = NULL;
	}

	/* If there's no match pending, find the shortest one. */
	if (current_match == NULL) {
		struct keymap *shortest = NULL;

		for (k = master_keymap; k != NULL; k = k->next) {
			if (k->successor != NULL)
				continue;
			if (code == k->codes[0]) {
				if (k->ncodes == 1) {
					shortest = k;
					break;
				}
				if (shortest == NULL ||
				    k->ncodes < shortest->ncodes) {
					shortest = k;
				}
			}
		}
		if (shortest != NULL) {
			current_match = shortest;
			consumed = 0;
		} else
			return NULL;
	}

	/* See if this character matches the next one we want. */
	if (code == current_match->codes[consumed]) {
		consumed++;
		if (consumed == current_match->ncodes) {
			/* Final match. */
			j = ambiguous(current_match, consumed);
			if (j == NULL)
				return status_ret(current_match->action, NULL);
			else
				return status_ret(ignore, j);
		} else {
			/* Keep looking. */
			trace_event(" partial match\n");
			return status_ret(ignore, current_match);
		}
	}

	/* It doesn't.  Try for a better candidate. */
	for (k = master_keymap; k != NULL; k = k->next) {
		if (k->successor != NULL)
			continue;
		if (k == current_match)
			continue;
		if (k->ncodes > consumed &&
		    !codecmp(k, current_match, consumed) &&
		    k->codes[consumed] == code) {
			consumed++;
			if (k->ncodes == consumed) {
				j = ambiguous(k, consumed);
				if (j == NULL)
					return status_ret(k->action,
					    NULL);
				else
					return status_ret(ignore, j);
			} else
				return status_ret(ignore, k);
		}
	}

	/* Complain. */
	beep();
	trace_event(" invalid sequence\n");
	return status_ret(ignore, NULL);
}

static struct {
	const char *name;
	int code;
} ncurses_key[] = {
	{ "BREAK",	KEY_BREAK },
	{ "DOWN",	KEY_DOWN },
	{ "UP",		KEY_UP },
	{ "LEFT",	KEY_LEFT },
	{ "RIGHT",	KEY_RIGHT },
	{ "HOME",	KEY_HOME },
	{ "BACKSPACE",	KEY_BACKSPACE },
	{ "F0",		KEY_F0 },
	{ "DL",		KEY_DL },
	{ "IL",		KEY_IL },
	{ "DC",		KEY_DC },
	{ "IC",		KEY_IC },
	{ "EIC",	KEY_EIC },
	{ "CLEAR",	KEY_CLEAR },
	{ "EOS",	KEY_EOS },
	{ "EOL",	KEY_EOL },
	{ "SF",		KEY_SF },
	{ "SR",		KEY_SR },
	{ "NPAGE",	KEY_NPAGE },
	{ "PPAGE",	KEY_PPAGE },
	{ "STAB",	KEY_STAB },
	{ "CTAB",	KEY_CTAB },
	{ "CATAB",	KEY_CATAB },
	{ "ENTER",	KEY_ENTER },
	{ "SRESET",	KEY_SRESET },
	{ "RESET",	KEY_RESET },
	{ "PRINT",	KEY_PRINT },
	{ "LL",		KEY_LL },
	{ "A1",		KEY_A1 },
	{ "A3",		KEY_A3 },
	{ "B2",		KEY_B2 },
	{ "C1",		KEY_C1 },
	{ "C3",		KEY_C3 },
	{ "BTAB",	KEY_BTAB },
	{ "BEG",	KEY_BEG },
	{ "CANCEL",	KEY_CANCEL },
	{ "CLOSE",	KEY_CLOSE },
	{ "COMMAND",	KEY_COMMAND },
	{ "COPY",	KEY_COPY },
	{ "CREATE",	KEY_CREATE },
	{ "END",	KEY_END },
	{ "EXIT",	KEY_EXIT },
	{ "FIND",	KEY_FIND },
	{ "HELP",	KEY_HELP },
	{ "MARK",	KEY_MARK },
	{ "MESSAGE",	KEY_MESSAGE },
	{ "MOVE",	KEY_MOVE },
	{ "NEXT",	KEY_NEXT },
	{ "OPEN",	KEY_OPEN },
	{ "OPTIONS",	KEY_OPTIONS },
	{ "PREVIOUS",	KEY_PREVIOUS },
	{ "REDO",	KEY_REDO },
	{ "REFERENCE",	KEY_REFERENCE },
	{ "REFRESH",	KEY_REFRESH },
	{ "REPLACE",	KEY_REPLACE },
	{ "RESTART",	KEY_RESTART },
	{ "RESUME",	KEY_RESUME },
	{ "SAVE",	KEY_SAVE },
	{ "SBEG",	KEY_SBEG },
	{ "SCANCEL",	KEY_SCANCEL },
	{ "SCOMMAND",	KEY_SCOMMAND },
	{ "SCOPY",	KEY_SCOPY },
	{ "SCREATE",	KEY_SCREATE },
	{ "SDC",	KEY_SDC },
	{ "SDL",	KEY_SDL },
	{ "SELECT",	KEY_SELECT },
	{ "SEND",	KEY_SEND },
	{ "SEOL",	KEY_SEOL },
	{ "SEXIT",	KEY_SEXIT },
	{ "SFIND",	KEY_SFIND },
	{ "SHELP",	KEY_SHELP },
	{ "SHOME",	KEY_SHOME },
	{ "SIC",	KEY_SIC },
	{ "SLEFT",	KEY_SLEFT },
	{ "SMESSAGE",	KEY_SMESSAGE },
	{ "SMOVE",	KEY_SMOVE },
	{ "SNEXT",	KEY_SNEXT },
	{ "SOPTIONS",	KEY_SOPTIONS },
	{ "SPREVIOUS",	KEY_SPREVIOUS },
	{ "SPRINT",	KEY_SPRINT },
	{ "SREDO",	KEY_SREDO },
	{ "SREPLACE",	KEY_SREPLACE },
	{ "SRIGHT",	KEY_SRIGHT },
	{ "SRSUME",	KEY_SRSUME },
	{ "SSAVE",	KEY_SSAVE },
	{ "SSUSPEND",	KEY_SSUSPEND },
	{ "SUNDO",	KEY_SUNDO },
	{ "SUSPEND",	KEY_SUSPEND },
	{ "UNDO",	KEY_UNDO },
	{ CN, 0 }
};

/* Look up a curses symbolic key. */
static int
lookup_ccode(const char *s)
{
	int i;
	unsigned long f;
	char *ptr;

	for (i = 0; ncurses_key[i].name != CN; i++) {
		if (!strcmp(s, ncurses_key[i].name))
			return ncurses_key[i].code;
	}
	if (s[0] == 'F' &&
	    (f = strtoul(s + 1, &ptr, 10)) < 64 &&
	    ptr != s + 1 &&
	    *ptr == '\0') {
		return KEY_F(f);
	}
	return -1;
}

/* Look up a curses key code. */
const char *
lookup_cname(int ccode)
{
	int i;

	for (i = 0; ncurses_key[i].name != CN; i++) {
		if (ccode == ncurses_key[i].code)
			return ncurses_key[i].name;
	}
	if (ccode >= KEY_F0 && ccode < KEY_F0 + 64) {
		static char buf[10];

		(void) sprintf(buf, "F%d", ccode - KEY_F0);
		return buf;
	}
	return CN;
}

/* Read each of the keymaps specified by the keymap resource. */
void
keymap_init(void)
{
	char *s0, *s;
	char *comma;
	static Boolean initted = False;

	read_keymap("base");
	if (appres.key_map == CN)
		return;
	s = s0 = NewString(appres.key_map);
	while ((comma = strchr(s, ',')) != CN) {
		*comma = '\0';
		if (*s)
			read_keymap(s);
		s = comma + 1;
	}
	if (*s)
		read_keymap(s);
	Free(s0);

	if (!initted) {
		last_nvt = IN_ANSI;
		register_schange(ST_3270_MODE, keymap_3270_mode);
		register_schange(ST_CONNECT, keymap_3270_mode);
		initted = True;
	}
}

/* Erase the current keymap. */
static void
clear_keymap(void)
{
	struct keymap *k, *next;

	for (k = master_keymap; k != NULL; k = next) {
		next = k->next;
		Free(k->codes);
		Free(k->hints);
		Free(k->file);
		Free(k->action);
		Free(k);
	}
	master_keymap = lastk = NULL;
}

/* 3270/NVT mode change. */
static void
keymap_3270_mode(Boolean ignored unused)
{
	if (last_nvt != IN_ANSI) {
		last_nvt = IN_ANSI;
		clear_keymap();
		keymap_init();
	}
}

/*
 * Decode a key.
 * Accepts a hint as to which form was used to specify it, if it came from a
 * keymap definition.
 *
 * Todo: Implement the hints.
 */
const char *
decode_key(int k, int hint)
{
	const char *n, *n2;
	static char buf[128];
	char *s = buf;

	/* Try ncurses first. */
	if (k > 0xff) {
		if ((n = lookup_cname(k)) != CN) {
			(void) sprintf(buf, "<Key>%s", n);
			return buf;
		} else
			return "?";
	}
	n = KeysymToString((KeySym)k);
	n2 = KeysymToString((KeySym)(k & 0x7f));
	if (n != CN) {
		if (hint) {
			if ((hint & KM_META) && n2 != CN && n2 != n)
				(void) sprintf(s, "Meta<Key>%s", n2);
			else
				(void) sprintf(s, "<Key>%s", n);
		} else {
			s += sprintf(s, "<Key>%s", n);
			if (n2 != CN && n2 != n)
				(void) sprintf(s, " (or Meta<Key>%s)", n2);
		}
		return buf;
	}
	if ((k & 0x7f) < ' ') {
		n = KeysymToString(k + '@');
		n2 = KeysymToString((k & 0x7f) + '@');

		if (hint) {
			if ((hint & KM_META) && n2 != CN && n2 != n)
				(void) sprintf(s, "Ctrl Meta<Key>%s", n2);
			else
				(void) sprintf(s, "Ctrl<Key>%s", n);
		} else {
			s += sprintf(s, "Ctrl<Key>%s", n);
			if (n2 != CN && n2 != n)
				(void) sprintf(s, " (or Ctrl Meta<Key>%s)", n2);
		}
		return buf;
	} else
		return "?";
}

/* Dump the current keymap. */
void
keymap_dump(void)
{
	struct keymap *k;

	for (k = master_keymap; k != NULL; k = k->next) {
		if (k->successor != NULL)
			action_output("[%s:%d]  (replaced by %s:%d)", k->file,
			    k->line, k->successor->file, k->successor->line);
		else {
			int i;
			char buf[1024];
			char *s = buf;

			s += sprintf(s, "[%s:%d] ", k->file, k->line);
			for (i = 0; i < k->ncodes; i++) {
				s += sprintf(s, " %s",
				    decode_key(k->codes[i],
					k->hints[i] | KM_KEYMAP));
			}
			action_output("%s: %s", buf, k->action);
		}
	}
}
