/*
 * Copyright 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	wide.c
 *		A 3270 Terminal Emulator for X11
 *		Wide character translation functions.
 */

#include "globals.h"
#include <errno.h>

#include "appres.h"
#include "resources.h"

#include "popupsc.h"
#include "trace_dsc.h"
#include "utilc.h"

#include "widec.h"

#if defined(HAVE_LIBICUI18N) /*[*/
#include <unicode/ucnv.h>

#define ICU_DATA	"ICU_DATA"
#endif /*]*/

#if defined(HAVE_LIBICUI18N) /*[*/
Boolean wide_initted = False;
static UConverter *dbcs_converter = NULL, *euc_converter = NULL;
#endif /*]*/

/* Initialize, or reinitialize the EBCDIC DBCS converters. */
int
wide_init(const char *csname)
{
#if defined(HAVE_LIBICUI18N) /*[*/
	UErrorCode err = U_ZERO_ERROR;
	char *cur_path = CN;
	Boolean lib_ok = False;
	Boolean dot_ok = False;
	char *converter_names, *cn_copy, *buf, *token;
	int n_converters = 0;

	/* This may be a reinit. */
	if (wide_initted) {
		ucnv_close(dbcs_converter);
		dbcs_converter = NULL;
		ucnv_close(euc_converter);
		euc_converter = NULL;
		wide_initted = False;
	}

	/* Make sure that $ICU_DATA has LIBX3270DIR in it. */
	cur_path = getenv(ICU_DATA);
	if (cur_path != CN) {
		char *t = NewString(cur_path);
		char *token;
		char *buf = t;

		while (!(lib_ok && dot_ok) &&
		       (token = strtok(buf, ":")) != CN) {
			buf = CN;
			if (!strcmp(token, LIBX3270DIR)) {
				lib_ok = True;
			} else if (!strcmp(token, ".")) {
				dot_ok = True;
			}
		}
		Free(t);
	}
	if (!lib_ok || !dot_ok) {
		char *s, *new_path;

		s = new_path = Malloc(strlen(ICU_DATA) +
		    (cur_path? strlen(cur_path): 0) + 
		    strlen(LIBX3270DIR) + 5 /* ICU_DATA=*:*:.\n */);

		s += sprintf(s, "%s=", ICU_DATA);
		if (cur_path != CN)
			s += sprintf(s, "%s", cur_path);
		if (!lib_ok) {
			if (s[-1] != '=' && s[-1] != ':')
				*s++ = ':';
			s += sprintf(s, "%s", LIBX3270DIR);
		}
		if (!dot_ok) {
			if (s[-1] != '=' && s[-1] != ':')
				*s++ = ':';
			*s++ = '.';
		}
		*s = '\0';
		if (putenv(new_path) < 0) {
			popup_an_errno(errno, "putenv for " ICU_DATA " failed");
			return -1;
		}
	}

	/* Decode converter names. */
	converter_names = get_fresource("%s.%s", ResDbcsConverters, csname);
	if (converter_names == CN)
		return 0;
	n_converters = 0;
	buf = cn_copy = NewString(converter_names);
	while ((token = strtok(buf, ",")) != CN) {
		buf = CN;
		switch (n_converters) {
		case 0: /* EBCDIC */
		    dbcs_converter = ucnv_open(token, &err);
		    if (dbcs_converter == NULL) {
			    popup_an_error("ucnv_open(%s) failed", token);
			    Free(cn_copy);
			    return -1;
		    }
		    break;
		case 1: /* display */
		    euc_converter = ucnv_open(token, &err);
		    if (euc_converter == NULL) {
			    popup_an_error("ucnv_open(%s) failed", token);
			    (void) ucnv_close(dbcs_converter);
			    dbcs_converter = NULL;
			    Free(cn_copy);
			    return -1;
		    }
		    break;
		default: /* extra */
		    popup_an_error("Extra converter name '%s' ignored", token);
		    break;
		}
		n_converters++;
	}

	Free(cn_copy);

	if (n_converters < 2) {
		popup_an_error("Missing %s value", ResDbcsConverters);
		if (dbcs_converter != NULL) {
			(void) ucnv_close(dbcs_converter);
			dbcs_converter = NULL;
		}
		return -1;
	}

	wide_initted = True;
	return 0;
#else /*][*/
	return 0;
#endif /*]*/
}

