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
 *	ctlr.c
 *		This module handles interpretation of the 3270 data stream and
 *		maintenance of the 3270 device state.  It was split out from
 *		screen.c, which handles X operations.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "localdefs.h"
#include "3270ds.h"
#include "ctlrc.h"
#include "trace_dsc.h"
#include "sfc.h"
#include "tablesc.h"

extern char *command;
extern int blanklines;	/* display blank lines even if empty (formatted LU3) */
extern int ignoreeoj;	/* ignore PRINT-EOJ commands */
extern int crlf;	/* expand newline to CR/LF */
extern int ffthru;	/* pass through SCS FF orders */
extern int ffskip;	/* skip FF orders at top of page */

#define CS_GE 0x04	/* hack */

#define WCC_LINE_LENGTH(c)		((c) & 0x30)
#define WCC_132				0x00
#define WCC_40				0x10
#define WCC_64				0x20
#define WCC_80				0x30

#define MAX_LL				132
#define MAX_BUF				(MAX_LL * MAX_LL)

#define VISIBLE		0x01	/* visible field */
#define INVISIBLE	0x02	/* invisible field */

static const char *ll_name[] = { "unformatted132", "formatted40", "formatted64", "formatted80" };
static int ll_len[] = { 132, 40, 64, 80 };

/* 3270 (formatted mode) data */
static unsigned char default_gr;
static unsigned char default_cs;
static int line_length = MAX_LL;
static char page_buf[MAX_BUF];	/* swag */
static int baddr = 0;
static Boolean page_buf_initted = False;
static Boolean any_3270_printable = False;
static int any_3270_output = 0;
static FILE *prfile = NULL;
static unsigned char wcc_line_length;

static void ctlr_erase(void);
static void dump_formatted(void);
static void dump_unformatted(void);
static void stash(unsigned char c);

#define DECODE_BADDR(c1, c2) \
	((((c1) & 0xC0) == 0x00) ? \
	(((c1) & 0x3F) << 8) | (c2) : \
	(((c1) & 0x3F) << 6) | ((c2) & 0x3F))

/* SCS constants and data. */
#define MAX_MPP	132
#define MAX_MPL	108

static char linebuf[MAX_MPP+1];
static struct {
    unsigned malloc_len;
    unsigned data_len;
    char *buf;
} trnbuf[MAX_MPP+1];
static char htabs[MAX_MPP+1];
static char vtabs[MAX_MPL+1];
static int lm, tm, bm, mpp, mpl, scs_any;
static int pp;
static int line;
static Boolean scs_initted = False;
static Boolean any_scs_output = False;


/*
 * Interpret an incoming 3270 command.
 */
enum pds
process_ds(unsigned char *buf, int buflen)
{
	if (!buflen)
		return PDS_OKAY_NO_OUTPUT;

	trace_ds("< ");

	switch (buf[0]) {	/* 3270 command */
	case CMD_EAU:	/* erase all unprotected */
	case SNA_CMD_EAU:
		trace_ds("EraseAllUnprotected\n");
		ctlr_erase();
		return PDS_OKAY_NO_OUTPUT;
		break;
	case CMD_EWA:	/* erase/write alternate */
	case SNA_CMD_EWA:
		trace_ds("EraseWriteAlternate");
		ctlr_erase();
		baddr = 0;
		ctlr_write(buf, buflen, True);
		return PDS_OKAY_NO_OUTPUT;
		break;
	case CMD_EW:	/* erase/write */
	case SNA_CMD_EW:
		trace_ds("EraseWrite");
		ctlr_erase();
		baddr = 0;
		ctlr_write(buf, buflen, True);
		return PDS_OKAY_NO_OUTPUT;
		break;
	case CMD_W:	/* write */
	case SNA_CMD_W:
		trace_ds("Write");
		ctlr_write(buf, buflen, False);
		return PDS_OKAY_NO_OUTPUT;
		break;
	case CMD_RB:	/* read buffer */
	case SNA_CMD_RB:
		trace_ds("ReadBuffer\n");
		return PDS_BAD_CMD;
		break;
	case CMD_RM:	/* read modifed */
	case SNA_CMD_RM:
		trace_ds("ReadModified\n");
		return PDS_BAD_CMD;
		break;
	case CMD_RMA:	/* read modifed all */
	case SNA_CMD_RMA:
		trace_ds("ReadModifiedAll\n");
		return PDS_BAD_CMD;
		break;
	case CMD_WSF:	/* write structured field */
	case SNA_CMD_WSF:
		trace_ds("WriteStructuredField");
		return write_structured_field(buf, buflen);
		break;
	case CMD_NOP:	/* no-op */
		trace_ds("NoOp\n");
		return PDS_OKAY_NO_OUTPUT;
		break;
	default:
		/* unknown 3270 command */
		errmsg("Unknown 3270 Data Stream command: 0x%X", buf[0]);
		return PDS_BAD_CMD;
	}
}

