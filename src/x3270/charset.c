/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000, 2001 by Paul Mattes.
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
 *	charset.c
 *		This module handles character sets.
 */

#include "globals.h"

#include "resources.h"
#include "appres.h"
#include "cg.h"

#include "charsetc.h"
#include "kybdc.h"
#include "popupsc.h"
#include "tablesc.h"
#include "utilc.h"

#include <errno.h>

/* Globals. */
Boolean charset_changed = False;
#define DEFAULT_CGEN	0x02b90000
#define DEFAULT_CSET	0x00000025
unsigned long cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;

/* Statics. */
static enum cs_result resource_charset(char *csname, char *cs, char *ftcs);
typedef enum { CS_ONLY, FT_ONLY, BOTH } remap_scope;
static enum cs_result remap_chars(char *csname, char *spec, remap_scope scope,
    int *ne);
static void remap_one(unsigned char ebc, KeySym iso, remap_scope scope);
#if defined(DEBUG_CHARSET) /*[*/
static enum cs_result check_charset(void);
static char *char_if_ascii7(unsigned long l);
#endif /*]*/
static void set_cgcsgid(char *spec);
static void set_display_charset(char *r);
static void set_charset_name(char *csname);

static char *display_charset = CN;	/* display character set */
static char *charset_name = CN;

static void
charset_defaults(void)
{
	/* Go to defaults first. */
	(void) memcpy((char *)ebc2cg, (char *)ebc2cg0, 256);
	(void) memcpy((char *)cg2ebc, (char *)cg2ebc0, 256);
	(void) memcpy((char *)ebc2asc, (char *)ebc2asc0, 256);
#if defined(X3270_FT) /*[*/
	(void) memcpy((char *)ft2asc, (char *)ft2asc0, 256);
	(void) memcpy((char *)asc2ft, (char *)asc2ft0, 256);
#endif /*]*/
	clear_xks();
	set_cgcsgid(CN);
	set_display_charset(CN);
	set_charset_name(CN);
}

/*
 * Change character sets.
 * Returns True if the new character set was found, False otherwise.
 */
enum cs_result
charset_init(char *csname)
{
	char *resname;
	char *cs, *ftcs;
	enum cs_result rc;
	char *ccs, *cftcs;

	/* Do nothing, successfully. */
	if (csname == CN)
		return CS_OKAY;

	/* Revert to defaults. */
	charset_defaults();

	/* Figure out if it's already in a resource or in a file. */
	resname = xs_buffer("%s.%s", ResCharset, csname);
	cs = get_resource(resname);
	Free(resname);
	if (cs == CN) {
		resname = xs_buffer("%s/charset/%s", appres.conf_dir, csname);
		if (access(resname, R_OK) < 0)
			return CS_NOTFOUND;
		if (read_resource_file(resname, False) < 0) {
			Free(resname);
			return CS_NOTFOUND;
		}
		Free(resname);
		resname = xs_buffer("%s.%s", ResCharset, csname);
		cs = get_resource(resname);
		Free(resname);
		if (cs == CN)
			return CS_NOTFOUND;
	}

	/* Grab the File Transfer character set. */
	resname = xs_buffer("%s.%s", ResFtCharset, csname);
	ftcs = get_resource(resname);
	Free(resname);

	/* Copy strings. */
	ccs = NewString(cs);
	cftcs = (ftcs == NULL)? NULL: NewString(ftcs);

	/* Interpret them. */
	rc = resource_charset(csname, ccs, cftcs);

	/* Free them. */
	Free(ccs);
	if (cftcs != NULL)
		Free(cftcs);

#if defined(DEBUG_CHARSET) /*[*/
	if (rc == CS_OKAY)
		rc = check_charset();
#endif /*]*/

	if (rc != CS_OKAY)
		charset_defaults();

	return rc;
}

/* Set the global CGCSGID. */
static void
set_cgcsgid(char *spec)
{
	unsigned long cp;
	char *ptr;

	if (spec != CN &&
	    (cp = strtoul(spec, &ptr, 0)) &&
	    ptr != spec &&
	    *ptr == '\0') {
		if (!(cp & ~0xffffL))
			cgcsgid = DEFAULT_CGEN | cp;
		else
			cgcsgid = cp;
	} else
		cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
}

/* Set the global display charset. */
static void
set_display_charset(char *r)
{
	char *d;

	if (display_charset != CN)
		Free(display_charset);
	if (r != CN &&
	    (d = strchr(r, '-')) != NULL &&
	    d != r)
		display_charset = NewString(r);
	else
		display_charset = CN;
}

