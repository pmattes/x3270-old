/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	ctlr.c
 *		This module handles interpretation of the 3270 data stream and
 *		maintenance of the 3270 device state.  It was split out from
 *		screen.c, which handles X operations.
 *
 */
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <X11/Intrinsic.h>
#include "globals.h"
#include "3270ds.h"
#include "screen.h"
#include "cg.h"

#define IsBlank(c)	(IS_FA(c) || (c == CG_null) || (c == CG_space))

/* Externals: x3270.c */
extern char	*pending_string;

/* Externals: screen.c */
extern unsigned char	*selected;	/* selection bitmap */

/* Externals: kybd.c */
extern unsigned char	aid;
extern unsigned char	aid2;
extern Boolean	oia_twait, oia_locked;

/* Externals: tables.c */
extern unsigned char	cg2ebc[];
extern unsigned char	ebc2asc[];
extern unsigned char	ebc2cg[];

/* Externals: telnet.c */
extern unsigned char	obuf[], *obptr;

/* Globals */
int		ROWS, COLS;
int		maxROWS, maxCOLS;
int		cursor_addr, buffer_addr;
Boolean		screen_alt = True;	/* alternate screen? */
Boolean		is_altbuffer = False;
unsigned char	*screen_buf;	/* 3270 display buffer */
unsigned char	*ea_buf;	/* 3270 extended attribute buffer */
unsigned char	*ascreen_buf;	/* alternate 3270 display buffer */
unsigned char	*aea_buf;	/* alternate 3270 extended attribute buffer */
unsigned char	*zero_buf;	/* empty buffer, for area clears */
Boolean		formatted = False;	/* set in screen_disp */
Boolean		screen_changed = False;

/* Statics */
static void	set_formatted();
static void	do_read_buffer();
static void	do_erase_all_unprotected();
static void	do_write();
static void	ctlr_blanks();
static unsigned char	fake_fa;
static Boolean	ps_is_loginstring = False;
static Boolean	trace_primed = False;

/* code_table is used to translate buffer addresses and attributes to the 3270
 * datastream representation
 */
static unsigned char	code_table[64] = {
	0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
	0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
	0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
};


/*
 * Initialize the emulated 3270 hardware.
 */
void
ctlr_init(keep_contents, model_changed)
Boolean keep_contents;
Boolean model_changed;
{
	/* Allocate buffers */

	if (model_changed) {
		if (screen_buf)
			XtFree((char *)screen_buf);
		screen_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
		if (ea_buf)
			XtFree((char *)ea_buf);
		ea_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
		if (ascreen_buf)
			XtFree((char *)ascreen_buf);
		ascreen_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
		if (aea_buf)
			XtFree((char *)aea_buf);
		aea_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
		if (zero_buf)
			XtFree((char *)zero_buf);
		zero_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
	}

	if (!keep_contents) {
		cursor_addr = 0;
		buffer_addr = 0;
	}
}

/*
 * Deal with the relationships between model numbers and rows/cols.
 */
void
set_rows_cols(mn)
char *mn;
{
	extern char *ttype_model;

	switch (atoi(mn)) {
	case 2:
		maxROWS = ROWS = 24; 
		maxCOLS = COLS = 80;
		model_num = 2;
		break;
	case 3:
		maxROWS = ROWS = 32; 
		maxCOLS = COLS = 80;
		model_num = 3;
		break;
	default:
	case 4:
		if (atoi(mn) != 4)
			xs_warning("Unknown model %s, defaulting to 4", mn);
		maxROWS = ROWS = 43; 
		maxCOLS = COLS = 80;
		model_num = 4;
		break;
	case 5:
		maxROWS = ROWS = 27; 
		maxCOLS = COLS = 132;
		model_num = 5;
		break;
	}
	*ttype_model = '0' + model_num;
}


/*
 * Set the formatted screen flag.  A formatted screen is a screen that
 * has at least one field somewhere on it.
 */