/*
 * Process a 3270 Write command.
 */
void
ctlr_write(unsigned char buf[], int buflen, Boolean erase)
{
	register unsigned char	*cp;
	unsigned char	new_attr;
	Boolean		last_cmd;
	Boolean		last_zpt;
	Boolean		wcc_keyboard_restore, wcc_sound_alarm;
	Boolean		wcc_start_printer;
	Boolean		ra_ge;
	int		i;
	unsigned char	na;
	int		any_fa;
	unsigned char	efa_fg;
	unsigned char	efa_gr;
	unsigned char	efa_cs;
	unsigned char	ra_xlate = 0;
	const char	*paren = "(";
	int		xbaddr;
	enum { NONE, ORDER, SBA, TEXT, NULLCH } previous = NONE;

#define END_TEXT0	{ if (previous == TEXT) trace_ds("'"); }
#define END_TEXT(cmd)	{ END_TEXT0; trace_ds(" %s", cmd); }

#define ATTR2FA(attr) \
	(FA_BASE | \
	 (((attr) & 0x20) ? FA_PROTECT : 0) | \
	 (((attr) & 0x10) ? FA_NUMERIC : 0) | \
	 (((attr) & 0x01) ? FA_MODIFY : 0) | \
	 (((attr) >> 2) & FA_INTENSITY))
#define START_FIELDx(fa) { \
			ctlr_add(FA_IS_ZERO(fa)?INVISIBLE:VISIBLE, 0, default_gr); \
			trace_ds(see_attr(fa)); \
		}
