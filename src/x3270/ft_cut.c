/*
 * Copyright 1996 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	ft_cut.c
 *		File transfer, data movement logic, CUT version
 */

#include <errno.h>

#include "globals.h"
#include "ctlr.h"
#include "3270ds.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "ft_cutc.h"
#include "ft_cut_ds.h"
#include "ftc.h"
#include "kybdc.h"
#include "popupsc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "trace_dsc.h"
#include "utilc.h"

Boolean cut_xfer_in_progress = False;

/* Data stream conversion tables. */

#define NQ		4	/* number of quadrants */
#define NE		77	/* number of elements per quadrant */
#define OTHER_2		2	/* "OTHER 2" quadrant (includes NULL) */
#define XLATE_NULL	0xc1	/* translation of NULL */

static char alphas[NE + 1] =
" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789%&_()<+,-./:>?";

static struct {
	unsigned char selector;
	unsigned char xlate[NE];
} conv[NQ] = {
    {	0x5e,	/* ';' */
	{ 0x40,0xc1,0xc2,0xc3, 0xc4,0xc5,0xc6,0xc7, 0xc8,0xc9,0xd1,0xd2,
	  0xd3,0xd4,0xd5,0xd6, 0xd7,0xd8,0xd9,0xe2, 0xe3,0xe4,0xe5,0xe6,
	  0xe7,0xe8,0xe9,0x81, 0x82,0x83,0x84,0x85, 0x86,0x87,0x88,0x89,
	  0x91,0x92,0x93,0x94, 0x95,0x96,0x97,0x98, 0x99,0xa2,0xa3,0xa4,
	  0xa5,0xa6,0xa7,0xa8, 0xa9,0xf0,0xf1,0xf2, 0xf3,0xf4,0xf5,0xf6,
	  0xf7,0xf8,0xf9,0x6c, 0x50,0x6d,0x4d,0x5d, 0x4c,0x4e,0x6b,0x60,
	  0x4b,0x61,0x7a,0x6e, 0x6f }
    },
    {	0x7e,	/* '=' */
	{ 0x20,0x41,0x42,0x43, 0x44,0x45,0x46,0x47, 0x48,0x49,0x4a,0x4b,
	  0x4c,0x4d,0x4e,0x4f, 0x50,0x51,0x52,0x53, 0x54,0x55,0x56,0x57,
	  0x58,0x59,0x5a,0x61, 0x62,0x63,0x64,0x65, 0x66,0x67,0x68,0x69,
	  0x6a,0x6b,0x6c,0x6d, 0x6e,0x6f,0x70,0x71, 0x72,0x73,0x74,0x75,
	  0x76,0x77,0x78,0x79, 0x7a,0x30,0x31,0x32, 0x33,0x34,0x35,0x36,
	  0x37,0x38,0x39,0x25, 0x26,0x27,0x28,0x29, 0x2a,0x2b,0x2c,0x2d,
	  0x2e,0x2f,0x3a,0x3b, 0x3f }
    },
    {	0x5c,	/* '*' */
	{ 0x00,0x00,0x01,0x02, 0x03,0x04,0x05,0x06, 0x07,0x08,0x09,0x0a,
	  0x0b,0x0c,0x0d,0x0e, 0x0f,0x10,0x11,0x12, 0x13,0x14,0x15,0x16,
	  0x17,0x18,0x19,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	  0x00,0x00,0x00,0x00, 0x00,0x3c,0x3d,0x3e, 0x00,0xfa,0xfb,0xfc,
	  0xfd,0xfe,0xff,0x7b, 0x7c,0x7d,0x7e,0x7f, 0x1a,0x1b,0x1c,0x1d,
	  0x1e,0x1f,0x00,0x00, 0x00 }
    },
    {	0x7d,	/* '\'' */
	{ 0x00,0xa0,0xa1,0xea, 0xeb,0xec,0xed,0xee, 0xef,0xe0,0xe1,0xaa,
	  0xab,0xac,0xad,0xae, 0xaf,0xb0,0xb1,0xb2, 0xb3,0xb4,0xb5,0xb6,
	  0xb7,0xb8,0xb9,0x80, 0x00,0xca,0xcb,0xcc, 0xcd,0xce,0xcf,0xc0,
	  0x00,0x8a,0x8b,0x8c, 0x8d,0x8e,0x8f,0x90, 0x00,0xda,0xdb,0xdc,
	  0xdd,0xde,0xdf,0xd0, 0x00,0x00,0x21,0x22, 0x23,0x24,0x5b,0x5c,
	  0x00,0x5e,0x5f,0x00, 0x9c,0x9d,0x9e,0x9f, 0xba,0xbb,0xbc,0xbd,
	  0xbe,0xbf,0x9a,0x9b, 0x00 }
    }
};
static char table6[] =
    "abcdefghijklmnopqrstuvwxyz&-.,:+ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

