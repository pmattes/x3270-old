/*
 * Copyright 2000, 2001 by Paul Mattes.
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
#include <errno.h>
#include "appres.h"
#include "resources.h"

#include "hostc.h"
#include "keymapc.h"
#include "macrosc.h"
#include "popupsc.h"
#include "screenc.h"
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

#define KM_3270_ONLY	0x0010	/* used in 3270 mode only */
#define KM_NVT_ONLY	0x0020	/* used in NVT mode only */
#define KM_INACTIVE	0x0040	/* wrong NVT/3270 mode, or overridden */

#define KM_KEYMAP	0x8000
#define KM_HINTS	(KM_CTRL|KM_META)

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

#define IS_INACTIVE(k)	((k)->hints[0] & KM_INACTIVE)

static struct keymap *master_keymap = NULL;
static struct keymap **nextk = &master_keymap;

static Boolean last_3270 = False;
static Boolean last_nvt = False;

static int lookup_ccode(const char *s);
static void keymap_3270_mode(Boolean);

#define codecmp(k1, k2, len)	\
	memcmp((k1)->codes, (k2)->codes, (len)*sizeof(int))

static void read_one_keymap(const char *name, const char *fn, const char *r0,
    int flags);
static void clear_keymap(void);
static void set_inactive(void);

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
	char *rs;			/* resource value */
	char *fnx;			/* expanded file name */
	int a;				/* access(fnx) */

	/* Return nothing, to begin with. */
	*fullname = CN;
	*r = CN;

	/* See if it's a resource. */
	rs = get_fresource(ResKeymap ".%s", name);

	/* If there's a plain version, return it. */
	if (rs != CN) {
		*fullname = NewString(name);
		*r = NewString(rs);
		return 1;
	}

	/* See if it's a file. */
	fnx = do_subst(name, True, True);
	a = access(fnx, R_OK);

	/* If there's a plain version, return it. */
	if (a == 0) {
		*fullname = fnx;
		return 1;
	}

	/* No dice. */
	Free(fnx);
	return -1;
}

/* Add a keymap entry. */
static void
add_keymap_entry(int ncodes, int *codes, int *hints, const char *file,
    int line, const char *action)
{
	struct keymap *k;
	struct keymap *j;

	/* Allocate a new node. */
	k = Malloc(sizeof(struct keymap));
	k->next = NULL;
	k->successor = NULL;
	k->ncodes = ncodes;
	k->codes = Malloc(ncodes * sizeof(int));
	(void) memcpy(k->codes, codes, ncodes * sizeof(int));
	k->hints = Malloc(ncodes * sizeof(int));
	(void) memcpy(k->hints, hints, ncodes * sizeof(int));
	k->file = NewString(file);
	k->line = line;
	k->action = NewString(action);

	/* See if it's inactive, or supercedes other entries. */
	if ((!last_3270 && (k->hints[0] & KM_3270_ONLY)) ||
	    (!last_nvt  && (k->hints[0] & KM_NVT_ONLY))) {
		k->hints[0] |= KM_INACTIVE;
	} else for (j = master_keymap; j != NULL; j = j->next) {
		/* It may supercede other entries. */
		if (j->ncodes == k->ncodes &&
		    !codecmp(j, k, k->ncodes)) {
			j->hints[0] |= KM_INACTIVE;
			j->successor = k;
		}
	}

	/* Link it in. */
	*nextk = k;
	nextk = &k->next;
}

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static void
read_keymap(const char *name)
{
	char *name_3270 = xs_buffer("%s.3270", name);
	char *name_nvt = xs_buffer("%s.nvt", name);
	int rc, rc_3270, rc_nvt;
	char *fn, *fn_3270, *fn_nvt;
	char *r0, *r0_3270, *r0_nvt;

	rc = locate_keymap(name, &fn, &r0);
	rc_3270 = locate_keymap(name_3270, &fn_3270, &r0_3270);
	rc_nvt = locate_keymap(name_nvt, &fn_nvt, &r0_nvt);
	if (rc < 0 && rc_3270 < 0 && rc_nvt < 0) {
		xs_warning("No such keymap resource or file: %s",
		    name);
		Free(name_3270);
		Free(name_nvt);
		return;
	}

	if (rc >= 0) {
		read_one_keymap(name, fn, r0, 0);
		Free(fn);
		Free(r0);
	}
	if (rc_3270 >= 0) {
		read_one_keymap(name_3270, fn_3270, r0_3270, KM_3270_ONLY);
		Free(fn_3270);
		Free(r0_3270);
	}
	if (rc_nvt >= 0) {
		read_one_keymap(name_nvt, fn_nvt, r0_nvt, KM_NVT_ONLY);
		Free(fn_nvt);
		Free(r0_nvt);
	}
	Free(name_3270);
	Free(name_nvt);
}

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but aren't.
 */
