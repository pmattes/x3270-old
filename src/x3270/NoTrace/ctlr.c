/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994, 1995 by Paul Mattes.
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

#include "globals.h"
#include <sys/types.h>
#if !defined(NO_SYS_TIME_H) /*[*/
#include <sys/time.h>
#endif /*]*/
#include <errno.h>
#if !defined(NO_MEMORY_H) /*[*/
#include <memory.h>
#endif /*]*/
#include "3270ds.h"
#include "screen.h"
#include "cg.h"

#define IsBlank(c)	((c == CG_null) || (c == CG_space))

/* Externals: x3270.c */
extern char	*pending_string;

/* Externals: screen.c */
extern unsigned char	*selected;	/* selection bitmap */

/* Externals: kybd.c */
extern unsigned char	aid;

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
struct ea	*ea_buf;	/* 3270 extended attribute buffer */
unsigned char	*ascreen_buf;	/* alternate 3270 display buffer */
struct ea	*aea_buf;	/* alternate 3270 extended attribute buffer */
unsigned char	*zero_buf;	/* empty buffer, for area clears */
Boolean		formatted = False;	/* set in screen_disp */
Boolean		screen_changed = False;
unsigned char	reply_mode = SF_SRM_FIELD;
int		crm_nattr = 0;
unsigned char	crm_attr[16];

/* Statics */
static void	set_formatted();
static void	ctlr_blanks();
static unsigned char	fake_fa;
static struct ea	fake_ea;
static Boolean	ps_is_loginstring = False;
static Boolean	trace_primed = False;
static unsigned char	default_fg;
static unsigned char	default_gr;

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
		ea_buf = (struct ea *)XtCalloc(sizeof(struct ea), maxROWS * maxCOLS);
		if (ascreen_buf)
			XtFree((char *)ascreen_buf);
		ascreen_buf = (unsigned char *)XtCalloc(sizeof(unsigned char), maxROWS * maxCOLS);
		if (aea_buf)
			XtFree((char *)aea_buf);
		aea_buf = (struct ea *)XtCalloc(sizeof(struct ea), maxROWS * maxCOLS);
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
	char *defmod;

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
	case 4:
#if defined(RESTRICT_3279) /*[*/
		if (appres.m3279) {
			XtWarning("No 3279 Model 4, defaulting to Model 3");
			set_rows_cols("3");
			return;
		}
#endif /*]*/
		maxROWS = ROWS = 43; 
		maxCOLS = COLS = 80;
		model_num = 4;
		break;
	case 5:
#if defined(RESTRICT_3279) /*[*/
		if (appres.m3279) {
			XtWarning("No 3279 Model 5, defaulting to Model 3");
			set_rows_cols("3");
			return;
		}
#endif /*]*/
		maxROWS = ROWS = 27; 
		maxCOLS = COLS = 132;
		model_num = 5;
		break;
	default:
#if defined(RESTRICT_3279) /*[*/
		defmod = appres.m3279 ? "3" : "4";
#else /*][*/
		defmod = "4";
#endif
		xs_warning("Unknown model, defaulting to %s", defmod);
		set_rows_cols(defmod);
		return;
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

	default_fg = 0x00;
	default_gr = 0x00;
	reply_mode = SF_SRM_FIELD;
	crm_nattr = 0;
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

	if (!formatted)
		return &fake_fa;

	sbaddr = baddr;
	do {
		if (IS_FA(screen_buf[baddr]))
			return &(screen_buf[baddr]);
		DEC_BA(baddr);
	} while (baddr != sbaddr);
	return &fake_fa;
}

/*
 * Find the field attribute for the given buffer address, bounded by another
 * buffer address.  Return the attribute in a parameter.
 *
 * Returns True if an attribute is found, False if boundary hit.
 */
Boolean
get_bounded_field_attribute(baddr, bound, fa_out)
register int	baddr;
register int	bound;
unsigned char	*fa_out;
{
	int	sbaddr;

	if (!formatted) {
		*fa_out = fake_fa;
		return True;
	}

	sbaddr = baddr;
	do {
		if (IS_FA(screen_buf[baddr])) {
			*fa_out = screen_buf[baddr];
			return True;
		}
		DEC_BA(baddr);
	} while (baddr != sbaddr && baddr != bound);

	/* Screen is unformatted (and 'formatted' is inaccurate). */
	if (baddr == sbaddr) {
		*fa_out = fake_fa;
		return True;
	}

	/* Wrapped to boundary. */
	return False;
}

