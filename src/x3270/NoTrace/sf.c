/*
 * Copyright 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	sf.c
 *		This module handles 3270 structured fields.
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

#if defined(__STDC__)
#define MASK32	0xff000000U
#define MASK24	0x00ff0000U
#define MASK16	0x0000ff00U
#define MASK08	0x000000ffU
#define MINUS1	0xffffffffU
#else
#define MASK32	0xff000000
#define MASK24	0x00ff0000
#define MASK16	0x0000ff00
#define MASK08	0x000000ff
#define MINUS1	0xffffffff
#endif

#define SET16(ptr, val) { \
    *ptr++ = (val & MASK16) >> 8; \
    *ptr++ = (val & MASK08); \
}
#define SET32(ptr, val) { \
    *ptr++ = (val & MASK32) >> 24; \
    *ptr++ = (val & MASK24) >> 16; \
    *ptr++ = (val & MASK16) >> 8; \
    *ptr++ = (val & MASK08); \
}

/* Externals: telnet.c */
extern unsigned char	obuf[], *obptr;

/* Externals: ctlr.c */
extern Boolean screen_alt;
extern unsigned char reply_mode;
extern int crm_nattr;
extern unsigned char crm_attr[];

/* Externals: screen.c */
extern int *char_width;
extern int *char_height;

/* Globals */
void		write_structured_field();

/* Statics */
static Boolean	qr_in_progress = False;
static void	sf_read_part();
static void	sf_erase_reset();
static void	sf_set_reply_mode();
static void	sf_outbound_ds();
static void	query_reply_start();
static void	do_query_reply();
static void	query_reply_end();

static unsigned char supported_replies[] = {
	QR_SUMMARY,		/* 0x80 */
	QR_USABLE_AREA,		/* 0x81 */
	QR_ALPHA_PART,		/* 0x84 */
	QR_CHARSETS,		/* 0x85 */
	QR_COLOR,		/* 0x86 */
	QR_HIGHLIGHTING,	/* 0x87 */
	QR_REPLY_MODES,		/* 0x88 */
	QR_IMP_PART,		/* 0xa6 */
};
#define NSR	(sizeof(supported_replies)/sizeof(unsigned char))



/*
 * Process a 3270 Write Structured Field command
 */
void
write_structured_field(buf, buflen)
unsigned char buf[];
int buflen;
{
	unsigned short fieldlen;
	unsigned char *cp = buf;

	/* Skip the WSF command itself. */
	cp++;
	buflen--;

	/* Interpret fields. */
	while (buflen > 0) {

		/* Pick out the field length. */
		if (buflen < 2)
			return;
		fieldlen = (cp[0] << 8) + cp[1];
		if (fieldlen == 0)
			fieldlen = buflen;
		if (fieldlen < 3)
			return;
		if ((int)fieldlen > buflen)
			return;

		/* Dispatch on the ID. */
		switch (cp[2]) {
		    case SF_READ_PART:
			sf_read_part(cp, (int)fieldlen);
			break;
		    case SF_ERASE_RESET:
			sf_erase_reset(cp, (int)fieldlen);
			break;
		    case SF_SET_REPLY_MODE:
			sf_set_reply_mode(cp, (int)fieldlen);
			break;
		    case SF_OUTBOUND_DS:
			sf_outbound_ds(cp, (int)fieldlen);
			break;
		    default:
			break;
		}

		/* Skip to the next field. */
		cp += fieldlen;
		buflen -= fieldlen;
	}
}

