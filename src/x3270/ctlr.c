/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 * Copyright 1990 Jeff Sparkes.
 * Copyright 1993 Paul Mattes.
 *
 *                         All Rights Reserved
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
#include "3270.h"
#include "3270_enc.h"

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
static void	trace_ds();

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


/* display a (row,col) */
char *
rcba(baddr)
int baddr;
{
	static char buf[16];

	(void) sprintf(buf, "(%d,%d)", baddr/COLS, baddr%COLS);
	return buf;
}

static char *
see_ebc(ch)
unsigned char ch;
{
	static char buf[8];

	switch (ch) {
	    case FCORDER_NULL:
		return "NULL";
	    case FCORDER_SUB:
		return "SUB";
	    case FCORDER_DUP:
		return "DUP";
	    case FCORDER_FM:
		return "FM";
	    case FCORDER_FF:
		return "FF";
	    case FCORDER_CR:
		return "CR";
	    case FCORDER_NL:
		return "NL";
	    case FCORDER_EM:
		return "EM";
	    case FCORDER_EO:
		return "EO";
	}
	if (ebc2asc[ch])
		(void) sprintf(buf, "%c", ebc2asc[ch]);
	else
		(void) sprintf(buf, "\\%o", ch);
	return buf;
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
 * Find the field attribute for the given buffer address.  Return its address
 * rather than its value.
 */
unsigned char *
get_field_attribute(baddr)
register int	baddr;
{
	static unsigned char	fake_fa;
	int	sbaddr;

	sbaddr = baddr;
	do {
		if (IS_FA(screen_buf[baddr]))
			return &(screen_buf[baddr]);
		DEC_BA(baddr);
	} while (baddr != sbaddr);
	if (ansi_host)
		fake_fa = 0xC4;
	else
		fake_fa = 0xE0;
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

	/*
	 * Before changing the screen dimensions, synchronize the screen image.
	 */
	screen_disp();

	if (alt) {
		/* Going from 24x80 to maximum. */
		ROWS = maxROWS;
		COLS = maxCOLS;
	} else {
		/* Going from maximum to 24x80. */
		if (maxROWS > 24)
			ROWS = 24;
		if (maxCOLS > 80)
			COLS = 80;
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


static char *
see_aid(code)
unsigned char code;
{
	switch (code) {
	case AID_NO: 
		return "NoAID";
	case AID_ENTER: 
		return "Enter";
	case AID_PF1: 
		return "PF1";
	case AID_PF2: 
		return "PF2";
	case AID_PF3: 
		return "PF3";
	case AID_PF4: 
		return "PF4";
	case AID_PF5: 
		return "PF5";
	case AID_PF6: 
		return "PF6";
	case AID_PF7: 
		return "PF7";
	case AID_PF8: 
		return "PF8";
	case AID_PF9: 
		return "PF9";
	case AID_PF10: 
		return "PF10";
	case AID_PF11: 
		return "PF11";
	case AID_PF12: 
		return "PF12";
	case AID_PF13: 
		return "PF13";
	case AID_PF14: 
		return "PF14";
	case AID_PF15: 
		return "PF15";
	case AID_PF16: 
		return "PF16";
	case AID_PF17: 
		return "PF17";
	case AID_PF18: 
		return "PF18";
	case AID_PF19: 
		return "PF19";
	case AID_PF20: 
		return "PF20";
	case AID_PF21: 
		return "PF21";
	case AID_PF22: 
		return "PF22";
	case AID_PF23: 
		return "PF23";
	case AID_PF24: 
		return "PF24";
	case AID_OICR: 
		return "OICR";
	case AID_MSR_MHS: 
		return "MSR_MHS";
	case AID_SELECT: 
		return "Select";
	case AID_PA1: 
		return "PA1";
	case AID_PA2: 
		return "PA2";
	case AID_PA3: 
		return "PA3";
	case AID_CLEAR: 
		return "Clear";
	case AID_SYSREQ: 
		return "SysReq";
	default: 
		return "???";
	}
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


static char *
see_attr(fa)
unsigned char fa;
{
	static char buf[256];
	char *paren = "(";

	buf[0] = '\0';

	if (fa & 0x04) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "protected");
		paren = ",";
		if (fa & 0x08) {
			(void) strcat(buf, paren);
			(void) strcat(buf, "skip");
			paren = ",";
		}
	} else if (fa & 0x08) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "numeric");
		paren = ",";
	}
	switch (fa & 0x03) {
	case 0:
		break;
	case 1:
		(void) strcat(buf, paren);
		(void) strcat(buf, "detectable");
		paren = ",";
		break;
	case 2:
		(void) strcat(buf, paren);
		(void) strcat(buf, "intensified");
		paren = ",";
		break;
	case 3:
		(void) strcat(buf, paren);
		(void) strcat(buf, "nondisplay");
		paren = ",";
		break;
	}
	if (fa & 0x20) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "modified");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");

	return buf;
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
						ctlr_add(baddr, CG_NULLBLANK);
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
	char		*paren = "(";