/*
 * Given the address of a field attribute, return the address of the
 * extended attribute structure.
 */
struct ea *
fa2ea(fa)
unsigned char *fa;
{
	if (fa == &fake_fa)
		return &fake_ea;
	else
		return &ea_buf[fa - screen_buf];
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
	kybd_inhibit(False);

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
		return 0;

	scroll_to_bottom();

	switch (buf[0]) {	/* 3270 command */
	case CMD_EAU:	/* erase all unprotected */
	case SNA_CMD_EAU:
		ctlr_erase_all_unprotected();
		break;
	case CMD_EWA:	/* erase/write alternate */
	case SNA_CMD_EWA:
		ctlr_erase(True);
		ctlr_write(buf, buflen, True);
		break;
	case CMD_EW:	/* erase/write */
	case SNA_CMD_EW:
		ctlr_erase(False);
		ctlr_write(buf, buflen, True);
		break;
	case CMD_W:	/* write */
	case SNA_CMD_W:
		ctlr_write(buf, buflen, False);
		break;
	case CMD_RB:	/* read buffer */
	case SNA_CMD_RB:
		ctlr_read_buffer(aid);
		break;
	case CMD_RM:	/* read modifed */
	case SNA_CMD_RM:
		ctlr_read_modified(aid, False);
		break;
	case CMD_RMA:	/* read modifed all */
	case SNA_CMD_RMA:
		ctlr_read_modified(aid, True);
		break;
	case CMD_WSF:	/* write structured field */
	case SNA_CMD_WSF:
		write_structured_field(buf, buflen);
		break;
	case CMD_NOP:	/* no-op */
		break;
	default:
		/* unknown 3270 command */
		popup_an_error("Unknown 3270 Data Stream command: 0x%X\n",
		    buf[0]);
		return -1;
	}

	return 0;
}

/*
 * Functions to insert SA attributes into the inbound data stream.
 */
static void
insert_sa1(attr, value, currentp)
unsigned char attr;
unsigned char value;
unsigned char *currentp;
{
	if (value == *currentp)
		return;
	*currentp = value;
	*obptr++ = ORDER_SA;
	*obptr++ = attr;
	*obptr++ = value;
}

static void
insert_sa(baddr, current_fgp, current_grp)
int baddr;
unsigned char *current_fgp;
unsigned char *current_grp;
{
	if (reply_mode != SF_SRM_CHAR)
		return;

	if (memchr((char *)crm_attr, XA_FOREGROUND, crm_nattr))
		insert_sa1(XA_FOREGROUND, ea_buf[baddr].fg, current_fgp);
	if (memchr((char *)crm_attr, XA_HIGHLIGHTING, crm_nattr)) {
		unsigned char gr;

		gr = ea_buf[baddr].gr;
		if (gr)
			gr |= 0xf0;
		insert_sa1(XA_HIGHLIGHTING, gr, current_grp);
	}
}


/*
 * Process a 3270 Read-Modified command and transmit the data back to the
 * host.
 */
void
ctlr_read_modified(aid_byte, all)
unsigned char aid_byte;
Boolean all;
{
	register int	baddr, sbaddr;
	Boolean		send_data = True;
	Boolean		short_read = False;
	unsigned char	current_fg = 0x00;
	unsigned char	current_gr = 0x00;

	obptr = &obuf[0];

	switch (aid_byte) {

	    case AID_SYSREQ:			/* test request */
		*obptr++ = 0x01;	/* soh */
		*obptr++ = 0x5b;	/*  %  */
		*obptr++ = 0x61;	/*  /  */
		*obptr++ = 0x02;	/* stx */
		break;

	    case AID_PA1:			/* short-read AIDs */
	    case AID_PA2:
	    case AID_PA3:
	    case AID_CLEAR:
		short_read = True;
		/* fall through... */

	    case AID_NO:			/* AIDs that send no data */
	    case AID_SELECT:			/* on READ MODIFIED */
		if (!all)
			send_data = False;
		/* fall through... */

	    default:				/* ordinary AID */
		*obptr++ = aid_byte;
		if (short_read)
		    goto rm_done;
		*obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
		*obptr++ = code_table[cursor_addr & 0x3F];
		break;
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
				INC_BA(baddr);
				*obptr++ = ORDER_SBA;
				*obptr++ = code_table[(baddr >> 6) & 0x3F];
				*obptr++ = code_table[baddr & 0x3F];
				while (!IS_FA(screen_buf[baddr])) {
					if (send_data &&
					    screen_buf[baddr]) {
						insert_sa(baddr,
						    &current_fg, &current_gr);
						*obptr++ = cg2ebc[screen_buf[baddr]];
					}
					INC_BA(baddr);
				}
			}
			else {	/* not modified - skip */
				do {
					INC_BA(baddr);
				} while (!IS_FA(screen_buf[baddr]));
			}
		} while (baddr != sbaddr);
	} else {
		do {
			if (screen_buf[baddr]) {
				insert_sa(baddr,
				    &current_fg, &current_gr);
				*obptr++ = cg2ebc[screen_buf[baddr]];
			}
			INC_BA(baddr);
		} while (baddr != 0);
	}

    rm_done:
	net_output(obuf, obptr - obuf);
}