#if defined(HAVE_LIBICUI18N) /*[*/
static void
xlate1(unsigned char from0, unsigned char from1, unsigned char to_buf[],
    UConverter *from_cnv, const char *from_name,
    UConverter *to_cnv, const char *to_name)
{
	UErrorCode err = U_ZERO_ERROR;
	UChar Ubuf[2];
	char from_buf[2];
	char tmp_to_buf[3];
	int32_t len;
#if defined(WIDE_DEBUG) /*[*/
	int i;
#endif /*]*/

	/* Do something reasonable in case of failure. */
	to_buf[0] = to_buf[1] = 0;

	/* Convert string from source to Unicode. */
	from_buf[0] = from0;
	from_buf[1] = from1;
	len = ucnv_toUChars(from_cnv, Ubuf, 2, from_buf, 2, &err);
	if (err != U_ZERO_ERROR) {
		trace_ds("[%s toUnicode of DBCS X'%02x%02x' failed, ICU "
		    "error %d]\n", from_name, from0, from1, (int)err);
		return;
	}
#if defined(WIDE_DEBUG) /*[*/
	printf("Got Unicode %x\n", Ubuf[0]);
#endif /*]*/

	if (to_cnv != NULL) {
		/* Convert string from Unicode to Destination. */
		len = ucnv_fromUChars(to_cnv, tmp_to_buf, 3, Ubuf, len, &err);
		if (err != U_ZERO_ERROR) {
			popup_an_error("[fromUnicode of U+%04x to %s failed, ICU "
			    "error %d]\n", to_name, Ubuf[0], (int)err);
			return;
		}
		to_buf[0] = tmp_to_buf[0];
		to_buf[1] = tmp_to_buf[1];
#if defined(WIDE_DEBUG) /*[*/
		printf("Got %u %s characters:", len, to_name);
		for (i = 0; i < len; i++) {
			printf(" %02x", to_buf[i]);
		}
		printf("\n");
#endif /*]*/
	} else {
		to_buf[0] = (Ubuf[0] >> 8) & 0xff;
		to_buf[1] = Ubuf[0] & 0xff;
	}
}
#endif /*]*/

/* Translate a wide character to a DBCS EBCDIC character. */
void
wchar_to_dbcs(unsigned char c1, unsigned char c2, unsigned char ebc[])
{
#if defined(HAVE_LIBICUI18N) /*[*/
	xlate1(c1, c2, ebc, euc_converter, "EUC", dbcs_converter, "DBCS");
#else /*][*/
        /* Temporary version. */
        ebc[0] = 0xe0;
        ebc[1] = 0xe1;
#endif /*]*/
}

/* Translate a DBCS EBCDIC character to a wide character. */
void
dbcs_to_wchar(unsigned char ebc1, unsigned char ebc2, unsigned char c[])
{
#if defined(HAVE_LIBICUI18N) /*[*/
	xlate1(ebc1, ebc2, c, dbcs_converter, "DBCS", euc_converter, "EUC");
#else /*][*/
	c[0] = 0xf0;
	c[1] = 0xf1;
#endif /*]*/
}

/* Translate a DBCS EBCDIC character to a 2-byte Unicode character. */
void
dbcs_to_unicode16(unsigned char ebc1, unsigned char ebc2, unsigned char c[])
{
#if defined(HAVE_LIBICUI18N) /*[*/
	xlate1(ebc1, ebc2, c, dbcs_converter, "DBCS", NULL, NULL);
#else /*][*/
	c[0] = 0xf0;
	c[1] = 0xf1;
#endif /*]*/
}