static void
set_formatted()
{
	register int	baddr;

	formatted = False;
	baddr = 0;
	do {
		if (IS_FA(screen_buf[baddr])) {
			formatted = True;
			break;
		}
		INC_BA(baddr);
	} while (baddr != 0);
}

/*
 * Called when a host connects, disconnects, or changes ANSI/3270 modes.
 */
void
ctlr_connect()
{
	if (ever_3270)
		fake_fa = 0xE0;
	else
		fake_fa = 0xC4;
}


/*
 * Find the field attribute for the given buffer address.  Return its address
 * rather than its value.
 */
unsigned char *
get_field_attribute(baddr)
register int	baddr;
{
	int	sbaddr;

	sbaddr = baddr;
	do {
		if (IS_FA(screen_buf[baddr]))
			return &(screen_buf[baddr]);
		DEC_BA(baddr);
	} while (baddr != sbaddr);
	return &fake_fa;
}

/*
 * Find the extended field attribute for the given buffer address.  Return its
 * its value.
 */
unsigned char
get_extended_attribute(baddr)
register int	baddr;
{
	return ea_buf[baddr];
}

/*
 * Find the next unprotected field.  Returns the address following the
 * unprotected attribute byte, or 0 if no nonzero-width unprotected field
 * can be found.
 */
int
next_unprotected(baddr0)
int baddr0;
{
	register int baddr, nbaddr;

	nbaddr = baddr0;
	do {
		baddr = nbaddr;
		INC_BA(nbaddr);
		if (IS_FA(screen_buf[baddr]) &&
		    !FA_IS_PROTECTED(screen_buf[baddr]) &&
		    !IS_FA(screen_buf[nbaddr]))
			return nbaddr;
	} while (nbaddr != baddr0);
	return 0;
}

/*
 * Perform an erase command, which may include changing the (virtual) screen
 * size.
 */
void
ctlr_erase(alt)
Boolean alt;
{
	ctlr_clear();

	if (alt == screen_alt)
		return;

	screen_disp();

	if (alt) {
		/* Going from 24x80 to maximum. */
		screen_disp();
		ROWS = maxROWS;
		COLS = maxCOLS;
	} else {
		/* Going from maximum to 24x80. */
		if (maxROWS > 24 || maxCOLS > 80) {
			if (*debugging_font) {
				ctlr_blanks();
				screen_disp();
			}
			ROWS = 24;
			COLS = 80;
		}
	}

	screen_alt = alt;
}


/*
 * Interpret an incoming 3270 command.
 */
int
process_ds(buf, buflen)
unsigned char	*buf;
int	buflen;
{
	if (!buflen)
		return;

	trace_ds("< ");

	switch (buf[0]) {	/* 3270 command */
	case CMD_EAU:	/* erase all unprotected */
	case SNA_CMD_EAU:
		trace_ds("EAU\n");
		do_erase_all_unprotected();
		break;
	case CMD_EWA:	/* erase/write alternate */
	case SNA_CMD_EWA:
		trace_ds("EWA");
		ctlr_erase(True);
		do_write(buf, buflen);
		break;
	case CMD_EW:	/* erase/write */
	case SNA_CMD_EW:
		trace_ds("EW");
		ctlr_erase(False);
		do_write(buf, buflen);
		break;
	case CMD_W:	/* write */
	case SNA_CMD_W:
		trace_ds("W");
		do_write(buf, buflen);
		break;
	case CMD_RB:	/* read buffer */
	case SNA_CMD_RB:
		trace_ds("RB\n");
		do_read_buffer();
		break;
	case CMD_RM:	/* read modifed */
	case SNA_CMD_RM:
		trace_ds("RM\n");
		ctlr_read_modified();
		break;
	case CMD_NOP:	/* no-op */
		trace_ds("NOP\n");
		break;
	default: {
		/* unknown 3270 command */
		char errmsg[48];

		(void) sprintf(errmsg, "Unknown 3270 Data Stream command: 0x%X\n",
		    buf[0]);
		popup_an_error(errmsg);
		return -1;
		}
	}

	return 0;
}