static int quadrant = -1;
static unsigned long expanded_length;
static char *saved_errmsg = CN;

#define XLATE_NBUF	4
static int xlate_buffered = 0;			/* buffer count */
static int xlate_buf_ix = 0;			/* buffer index */
static unsigned char xlate_buf[XLATE_NBUF];	/* buffer */

static void cut_control_code();
static void cut_data_request();
static void cut_retransmit();
static void cut_data();

static void cut_ack();
static void cut_abort();

static unsigned from6();
static int xlate_getc();

/*
 * Convert a buffer for uploading (host->local).  Overwrites the buffer.
 * Returns the length of the converted data.
 * If there is a conversion error, calls cut_abort() and returns -1.
 */
int
upload_convert(buf, len)
unsigned char *buf;
int len;
{
	unsigned char *ob0 = buf;
	unsigned char *ob = ob0;

	while (len--) {
		unsigned char c = *buf++;
		char *ixp;
		int ix;
		int oq = -1;

	    retry:
		if (quadrant < 0) {
			/* Find the quadrant. */
			for (quadrant = 0; quadrant < NQ; quadrant++) {
				if (c == conv[quadrant].selector)
					break;
			}
			if (quadrant >= NQ) {
				cut_abort(get_message("ftCutConversionError"),
				    SC_ABORT_XMIT);
				return -1;
			}
			continue;
		}

		/* Make sure it's in a valid range. */
		if (c < 0x40 || c > 0xf9) {
			cut_abort(get_message("ftCutConversionError"),
			    SC_ABORT_XMIT);
			return -1;
		}

		/* Translate to a quadrant index. */
		ixp = strchr(alphas, ebc2asc[c]);
		if (ixp == (char *)NULL) {
			/* Try a different quadrant. */
			oq = quadrant;
			quadrant = -1;
			goto retry;
		}
		ix = ixp - alphas;

		/*
		 * See if it's mapped by that quadrant, handling NULLs
		 * specially.
		 */
		if (quadrant != OTHER_2 && c != XLATE_NULL &&
		    !conv[quadrant].xlate[ix]) {
			/* Try a different quadrant. */
			oq = quadrant;
			quadrant = -1;
			goto retry;
		}

		/* Map it. */
		c = conv[quadrant].xlate[ix];
		if (ascii_flag && cr_flag && (c == '\r' || c == 0x1a))
			continue;
		*ob++ = c;
	}

	return ob - ob0;
}

/* Convert a buffer for downloading (local->host). */
int
download_convert(buf, len, obuf)
unsigned char *buf;
int len;
unsigned char *obuf;
{
	unsigned char *ob0 = obuf;
	unsigned char *ob = ob0;

	while (len--) {
		unsigned char c = *buf++;
		unsigned char *ixp;
		unsigned ix;
		int oq;

		/* Handle nulls separately. */
		if (!c) {
			if (quadrant != OTHER_2) {
				quadrant = OTHER_2;
				*ob++ = conv[quadrant].selector;
			}
			*ob++ = XLATE_NULL;
			continue;
		}

		/* Quadrant already defined. */
		if (quadrant >= 0) {
			ixp = (unsigned char *)memchr(conv[quadrant].xlate, c,
							NE);
			if (ixp != (unsigned char *)NULL) {
				ix = ixp - conv[quadrant].xlate;
				*ob++ = cg2ebc[asc2cg[(int)alphas[ix]]];
				continue;
			}
		}

		/* Locate a quadrant. */
		oq = quadrant;
		for (quadrant = 0; quadrant < NQ; quadrant++) {
			if (quadrant == oq)
				continue;
			ixp = (unsigned char *)memchr(conv[quadrant].xlate, c,
				    NE);
			if (ixp == (unsigned char *)NULL)
				continue;
			ix = ixp - conv[quadrant].xlate;
			*ob++ = conv[quadrant].selector;
			*ob++ = cg2ebc[asc2cg[(int)alphas[ix]]];
			break;
		}
		if (quadrant >= NQ) {
			quadrant = -1;
			fprintf(stderr, "Oops\n");
			continue;
		}
	}
	return ob - ob0;
}

/*
 * Main entry point from ctlr.c.
 * We have received what looks like an appropriate message from the host.
 */