/*
 * Calculate the proper 3270 DS value for an internal field attribute.
 */
static unsigned char
calc_fa(fa)
unsigned char fa;
{
	register unsigned char r = 0x00;

	if (FA_IS_PROTECTED(fa))
		r |= 0x20;
	if (FA_IS_NUMERIC(fa))
		r |= 0x10;
	if (FA_IS_MODIFIED(fa))
		r |= 0x01;
	r |= ((fa & FA_INTENSITY) << 2);
	return r;
}

/*
 * Process a 3270 Read-Buffer command and transmit the data back to the
 * host.
 */
void
ctlr_read_buffer(aid_byte)
unsigned char aid_byte;
{
	register int	baddr;
	unsigned char	fa;
	unsigned char	*attr_count;
	unsigned char	current_fg = 0x00;
	unsigned char	current_gr = 0x00;

	obptr = &obuf[0];

	*obptr++ = aid_byte;
	*obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
	*obptr++ = code_table[cursor_addr & 0x3F];

	baddr = 0;
	do {
		if (IS_FA(screen_buf[baddr])) {
			if (reply_mode == SF_SRM_FIELD)
				*obptr++ = ORDER_SF;
			else {
				*obptr++ = ORDER_SFE;
				attr_count = obptr;
				*obptr++ = 1; /* for now */
				*obptr++ = XA_3270;
			}
			fa = calc_fa(screen_buf[baddr]);
			*obptr++ = code_table[fa];
			if (reply_mode != SF_SRM_FIELD) {
				if (ea_buf[baddr].fg) {
					*obptr++ = XA_FOREGROUND;
					*obptr++ = ea_buf[baddr].fg;
					(*attr_count)++;
				}
				if (ea_buf[baddr].gr) {
					*obptr++ = XA_HIGHLIGHTING;
					*obptr++ = ea_buf[baddr].gr | 0xf0;
					(*attr_count)++;
				}
			}
		} else {
			insert_sa(baddr, &current_fg, &current_gr);
			*obptr++ = cg2ebc[screen_buf[baddr]];
		}
		INC_BA(baddr);
	} while (baddr != 0);

	net_output(obuf, obptr - obuf);
}

/*
 * Construct a 3270 command to reproduce the current state of the display.
 */
void
ctlr_snap_buffer()
{
	register int	baddr = 0;
	unsigned char	*attr_count;
	unsigned char	current_fg = 0x00;
	unsigned char	current_gr = 0x00;
	unsigned char   av;

	obptr = &obuf[0];
	*obptr++ = screen_alt ? CMD_EWA : CMD_EW;
	*obptr++ = code_table[0];

	do {
		if (IS_FA(screen_buf[baddr])) {
			*obptr++ = ORDER_SFE;
			attr_count = obptr;
			*obptr++ = 1; /* for now */
			*obptr++ = XA_3270;
			*obptr++ = code_table[calc_fa(screen_buf[baddr])];
			if (ea_buf[baddr].fg) {
				*obptr++ = XA_FOREGROUND;
				*obptr++ = ea_buf[baddr].fg;
				(*attr_count)++;
			}
			if (ea_buf[baddr].gr) {
				*obptr++ = XA_HIGHLIGHTING;
				*obptr++ = ea_buf[baddr].gr | 0xf0;
				(*attr_count)++;
			}
		} else {
			av = ea_buf[baddr].fg;
			if (current_fg != av) {
				current_fg = av;
				*obptr++ = ORDER_SA;
				*obptr++ = XA_FOREGROUND;
				*obptr++ = av;
			}
			av = ea_buf[baddr].gr;
			if (av)
				av |= 0xf0;
			if (current_gr != av) {
				current_gr = av;
				*obptr++ = ORDER_SA;
				*obptr++ = XA_HIGHLIGHTING;
				*obptr++ = av;
			}
			*obptr++ = cg2ebc[screen_buf[baddr]];
		}
		INC_BA(baddr);
	} while (baddr != 0);

	*obptr++ = ORDER_SBA;
	*obptr++ = code_table[(cursor_addr >> 6) & 0x3F];
	*obptr++ = code_table[cursor_addr & 0x3F];
	*obptr++ = ORDER_IC;
}