#define START_TEXT	{ if (last_cmd) trace_ds(" '"); }
#define END_TEXT(cmd)	{ if (!last_cmd) trace_ds("'"); \
			  if (cmd) { trace_ds(" "); trace_ds(cmd); } }

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
			trace_ds(rcba(buffer_addr));
			cp++;		/* skip field attribute */
			new_attr = FA_BASE;
			if (*cp & 0x20)
				new_attr |= FA_PROTECT;
			if (*cp & 0x10)
				new_attr |= FA_NUMERIC;
			if (*cp & 0x01)
				new_attr |= FA_MODIFY;
			new_attr |= (*cp >> 2) & FA_INTENSITY;
			current_fa = &(screen_buf[buffer_addr]);
			ctlr_add(buffer_addr, new_attr);
			trace_ds(see_attr(new_attr));
			formatted = True;
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
			cursor_move(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_PT:	/* program tab */
			END_TEXT("PT");
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
					ctlr_add(buffer_addr, CG_NULLBLANK);
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
			cp += 2;	/* skip buffer address */
			if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
				baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
			else	/* 12-bit coded */
				baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
			cp++;		/* skip char to repeat */
			if (*cp == ORDER_GE)
				cp++;
			END_TEXT("RA");
			trace_ds(rcba(baddr));
			trace_ds("'");
			trace_ds(see_ebc(*cp));
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
			trace_ds(rcba(baddr));
			if (baddr >= COLS * ROWS) {
				trace_ds(" [invalid address, write command terminated]\n");
				return;
			}
			do {
				if (IS_FA(screen_buf[buffer_addr]))
					current_fa = &(screen_buf[buffer_addr]);
				else if (!FA_IS_PROTECTED(*current_fa)) {
					ctlr_add(buffer_addr, CG_NULLBLANK);
				}
				INC_BA(buffer_addr);
			} while (buffer_addr != baddr);
			current_fa = get_field_attribute(buffer_addr);
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_GE:	/* graphic escape */
			END_TEXT("GE[unsupported]");
			cp++;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_MF:	/* modify field */
			END_TEXT("MF[unsupported]");
			cp += *(cp + 1) * 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SFE:	/* start field extended */
			END_TEXT("SFE[unsupported]");
			cp += *(cp + 1) * 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case ORDER_SA:	/* set attribute */
			END_TEXT("SA[unsupported]");
			cp += 2;
			last_cmd = True;
			last_zpt = False;
			break;
		case FCORDER_NULL:	/* format control orders */
		case FCORDER_SUB:
		case FCORDER_DUP:
		case FCORDER_FM:
		case FCORDER_FF:
		case FCORDER_CR:
		case FCORDER_NL:
		case FCORDER_EM:
		case FCORDER_EO:
			END_TEXT(see_ebc(*cp));
			ctlr_add(buffer_addr, ebc2cg[*cp]);
			INC_BA(buffer_addr);
			last_cmd = True;
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
			START_TEXT;
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

	ps_process();
}

#undef START_TEXT
#undef END_TEXT

/*
 * Set the pending string.  's' must be NULL or point to XtMalloc'd memory.
 */
void
ps_set(s)
char *s;
{
	static char *ps_source = (char *) NULL;

	XtFree(ps_source);
	ps_source = (char *) NULL;
	ps_source = s;
	pending_string = s;
}

/*
 * Process the pending input string
 */
void
ps_process()
{
	int	len;
	int	len_left;

	if (!pending_string)
		return;
	if (IN_3270) {	/* Add keystrokes only if unlocked on a data field */
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
	else
		ps_set((char *) 0);
}

/*
 * Clear the text (non-status) portion of the display.  Also resets the cursor
 * and buffer addresses and extended attributes.
 */
void
ctlr_clear()
{
	(void) memset((char *)screen_buf, 0, ROWS*COLS);
	(void) memset((char *)ea_buf, 0, ROWS*COLS);
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
	if (screen_buf[baddr] != c) {
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
static struct timeval t0;
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
	status_timing(&t0, &t1);
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
	(void) gettimeofday(&t0, (struct timezone *) 0);
	tick_id = XtAppAddTimeOut(appcontext, 1000, keep_ticking, 0);
	t_want = t0;
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
	status_timing(&t0, &t1);
}

void
toggle_timing()
{
	if (!toggled(TIMING))
		status_untiming();
}


/*
 * Toggle TELNET tracing
 */
void
toggle_tracetn()
{
}

/*
 * Placeholder for no-op toggle.
 */
void
toggle_nop()
{
}


/*
 * 3270 Data Stream Tracing
 */
static int dscnt = 0;

static void
trace_ds(s)
char *s;
{
	int len = strlen(s);
	Boolean nl = False;

	if (!toggled(TRACE3270))
		return;

	if (s && s[len-1] == '\n') {
		len--;
		nl = True;
	}
	while (dscnt + len >= 72) {
		int plen = 72-dscnt;

		(void) printf("%.*s ...\n... ", plen, s);
		dscnt = 4;
		s += plen;
		len -= plen;
	}
	if (len) {
		(void) printf("%.*s", len, s);
		dscnt += len;
	}
	if (nl) {
		(void) printf("\n");
		dscnt = 0;
	}
}