static void
read_one_keymap(const char *name, const char *fn, const char *r0, int flags)
{
	char *r = CN;			/* resource value */
	char *r_copy = CN;		/* initial value of r */
	FILE *f = NULL;			/* resource file */
	char buf[1024];			/* file read buffer */
	int line = 0;			/* line number */
	char *left, *right;		/* chunks of line */
	static int ncodes = 0;
	static int maxcodes = 0;
	static int *codes = NULL, *hints = NULL;
	int rc = 0;

	/* Find the resource or file. */
	if (r0 != CN)
		r = r_copy = NewString(r0);
	else {
		f = fopen(fn, "r");
		if (f == NULL) {
			xs_warning("Cannot open file: %s", fn);
			return;
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
			goto done;
		}

		pkr = parse_keydef(&left, &ccode, &hint);
		if (pkr == 0) {
			popup_an_error("%s, line %d: Missing <Key>",
			    fn, line);
			goto done;
		}
		if (pkr < 0) {
			popup_an_error("%s, line %d: %s",
			    fn, line, pk_errmsg[-1 - pkr]);
			goto done;
		}

		/* Accumulate keycodes. */
		ncodes = 0;
		do {
			if (++ncodes > maxcodes) {
				maxcodes = ncodes;
				codes = Realloc(codes, maxcodes * sizeof(int));
				hints = Realloc(hints, maxcodes * sizeof(int));
			}
			codes[ncodes - 1] = ccode;
			hints[ncodes - 1] = hint;
			pkr = parse_keydef(&left, &ccode, &hint);
			if (pkr < 0) {
				popup_an_error("%s, line %d: %s",
				    fn, line, pk_errmsg[-1 - pkr]);
				goto done;
			}
		} while (pkr != 0);

		/* Add it to the list. */
		hints[0] |= flags;
		add_keymap_entry(ncodes, codes, hints, fn, line, right);
	}

    done:
	Free(r_copy);
	if (f != NULL)
		fclose(f);
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
		if (IS_INACTIVE(j))
			continue;
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
 * partial matches.
 *
 * It also handles keyboards that generate ESC for the Meta or Alt key.
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
			if (IS_INACTIVE(k))
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
		if (IS_INACTIVE(k))
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
static const char *
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

	/* In case this is a subsequent call, wipe out the current keymap. */
	clear_keymap();

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

	last_3270 = IN_3270;
	last_nvt = IN_ANSI;
	set_inactive();

	if (!initted) {
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
	master_keymap = NULL;
	nextk = &master_keymap;
}

/* Set the inactive flags for the current keymap. */
static void
set_inactive(void)
{
	struct keymap *k;

	/* Clear the inactive flags and successors. */
	for (k = master_keymap; k != NULL; k = k->next) {
		k->hints[0] &= ~KM_INACTIVE;
		k->successor = NULL;
	}

	/* Turn off elements which have the wrong mode. */
	for (k = master_keymap; k != NULL; k = k->next) {
		/* If the mode is wrong, turn it off. */
		if ((!last_3270 && (k->hints[0] & KM_3270_ONLY)) ||
		    (!last_nvt  && (k->hints[0] & KM_NVT_ONLY))) {
			k->hints[0] |= KM_INACTIVE;
		}
	}

	/* Turn off elements with successors. */
	for (k = master_keymap; k != NULL; k = k->next) {
		struct keymap *j;
		struct keymap *last_j = NULL;

		if (IS_INACTIVE(k))
			continue;

		/* If it now has a successor, turn it off. */
		for (j = k->next; j != NULL; j = j->next) {
			if (!IS_INACTIVE(j) &&
			    k->ncodes == j->ncodes &&
			    !codecmp(k, j, k->ncodes)) {
				last_j = j;
			}
		}
		if (last_j != NULL) {
			k->successor = last_j;
			k->hints[0] |= KM_INACTIVE;
		}
	}
}

/* 3270/NVT mode change. */
static void
keymap_3270_mode(Boolean ignored unused)
{
	if (last_3270 != IN_3270 || last_nvt != IN_ANSI) {
		last_3270 = IN_3270;
		last_nvt = IN_ANSI;
		set_inactive();
	}
}

/*
 * Decode a key.
 * Accepts a hint as to which form was used to specify it, if it came from a
 * keymap definition.
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
		else if (!IS_INACTIVE(k)) {
			int i;
			char buf[1024];
			char *s = buf;

			for (i = 0; i < k->ncodes; i++) {
				s += sprintf(s, " %s",
				    decode_key(k->codes[i],
					(k->hints[i] & KM_HINTS) | KM_KEYMAP));
			}
			action_output("[%s:%d]%s: %s", k->file, k->line,
			    buf, k->action);
		}
	}
}