static void
sf_read_part(buf, buflen)
unsigned char buf[];
int buflen;
{
	unsigned char partition;
	int i;
	int any = 0;

	if (buflen < 5)
		return;

	partition = buf[3];

	switch (buf[4]) {
	    case SF_RP_QUERY:
		if (partition != 0xff)
			return;
		query_reply_start();
		for (i = 0; i < NSR; i++)
			do_query_reply(supported_replies[i]);
		query_reply_end();
		break;
	    case SF_RP_QLIST:
		if (partition != 0xff)
			return;
		if (buflen < 6)
			return;
		query_reply_start();
		switch (buf[5]) {
		    case SF_RPQ_LIST:
			if (buflen < 7) {
				do_query_reply(QR_NULL);
			} else {
				for (i = 0; i < NSR; i++) {
					if (memchr((char *)&buf[6],
						   (char)supported_replies[i],
						   buflen-6)) {
						do_query_reply(supported_replies[i]);
						any++;
					}
				}
				if (!any) {
					do_query_reply(QR_NULL);
				}
			}
			break;
		    case SF_RPQ_EQUIV:
			for (i = 0; i < NSR; i++)
				do_query_reply(supported_replies[i]);
			break;
		    case SF_RPQ_ALL:
			for (i = 0; i < NSR; i++)
				do_query_reply(supported_replies[i]);
			break;
		    default:
			return;
		}
		query_reply_end();
		break;
	    case SNA_CMD_RMA:
		if (partition != 0x00)
			return;
		ctlr_read_modified(AID_QREPLY, True);
		break;
	    case SNA_CMD_RB:
		if (partition != 0x00)
			return;
		ctlr_read_buffer(AID_QREPLY);
		break;
	    case SNA_CMD_RM:
		if (partition != 0x00)
			return;
		ctlr_read_modified(AID_QREPLY, False);
		break;
	    default:
		return;
	}
}

static void
sf_erase_reset(buf, buflen)
unsigned char buf[];
int buflen;
{
	if (buflen != 4)
		return;

	switch (buf[3]) {
	    case SF_ER_DEFAULT:
		ctlr_erase(False);
		break;
	    case SF_ER_ALT:
		ctlr_erase(True);
		break;
	    default:
		return;
	}
}

static void
sf_set_reply_mode(buf, buflen)
unsigned char buf[];
int buflen;
{
	unsigned char partition;
	int i;

	if (buflen < 5)
		return;

	partition = buf[3];
	if (partition != 0x00)
		return;

	switch (buf[4]) {
	    case SF_SRM_FIELD:
		break;
	    case SF_SRM_XFIELD:
		break;
	    case SF_SRM_CHAR:
		break;
	    default:
		return;
	}
	reply_mode = buf[4];
	if (buf[4] == SF_SRM_CHAR) {
		crm_nattr = buflen - 5;
		for (i = 5; i < buflen; i++)
			crm_attr[i - 5] = buf[i];
	}
}

static void
sf_outbound_ds(buf, buflen)
unsigned char buf[];
int buflen;
{
	if (buflen < 5)
		return;

	if (buf[3] != 0x00)
		return;

	switch (buf[4]) {
	    case SNA_CMD_W:
		if (buflen > 5)
			ctlr_write(&buf[4], buflen-4, False);
		break;
	    case SNA_CMD_EW:
		ctlr_erase(screen_alt);
		if (buflen > 5)
			ctlr_write(&buf[4], buflen-4, True);
		break;
	    case SNA_CMD_EWA:
		ctlr_erase(screen_alt);
		if (buflen > 5)
			ctlr_write(&buf[4], buflen-4, True);
		break;
	    case SNA_CMD_EAU:
		ctlr_erase_all_unprotected();
		break;
	    default:
		return;
	}
}

static void
query_reply_start()
{
	obptr = &obuf[0];
	*obptr++ = AID_SF;
	qr_in_progress = True;
}

