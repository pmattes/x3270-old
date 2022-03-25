/*
 * Modifications Copyright 1993-2008 by Paul Mattes.
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
 *
 * x3270, c3270, s3270 and tcl3270 are distributed in the hope that they will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file LICENSE
 * for more details.
 */

/*
 *	charset.c
 *		This module handles character sets.
 */

#include "globals.h"

#include "3270ds.h"
#include "resources.h"
#include "appres.h"
#include "cg.h"

#include "charsetc.h"
#include "kybdc.h"
#include "popupsc.h"
#if defined(X3270_DISPLAY) || (defined(C3270) && !defined(_WIN32)) /*[*/
#include "screenc.h"
#endif /*]*/
#include "tablesc.h"
#include "unicodec.h"
#include "unicode_dbcsc.h"
#include "utf8c.h"
#include "utilc.h"

#include <errno.h>
#include <locale.h>
#if !defined(_WIN32) /*[*/
#include <langinfo.h>
#endif /*]*/

#if defined(_WIN32) /*[*/
#include <windows.h>
#endif /*]*/

#if defined(__CYGWIN__) /*[*/
#include <w32api/windows.h>
#undef _WIN32
#endif /*]*/

/* Globals. */
Boolean charset_changed = False;
#define DEFAULT_CGEN	0x02b90000
#define DEFAULT_CSET	0x00000025
unsigned long cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
unsigned long cgcsgid_dbcs = 0L;
char *default_display_charset = "3270cg-1a,3270cg-1,iso8859-1";

/* Statics. */
static enum cs_result charset_init2(char *csname, const char *codepage,
	const char *display_charsets);
static void set_cgcsgids(const char *spec);
static int set_cgcsgid(char *spec, unsigned long *idp);
static void set_charset_name(char *csname);

static char *charset_name = CN;

/*
 * Change character sets.
 */
enum cs_result
charset_init(char *csname)
{
    	enum cs_result rc;
#if !defined(_WIN32) /*[*/
	char *codeset_name;
#endif /*]*/
	const char *codepage;
	const char *display_charsets;
#if defined(X3270_DBCS) /*[*/
	const char *dbcs_codepage = NULL;
	const char *dbcs_display_charsets = NULL;
	Boolean need_free = False;
#endif /*]*/

#if !defined(_WIN32) /*[*/
	/* Get all of the locale stuff right. */
	setlocale(LC_ALL, "");

	/* Figure out the locale code set (character set encoding). */
	codeset_name = nl_langinfo(CODESET);
#if defined(__CYGWIN__) /*[*/
	/*
	 * Cygwin's locale support is quite limited.  If the locale
	 * indicates "US-ASCII", which appears to be the only supported
	 * encoding, ignore it and use the Windows ANSI code page, which
	 * observation indicates is what is actually supported.
	 *
	 * Hopefully at some point Cygwin will start returning something
	 * meaningful here and this logic will stop triggering.
	 */
	if (!strcmp(codeset_name, "US-ASCII"))
	    	codeset_name = xs_buffer("CP%d", GetACP());
#endif /*]*/
	set_codeset(codeset_name);
#endif /*]*/

	/* Do nothing, successfully. */
	if (csname == CN || !strcasecmp(csname, "us")) {
		set_cgcsgids(CN);
		set_charset_name(CN);
#if defined(X3270_DISPLAY) /*[*/
		(void) screen_new_display_charsets(default_display_charset,
		    "us");
#endif /*]*/
		(void) set_uni("us", &codepage, &display_charsets);
#if defined(X3270_DBCS) /*[*/
		(void) set_uni_dbcs("", NULL, NULL);
#endif /*]*/
		return CS_OKAY;
	}

	if (set_uni(csname, &codepage, &display_charsets) < 0)
		return CS_NOTFOUND;
#if defined(X3270_DBCS) /*[*/
	if (set_uni_dbcs(csname, &dbcs_codepage,
			     &dbcs_display_charsets) == 0) {
	    codepage = xs_buffer("%s+%s", codepage, dbcs_codepage);
	    display_charsets = xs_buffer("%s+%s", display_charsets,
		    dbcs_display_charsets);
	    need_free = True;
	}
#endif /*]*/

	rc = charset_init2(csname, codepage, display_charsets);
#if defined(X3270_DBCS) /*[*/
	if (need_free) {
	    Free((char *)codepage);
	    Free((char *)display_charsets);
	}
#endif /*]*/
	if (rc != CS_OKAY) {
		return rc;
	}

	return CS_OKAY;
}