/*
 * Construct a 3270 command to reproduce the reply mode.
 * Returns a Boolean indicating if one is necessary.
 */
Boolean
ctlr_snap_modes()
{
	int i;

	if (!IN_3270 || reply_mode == SF_SRM_FIELD)
		return False;

	obptr = &obuf[0];
	*obptr++ = CMD_WSF;
	*obptr++ = 0x00;	/* implicit length */
	*obptr++ = 0x00;
	*obptr++ = SF_SET_REPLY_MODE;
	*obptr++ = 0x00;	/* partition 0 */
	*obptr++ = reply_mode;
	if (reply_mode == SF_SRM_CHAR)
		for (i = 0; i < crm_nattr; i++)
			*obptr++ = crm_attr[i];
	return True;
}


/*
 * Process a 3270 Erase All Unprotected command.
 */
void
ctlr_erase_all_unprotected()
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

	kybd_inhibit(False);

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
	do_reset(False);
}



/*
 * Process a 3270 Write command.
 */
void
ctlr_write(buf, buflen, erase)
unsigned char	buf[];
int	buflen;
Boolean erase;
{
	register unsigned char	*cp;
	register int	baddr;
	unsigned char	*current_fa;
	unsigned char	new_attr;
	Boolean		last_cmd;
	Boolean		last_zpt;
	Boolean		wcc_keyboard_restore, wcc_sound_alarm;
	int		i;
	unsigned char	na;
	int		any_fa;
	unsigned char	efa_fg;
	unsigned char	efa_gr;

#define START_FIELDx(fa) { \
			current_fa = &(screen_buf[buffer_addr]); \
			ctlr_add(buffer_addr, fa); \
			ctlr_add_color(buffer_addr, 0); \
			ctlr_add_gr(buffer_addr, 0); \
			formatted = True; \
		}