/*
 * Process a 3270 Read-Modified command and transmit the data back to the
 * host.
 */
void
ctlr_read_modified()
{
	register int	baddr, sbaddr;

	trace_ds("> ");
	obptr = &obuf[0];
	if (aid != AID_PA1 && aid != AID_PA2
	    &&  aid != AID_PA3 && aid != AID_CLEAR) {
		if (aid == AID_SYSREQ) {
			*obptr++ = 0x01;	/* soh */
			*obptr++ = 0x5B;	/*  %  */
			*obptr++ = 0x61;	/*  /  */
			*obptr++ = 0x02;	/* stx */
			trace_ds("SYSREQ");
		}
		else {
			*obptr++ = aid;
			*obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
			*obptr++ = code_table[cursor_addr & 0x3F];
			trace_ds(see_aid(aid));
			trace_ds(rcba(cursor_addr));
		}
		baddr = 0;
		if (formatted) {
			/* find first field attribute */
			do {
				if (IS_FA(screen_buf[baddr]))
					break;
				INC_BA(baddr);
			} while (baddr != 0);
			sbaddr = baddr;
			do {
				if (FA_IS_MODIFIED(screen_buf[baddr])) {
					Boolean	any = False;

					INC_BA(baddr);
					*obptr++ = ORDER_SBA;
					*obptr++ = code_table[(baddr >> 6) & 0x3F];
					*obptr++ = code_table[baddr & 0x3F];
					trace_ds(" SBA");
					trace_ds(rcba(baddr));
					do {
						if (screen_buf[baddr]) {
							*obptr++ = cg2ebc[screen_buf[baddr]];
							if (!any)
								trace_ds(" '");
							trace_ds(see_ebc(cg2ebc[screen_buf[baddr]]));
							any = True;
						}
						INC_BA(baddr);
					} while (!IS_FA(screen_buf[baddr]));
					if (any)
						trace_ds("'");
				}
				else {	/* not modified - skip */
					do {
						INC_BA(baddr);
					} while (!IS_FA(screen_buf[baddr]));
				}
			} while (baddr != sbaddr);
		}
		else {
			Boolean	any = False;

			do {
				if (screen_buf[baddr]) {
					*obptr++ = cg2ebc[screen_buf[baddr]];
					if (!any)
						trace_ds("'");
					trace_ds(see_ebc(cg2ebc[screen_buf[baddr]]));
					any = True;
				}
				INC_BA(baddr);
			} while (baddr != 0);
			if (any)
				trace_ds("'");
		}
	}
	else {
		*obptr++ = aid;
		trace_ds(see_aid(aid));
	}
	trace_ds("\n");
	net_output(obuf, obptr - obuf);
}

/*
 * Process a 3270 Read-Buffer command and transmit the data back to the
 * host.
 */
static void
do_read_buffer()
{
	register int	baddr;
	unsigned char	fa;
	Boolean		last_fa = True;

	trace_ds("> ");
	obptr = &obuf[0];
	*obptr++ = aid;
	*obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
	*obptr++ = code_table[cursor_addr & 0x3F];
	trace_ds(see_aid(aid));
	trace_ds(rcba(cursor_addr));
	baddr = 0;
	do {
		if (IS_FA(screen_buf[baddr])) {
			*obptr++ = ORDER_SF;
			fa = 0x00;
			if (FA_IS_PROTECTED(screen_buf[baddr]))
				fa |= 0x20;
			if (FA_IS_NUMERIC(screen_buf[baddr]))
				fa |= 0x10;
			if (FA_IS_MODIFIED(screen_buf[baddr]))
				fa |= 0x01;
			fa |= ((screen_buf[baddr] & FA_INTENSITY) << 2);
			*obptr++ = code_table[fa];
			if (!last_fa)
				trace_ds("'");
			trace_ds(" SF");
			trace_ds(rcba(baddr));
			trace_ds(see_attr(fa));
			last_fa = True;
		} else {
			*obptr++ = cg2ebc[screen_buf[baddr]];
			if (cg2ebc[screen_buf[baddr]] <= 0x3f ||
			    cg2ebc[screen_buf[baddr]] == 0xff) {
				if (!last_fa)
					trace_ds("'");

				trace_ds(" ");
				trace_ds(see_ebc(cg2ebc[screen_buf[baddr]]));
				last_fa = True;
			} else {
				if (last_fa)
					trace_ds(" '");
				trace_ds(see_ebc(cg2ebc[screen_buf[baddr]]));
				last_fa = False;
			}
		}
		INC_BA(baddr);
	} while (baddr != 0);
	if (!last_fa)
		trace_ds("'");
	trace_ds("\n");
	net_output(obuf, obptr - obuf);
}