/* Set a CGCSGID.  Return 0 for success, -1 for failure. */
static int
set_cgcsgid(char *spec, unsigned long *r)
{
	unsigned long cp;
	char *ptr;

	if (spec != CN &&
	    (cp = strtoul(spec, &ptr, 0)) &&
	    ptr != spec &&
	    *ptr == '\0') {
		if (!(cp & ~0xffffL))
			*r = DEFAULT_CGEN | cp;
		else
			*r = cp;
		return 0;
	} else
		return -1;
}

/* Set the CGCSGIDs. */
static void
set_cgcsgids(const char *spec)
{
	int n_ids = 0;
	char *spec_copy;
	char *buf;
	char *token;

	if (spec != CN) {
		buf = spec_copy = NewString(spec);
		while (n_ids >= 0 && (token = strtok(buf, "+")) != CN) {
			unsigned long *idp = NULL;

			buf = CN;
			switch (n_ids) {
			case 0:
			    idp = &cgcsgid;
			    break;
#if defined(X3270_DBCS) /*[*/
			case 1:
			    idp = &cgcsgid_dbcs;
			    break;
#endif /*]*/
			default:
			    popup_an_error("Extra CGCSGID(s), ignoring");
			    break;
			}
			if (idp == NULL)
				break;
			if (set_cgcsgid(token, idp) < 0) {
				popup_an_error("Invalid CGCSGID '%s', ignoring",
				    token);
				n_ids = -1;
				break;
			}
			n_ids++;
		}
		Free(spec_copy);
		if (n_ids > 0)
			return;
	}

	cgcsgid = DEFAULT_CGEN | DEFAULT_CSET;
#if defined(X3270_DBCS) /*[*/
	cgcsgid_dbcs = 0L;
#endif /*]*/
}

/* Set the global charset name. */
static void
set_charset_name(char *csname)
{
	if (csname == CN) {
		Replace(charset_name, NewString("us"));
		charset_changed = False;
		return;
	}
	if ((charset_name != CN && strcmp(charset_name, csname)) ||
	    (appres.charset != CN && strcmp(appres.charset, csname))) {
		Replace(charset_name, NewString(csname));
		charset_changed = True;
	}
}

/* Character set init, part 2. */
static enum cs_result
charset_init2(char *csname, const char *codepage, const char *display_charsets)
{
	const char *rcs = display_charsets;
	int n_rcs = 0;
	char *rcs_copy, *buf, *token;

	/* Isolate the pieces. */
	buf = rcs_copy = NewString(rcs);
	while ((token = strtok(buf, "+")) != CN) {
		buf = CN;
		switch (n_rcs) {
		case 0:
#if defined(X3270_DBCS) /*[*/
		case 1:
#endif /*]*/
		    break;
		default:
		    popup_an_error("Extra charset value(s), ignoring");
		    break;
		}
		n_rcs++;
	}

#if defined(X3270_DBCS) /*[*/
	/* Can't swap DBCS modes while connected. */
	if (IN_3270 && (n_rcs == 2) != dbcs) {
		popup_an_error("Can't change DBCS modes while connected");
		return CS_ILLEGAL;
	}
#endif /*]*/

#if defined(X3270_DISPLAY) /*[*/
	if (!screen_new_display_charsets(
		    rcs? rcs: default_display_charset,
		    csname)) {
		return CS_PREREQ;
	}
#else /*][*/
#if defined(X3270_DBCS) /*[*/
	if (n_rcs > 1)
		dbcs = True;
	else
		dbcs = False;
#endif /*]*/
#endif /*]*/

	/* Set up the cgcsgid. */
	set_cgcsgids(codepage);

	/* Set up the character set name. */
	set_charset_name(csname);

	return CS_OKAY;
}

/* Return the current character set name. */
char *
get_charset_name(void)
{
	return (charset_name != CN)? charset_name:
	    ((appres.charset != CN)? appres.charset: "us");
}
