/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000 by Paul Mattes.
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
#include "tablesc.h"
#include "utilc.h"

/* Globals. */
Boolean charset_changed = False;

/* Statics. */
static void remap_chars(char *spec);

/*
 * Change character sets.
 * Returns True if the new character set was found, False otherwise.
 */
Boolean
charset_init(char *csname)
{
	char *resname;
	char *cs;

	if (csname == CN)
		return True;

	/* Find the character set definition. */
	resname = xs_buffer("%s.%s", ResCharset, csname);
	cs = get_resource(resname);
	Free(resname);
	if (cs == CN)
		return False;
	if (appres.charset == CN || strcmp(csname, appres.charset)) {
		appres.charset = NewString(csname);
		charset_changed = True;
	}

	/* Set up the translation tables. */
	(void) memcpy((char *)ebc2cg, (char *)ebc2cg0, 256);
	(void) memcpy((char *)cg2ebc, (char *)cg2ebc0, 256);
#if defined(X3270_FT) /*[*/
	(void) memcpy((char *)ft2asc, (char *)ft2asc0, 256);
	(void) memcpy((char *)asc2ft, (char *)asc2ft0, 256);
#endif /*]*/
	clear_xks();
	remap_chars(NewString(cs));
	return True;
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
		else
			return NoSymbol;
	}
	if (k < ' ' || (!extended && k > 0xff))
		return NoSymbol;
	else
		return k;
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
static void
remap_chars(char *spec)
{
	char	*s;
	char	*ebcs, *isos;
	unsigned char	ebc, cg;
	KeySym	iso;
	int	ne = 0;
	int	ns;

	/* Pick apart a copy of the spec. */
	s = spec = NewString(spec);

	while ((ns = split_dresource(&s, &ebcs, &isos))) {
		ne++;
		if (ns < 0 ||
		    !(ebc = strtol(ebcs, (char **)NULL, 0)) ||
		    (iso = parse_keysym(isos, True)) == NoSymbol) {
			char	nbuf[16];

			(void) sprintf(nbuf, "%d", ne);
			xs_warning("Cannot parse %s \"%s\", entry %s",
			    ResCharset, appres.charset, nbuf);
			break;
		}
		if (iso <= 0xff) {
#if defined(X3270_FT) /*[*/
			unsigned char aa;
#endif /*]*/

			cg = asc2cg[iso];
			if (cg2asc[cg] == iso) {	/* well-defined */
				ebc2cg[ebc] = cg;
				cg2ebc[cg] = ebc;
			} else {			/* into a hole */
				ebc2cg[ebc] = CG_boxsolid;
			}
#if defined(X3270_FT) /*[*/
			/*
			 * Change the file transfer translation table as well.
			 * This works properly with the US-English character
			 * set and the bracket character set (which are the
			 * same, as far as the host is concerned), but has not
			 * yet been tried out with hosts that genuinely think
			 * they are using a non-English character set.
			 */
			aa = asc2ft[ebc2asc[ebc]];
			ft2asc[aa] = iso;
			asc2ft[iso] = aa;
#endif /*]*/
		} else {
			add_xk(iso, (KeySym)cg2asc[ebc2cg[ebc]]);
		}

	}
	Free(spec);
}