void
ft_cut_data()
{
	switch (cg2ebc[screen_buf[O_FRAME_TYPE]]) {
	    case FT_CONTROL_CODE:
		cut_control_code();
		break;
	    case FT_DATA_REQUEST:
		cut_data_request();
		break;
	    case FT_RETRANSMIT:
		cut_retransmit();
		break;
	    case FT_DATA:
		cut_data();
		break;
	    default:
		trace_ds("< FT unknown 0x%02x\n",
		    cg2ebc[screen_buf[O_FRAME_TYPE]]);
		cut_abort(get_message("ftCutUnknownFrame"), SC_ABORT_XMIT);
		break;
	}
}

/*
 * Process a control code from the host.
 */
static void
cut_control_code()
{
	unsigned short code;
	char *buf;
	char *bp;
	int i;

	trace_ds("< FT CONTROL_CODE ");
	code = (cg2ebc[screen_buf[O_CC_STATUS_CODE]] << 8) |
		cg2ebc[screen_buf[O_CC_STATUS_CODE + 1]];
	switch (code) {
	    case SC_HOST_ACK:
		trace_ds("HOST_ACK\n");
		cut_xfer_in_progress = True;
		expanded_length = 0;
		quadrant = -1;
		xlate_buffered = 0;
		cut_ack();
		ft_running(True);
		break;
	    case SC_XFER_COMPLETE:
		trace_ds("XFER_COMPLETE\n");
		cut_ack();
		cut_xfer_in_progress = False;
		ft_complete((String)NULL);
		break;
	    case SC_ABORT_FILE:
	    case SC_ABORT_XMIT:
		trace_ds("ABORT\n");
		cut_xfer_in_progress = False;
		cut_ack();
		if (ft_state == FT_ABORT_SENT && saved_errmsg != CN) {
			buf = saved_errmsg;
			saved_errmsg = CN;
		} else {
			bp = buf = XtMalloc(81);
			for (i = 0; i < 80; i++)
				*bp++ = cg2asc[screen_buf[O_CC_MESSAGE + i]];
			*bp-- = '\0';
			while (bp >= buf && *bp == ' ')
				*bp-- = '\0';
			if (bp >= buf && *bp == '$')
				*bp-- = '\0';
			while (bp >= buf && *bp == ' ')
				*bp-- = '\0';
			if (!*buf)
				strcpy(buf, get_message("ftHostCancel"));
		}
		ft_complete(buf);
		break;
	    default:
		trace_ds("unknown 0x%04x\n", code);
		cut_abort(get_message("ftCutUnknownControl"), SC_ABORT_XMIT);
		break;
	}
}

/*
 * Process a data request from the host.
 */
static void
cut_data_request()
{
	unsigned char seq = screen_buf[O_DR_FRAME_SEQ];
	int count;
	unsigned char cs;
	int c;
	int i;
	unsigned char attr;

	trace_ds("< FT DATA_REQUEST %u\n", from6(seq));
	if (ft_state == FT_ABORT_WAIT) {
		cut_abort(get_message("ftUserCancel"), SC_ABORT_FILE);
		return;
	}

	/* Copy data into the screen buffer. */
	count = 0;
	while (count < O_UP_MAX && (c = xlate_getc()) != EOF) {
		ctlr_add(O_UP_DATA + count, ebc2cg[c], 0);
		count++;
	}

	/* Check for errors. */
	if (ferror(ft_local_file)) {
		int i;
		char *msg;

		/* Clean out any data we may have written. */
		for (i = 0; i < count; i++)
			ctlr_add(O_UP_DATA + i, 0, 0);

		/* Abort the transfer. */
		msg = xs_buffer("read(%s): %s", ft_local_filename,
		    local_strerror(errno));
		cut_abort(msg, SC_ABORT_FILE);
		XtFree(msg);
		return;
	}

	/* Send special data for EOF. */
	if (!count && feof(ft_local_file)) {
		ctlr_add(O_UP_DATA, ebc2cg[EOF_DATA1], 0);
		ctlr_add(O_UP_DATA+1, ebc2cg[EOF_DATA2], 0);
		count = 2;
	}

	/* Compute the other fields. */
	ctlr_add(O_UP_FRAME_SEQ, seq, 0);
	cs = 0;
	for (i = 0; i < count; i++)
		cs ^= cg2ebc[screen_buf[O_UP_DATA + i]];
	ctlr_add(O_UP_CSUM, asc2cg[(int)table6[cs & 0x3f]], 0);
	ctlr_add(O_UP_LEN, asc2cg[(int)table6[(count >> 6) & 0x3f]], 0);
	ctlr_add(O_UP_LEN+1, asc2cg[(int)table6[count & 0x3f]], 0);

	/* XXX: Change the data field attribute so it doesn't display. */
	attr = screen_buf[O_DR_SF];
	attr = (attr & ~FA_INTENSITY) | FA_INT_ZERO_NSEL;
	ctlr_add(O_DR_SF, attr, 0);

	/* Send it up to the host. */
	trace_ds("> FT DATA %u\n", from6(seq));
	ft_update_length();
	expanded_length += count;
	action_internal(Enter_action, IA_FT, CN, CN);
}

