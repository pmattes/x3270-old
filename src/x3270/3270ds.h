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
 *	3270ds.h
 *
 *		Header file for the 3270 Data Stream Protocol.
 */

/* 3270 commands */
#define CMD_EAU		0x0f	/* erase all unprotected */
#define CMD_EW		0x05	/* erase/write */
#define CMD_EWA		0x0d	/* erase/write alternate */
#define CMD_RB		0x02	/* read buffer */
#define CMD_RM		0x06	/* read modified */
#define CMD_W		0x01	/* write */
#define CMD_NOP		0x03	/* no-op */

/* SNA 3270 commands */
#define SNA_CMD_EAU	0x6f
#define SNA_CMD_EW	0xf5
#define SNA_CMD_EWA	0x7e
#define SNA_CMD_RB	0xf2
#define SNA_CMD_RM	0xf6
#define SNA_CMD_W	0xf1

/* 3270 orders */
#define ORDER_SF	0x1d	/* start field */
#define ORDER_SFE	0x29	/* start field extended */
#define ORDER_SBA	0x11	/* set buffer address */
#define ORDER_SA	0x28	/* set attribute */
#define ORDER_MF	0x2c	/* modify field */
#define ORDER_IC	0x13	/* insert cursor */
#define ORDER_PT	0x05	/* program tab */
#define ORDER_RA	0x3c	/* repeat to address */
#define ORDER_EUA	0x12	/* erase unprotected to address */
#define ORDER_GE	0x08	/* graphic escape */
#define ORDER_YALE	0x2b	/* Yale sub command */

#define FCORDER_NULL	0x00	/* format control: null */
#define FCORDER_SUB	0x3f	/*		   substitute */
#define FCORDER_DUP	0x1c	/*		   duplicate */
#define FCORDER_FM	0x1e	/*		   field mark */
#define FCORDER_FF	0x0c	/*		   form feed */
#define FCORDER_CR	0x0d	/*		   carriage return */
#define FCORDER_NL	0x15	/*		   new line */
#define FCORDER_EM	0x19	/*		   end of medium */
#define FCORDER_EO	0xff	/*		   eight ones */

#define BA_TO_ROW(ba)		((ba) / COLS)
#define BA_TO_COL(ba)		((ba) % COLS)
#define ROWCOL_TO_BA(r,c)	(((r) * COLS) + c)
#define INC_BA(ba)		{ (ba) = ((ba) + 1) % (COLS * ROWS); }
#define DEC_BA(ba)		{ (ba) = (ba) ? (ba - 1) : ((COLS*ROWS) - 1); }

/* field attribute definitions
 * 	The 3270 fonts are based on the 3270 character generator font found on
 *	page 12-2 in the IBM 3270 Information Display System Character Set
 *	Reference.  Characters 0xC0 through 0xCF and 0xE0 through 0xEF
 *	(inclusive) are purposely left blank and are used to represent field
 *	attributes as follows:
 *
 *		11x0xxxx
 *		  | ||||
 *		  | ||++--- 00 normal intensity/non-selectable
 *		  | ||      01 normal intensity/selectable
 *		  | ||      10 high intensity/selectable
 *		  | ||	    11 zero intensity/non-selectable
 *		  | |+----- unprotected(0)/protected(1)
 *		  | +------ alphanumeric(0)/numeric(1)
 *		  +-------- unmodified(0)/modified(1)
 */
#define FA_BASE			0xc0
#define FA_MASK			0xd0
#define FA_MODIFY		0x20
#define FA_NUMERIC		0x08
#define FA_PROTECT		0x04
#define FA_INTENSITY		0x03

#define FA_INT_NORM_NSEL	0x00
#define FA_INT_NORM_SEL		0x01
#define FA_INT_HIGH_SEL		0x02
#define FA_INT_ZERO_NSEL	0x03

#define IS_FA(c)		(((c) & FA_MASK) == FA_BASE)

#define FA_IS_MODIFIED(c)	((c) & FA_MODIFY)
#define FA_IS_NUMERIC(c)	((c) & FA_NUMERIC)
#define FA_IS_PROTECTED(c)	((c) & FA_PROTECT)
#define FA_IS_SKIP(c)		(FA_IS_NUMERIC(c) && FA_IS_PROTECTED(c))

#define FA_IS_ZERO(c)					\
	(((c) & FA_INTENSITY) == FA_INT_ZERO_NSEL)
#define FA_IS_HIGH(c)					\
	(((c) & FA_INTENSITY) == FA_INT_HIGH_SEL)
#define FA_IS_NORMAL(c)					\
    (							\
	((c) & FA_INTENSITY) == FA_INT_NORM_NSEL	\
	||						\
	((c) & FA_INTENSITY) == FA_INT_NORM_SEL		\
    )
#define FA_IS_SELECTABLE(c)				\
    (							\
	((c) & FA_INTENSITY) == FA_INT_NORM_SEL		\
	||						\
	((c) & FA_INTENSITY) == FA_INT_HIGH_SEL		\
    )

/* Extended attributes */
#define XA_ALL		0x00
#define XA_3270		0xc0
#define XA_VALIDATION	0xc1
#define  XAV_FILL	0x04
#define  XAV_ENTRY	0x02
#define  XAV_TRIGGER	0x01
#define XA_OUTLINING	0xc2
#define  XAO_UNDERLINE	0x01
#define  XAO_RIGHT	0x02
#define  XAO_OVERLINE	0x04
#define  XAO_LEFT	0x08
#define XA_HIGHLIGHTING	0x41
#define  XAH_DEFAULT	0x00
#define  XAH_NORMAL	0xf0
#define  XAH_REVERSE	0xf1
#define  XAH_UNDERSCORE	0xf4
#define XA_FOREGROUND	0x42
#define  XAC_DEFAULT	0x00
#define XA_CHARSET	0x43
#define XA_BACKGROUND	0x45
#define XA_TRANSPARENCY	0x46
#define  XAT_DEFAULT	0x00
#define  XAT_OR		0xf0
#define  XAT_XOR	0xf1
#define  XAT_OPAQUE	0xff

/* WCC definitions */
#define WCC_START_PRINTER(c)	((c) & 0x08)
#define WCC_SOUND_ALARM(c)	((c) & 0x04)
#define WCC_KEYBOARD_RESTORE(c)	((c) & 0x02)
#define WCC_RESET_MDT(c)	((c) & 0x01)

/* AIDs */
#define AID_NO		0x60	/* no AID generated */
#define AID_ENTER	0x7d
#define AID_PF1		0xf1
#define AID_PF2		0xf2
#define AID_PF3		0xf3
#define AID_PF4		0xf4
#define AID_PF5		0xf5
#define AID_PF6		0xf6
#define AID_PF7		0xf7
#define AID_PF8		0xf8
#define AID_PF9		0xf9
#define AID_PF10	0x7a
#define AID_PF11	0x7b
#define AID_PF12	0x7c
#define AID_PF13	0xc1
#define AID_PF14	0xc2
#define AID_PF15	0xc3
#define AID_PF16	0xc4
#define AID_PF17	0xc5
#define AID_PF18	0xc6
#define AID_PF19	0xc7
#define AID_PF20	0xc8
#define AID_PF21	0xc9
#define AID_PF22	0x4a
#define AID_PF23	0x4b
#define AID_PF24	0x4c
#define AID_OICR	0xe6
#define AID_MSR_MHS	0xe7
#define AID_SELECT	0x7e
#define AID_PA1		0x6c
#define AID_PA2		0x6e
#define AID_PA3		0x6b
#define AID_CLEAR	0x6d
#define AID_SYSREQ	0xf0