#define START_FIELD0	{ START_FIELDx(FA_BASE); }
#define START_FIELD(attr) { \
			new_attr = FA_BASE; \
			if ((attr) & 0x20) \
				new_attr |= FA_PROTECT; \
			if ((attr) & 0x10) \
				new_attr |= FA_NUMERIC; \
			if ((attr) & 0x01) \
				new_attr |= FA_MODIFY; \
			new_attr |= ((attr) >> 2) & FA_INTENSITY; \
			START_FIELDx(new_attr); \
		}

	kybd_inhibit(False);

	if (buflen < 2)
		return;

	default_fg = 0;
	default_gr = 0;
	trace_primed = True;
	buffer_addr = cursor_addr;
	if (WCC_RESET(buf[1]) && erase)
		reply_mode = SF_SRM_FIELD;
	wcc_sound_alarm = WCC_SOUND_ALARM(buf[1]);
	wcc_keyboard_restore = WCC_KEYBOARD_RESTORE(buf[1]);
	if (wcc_keyboard_restore)
		ticking_stop();

	if (WCC_RESET_MDT(buf[1])) {
		baddr = 0;
		screen_changed = True;
		do {
			if (IS_FA(screen_buf[baddr]))
				mdt_clear(&screen_buf[baddr]);
			INC_BA(baddr);
		} while (baddr != 0);
	}

	last_cmd = True;
	last_zpt = False;
	current_fa = get_field_attribute(buffer_addr);
	for (cp = &buf[2]; cp < (buf + buflen); cp++) {
		switch (*cp) {
		case ORDER_SF:	/* start field */
		    cp++;		/* skip field attribute */
		    START_FIELD(*cp);
		    ctlr_add_color(buffer_addr, 0);
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
		    if (buffer_addr >= COLS * ROWS)
			    return;
		    current_fa = get_field_attribute(buffer_addr);
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_IC:	/* insert cursor */
		    cursor_move(buffer_addr);
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_PT:	/* program tab */
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
		    cp += 2;	/* skip buffer address */
		    if ((*(cp-1) & 0xC0) == 0x00) /* 14-bit binary */
			    baddr = ((*(cp-1) & 0x3F) << 8) | *cp;
		    else	/* 12-bit coded */
			    baddr = ((*(cp-1) & 0x3F) << 6) | (*cp & 0x3F);
		    cp++;		/* skip char to repeat */
		    if (*cp == ORDER_GE) 	/* ignore it */
			    cp++;
		    if (baddr >= COLS * ROWS)
			    return;
		    do {
			    ctlr_add(buffer_addr, ebc2cg[*cp]);
			    ctlr_add_color(buffer_addr, default_fg);
			    ctlr_add_gr(buffer_addr, default_gr);
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
		    if (baddr >= COLS * ROWS)
			    return;
		    do {
			    if (IS_FA(screen_buf[buffer_addr]))
				    current_fa = &(screen_buf[buffer_addr]);
			    else if (!FA_IS_PROTECTED(*current_fa))
				    ctlr_add(buffer_addr, CG_null);
			    INC_BA(buffer_addr);
		    } while (buffer_addr != baddr);
		    current_fa = get_field_attribute(buffer_addr);
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_GE:	/* graphic escape, ignored */
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_MF:	/* modify field */
		    cp++;
		    na = *cp;
		    if (IS_FA(screen_buf[buffer_addr])) {
			    if (na == 0) {
				    INC_BA(buffer_addr);
			    } else {
				    for (i = 0; i < (int)na; i++) {
					    cp++;
					    switch (*cp) {
						case XA_3270:
						    cp++;
						    START_FIELD(*cp);
						    break;
						case XA_FOREGROUND:
						    cp++;
						    if (appres.m3279)
							    ctlr_add_color(buffer_addr, *cp);
						    break;
						case XA_HIGHLIGHTING:
						    cp++;
						    ctlr_add_gr(buffer_addr, *cp & 0x07);
						    break;
						default:
						    cp++;
						    break;
					    }
				    }
			    }
			    INC_BA(buffer_addr);
		    } else
			    cp += na * 2;
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_SFE:	/* start field extended */
		    cp++;	/* skip order */
		    na = *cp;
		    any_fa = 0;
		    efa_fg = 0;
		    efa_gr = 0;
		    for (i = 0; i < (int)na; i++) {
			    cp++;
			    switch (*cp) {
				case XA_3270:
				    cp++;
				    START_FIELD(*cp);
				    any_fa++;
				    break;
				case XA_FOREGROUND:
				    cp++;
				    if (appres.m3279)
					    efa_fg = *cp;
				    break;
				case XA_HIGHLIGHTING:
				    cp++;
				    efa_gr = *cp & 0x07;
				    break;
				default:
				    cp++;
				    break;
			    }
		    }
		    if (!any_fa)
			    START_FIELD0;
		    ctlr_add_color(buffer_addr, efa_fg);
		    ctlr_add_gr(buffer_addr, efa_gr);
		    INC_BA(buffer_addr);
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case ORDER_SA:	/* set attribute */
		    cp++;
		    if (*cp == XA_FOREGROUND)  {
			    if (appres.m3279)
				    default_fg = *(cp + 1);
		    } else if (*cp == XA_HIGHLIGHTING)  {
			    default_gr = *(cp + 1) & 0x07;
		    } else if (*cp == XA_ALL)  {
			    default_fg = 0;
			    default_gr = 0;
		    }
		    cp++;
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
		    ctlr_add(buffer_addr, ebc2cg[*cp]);
		    ctlr_add_color(buffer_addr, default_fg);
		    ctlr_add_gr(buffer_addr, default_gr);
		    INC_BA(buffer_addr);
		    last_cmd = True;
		    last_zpt = False;
		    break;
		case FCORDER_NULL:
		    ctlr_add(buffer_addr, ebc2cg[*cp]);
		    ctlr_add_color(buffer_addr, default_fg);
		    ctlr_add_gr(buffer_addr, default_gr);
		    INC_BA(buffer_addr);
		    last_cmd = False;
		    last_zpt = False;
		    break;
		default:	/* enter character */
		    if (*cp <= 0x3F) {
			    last_cmd = True;
			    last_zpt = False;
			    break;
		    }
		    ctlr_add(buffer_addr, ebc2cg[*cp]);
		    ctlr_add_color(buffer_addr, default_fg);
		    ctlr_add_gr(buffer_addr, default_gr);
		    INC_BA(buffer_addr);
		    last_cmd = False;
		    last_zpt = False;
		    break;
		}
	}
	set_formatted();
	if (wcc_keyboard_restore) {
		aid = AID_NO;
		do_reset(False);
	} else if (kybdlock & KL_OIA_TWAIT) {
		kybdlock_clr(KL_OIA_TWAIT, "ctlr_write");
		status_syswait();
	}
	if (wcc_sound_alarm)
		ring_bell();

	trace_primed = False;

	ps_process();
}

