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
	(void) memcpy((char *)ebc2cg_ge, (char *)ebc2cg0, 256);
	(void) memcpy((char *)cg2ebc, (char *)cg2ebc0, 256);
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
 * If the EBCDIC code is in the range 0..255:
 *
 *   If the keysym is in the range 1..255, it is a remapping of the EBCDIC code
 *   for a standard Latin-1 graphic, and the CG-to-EBCDIC map will be modified
 *   to match.
 *
 *   Otherwise (keysym > 255), it is a definition for the EBCDIC code to use for
 *   a multibyte keysym.  This is intended for 8-bit fonts that with special
 *   characters that replace certain standard Latin-1 graphics.  The keysym
 *   will be entered into the extended keysym translation table.
 *
 * If the EBCDIC code is in the range 256..511, it is a one-way (EBCDIC-to-CG)
 * remapping of a the GE space for the lower 8 bits of EBCDIC code.
 */
static void
remap_chars(char *spec)
{
	char	*s;
	char	*ebcs, *isos;
	unsigned	ebc;
	unsigned char	cg;
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
			cg = asc2cg[iso];
			if (ebc & ~0xff) {
				/* GE range */
				ebc2cg_ge[ebc & 0xff] = cg;
			} else {
				if (cg2asc[cg] == iso) { /* well-defined */
					ebc2cg[ebc] = cg;
					cg2ebc[cg] = ebc;
				} else {		 /* into a hole */
					ebc2cg[ebc] = CG_boxsolid;
				}
			}
		} else {
			add_xk(iso, (KeySym)cg2asc[ebc2cg[ebc]]);
		}

	}
	Free(spec);
}