/*
 * (Improperly) process a retransmit from the host.
 */
static void
cut_retransmit()
{
	trace_ds("< FT RETRANSMIT\n");
	cut_abort(get_message("ftCutRetransmit"), SC_ABORT_XMIT);
}

/*
 * Convert an encoded integer.
 */
static unsigned
from6(c)
unsigned char c;
{
	char *p;

	c = cg2asc[c];
	p = strchr(table6, c);
	if (p == CN)
		return 0;
	return p - table6;
}

/*
 * Process data from the host.
 */
static void
cut_data()
{
	static unsigned char cvbuf[O_RESPONSE - O_DT_DATA];
	unsigned short raw_length;
	int conv_length;
	register int i;

	trace_ds("< FT DATA\n");
	if (ft_state == FT_ABORT_WAIT) {
		cut_abort(get_message("ftUserCancel"), SC_ABORT_FILE);
		return;
	}

	/* Copy and convert the data. */
	raw_length = from6(screen_buf[O_DT_LEN]) << 6 |
		     from6(screen_buf[O_DT_LEN + 1]);
	if ((int)raw_length > O_RESPONSE - O_DT_DATA) {
		cut_abort(get_message("ftCutOversize"), SC_ABORT_XMIT);
		return;
	}
	for (i = 0; i < (int)raw_length; i++)
		cvbuf[i] = cg2ebc[screen_buf[O_DT_DATA + i]];

	if (raw_length == 2 && cvbuf[0] == EOF_DATA1 && cvbuf[1] == EOF_DATA2) {
		trace_ds("< FT EOF\n");
		cut_ack();
		return;
	}
	conv_length = upload_convert(cvbuf, raw_length);
	if (conv_length < 0)
		return;

	/* Write it to the file. */
	if (fwrite((char *)cvbuf, conv_length, 1, ft_local_file) == 0) {
		char *msg;

		msg = xs_buffer("write(%s): %s", ft_local_filename,
		    local_strerror(errno));
		cut_abort(msg, SC_ABORT_FILE);
		XtFree(msg);
	} else {
		ft_length += conv_length;
		ft_update_length();
		cut_ack();
	}
}

/*
 * Acknowledge a host command.
 */
static void
cut_ack()
{
	trace_ds("> FT ACK\n");
	action_internal(Enter_action, IA_FT, CN, CN);
}

/*
 * Abort a transfer in progress.
 */
static void
cut_abort(s, reason)
char *s;
unsigned short reason;
{
	/* Save the error message. */
	if (saved_errmsg != CN)
		XtFree(saved_errmsg);
	saved_errmsg = XtNewString(s);

	/* Send the abort sequence. */
	ctlr_add(RO_FRAME_TYPE, ebc2cg[RFT_CONTROL_CODE], 0);
	ctlr_add(RO_FRAME_SEQ, screen_buf[O_DT_FRAME_SEQ], 0);
	ctlr_add(RO_REASON_CODE, ebc2cg[HIGH8(reason)], 0);
	ctlr_add(RO_REASON_CODE+1, ebc2cg[LOW8(reason)], 0);
	trace_ds("> FT CONTROL_CODE ABORT\n");
	action_internal(PF_action, IA_FT, "2", CN);

	/* Update the in-progress pop-up. */
	ft_aborting();
}

/*
 * Get the next translated character from the local file.
 * Returns the character (in EBCDIC), or EOF.
 */
static int
xlate_getc()
{
	int r;
	int c;
	unsigned char cc;
	unsigned char cbuf[4];
	int nc;

	/* If there is a data buffered, return it. */
	if (xlate_buffered) {
		r = xlate_buf[xlate_buf_ix];
		xlate_buf_ix++;
		xlate_buffered--;
		return r;
	}

	/* Get the next byte from the file. */
	c = fgetc(ft_local_file);
	if (c == EOF)
		return c;
	ft_length++;

	/* Expand it. */
	if (ascii_flag && cr_flag && c == '\n') {
		nc = download_convert("\r", 1, cbuf);
	} else
		nc = 0;

	/* Convert it. */
	cc = (unsigned char)c;
	nc += download_convert(&cc, 1, &cbuf[nc]);

	/* Return it and buffer what's left. */
	r = cbuf[0];
	if (nc > 1) {
		int i;

		for (i = 1; i < nc; i++)
			xlate_buf[xlate_buffered++] = cbuf[i];
		xlate_buf_ix = 0;
	}
	return r;
}