static void
do_query_reply(code)
unsigned char code;
{
	int len;
	int i;
	unsigned char *obptr0 = obptr;

	if (qr_in_progress)
		qr_in_progress = False;

	obptr += 2;	/* skip length for now */
	*obptr++ = SFID_QREPLY;
	*obptr++ = code;
	switch (code) {

	    case QR_CHARSETS:
		*obptr++ = 0x02;	/* flags: CGCSGID present */
		*obptr++ = 0x00;	/* more flags */
		*obptr++ = *char_width;	/* SDW */
		*obptr++ = *char_height;/* SDH */
		*obptr++ = 0x00;	/* Load PS format types */
		*obptr++ = 0x00;
		*obptr++ = 0x00;
		*obptr++ = 0x00;
		*obptr++ = 0x07;	/* DL */
		*obptr++ = 0x00;	/* SET */
		*obptr++ = 0x00;	/* FLAGS: non-loadable, single-plane,
					   single-byte, no compare */
		*obptr++ = 0xf0;	/* LCID */
		*obptr++ = 0x00;	/* CGCSGID */
		*obptr++ = 0x67;	/* international */
		*obptr++ = 0x00;
		*obptr++ = 0x26;
		break;

	    case QR_IMP_PART:
		*obptr++ = 0x0;		/* reserved */
		*obptr++ = 0x0;
		*obptr++ = 0x0b;	/* length of display size */
		*obptr++ = 0x01;	/* "implicit partition size" */
		*obptr++ = 0x00;	/* reserved */
		SET16(obptr, 80);	/* implicit partition width */
		SET16(obptr, 24);	/* implicit partition height */
		SET16(obptr, maxCOLS);	/* alternate height */
		SET16(obptr, maxROWS);	/* alternate width */
		break;

	    case QR_NULL:
		break;

	    case QR_SUMMARY:
		for (i = 0; i < NSR; i++)
			*obptr++ = supported_replies[i];
		break;

	    case QR_USABLE_AREA:
		*obptr++ = 0x01;	/* 12/14-bit addressing */
		*obptr++ = 0x00;	/* no special character features */
		SET16(obptr, maxCOLS);	/* usable width */
		SET16(obptr, maxROWS);	/* usable height */
		*obptr++ = 0x00;	/* units (inches) */
		SET32(obptr, MINUS1);	/* Xr */
		SET32(obptr, MINUS1);	/* Yr */
		*obptr++ = *char_width;	/* AW */
		*obptr++ = *char_height;/* AH */
		break;

	    case QR_COLOR:
		*obptr++ = 0x00;	/* no options */
		*obptr++ = 16;		/* report on 16 colors */
		*obptr++ = 0x00;	/* default color: */
		*obptr++ = 0xf0 + COLOR_GREEN;	/*  green */
		for (i = 0xf1; i <= 0xff; i++) {
			*obptr++ = i;
			if (appres.m3279)
				*obptr++ = i;
			else
				*obptr++ = 0x00;
		}
		break;

	    case QR_HIGHLIGHTING:
		*obptr++ = 5;		/* report on 5 pairs */
		*obptr++ = XAH_DEFAULT;	/* default: */
		*obptr++ = XAH_NORMAL;	/*  normal */
		*obptr++ = XAH_BLINK;	/* blink: */
		*obptr++ = XAH_BLINK;	/*  blink */
		*obptr++ = XAH_REVERSE;	/* reverse: */
		*obptr++ = XAH_REVERSE;	/*  reverse */
		*obptr++ = XAH_UNDERSCORE; /* underscore: */
		*obptr++ = XAH_UNDERSCORE; /*  underscore */
		*obptr++ = XAH_INTENSIFY; /* intensify: */
		*obptr++ = XAH_INTENSIFY; /*  intensify */
		break;

	    case QR_REPLY_MODES:
		*obptr++ = SF_SRM_FIELD;
		*obptr++ = SF_SRM_XFIELD;
		*obptr++ = SF_SRM_CHAR;
		break;

	    case QR_ALPHA_PART:
		*obptr++ = 0;		/* 1 partition */
		SET16(obptr, maxROWS*maxCOLS);	/* buffer space */
		*obptr++ = 0;		/* no special features */
		break;

	    default:
		return;	/* internal error */
	}
	len = obptr - obptr0;
	SET16(obptr0, len);
}

static void
query_reply_end()
{
	net_output(obuf, obptr - obuf);
	kybd_inhibit(True);
}
