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

#define CS_GE 0x04	 /* hack */

#define WCC_LINE_LENGTH(c)		((c) & 0x30)
#define WCC_132				0x00
#define WCC_40				0x10
#define WCC_64				0x20
#define WCC_80				0x30

#define MAX_LL				132
#define MAX_BUF				(MAX_LL * MAX_LL)

static char *ll_name[] = { "(none) 132", "40", "64", "80" };
static ll_len[] = { 132, 40, 64, 80 };

/* Statics */
static unsigned char default_gr;
static unsigned char default_cs;
static int line_length = MAX_LL;
static char page_buf[MAX_BUF];	/* swag */
static int baddr = 0;
static int initted = 0;
static int any_output = 0;
static FILE *prfile;

static void ctlr_erase(void);
static void dump_page(void);

#define DECODE_BADDR(c1, c2) \
	((((c1) & 0xC0) == 0x00) ? \
	(((c1) & 0x3F) << 8) | (c2) : \
	(((c1) & 0x3F) << 6) | ((c2) & 0x3F))


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
		fprintf(stderr, "Unknown 3270 Data Stream command: 0x%X\n",
		    buf[0]);
		return PDS_BAD_CMD;
	}

	return 0;
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
	unsigned char	wcc_line_length;
	Boolean		ra_ge;
	int		i;
	unsigned char	na;
	int		any_fa;
	unsigned char	efa_fg;
	unsigned char	efa_gr;
	unsigned char	efa_cs;
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
			ctlr_add(' ', 0, default_gr); \
			trace_ds(see_attr(fa)); \
		}
#define START_FIELD0	{ START_FIELDx(FA_BASE); }
#define START_FIELD(attr) { \
			new_attr = ATTR2FA(attr); \
			START_FIELDx(new_attr); \
		}

	if (buflen < 2)
		return;

	if (!initted) {
		initted = 1;
		(void) memset(page_buf, ' ', MAX_BUF);
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
			trace_ds("(%d,%d)", 1+(xbaddr/line_length),
				1+(xbaddr%line_length));
			baddr = ((xbaddr/line_length) * MAX_LL) +
				(xbaddr%line_length);
			if (baddr >= MAX_BUF) {
				/* Error! */
				baddr = 0;
				return;
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
			END_TEXT("RepeatToAddress");
			cp += 2;	/* skip buffer address */
			xbaddr = DECODE_BADDR(*(cp-1), *cp);
			cp++;		/* skip char to repeat */
			if (*cp == ORDER_GE){
				ra_ge = True;
				trace_ds("GraphicEscape");
				cp++;
			} else
				ra_ge = False;
			previous = ORDER;
			if (*cp)
				trace_ds("'");
			trace_ds(see_ebc(*cp));
			if (*cp)
				trace_ds("'");
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
			ctlr_add(' ', 0, default_gr);
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
			if (wcc_line_length)
				ctlr_add(' ', default_cs, default_gr);
			else
				ctlr_add('\f', default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_CR:	/* Carriage Return */
			END_TEXT("CR");
			previous = ORDER;
			if (wcc_line_length)
				ctlr_add(' ', default_cs, default_gr);
			else
				ctlr_add('\r', default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NL:	/* New Line */
			END_TEXT("NL");
			previous = ORDER;
			if (wcc_line_length)
				ctlr_add(' ', default_cs, default_gr);
			else
				ctlr_add('\n', default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_EM:	/* End of Media */
			END_TEXT("EM");
			previous = ORDER;
			if (wcc_line_length)
				ctlr_add(' ', default_cs, default_gr);
			else
				print_eoj();
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_SUB:	/* misc format control orders */
		case FCORDER_DUP:
		case FCORDER_FM:
		case FCORDER_EO:
			END_TEXT(see_ebc(*cp));
			previous = ORDER;
			ctlr_add(ebc2cg[*cp], default_cs, default_gr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NULL:
			END_TEXT("NULL");
			previous = NULLCH;
			ctlr_add(' ', default_cs, default_gr);
			last_cmd = False;
			last_zpt = False;
			break;
		default:	/* enter character */
			if (*cp <= 0x3F) {
				END_TEXT("ILLEGAL_ORDER");
				previous = ORDER;
				ctlr_add(' ', default_cs, default_gr);
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

	/* Do the implied trailing newline, unless we just processed an EM. */
	if (previous != ORDER || *cp != FCORDER_EM || wcc_line_length ||
			any_output) {
		ctlr_add('\n', default_cs, default_gr);
	}
	trace_ds("\n");
}

#undef START_FIELDx
#undef START_FIELD0
#undef START_FIELD
#undef END_TEXT0
#undef END_TEXT


/*
 * This should use popen, but it's not worth it yet.
 */
void
stash(unsigned char c)
{
	if (prfile == NULL) {
		prfile = popen(command, "w");
		if (prfile == NULL) {
			perror(command);
			exit(1);
		}
	}
	(void) fputc(c, prfile);
}

/*
 * Change a character in the 3270 buffer.
 */
void
ctlr_add(unsigned char c, unsigned char cs, unsigned char gr)
{
	switch (c) {
		case '\r':
			baddr -= baddr % MAX_LL;
			break;
		case '\n':
			baddr = (baddr - (baddr % MAX_LL) + MAX_LL) % MAX_BUF;
			break;
		case '\f':
			dump_page();
			break;
		default:
			if ((c & 0x7f) < ' ')	/* just in case */
				c = ' ';
			page_buf[baddr] = c;
			baddr = (baddr + 1) % MAX_BUF;
			if (baddr % MAX_LL >= line_length)
				baddr -= baddr % MAX_LL;
			break;
	}
	any_output = 1;
}

static void
dump_page(void)
{
	int i, j;
	int blanks = 0;
	int blank_lines = 0;

	if (!any_output)
		return;
	for (i = 0; i < MAX_LL; i++) {
		for (j = 0; j < MAX_LL; j++) {
			char c;

			c = page_buf[i*MAX_LL + j];
			if (c == ' ') {
				blanks++;
				continue;
			}
			while (blank_lines) {
				stash('\n');
				blank_lines--;
			}
			while (blanks) {
				stash(' ');
				blanks--;
			}
			stash(c);
		}
		if (blanks >= MAX_LL)
			blank_lines++;
		else
			stash('\n');
		blanks = 0;
	}
	stash('\f');
	(void) memset(page_buf, ' ', MAX_BUF);
	fflush(prfile);
	any_output = 0;
}

void
print_eoj(void)
{
	dump_page();
	if (pclose(prfile) < 0)
		perror(command);
	prfile = NULL;
}

static void
ctlr_erase(void)
{
	/*(void) memset(page_buf, ' ', MAX_BUF);*/
}