#define START_FIELD0	{ START_FIELDx(FA_BASE); }
#define START_FIELD(attr) { \
			new_attr = ATTR2FA(attr); \
			START_FIELDx(new_attr); \
		}

	if (buflen < 2)
		return;

	if (!page_buf_initted) {
		(void) memset(page_buf, '\0', MAX_BUF);
		page_buf_initted = True;
		baddr = 0;
	}

	default_gr = 0;
	default_cs = 0;

	if (WCC_RESET(buf[1])) {
		trace_ds("%sreset", paren);
		paren = ",";
	}
	wcc_line_length = WCC_LINE_LENGTH(buf[1]);
	if (wcc_line_length) {
		trace_ds("%s%s", paren, ll_name[wcc_line_length >> 4]);
		paren = ",";
	} else {
		trace_ds("%sunformatted", paren);
		paren = ",";
	}
	line_length = ll_len[wcc_line_length >> 4];
	wcc_sound_alarm = WCC_SOUND_ALARM(buf[1]);
	if (wcc_sound_alarm) {
		trace_ds("%salarm", paren);
		paren = ",";
	}
	wcc_keyboard_restore = WCC_KEYBOARD_RESTORE(buf[1]);
	if (wcc_keyboard_restore) {
		trace_ds("%srestore", paren);
		paren = ",";
	}

	if (WCC_RESET_MDT(buf[1])) {
		trace_ds("%sresetMDT", paren);
		paren = ",";
	}
	wcc_start_printer = WCC_START_PRINTER(buf[1]);
	if (wcc_start_printer) {
		trace_ds("%sstartprinter", paren);
		paren = ",";
	}
	if (strcmp(paren, "("))
		trace_ds(")");

	last_cmd = True;
	last_zpt = False;
	for (cp = &buf[2]; cp < (buf + buflen); cp++) {
		switch (*cp) {
		case ORDER_SF:	/* start field */
			END_TEXT("StartField");
			previous = ORDER;
			cp++;		/* skip field attribute */
			START_FIELD(*cp);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SBA:	/* set buffer address */
			cp += 2;	/* skip buffer address */
			xbaddr = DECODE_BADDR(*(cp-1), *cp);
			END_TEXT("SetBufferAddress");
			if (wcc_line_length)
				trace_ds("(%d,%d)", 1+(xbaddr/line_length),
					1+(xbaddr%line_length));
			else
				trace_ds("(%d[%+d])", xbaddr, xbaddr-baddr);
			if (xbaddr >= MAX_BUF) {
				/* Error! */
				baddr = 0;
				return;
			}
			if (wcc_line_length) {
				/* Formatted. */
				baddr = xbaddr;
			} else if (xbaddr > baddr) {
				/* Unformatted. */
				while (baddr < xbaddr) {
					ctlr_add(' ', default_cs, default_gr);
				}
			}
			previous = SBA;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_IC:	/* insert cursor */
			END_TEXT("InsertCursor");
			previous = ORDER;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_PT:	/* program tab */
			END_TEXT("ProgramTab");
			previous = ORDER;
			last_cmd = True;
			break;
		case ORDER_RA:	/* repeat to address */
			cp += 2;	/* skip buffer address */
			xbaddr = DECODE_BADDR(*(cp-1), *cp);
			END_TEXT("RepeatToAddress");
			if (wcc_line_length)
				trace_ds("(%d,%d)", 1+(xbaddr/line_length),
					1+(xbaddr%line_length));
			else
				trace_ds("(%d[%+d])", xbaddr, xbaddr-baddr);
			cp++;		/* skip char to repeat */
			if (*cp == ORDER_GE){
				ra_ge = True;
				trace_ds("GraphicEscape");
				cp++;
			} else
				ra_ge = False;
			trace_ds("'%s'", see_ebc(*cp));
			previous = ORDER;
			if (xbaddr > MAX_BUF || xbaddr < baddr) {
				baddr = 0;
				return;
			}
			/* Translate '*cp' once. */
			switch (*cp) {
			case FCORDER_FF:
			case FCORDER_CR:
			case FCORDER_NL:
			case FCORDER_EM:
				ra_xlate = *cp;
				break;
			default:
				if (*cp <= 0x3F) {
					ra_xlate = '\0';
				} else {
					ra_xlate = ebc2asc[*cp];
				}
				break;
			}
			while (baddr < xbaddr) {
				ctlr_add(ra_xlate, ra_ge? CS_GE: default_cs,
				    default_gr);
			}
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_EUA:	/* erase unprotected to address */
			cp += 2;	/* skip buffer address */
			xbaddr = DECODE_BADDR(*(cp-1), *cp);
			END_TEXT("EraseUnprotectedAll");
			previous = ORDER;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_GE:	/* graphic escape */
			END_TEXT("GraphicEscape ");
			cp++;		/* skip char */
			previous = ORDER;
			if (*cp)
				trace_ds("'");
			trace_ds(see_ebc(*cp));
			if (*cp)
				trace_ds("'");
			ctlr_add(ebc2asc[*cp], CS_GE, default_gr);
			last_cmd = False;
			last_zpt = False;
			break;
		case ORDER_MF:	/* modify field */
			END_TEXT("ModifyField");
			previous = ORDER;
			cp++;
			na = *cp;
			cp += na * 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SFE:	/* start field extended */
			END_TEXT("StartFieldExtended");
			previous = ORDER;
			cp++;	/* skip order */
			na = *cp;
			any_fa = 0;
			efa_fg = 0;
			efa_gr = 0;
			efa_cs = 0;
			for (i = 0; i < (int)na; i++) {
				cp++;
				if (*cp == XA_3270) {
					trace_ds(" 3270");
					cp++;
					START_FIELD(*cp);
					any_fa++;
				} else if (*cp == XA_FOREGROUND) {
					trace_ds("%s", see_efa(*cp, *(cp + 1)));
					cp++;
					efa_fg = *cp;
				} else if (*cp == XA_HIGHLIGHTING) {
					trace_ds("%s", see_efa(*cp, *(cp + 1)));
					cp++;
					efa_gr = *cp & 0x07;
				} else if (*cp == XA_CHARSET) {
					trace_ds("%s", see_efa(*cp, *(cp + 1)));
					cp++;
					if (*cp == 0xf1)
						efa_cs = 1;
				} else if (*cp == XA_ALL) {
					trace_ds("%s", see_efa(*cp, *(cp + 1)));
					cp++;
				} else {
					trace_ds("%s[unsupported]",
						see_efa(*cp, *(cp + 1)));
					cp++;
				}
			}
			if (!any_fa)
				START_FIELD0;
			ctlr_add('\0', 0, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SA:	/* set attribute */
			END_TEXT("SetAttribtue");
			previous = ORDER;
			cp++;
			if (*cp == XA_FOREGROUND)  {
				trace_ds("%s", see_efa(*cp, *(cp + 1)));
			} else if (*cp == XA_HIGHLIGHTING)  {
				trace_ds("%s", see_efa(*cp, *(cp + 1)));
				default_gr = *(cp + 1) & 0x07;
			} else if (*cp == XA_ALL)  {
				trace_ds("%s", see_efa(*cp, *(cp + 1)));
				default_gr = 0;
				default_cs = 0;
			} else if (*cp == XA_CHARSET) {
				trace_ds("%s", see_efa(*cp, *(cp + 1)));
				default_cs = (*(cp + 1) == 0xf1) ? 1 : 0;
			} else
				trace_ds("%s[unsupported]",
				    see_efa(*cp, *(cp + 1)));
			cp++;
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_FF:	/* Form Feed */
			END_TEXT("FF");
			previous = ORDER;
			ctlr_add(FCORDER_FF, default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_CR:	/* Carriage Return */
			END_TEXT("CR");
			previous = ORDER;
			ctlr_add(FCORDER_CR, default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NL:	/* New Line */
			END_TEXT("NL");
			previous = ORDER;
			ctlr_add(FCORDER_NL, default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_EM:	/* End of Media */
			END_TEXT("EM");
			previous = ORDER;
			ctlr_add(FCORDER_EM, default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_DUP:	/* Visible control characters */
		case FCORDER_FM:
			END_TEXT(see_ebc(*cp));
			previous = ORDER;
			ctlr_add(ebc2asc[*cp], default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_SUB:	/* misc format control orders */
		case FCORDER_EO:
			END_TEXT(see_ebc(*cp));
			previous = ORDER;
			ctlr_add('\0', default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NULL:
			END_TEXT("NULL");
			previous = NULLCH;
			ctlr_add('\0', default_cs, default_gr);
			last_cmd = False;
			last_zpt = False;
			break;
		default:	/* enter character */
			if (*cp <= 0x3F) {
				END_TEXT("ILLEGAL_ORDER");
				previous = ORDER;
				ctlr_add('\0', default_cs, default_gr);
				trace_ds(see_ebc(*cp));
				last_cmd = True;
				last_zpt = False;
				break;
			}
			if (previous != TEXT)
				trace_ds(" '");
			previous = TEXT;
			trace_ds(see_ebc(*cp));
			ctlr_add(ebc2asc[*cp], default_cs, default_gr);
			last_cmd = False;
			last_zpt = False;
			break;
		}
	}

	trace_ds("\n");

#if 0
	/* If it's formatted, print it out now. */
	if (wcc_line_length)
		dump_formatted();
#endif
}

#undef START_FIELDx
#undef START_FIELD0
#undef START_FIELD
#undef END_TEXT0
#undef END_TEXT

/*
 * Process SCS (SNA Character Stream) data.
 */

/* Reinitialize the SCS virtual 3287. */
static void
init_scs_horiz(void)
{
	int i;

	mpp = MAX_MPP;
	lm = 1;
	htabs[1] = 1;
	for (i = 2; i <= MAX_MPP; i++) {
		htabs[i] = 0;
	}
}

static void
init_scs_vert(void)
{
	int i;

	mpl = 1;
	tm = 1;
	bm = mpl;
	vtabs[1] = 1;
	for (i = 0; i <= MAX_MPL; i++) {
		vtabs[i] = 0;
	}
}

static void
init_scs(void)
{
	if (scs_initted)
		return;

	trace_ds("Initializing SCS virtual 3287.\n");
	init_scs_horiz();
	init_scs_vert();
	pp = 1;
	line = 1;
	scs_any = 0;
	(void) memset(linebuf, ' ', MAX_MPP+1);
	scs_initted = True;
}

/*
 * Our philosophy for automatic newlines and formfeeds is that we generate them
 * only if the user attempts to put data outside the MPP/MPL-defined area.
 * Therefore, the user can put a byte on the last column of each line, and on
 * the last column of the last line of the page, and not need to worry about 
 * suppressing their own NL or FF.
 */

/*
 * Dump and reset the current line.
 * This will always result in at least one byte of output to the printer (a
 * newline).  The 'line' variable is always incremented, and may end up
 * pointing past the bottom margin.  The 'pp' variable is set to the left
 * margin.
 */
static void
dump_scs_line(Boolean reset_pp, Boolean always_nl)
{
	int i;
	Boolean any_data = False;

	/* Find the last non-space character in the line buffer. */
	for (i = mpp; i >= 1; i--) {
		if (trnbuf[i].data_len != 0 || linebuf[i] != ' ')
			break;
	}

	/*
	 * If there is data there, print it with a trailing newline and
	 * clear out the line buffer for next time.  If not, just print the
	 * newline.
	 */
	if (i >= 1) {
		int j;
		int n_data = 0;
		int n_trn = 0;

		for (j = 1; j <= i; j++) {
			/*
			 * Dump and transparent data that precedes this
			 * character.
			 */
			if (trnbuf[j].data_len) {
				int k;

				n_trn += trnbuf[j].data_len;
				for (k = 0; k < trnbuf[j].data_len; k++) {
					stash(trnbuf[j].buf[k]);
				}
				trnbuf[j].data_len = 0;
			}
			if (j < i || linebuf[j] != ' ') {
				n_data++;
				any_data = True;
				scs_any = True;
				stash(linebuf[j]);
			}
		}
#if defined(DEBUG_FF) /*[*/
		trace_ds(" [dumping %d+%dt]", n_data, n_trn);
#endif /*]*/
		(void) memset(linebuf, ' ', MAX_MPP+1);
	}
	if (any_data || always_nl) {
		if (crlf)
			stash('\r');
		stash('\n');
		line++;
	}
#if defined(DEBUG_FF) /*[*/
	trace_ds(" [line=%d]", line);
#endif /*]*/
	if (reset_pp)
		pp = lm;
	any_scs_output = False;
}

/* SCS formfeed. */
static void
scs_formfeed(Boolean explicit)
{
	int nls = 0;

	/*
	 * In ffskip mode, if it's an explicit formfeed, and we haven't
	 * printed any non-transparent data, do nothing.
	 */
	if (ffskip && explicit && !scs_any)
		return;

	/*
	 * In ffthru mode, pass through a \f, but only if it's explicit.
	 */
	if (ffthru) {
		if (explicit) {
			stash('\f');
			scs_any = 0;
		}
		line = 1;
		return;
	}

	if (explicit)
		scs_any = 0;

	/* Skip to the end of the physical page. */
	while (line < mpl) {
		if (crlf)
			stash('\r');
		stash('\n');
		nls++;
		line++;
	}
	line = 1;

	/* Skip the top margin. */
	while (line < tm) {
		if (crlf)
			stash('\r');
		stash('\n');
		nls++;
		line++;
	}
#if defined(DEBUG_FF) /*[*/
	if (nls)
		trace_ds(" [formfeed %s %d]", explicit? "explicit": "implicit",
			nls);
#endif /*]*/
}

/*
 * Add a printable character to the SCS virtual 3287.
 * If the line position is past the bottom margin, we will skip to the top of
 * the next page.  If the character position is past the MPP, we will skip to
 * the left margin of the next line.
 */
static void
add_scs(char c)
{
	/*
	 * They're about to print something.
	 * If the line is past the bottom margin, we need to skip to the
	 * MPL, and then past the top margin.
	 */
	if (line > bm)
		scs_formfeed(False);

	/*
	 * If this character would overflow the line, then dump the current
	 * line and start over at the left margin.
	 */
	if (pp > mpp)
		dump_scs_line(True, True);

	/*
	 * Store this character in the line buffer and advance the print
	 * position.
	 */
	linebuf[pp++] = c;
	any_scs_output = True;
}

/*
 * Add a string of transparent data to the SCS virtual 3287.
 * Transparent data lives between the 'counted' 3287 characters.  Really.
 */
static void
add_scs_trn(unsigned char *cp, int cnt)
{
	int i;
	int new_malloc_len;

	for (i = 0; i < cnt; i++) {
	    trace_ds(" %02x", cp[i]);
	}

	new_malloc_len = trnbuf[pp].data_len + cnt;
	if (trnbuf[pp].malloc_len < new_malloc_len) {
		trnbuf[pp].buf = Realloc(trnbuf[pp].buf, new_malloc_len);
		trnbuf[pp].malloc_len = new_malloc_len;
	}
	(void) memcpy(trnbuf[pp].buf + trnbuf[pp].data_len, cp, cnt);
	trnbuf[pp].data_len += cnt;
}

/* Process a bufferful of SCS data. */
enum pds
process_scs(unsigned char *buf, int buflen)
{
	register unsigned char *cp;
	int i;
	int cnt;
	int tab;
	enum { NONE, DATA, ORDER } last = NONE;
#	define END_TEXT(s) { \
		if (last == DATA) \
			trace_ds("'"); \
		trace_ds(" " s); \
		last = ORDER; \
	}

	trace_ds("< ");

	init_scs();

	for (cp = &buf[0]; cp < (buf + buflen); cp++) {
		switch (*cp) {
		case SCS_BS:	/* back space */
			END_TEXT("BS");
			if (pp != 1)
				pp--;
			break;
		case SCS_CR:	/* carriage return */
			END_TEXT("CR");
			pp = lm;
			break;
		case SCS_ENP:	/* enable presentation */
			END_TEXT("ENP");
			/* No-op. */
			break;
		case SCS_FF:	/* form feed */
			END_TEXT("FF");
			/* Dump any pending data, and go to the next line. */
			dump_scs_line(True, False);
			/*
			 * If there is a max page length, skip to the next
			 * page.
			 */
			scs_formfeed(True);
			break;
		case SCS_HT:	/* horizontal tab */
			END_TEXT("HT");
			for (i = pp; i <= mpp; i++) {
				if (htabs[i])
					break;
			}
			if (i <= mpp)
				i = mpp;
			else
				add_scs(' ');
			break;
		case SCS_INP:	/* inhibit presentation */
			END_TEXT("INP");
			/* No-op. */
			break;
		case SCS_IRS:	/* inter-record separator */
			END_TEXT("IRS");
		case SCS_NL:	/* new line */
			if (*cp == SCS_NL)
				END_TEXT("NL");
			dump_scs_line(True, True);
			break;
		case SCS_VT:	/* vertical tab */
			END_TEXT("VT");
			for (i = line+1; i <= MAX_MPL; i++){
				if (vtabs[i])
					break;
			}
			if (i <= MAX_MPL) {
				dump_scs_line(False, True);
				while (line < i) {
					if (crlf)
						stash('\r');
					stash('\n');
					line++;
				}
				break;
			} else {
				/* fall through... */
			}
		case SCS_VCS:	/* vertical channel select */
			if (*cp == SCS_VCS)
				END_TEXT("VCS");
		case SCS_LF:	/* line feed */
			if (*cp == SCS_LF)
				END_TEXT("LF");
			dump_scs_line(False, True);
			break;
		case SCS_GE:	/* graphic escape */
			END_TEXT("GE");
			/* Skip over the order. */
			cp++;
			if (cp >= buf + buflen)
				break;	/* be gentle */
			/* No support, so all characters are spaces. */
			trace_ds(" %02x", *cp);
			add_scs(' ');
			break;
		case SCS_SA:	/* set attribute */
			END_TEXT("SA");
			/* We don't support it, skip it. */
			trace_ds(" %02x %02x", *(cp + 1), *(cp + 2));
			cp += 2;
			break;
		case SCS_TRN:	/* transparent */
			END_TEXT("TRN");
			/* Skip over the order. */
			cp++;
			/* Make sure a length byte is present. */
			if (cp >= buf + buflen)
				break;	/* be gentle */
			/*
			 * Next byte is the length of the transparent data,
			 * not including the length byte itself.
			 */
			cnt = *cp;
			trace_ds("(%d)", cnt);
			/* Copy out the data literally. */
			add_scs_trn(cp+1, cnt);
			cp += cnt;
			break;
		case SCS_SET:	/* set... */
			/* Skip over the first byte of the order. */
			cp++;
			if (cp >= buf + buflen)
				break;	/* be gentle */
			switch (*cp) {
				case SCS_SHF:	/* set horizontal format */
					END_TEXT("SHF");
					/* Take defaults first. */
					init_scs_horiz();
					/*
					 * Skip over the second byte of the
					 * order.
					 */
					cp++;
					/*
					 * The length is next.  It includes the
					 * length field itself.
					 */
					if (cp >= buf + buflen)
						break;
					cnt = *cp;
					trace_ds("(%d)", cnt);
					if (cnt < 2)
						break;	/* no more data */
					/* Skip over the length byte. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* The MPP is next. */
					mpp = *cp;
					trace_ds(" mpp=%d", mpp);
					if (!mpp || mpp > MAX_MPP)
						mpp = MAX_MPP;
					/* Skip over the MPP. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* The LM is next. */
					lm = *cp;
					trace_ds(" lm=%d", lm);
					if (lm < 1 || lm >= mpp)
						lm = 1;
					/* Skip over the LM. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* Skip over the RM. */
					trace_ds(" rm=%d", *cp);
					cp++;
					cnt--;
					/* Next are the tab stops. */
					while (cnt && cp < buf + buflen) {
						tab = *cp;
						trace_ds(" tab=%d", tab);
						if (tab >= 1 && tab <= mpp)
							htabs[tab] = 1;
						cp++;
						cnt--;
					}
					break;
				case SCS_SLD:	/* set line density */
					END_TEXT("SLD");
					/*
					 * Skip over the second byte of the
					 * order.
					 */
					cp++;
					/*
					 * The length is next.  It does not
					 * include length field itself.
					 */
					if (cp >= buf + buflen)
						break;
					cnt = *cp;
					trace_ds("(%d)", cnt);
					if (cnt != 2)
						break; /* be gentle */
					cnt--;
					trace_ds(" %02x", *(cp + 1));
					cp += cnt;
					break;
				case SCS_SVF:	/* set vertical format */
					END_TEXT("SVF");
					/* Take defaults first. */
					init_scs_vert();
					/*
					 * Skip over the second byte of the
					 * order.
					 */
					cp++;
					/*
					 * The length is next.  It includes the
					 * length field itself.
					 */
					if (cp >= buf + buflen)
						break;
					cnt = *cp;
					trace_ds("(%d)", cnt);
					if (cnt < 2)
						break;	/* no more data */
					/* Skip over the length byte. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* The MPL is next. */
					mpl = *cp;
					trace_ds(" mpl=%d", mpl);
					if (!mpl || mpl > MAX_MPL)
						mpl = 1;
					if (cnt < 2) {
						bm = mpl;
						break;
					}
					/* Skip over the MPL. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* The TM is next. */
					tm = *cp;
					trace_ds(" tm=%d", tm);
					if (tm < 1 || tm >= mpl)
						tm = 1;
					if (cnt < 2)
						break;
					/* Skip over the TM. */
					cp++;
					cnt--;
					if (!cnt || cp >= buf + buflen)
						break;
					/* The BM is next. */
					bm = *cp;
					trace_ds(" bm=%d", bm);
					if (bm < tm || bm >= mpl)
						bm = mpl;
					if (cnt < 2)
						break;
					/* Skip over the BM. */
					cp++;
					cnt--;
					/* Next are the tab stops. */
					while (cnt > 1 && cp < buf + buflen) {
						tab = *cp;
						trace_ds(" tab=%d", tab);
						if (tab >= 1 && tab <= mpp)
							vtabs[tab] = 1;
						cp++;
						cnt--;
					}
					break;
				default:
					break;	/* be gentle */
			}
			break;
		default:
			/*
			 * Stray control codes are spaces, all else gets
			 * translated from EBCDIC to ASCII.
			 */
			if (*cp <= 0x3f) {
				END_TEXT("?");
				trace_ds("%02x", *cp);
				add_scs(' ');
			} else {
				if (last == NONE)
					trace_ds("'");
				else if (last == ORDER)
					trace_ds(" '");
				trace_ds("%c", ebc2asc[*cp]);
				add_scs(ebc2asc[*cp]);
				last = DATA;
			}
			break;
		}
	}

	if (last == DATA)
		trace_ds("'");
	trace_ds("\n");
	return PDS_OKAY_NO_OUTPUT;
}


/*
 * Send a character to the printer.
 */
static void
stash(unsigned char c)
{
	if (prfile == NULL) {
		prfile = popen(command, "w");
		if (prfile == NULL) {
			errmsg("%s: %s", command, strerror(errno));
			exit(1);
		}
	}
	(void) fputc(c, prfile);
	if (ferror(prfile)) {
		errmsg("Write error to '%s'", command);
		exit(1);
	}
}

/*
 * Change a character in the 3270 buffer.
 */
void
ctlr_add(unsigned char c, unsigned char cs, unsigned char gr)
{
	/* Map control characters, according to the write mode. */
	if ((c & 0x7f) < ' ') {
		if (wcc_line_length) {
			/*
			 * When formatted, all control characters but FFs and
			 * the funky VISIBLE/INVISIBLE controls are translated
			 * to NULLs, so they don't display, and don't
			 * contribute to empty lines.
			 */
			if (c != FCORDER_FF && c != VISIBLE && c != INVISIBLE)
				c = '\0';
		} else {
			/*
			 * Unformatted, all control characters but CR/NL/FF/EM
			 * are displayed as spaces.
			 */
			if (c != FCORDER_CR && c != FCORDER_NL &&
			    c != FCORDER_FF && c != FCORDER_EM)
				c = ' ';
		}
	}

	/* Add the character. */
	page_buf[baddr] = c;
	baddr = (baddr + 1) % MAX_BUF;
	any_3270_output = 1;

#if 0
	/* If this is an EM, dump the unformatted buffer. */
	if (c == FCORDER_EM)
		dump_unformatted();
#endif
}

/*
 * Unformatted output function.  Processes one character of output data.
 *
 * This function will buffer up to MAX_LL characters of output, until it is
 * passed a '\n' or '\f' character.
 *
 * It will process '\r' characters like a printer, i.e., it will not overwrite
 * a buffered non-space character with a space character.  This is how
 * an output line can span multiple 3270 unformatted write commands.
 */
static void
uoutput(char c)
{
	static char buf[MAX_LL];
	static int col = 0;
	static int maxcol = 0;
	int i;

	switch (c) {
	case '\r':
		col = 0;
		break;
	case '\n':
		for (i = 0; i < maxcol; i++) {
			stash(buf[i]);
		}
		if (crlf)
		    stash('\r');
		stash(c);
		col = maxcol = 0;
		break;
	case '\f':
		if (any_3270_printable || !ffskip) {
			for (i = 0; i < maxcol; i++) {
				stash(buf[i]);
			}
			stash(c);
		}
		col = maxcol = 0;
		break;
	default:
		/* Don't overwrite with spaces. */
		if (c == ' ') {
			if (col >= maxcol)
				buf[col++] = c;
			else
				col++;
		} else {
			buf[col++] = c;
			any_3270_printable = True;
		}
		if (col > maxcol)
			maxcol = col;
		break;
	}
}

/*
 * Dump an unformatted output buffer.
 *
 * The buffer is treated as a sequence of characters, with control characters
 * for new line, carriage return, form feed and end of media.
 *
 * By definition, the "print position" is 0 when this function begins and ends.
 */
static void
dump_unformatted(void)
{
	int i;
	int prcol = 0;
	char c;
	int done = 0;

	if (!any_3270_output)
		return;

	for (i = 0; i < MAX_BUF && !done; i++) {
		switch (c = page_buf[i]) {
		case '\0':
			break;
		case FCORDER_CR:
			uoutput('\r');
			prcol = 0;
			break;
		case FCORDER_NL:
			uoutput('\n');
			prcol = 0;
			break;
		case FCORDER_FF:
			uoutput('\f');
			prcol = 0;
			break;
		case FCORDER_EM:
			if (prcol != 0)
				uoutput('\n');
			done = 1;
			break;
		default:	/* printable */
			uoutput(c);

			/* Handle implied newlines. */
			if (++prcol >= MAX_LL) {
				uoutput('\n');
				prcol = 0;
			}
			break;
		}
	}

	/* If the buffer didn't end with an EM, flush any pending line. */
	if (!done)
		uoutput('\n');

	/* Clear out the buffer. */
	(void) memset(page_buf, '\0', MAX_BUF);

	/* Flush buffered data. */
	fflush(prfile);
	any_3270_output = 0;
}

/*
 * Dump a formatted output buffer.
 *
 * The buffer is treated as a sequence of lines, with the length specified by
 * the write control character.  
 *
 * Each line is terminated by a newline, with trailing spaces and nulls
 * suppressed.
 * Nulls are displayed as spaces, except when they constitute an entire line,
 * in which case the line is suppressed.
 * Formfeeds are passed through, and otherwise treated like nulls.
 */
static void
dump_formatted(void)
{
	int i;
	char *cp = page_buf;
	int visible = 1;
	int newlines = 0;

	if (!any_3270_output)
		return;
	for (i = 0; i < MAX_LL; i++) {
		int blanks = 0;
		int any_data = 0;
		int j;

		for (j = 0;
		     j < line_length && ((i * line_length) + j) < MAX_BUF;
		     j++) {
			char c = *cp++;

			switch (c) {
			case VISIBLE:	/* visible field */
				visible = 1;
				blanks++;
				break;
			case INVISIBLE:	/* invisible field */
				visible = 0;
				blanks++;
				break;
			case '\f':
				while (newlines) {
					if (crlf)
						stash('\r');
					stash('\n');
					newlines--;
				}
				if (any_3270_printable || !ffskip)
					stash('\f');
				blanks++;
				break;
			case '\0':
				blanks++;
				break;
			case ' ':
				blanks++;
				any_data++;
				break;
			default:
				while (newlines) {
					if (crlf)
						stash('\r');
					stash('\n');
					newlines--;
				}
				while (blanks) {
					stash(' ');
					blanks--;
				}
				any_data++;
				stash(visible? c: ' ');
				if (visible)
					any_3270_printable = True;
				break;
			}
		}
		if (any_data || blanklines)
			newlines++;
	}
	(void) memset(page_buf, '\0', MAX_BUF);
	fflush(prfile);
	any_3270_output = 0;
}

void
print_eoj(void)
{
	/* Dump any pending 3270-mode output. */
	if (wcc_line_length)
		dump_formatted();
	else
		dump_unformatted();

	/* Dump any pending SCS-mode output. */
	if (any_scs_output)
		dump_scs_line(True, False);

	/* Close the stream to the print process. */
	if (prfile != NULL) {
		if (pclose(prfile) < 0)
			errmsg("Close error on '%s'", command);
		prfile = NULL;
		trace_ds("End of print job.\n");
	}

	/* Make sure the next 3270 job starts with clean conditions. */
	page_buf_initted = 0;

	/*
	 * Make sure that the next SCS job starts with clean conditions.
	 *
	 * XXX: Not sure if this should happen at the end of each job, or
	 * when an UNBIND is processed.
	 */
	scs_initted = False;

	/*
	 * Reset the FF suprpession logic.
	 */
	any_3270_printable = False;
}

static void
ctlr_erase(void)
{
	/* Dump whatever we've got so far. */
	/* Dump any pending 3270-mode output. */
	if (wcc_line_length)
		dump_formatted();
	else
		dump_unformatted();

	/* Dump any pending SCS-mode output. */
	if (any_scs_output)
		dump_scs_line(True, False); /* XXX: 1st True? */

	/* Make sure the buffer is clean. */
	(void) memset(page_buf, '\0', MAX_BUF);
	any_3270_output = 0;
	baddr = 0;
}