#undef START_FIELDx
#undef START_FIELD0
#undef START_FIELD


/*
 * Set the pending string.  's' must be NULL or point to XtMalloc'd memory.
 */
void
ps_set(s, is_loginstring)
char *s;
Boolean is_loginstring;
{
	static char *ps_source = CN;

	XtFree(ps_source);
	ps_source = CN;
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

	while (run_ta())
		;
	if (!pending_string) {
		script_continue();
		return;
	}

	/* Add loginstring keystrokes only if safe yet. */
	if (ps_is_loginstring) {
		if (IN_3270) {
			unsigned char	fa;

			if (!formatted || kybdlock || !cursor_addr)
				return;
			fa = *get_field_attribute(cursor_addr);
			if (FA_IS_PROTECTED(fa))
				return;
		} else if (IN_ANSI) {
			if (kybdlock & KL_AWAITING_FIRST)
				return;
		}
	}

	len = strlen(pending_string);
	if ((len_left = emulate_input(pending_string, len, False)))
		pending_string += len - len_left;
	else {
		ps_set(CN, False);
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
		if (!IS_FA(oc) && !IsBlank(oc))
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
	if (ctlr_any_data()) {
		if (toggled(SCREEN_TRACE))
			trace_screen();
		scroll_save(maxROWS, ever_3270 ? False : True);
	}

	/* Clear the screen. */
	(void) memset((char *)screen_buf, 0, ROWS*COLS);
	(void) memset((char *)ea_buf, 0, ROWS*COLS*sizeof(struct ea));
	screen_changed = True;
	cursor_move(0);
	buffer_addr = 0;
	(void) unselect(0, ROWS*COLS);
	formatted = False;
	default_fg = 0;
	default_gr = 0;
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
			if (toggled(SCREEN_TRACE))
				trace_screen();
			scroll_save(maxROWS, False);
			trace_primed = False;
		}
		if (SELECTED(baddr))
			(void) unselect(baddr, 1);
		screen_changed = True;
		screen_buf[baddr] = c;
	}
}

/*
 * Change the graphic rendition of a character in the 3270 buffer.
 */
void
ctlr_add_gr(baddr, gr)
int	baddr;
unsigned char	gr;
{
	if (ea_buf[baddr].gr != gr) {
		if (SELECTED(baddr))
			(void) unselect(baddr, 1);
		screen_changed = True;
		ea_buf[baddr].gr = gr;
		if (gr & GR_BLINK)
			blink_start();
	}
}

/*
 * Change the color for a character in the 3270 buffer.
 */
void
ctlr_add_color(baddr, color)
int	baddr;
unsigned char	color;
{
	if ((color & 0xf0) != 0xf0)
		color = 0;
	if (ea_buf[baddr].fg != color) {
		if (SELECTED(baddr))
			(void) unselect(baddr, 1);
		screen_changed = True;
		ea_buf[baddr].fg = color;
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
			      count*sizeof(struct ea))) {
		(void) MEMORY_MOVE((char *) &ea_buf[baddr_to],
		                   (char *) &ea_buf[baddr_from],
			           count*sizeof(struct ea));
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
	if (clear_ea && memcmp((char *) &ea_buf[baddr], (char *) zero_buf, count*sizeof(struct ea))) {
		(void) memset((char *) &ea_buf[baddr], 0, count*sizeof(struct ea));
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
	unsigned char *stmp;
	struct ea *etmp;

	if (alt != is_altbuffer) {

		stmp = screen_buf;
		screen_buf = ascreen_buf;
		ascreen_buf = stmp;

		etmp = ea_buf;
		ea_buf = aea_buf;
		aea_buf = etmp;

		is_altbuffer = alt;
		screen_changed = True;
		(void) unselect(0, ROWS*COLS);

		/*
		 * There may be blinkers on the alternate screen; schedule one
		 * iteration just in case.
		 */
		blink_start();
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
 * Support for screen-size swapping for scrolling
 */
void
ctlr_shrink()
{
	(void) memset((char *)screen_buf,
	    *debugging_font ? CG_space : CG_null,
	    ROWS*COLS);
	screen_changed = True;
	screen_disp();
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
	if (!toggled(SHOW_TIMING) && !anyway)
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
toggle_showTiming(t, tt)
struct toggle *t;
enum toggle_type tt;
{
	if (!toggled(SHOW_TIMING))
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