/* Set the global charset name. */
static void
set_charset_name(char *csname)
{
	if (csname == CN) {
		if (charset_name != CN)
			Free(charset_name);
		charset_name = NewString("us");
		charset_changed = False;
		return;
	}
	if ((charset_name != CN && strcmp(charset_name, csname)) ||
	    (appres.charset != CN && strcmp(appres.charset, csname))) {
		if (charset_name != CN)
			Free(charset_name);
		charset_name = NewString(csname);
		charset_changed = True;
	}
}

/* Define a charset from resources. */
static enum cs_result
resource_charset(char *csname, char *cs, char *ftcs)
{
	enum cs_result rc;
	char *name;
	int ne = 0;

	/* Interpret the spec. */
	rc = remap_chars(csname, cs, (ftcs == NULL)? BOTH: CS_ONLY, &ne);
	if (rc != CS_OKAY)
		return rc;
	if (ftcs != NULL) {
		rc = remap_chars(csname, ftcs, FT_ONLY, &ne);
		if (rc != CS_OKAY)
			return rc;
	}

	/* Set up the cgcsgid. */
	name = xs_buffer("%s.%s", ResCodepage, csname);
	set_cgcsgid(get_resource(name));
	Free(name);

	/* Set up the display character set. */
	name = xs_buffer("%s.%s", ResDisplayCharset, csname);
	set_display_charset(get_resource(name));
	Free(name);

	/* Set up the character set name. */
	set_charset_name(csname);

	return CS_OKAY;
}

/*
 * Map a keysym name or literal string into a character.
 * Returns NoSymbol if there is a problem.
 */
static KeySym
parse_keysym(char *s, Boolean extended)
{
	KeySym	k;

	k = StringToKeysym(s);
	if (k == NoSymbol) {
		if (strlen(s) == 1)
			k = *s & 0xff;
		else if (s[0] == '0' && s[1] == 'x') {
			unsigned long l;
			char *ptr;

			l = strtoul(s, &ptr, 16);
			if (*ptr != '\0' || (l & ~0xff))
				return NoSymbol;
			return (KeySym)l;
		} else
			return NoSymbol;
	}
	if (k < ' ' || (!extended && k > 0xff))
		return NoSymbol;
	else
		return k;
}

/* Process a single character definition. */
static void
remap_one(unsigned char ebc, KeySym iso, remap_scope scope)
{
	unsigned char cg;

	/* Ignore mappings of EBCDIC control codes and the space character. */
	if (ebc <= 0x40)
		return;

	/* If they want to map to a space, map it to a NULL instead. */
	if (iso == 0x20)
		iso = 0;

	if (iso <= 0xff) {
#if defined(X3270_FT) /*[*/
		unsigned char aa;
#endif /*]*/

		if (scope == BOTH || scope == CS_ONLY) {
			cg = asc2cg[iso];

			if (iso != 0 && cg != 0) {
				unsigned ebc2;

				for (ebc2 = 0; ebc2 < 256; ebc2++) {
					if (ebc2 != ebc && ebc2cg[ebc2] == cg) {
#if defined(DEBUG_CHARSET) /*[*/
						xs_warning("Removing X'%02X' "
						    "(conflict with X'%02X' "
						    "-> 0x%x%s)", ebc2, ebc,
						    iso, char_if_ascii7(iso));
#endif /*]*/
						ebc2cg[ebc2] = 0;
					}
				}
			}

			if (cg2asc[cg] == iso || iso == 0) {
				/* well-defined */
				ebc2cg[ebc] = cg;
				cg2ebc[cg] = ebc;
			} else {
				/* into a hole */
				ebc2cg[ebc] = CG_boxsolid;
			}
			if (ebc > 0x40)
				ebc2asc[ebc] = iso;
		}
#if defined(X3270_FT) /*[*/
		if (ebc > 0x40) {
			/* Change the file transfer translation table. */
			if (scope == BOTH) {
				/*
				 * We have an alternate mapping of an EBCDIC
				 * code to an ASCII code.  Modify the existing
				 * ASCII(ft)-to-ASCII(desired) maps.
				 *
				 * This is done by figuring out which ASCII
				 * code the host usually translates the given
				 * EBCDIC code to (asc2ft0[ebc2asc0[ebc]]).
				 * Now we want to translate that code to the
				 * given ISO code, and vice-versa.
				 */
				aa = asc2ft0[ebc2asc0[ebc]];
				if (aa != ' ') {
					ft2asc[aa] = iso;
					asc2ft[iso] = aa;
				}
			} else if (scope == FT_ONLY) {
				/*
				 * We have a map of how the host translates
				 * the given EBCDIC code to an ASCII code.
				 * Generate the translation between that code
				 * and the ISO code that we would normally
				 * use to display that EBCDIC code.
				 */
				ft2asc[iso] = ebc2asc[ebc];
				asc2ft[ebc2asc[ebc]] = iso;
			}
		}
#endif /*]*/
	} else {
		add_xk(iso, (KeySym)cg2asc[ebc2cg[ebc]]);
	}
}