/*
 * Process a 3270 Erase All Unprotected command.
 */
static void
do_erase_all_unprotected()
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

	screen_changed = True;
	if (formatted) {
		/* find first field attribute */
		baddr = 0;
		do {
			if (IS_FA(screen_buf[baddr]))
				break;
			INC_BA(baddr);
		} while (baddr != 0);
		sbaddr = baddr;
		f = False;
		do {
			fa = screen_buf[baddr];
			if (!FA_IS_PROTECTED(fa)) {
				mdt_clear(&screen_buf[baddr]);
				do {
					INC_BA(baddr);
					if (!f) {
						cursor_move(baddr);
						f = True;
					}
					if (!IS_FA(screen_buf[baddr])) {
						ctlr_add(baddr, CG_null);
					}
				} while (!IS_FA(screen_buf[baddr]));
			}
			else {
				do {
					INC_BA(baddr);
				} while (!IS_FA(screen_buf[baddr]));
			}
		} while (baddr != sbaddr);
		if (!f)
			cursor_move(0);
	} else {
		ctlr_clear();
	}
	aid = AID_NO;
	aid2 = AID_NO;
	key_Reset();
}



/*
 * Process a 3270 Write command.
 */
static void
do_write(buf, buflen)
unsigned char	buf[];
int	buflen;
{
	register unsigned char	*cp;
	register int	baddr;
	unsigned char	*current_fa;
	unsigned char	new_attr;
	Boolean		last_cmd;
	Boolean		last_zpt;
	Boolean		wcc_keyboard_restore, wcc_sound_alarm;
	unsigned char	temp_aid2;
	int		i;
	unsigned char	na;
	int		any_fa;
	char		*paren = "(";
	enum { NONE, ORDER, SBA, TEXT, NULLCH } previous = NONE;

#define END_TEXT(cmd)	{ if (previous == TEXT) trace_ds("'"); \
			  if (cmd != (char *) NULL) { \
			    trace_ds(" "); \
			    trace_ds(cmd); } }

