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

#include "keymapc.h"
#include "popupsc.h"
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
#if defined(USE_CURSES) /*[*/
#include <curses.h>
#else /*][*/
#include <ncurses.h>
#endif /*]*/

#define KM_CTRL	0x0001
#define KM_META	0x0002

struct keymap {
	struct keymap *next;
	struct keymap *successor;
	int code;		/* key code (ASCII or ncurses symbol) */
	char *file;		/* file or resource name */
	int line;		/* keymap line number */
	char *action;		/* actions to perform */
};

static struct keymap *master_keymap = NULL;
static struct keymap *lastk = NULL;

static int lookup_ccode(const char *s);

/*
 * Read a keymap from a file.
 * Returns 0 for success, -1 for an error.
 *
 * Keymap files look suspiciously like x3270 keymaps, but of course, they
 * are different.
 */
int
read_keymap(const char *filename)
{
	char *r;
	char *fn = CN;
	int rc = 0;
	FILE *f = NULL;
	char buf[1024];
	int line = 0;
	struct keymap *j, *k;
	char *left, *right;

	/* See if it's a resource. */
	fn = xs_buffer(ResKeymap ".%s", filename);
	r = get_resource(fn);
	if (r == CN) {
		/* Try for a file. */
		fn = do_subst(filename, True, True);
		f = fopen(fn, "r");
		if (f == NULL) {
			xs_warning("No such keymap resource or file: %s", fn);
			rc = -1;
			goto done;
		}
	}
	while ((r != CN)? (rc = split_dresource(&r, &left, &right)):
		          fgets(buf, sizeof(buf), f) != CN) {
		char *s, *ks;
		int flags = 0;
		KeySym Ks;
		int ccode;

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

		/*
		 * The left-hand side is:
		 *  [Ctrl|Meta]<Key> k
		 * The right-hand side is an action.
		 */
		s = strstr(left, "<Key>");
		if (s == CN) {
			popup_an_error("%s, line %d: Missing <Key>",
			    fn, line);
			rc = -1;
			goto done;
		}
		ks = s + 5;
		*s = '\0';
		s = left;
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
			} else {
				popup_an_error("%s, line %d: Unknown modifier",
				    fn, line);
				rc = -1;
				goto done;
			}
		}
		s = ks;
		while (isspace(*s))
			s++;
		if (!*s) {
			popup_an_error("%s, line %d: Missing keysym",
			    fn, line);
			rc = -1;
			goto done;
		}
		Ks = StringToKeysym(s);
		if (Ks == NoSymbol) {
			ccode = lookup_ccode(s);
			if (ccode == -1) {
				popup_an_error("%s, line %d: Unknown keysym "
				    "'%s'", fn, line, s);
				rc = -1;
				goto done;
			}
			if (flags) {
				popup_an_error("%s, line %d: Can't use "
				    "modifier with '%s'", fn, line, s);
				rc = -1;
				goto done;
			}
		} else
			ccode = (int)Ks;

		if (flags & KM_CTRL)
			ccode &= 0x1f;
		if (flags & KM_META)
			ccode |= 0x80;

		/* Add a new one at the end of the list. */
		k = Malloc(sizeof(struct keymap));
		k->next = NULL;
		k->successor = NULL;
		k->code = ccode;
		k->file = NewString(filename);
		k->line = line;
		k->action = NewString(right);
		if (lastk != NULL)
			lastk->next = k;
		else
			master_keymap = k;
		lastk = k;

		/* Later definitions supercede older ones. */
		for (j = master_keymap; j != NULL; j = j->next) {
			if (j != k && j->code == ccode)
				j->successor = k;
		}
	}

    done:
	if (f != NULL)
		fclose(f);
	if (fn != CN)
		Free(fn);
	return rc;
}

/* Look up an key in the keymap, return the matching action if there is one. */
char *
lookup_key(int code)
{
	struct keymap *k;

	for (k = master_keymap; k != NULL; k = k->next) {
		if (k->successor != NULL)
			continue;
		if (code == k->code) {
			trace_event(" %s:%d -> %s\n", k->file, k->line,
			    k->action);
			return k->action;
		}
	}
	return CN;
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
}

/* Decode a key. */
const char *
decode_key(int k)
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
		s += sprintf(s, "<Key>%s", n);
		if (n2 != CN && n2 != n)
			(void) sprintf(s, " (or Meta<Key>%s)", n2);
		return buf;
	}
	if ((k & 0x7f) < ' ') {
		n = KeysymToString(k + '@');
		n2 = KeysymToString((k & 0x7f) + '@');

		s += sprintf(s, "Ctrl<Key>%s", n);
		if (n2 != CN && n2 != n)
			(void) sprintf(s, " (or Ctrl Meta<Key>%s)", n2);
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
			action_output("[%s:%d]  (superceded by %s:%d)", k->file,
			    k->line, k->successor->file, k->successor->line);
		else
			action_output("[%s:%d]  %s: %s", k->file, k->line,
			    decode_key(k->code), k->action);
	}
}