/*
 * Parse an EBCDIC character set map, a series of pairs of numeric EBCDIC codes
 * and keysyms.
 *
 * If the keysym is in the range 1..255, it is a remapping of the EBCDIC code
 * for a standard Latin-1 graphic, and the CG-to-EBCDIC map will be modified
 * to match.
 *
 * Otherwise (keysym > 255), it is a definition for the EBCDIC code to use for
 * a multibyte keysym.  This is intended for 8-bit fonts that with special
 * characters that replace certain standard Latin-1 graphics.  The keysym
 * will be entered into the extended keysym translation table.
 */
static enum cs_result
remap_chars(char *csname, char *spec, remap_scope scope, int *ne)
{
	char *s;
	char *ebcs, *isos;
	unsigned char ebc;
	KeySym iso;
	int ns;
	enum cs_result rc = CS_OKAY;
	Boolean is_table = False;

	/* Pick apart a copy of the spec. */
	s = spec = NewString(spec);
	while (isspace(*s)) {
		s++;
	}
	if (!strncmp(s, "#table", 6)) {
		is_table = True;
		s += 6;
	}

	if (is_table) {
		int ebc = 0;
		char *tok;
		char *ptr;

		while (ebc < 256 &&
		       (tok = strtok(s, " \t\n")) != CN) {
			iso = strtoul(tok, &ptr, 0);
			if (ptr == tok || *ptr != '\0' || iso > 256L) {
				if (strlen(tok) == 1)
					iso = tok[0] & 0xff;
				else {
					popup_an_error("Invalid charset "
					    "entry '%s' (#%d)",
					    tok, ebc);
					rc = CS_BAD;
					break;
				}
			}
			remap_one(ebc, iso, scope);

			ebc++;
			s = CN;
		}
	} else {
		while ((ns = split_dresource(&s, &ebcs, &isos))) {
			char *ptr;

			(*ne)++;
			if (ns < 0 ||
			    ((ebc = strtoul(ebcs, &ptr, 0)),
			     ptr == ebcs || *ptr != '\0') ||
			    (iso = parse_keysym(isos, True)) == NoSymbol) {
				popup_an_error("Cannot parse %s \"%s\", entry %d",
				    ResCharset, csname, *ne);
				rc = CS_BAD;
				break;
			}
			remap_one(ebc, iso, scope);
		}
	}
	Free(spec);
	return rc;
}

#if defined(DEBUG_CHARSET) /*[*/
static char *
char_if_ascii7(unsigned long l)
{
	static char buf[6];

	if (((l & 0x7f) > ' ' && (l & 0x7f) < 0x7f) || l == 0xff) {
		(void) sprintf(buf, " ('%c')", (char)l);
		return buf;
	} else
		return "";
}
#endif /*]*/


#if defined(DEBUG_CHARSET) /*[*/
/*
 * Verify that a character set is not ambiguous.
 * (All this checks is that multiple EBCDIC codes map onto the same ISO code.
 *  Hmm.  God, I find the CG stuff confusing.)
 */
static enum cs_result
check_charset(void)
{
	unsigned long iso;
	unsigned char ebc;
	enum cs_result rc = CS_OKAY;

	for (iso = 1; iso <= 255; iso++) {
		unsigned char multi[256];
		int n_multi = 0;

		if (iso == ' ')
			continue;

		for (ebc = 0x41; ebc < 0xff; ebc++) {
			if (cg2asc[ebc2cg[ebc]] == iso) {
				multi[n_multi] = ebc;
				n_multi++;
			}
		}
		if (n_multi > 1) {
			xs_warning("Display character 0x%02x%s has multiple "
			    "EBCDIC definitions: X'%02X', X'%02X'%s",
			    iso, char_if_ascii7(iso),
			    multi[0], multi[1], (n_multi > 2)? ", ...": "");
			rc = CS_BAD;
		}
	}
	return rc;
}
#endif /*]*/

/* Return the display character set name. */
char *
get_display_charset(void)
{
	return (display_charset != CN)? display_charset: "iso8859-1";
}

/* Return the current character set name. */
char *
get_charset_name(void)
{
	return (charset_name != CN)? charset_name:
	    ((appres.charset != CN)? appres.charset: "us");
}