#define START_FIELD(attr) { \
			new_attr = FA_BASE; \
			if ((attr) & 0x20) \
				new_attr |= FA_PROTECT; \
			if ((attr) & 0x10) \
				new_attr |= FA_NUMERIC; \
			if ((attr) & 0x01) \
				new_attr |= FA_MODIFY; \
			new_attr |= ((attr) >> 2) & FA_INTENSITY; \
			current_fa = &(screen_buf[buffer_addr]); \
			ctlr_add(buffer_addr, new_attr); \
			trace_ds(see_attr(new_attr)); \
			formatted = True; \
		}

	if (buflen < 2) {
		trace_ds(" [short buffer, ignored]\n");
		return;
	}

	if (toggled(SCREENTRACE))
		trace_primed = True;
	buffer_addr = cursor_addr;
	wcc_sound_alarm = WCC_SOUND_ALARM(buf[1]);
	if (wcc_sound_alarm) {
		trace_ds(paren);
		trace_ds("alarm");
		paren = ",";
	}
	wcc_keyboard_restore = WCC_KEYBOARD_RESTORE(buf[1]);
	if (wcc_keyboard_restore)
		ticking_stop();
	if (wcc_keyboard_restore) {
		trace_ds(paren);
		trace_ds("restore");
		paren = ",";
	}

	if (WCC_RESET_MDT(buf[1])) {
		trace_ds(paren);
		trace_ds("resetMDT");
		paren = ",";
		baddr = 0;
		screen_changed = True;
		do {
			if (IS_FA(screen_buf[baddr])) {
				mdt_clear(&screen_buf[baddr]);
			}
			INC_BA(baddr);
		} while (baddr != 0);
	}
	if (strcmp(paren, "("))
		trace_ds(")");

	last_cmd = True;
	last_zpt = False;
	current_fa = get_field_attribute(buffer_addr);
	for (cp = &buf[2]; cp < (buf + buflen); cp++) {
		switch (*cp) {
		case ORDER_SF:	/* start field */
			END_TEXT("SF");
			if (previous != SBA)
				trace_ds(rcba(buffer_addr));
			previous = ORDER;
			cp++;		/* skip field attribute */
			START_FIELD(*cp);
			INC_BA(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SBA:	/* set buffer address */
			cp += 2;	/* skip buffer address */
			if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
				buffer_addr = ((*(cp-1) & 0x3F) << 8) | *cp;
			else	/* 12-bit coded */
				buffer_addr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
			END_TEXT("SBA");
			previous = SBA;
			trace_ds(rcba(buffer_addr));
			if (buffer_addr >= COLS * ROWS) {
				trace_ds(" [invalid address, write command terminated]\n");
				return;
			}
			current_fa = get_field_attribute(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_IC:	/* insert cursor */
			END_TEXT("IC");
			previous = ORDER;
			cursor_move(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_PT:	/* program tab */
			END_TEXT("PT");
			previous = ORDER;
			baddr = next_unprotected(buffer_addr);
			if (baddr < buffer_addr)
				baddr = 0;
			/*
			 * Null out the remainder of the current field -- even
			 * if protected -- if the PT doesn't follow a command
			 * or order, or (honestly) if the last order we saw was
			 * a null-filling PT that left the buffer address at 0.
			 */
			if (!last_cmd || last_zpt) {
				trace_ds("(nulling)");
				while ((buffer_addr != baddr) &&
				       (!IS_FA(screen_buf[buffer_addr]))) {
					ctlr_add(buffer_addr, CG_null);
					INC_BA(buffer_addr);
				}
				if (baddr == 0)
					last_zpt = True;
			} else
				last_zpt = False;
			buffer_addr = baddr;
			last_cmd = True;
			break;
		case ORDER_RA:	/* repeat to address */
			END_TEXT("RA");
			cp += 2;	/* skip buffer address */
			if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
				baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
			else	/* 12-bit coded */
				baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
			trace_ds(rcba(baddr));
			cp++;		/* skip char to repeat */
			if (*cp == ORDER_GE) {	/* ignore it */
				trace_ds("GE");
				cp++;
			}
			previous = ORDER;
			if (*cp)
				trace_ds("'");
			trace_ds(see_ebc(*cp));
			if (*cp)
				trace_ds("'");
			if (baddr >= COLS * ROWS) {
				trace_ds(" [invalid address, write command terminated]\n");
				return;
			}
			do {
				ctlr_add(buffer_addr, ebc2cg[*cp]);
				INC_BA(buffer_addr);
			} while (buffer_addr != baddr);
			current_fa = get_field_attribute(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_EUA:	/* erase unprotected to address */
			cp += 2;	/* skip buffer address */
			if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
				baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
			else	/* 12-bit coded */
				baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
			END_TEXT("EUA");
			if (previous != SBA)
				trace_ds(rcba(baddr));
			previous = ORDER;
			if (baddr >= COLS * ROWS) {
				trace_ds(" [invalid address, write command terminated]\n");
				return;
			}
			do {
				if (IS_FA(screen_buf[buffer_addr]))
					current_fa = &(screen_buf[buffer_addr]);
				else if (!FA_IS_PROTECTED(*current_fa)) {
					ctlr_add(buffer_addr, CG_null);
				}
				INC_BA(buffer_addr);
			} while (buffer_addr != baddr);
			current_fa = get_field_attribute(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_GE:	/* graphic escape, ignored */
			END_TEXT("GE");
			previous = ORDER;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_MF:	/* modify field */
			END_TEXT("MF");
			if (previous != SBA)
				trace_ds(rcba(buffer_addr));
			previous = ORDER;
			cp++;
			na = *cp;
			if (IS_FA(screen_buf[buffer_addr])) {
				if (na == 0) {
					INC_BA(buffer_addr);
				} else {
					for (i = 0; i < (int)na; i++) {
						cp++;
						if (*cp == XA_3270) {
							trace_ds(" 3270");
							cp++;
							START_FIELD(*cp);
						} else {
							trace_ds(see_efa(*cp, *(cp + 1)));
							trace_ds("[unsupported]");
							cp++;
						}
					}
				}
			} else
				cp += na * 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SFE:	/* start field extended */
			END_TEXT("SFE");
			if (previous != SBA)
				trace_ds(rcba(buffer_addr));
			previous = ORDER;
			cp++;	/* skip order */
			na = *cp;
			any_fa = 0;
			for (i = 0; i < (int)na; i++) {
				cp++;
				if (*cp == XA_3270) {
					trace_ds(" 3270");
					cp++;
					START_FIELD(*cp);
					any_fa++;
				} else {
					trace_ds(see_efa(*cp, *(cp + 1)));
					trace_ds("[unsupported]");
					cp++;
				}
			}
			if (!any_fa)
				START_FIELD(0);
			INC_BA(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SA:	/* set attribute */
			END_TEXT("SA");
			previous = ORDER;
			trace_ds(see_efa(*(cp + 1), *(cp + 2)));
			trace_ds("[unsupported]");
			cp += 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_SUB:	/* format control orders */
		case FCORDER_DUP:
		case FCORDER_FM:
		case FCORDER_FF:
		case FCORDER_CR:
		case FCORDER_NL:
		case FCORDER_EM:
		case FCORDER_EO:
			END_TEXT(see_ebc(*cp));
			previous = ORDER;
			ctlr_add(buffer_addr, ebc2cg[*cp]);
			INC_BA(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NULL:
			END_TEXT("NULL");
			previous = NULLCH;
			ctlr_add(buffer_addr, ebc2cg[*cp]);
			INC_BA(buffer_addr);
			last_cmd = False;
			last_zpt = False;
			break;
		default:	/* enter character */
			if (*cp <= 0x3F) {
				END_TEXT("ILLEGAL_ORDER");
				trace_ds(see_ebc(*cp));
				last_cmd = True;
				last_zpt = False;
				break;
			}
			if (previous != TEXT)
				trace_ds(" '");
			previous = TEXT;
			trace_ds(see_ebc(*cp));
			ctlr_add(buffer_addr, ebc2cg[*cp]);
			INC_BA(buffer_addr);
			last_cmd = False;
			last_zpt = False;
			break;
		}
	}
	set_formatted();
	END_TEXT((char *)0);
	trace_ds("\n");
	if (wcc_keyboard_restore) {
		aid = AID_NO;
		if (oia_locked) {
			temp_aid2 = aid2;
			key_Reset();

			/* If there is a second AID key pending, process it now */
			if (temp_aid2 != AID_NO)
				key_AID(temp_aid2);
		}
	} else if (oia_twait) {
		oia_twait = False;
		status_syswait();
	}
	if (wcc_sound_alarm)
		ring_bell();

	trace_primed = False;

	ps_process();
}

#undef START_TEXT
#undef END_TEXT

/*
 * Set the pending string.  's' must be NULL or point to XtMalloc'd memory.
 */
void
ps_set(s, is_loginstring)
char *s;
Boolean is_loginstring;
{
	static char *ps_source = (char *) NULL;

	XtFree(ps_source);
	ps_source = (char *) NULL;
	ps_source = s;
	pending_string = s;
	ps_is_loginstring = is_loginstring;
}

/*
 * Process the pending input string
 */
void
ps_process()
{
	int	len;
	int	len_left;

	if (!pending_string) {
		script_continue();
		return;
	}

	/* Add keystrokes only if unlocked on a data field */
	if (IN_3270 && ps_is_loginstring) {
		unsigned char	fa;

		if (!formatted || oia_twait || oia_locked || !cursor_addr)
			return;
		fa = *get_field_attribute(cursor_addr);
		if (FA_IS_PROTECTED(fa))
			return;
	}
	len = strlen(pending_string);
	if (len_left = emulate_input(pending_string, len, False))
		pending_string += len - len_left;
	else {
		ps_set((char *) 0, False);
		script_continue();
	}
}

/*
 * Tell me if there is any data on the screen.
 */
Boolean
ctlr_any_data()
{
	register unsigned char *c = screen_buf;
	register int i;
	register unsigned char oc;

	for (i = 0; i < ROWS*COLS; i++) {
		oc = *c++;
		if (!IsBlank(oc))
			return True;
	}
	return False;
}

/*
 * Clear the text (non-status) portion of the display.  Also resets the cursor
 * and buffer addresses and extended attributes.
 */
void
ctlr_clear()
{
	/* Snap any data that is about to be lost into the trace file. */
	if (toggled(SCREENTRACE) && ctlr_any_data())
		trace_screen();

	/* Clear the screen. */
	(void) memset((char *)screen_buf, 0, ROWS*COLS);
	(void) memset((char *)ea_buf, 0, ROWS*COLS);
	screen_changed = True;
	cursor_move(0);
	buffer_addr = 0;
	(void) unselect(0, ROWS*COLS);
	formatted = False;
}

/*
 * Fill the screen buffer with blanks.
 */
static void
ctlr_blanks()
{
	(void) memset((char *)screen_buf, CG_space, ROWS*COLS);
	screen_changed = True;
	cursor_move(0);
	buffer_addr = 0;
	(void) unselect(0, ROWS*COLS);
	formatted = False;
}


/*
 * Change a character in the 3270 buffer.
 */
void
ctlr_add(baddr, c)
int	baddr;
unsigned char	c;
{
	unsigned char oc;

	if ((oc = screen_buf[baddr]) != c) {
		if (trace_primed && !IsBlank(oc)) {
			trace_screen();
			trace_primed = False;
		}
		if (SELECTED(baddr))
			(void) unselect(baddr, 1);
		screen_changed = True;
		screen_buf[baddr] = c;
	}
}

/*
 * Change the extended attribute for a character in the 3270 buffer.
 */
void
ctlr_add_ea(baddr, ea)
int	baddr;
unsigned char	ea;
{
	if (ea_buf[baddr] != ea) {
		if (SELECTED(baddr))
			(void) unselect(baddr, 1);
		screen_changed = True;
		ea_buf[baddr] = ea;
	}
}

/*
 * Copy a block of characters in the 3270 buffer, optionally including the
 * extended attributes.
 */
void
ctlr_bcopy(baddr_from, baddr_to, count, move_ea)
int	baddr_from;
int	baddr_to;
int	count;
int	move_ea;
{
	if (memcmp((char *) &screen_buf[baddr_from],
	           (char *) &screen_buf[baddr_to],
		   count)) {
		(void) MEMORY_MOVE((char *) &screen_buf[baddr_to],
			           (char *) &screen_buf[baddr_from],
			           count);
		screen_changed = True;
		/*
		 * For the time being, if any selected text shifts around on
		 * the screen, unhighlight it.  Eventually there should be
		 * logic for preserving the highlight if the *all* of the
		 * selected text moves.
		 */
		if (area_is_selected(baddr_to, count))
			(void) unselect(baddr_to, count);
	}
	if (move_ea && memcmp((char *) &ea_buf[baddr_from],
			      (char *) &ea_buf[baddr_to],
			      count)) {
		(void) MEMORY_MOVE((char *) &ea_buf[baddr_to],
		                   (char *) &ea_buf[baddr_from],
			           count);
		screen_changed = True;
	}
}

/*
 * Erase a region of the 3270 buffer, optionally clearing extended attributes
 * as well.
 */
void
ctlr_aclear(baddr, count, clear_ea)
int	baddr;
int	count;
int	clear_ea;
{
	if (memcmp((char *) &screen_buf[baddr], (char *) zero_buf, count)) {
		(void) memset((char *) &screen_buf[baddr], 0, count);
		screen_changed = True;
		if (area_is_selected(baddr, count))
			(void) unselect(baddr, count);
	}
	if (clear_ea && memcmp((char *) &ea_buf[baddr], (char *) zero_buf, count)) {
		(void) memset((char *) &ea_buf[baddr], 0, count);
		screen_changed = True;
	}
}

/*
 * Swap the regular and alternate screen buffers
 */
void
ctlr_altbuffer(alt)
Boolean	alt;
{
	unsigned char *tmp;
#define	SWAP(a, b)	tmp = a; a = b; b = tmp

	if (alt != is_altbuffer) {
		SWAP(screen_buf, ascreen_buf);
		SWAP(ea_buf, aea_buf);
		is_altbuffer = alt;
		screen_changed = True;
		(void) unselect(0, ROWS*COLS);
	}
}
#undef SWAP


/*
 * Set or clear the MDT on an attribute
 */
void
mdt_set(fa)
unsigned char *fa;
{
	if (*fa & FA_MODIFY)
		return;
	*fa |= FA_MODIFY;
	screen_changed = True;
}

void
mdt_clear(fa)
unsigned char *fa;
{
	if (!(*fa & FA_MODIFY))
		return;
	*fa &= ~FA_MODIFY;
	screen_changed = True;
}


/*
 * Transaction timing.  The time between sending an interrupt (PF, PA, Enter,
 * Clear) and the host unlocking the keyboard is indicated on the status line
 * to an accuracy of 0.1 seconds.  If we don't repaint the screen before we see
 * the unlock, the time should be fairly accurate.
 */
static struct timeval t_start;
static Boolean ticking = False;
static XtIntervalId tick_id;
static struct timeval t_want;

/* Return the difference in milliseconds between two timevals. */
static long
delta_msec(t1, t0)
struct timeval *t1, *t0;
{
	return (t1->tv_sec - t0->tv_sec) * 1000 +
	       (t1->tv_usec - t0->tv_usec + 500) / 1000;
}

/*ARGSUSED*/
static void
keep_ticking(closure, id)
XtPointer closure;
XtIntervalId *id;
{
	struct timeval t1;
	long msec;

	do {
		(void) gettimeofday(&t1, (struct timezone *) 0);
		t_want.tv_sec++;
		msec = delta_msec(&t_want, &t1);
	} while (msec <= 0);
	tick_id = XtAppAddTimeOut(appcontext, msec, keep_ticking, 0);
	status_timing(&t_start, &t1);
}

void
ticking_start(anyway)
Boolean anyway;
{
	if (!toggled(TIMING) && !anyway)
		return;
	status_untiming();
	if (ticking)
		XtRemoveTimeOut(tick_id);
	ticking = True;
	(void) gettimeofday(&t_start, (struct timezone *) 0);
	tick_id = XtAppAddTimeOut(appcontext, 1000, keep_ticking, 0);
	t_want = t_start;
}

void
ticking_stop()
{
	struct timeval t1;

	if (!ticking)
		return;
	XtRemoveTimeOut(tick_id);
	(void) gettimeofday(&t1, (struct timezone *) 0);
	ticking = False;
	status_timing(&t_start, &t1);
}

/*ARGSUSED*/
void
toggle_timing(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	if (!toggled(TIMING))
		status_untiming();
}


/*
 * No-op toggle.
 */
/*ARGSUSED*/
void
toggle_nop(t, tt)
struct toggle *t;
enum toggle_type tt;
{
}
