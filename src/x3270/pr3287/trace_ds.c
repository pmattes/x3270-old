/*
 * Copyright 1993, 1994, 1995, 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	trace_ds.c
 *		3270 data stream tracing.
 *
 */

#include "globals.h"

#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include "3270ds.h"

#include "ctlrc.h"
#include "tablesc.h"
#include "trace_dsc.h"

/* Statics */
static int      dscnt = 0;

/* Globals */
FILE           *tracef = (FILE *) 0;

const char *
unknown(unsigned char value)
{
	static char buf[64];

	(void) sprintf(buf, "unknown[0x%x]", value);
	return buf;
}

const char *
see_ebc(unsigned char ch)
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

const char *
see_aid(unsigned char code)
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
	case AID_QREPLY:
		return "QueryReplyAID";
	default: 
		return unknown(code);
	}
}

const char *
see_attr(unsigned char fa)
{
	static char buf[256];
	const char *paren = "(";

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
	else
		(void) strcpy(buf, "(default)");

	return buf;
}

static const char *
see_highlight(unsigned char setting)
{
	switch (setting) {
	    case XAH_DEFAULT:
		return "default";
	    case XAH_NORMAL:
		return "normal";
	    case XAH_BLINK:
		return "blink";
	    case XAH_REVERSE:
		return "reverse";
	    case XAH_UNDERSCORE:
		return "underscore";
	    case XAH_INTENSIFY:
		return "intensify";
	    default:
		return unknown(setting);
	}
}

const char *
see_color(unsigned char setting)
{
	static const char *color_name[] = {
	    "neutralBlack",
	    "blue",
	    "red",
	    "pink",
	    "green",
	    "turquoise",
	    "yellow",
	    "neutralWhite",
	    "black",
	    "deepBlue",
	    "orange",
	    "purple",
	    "paleGreen",
	    "paleTurquoise",
	    "grey",
	    "white"
	};

	if (setting == XAC_DEFAULT)
		return "default";
	else if (setting < 0xf0 || setting > 0xff)
		return unknown(setting);
	else
		return color_name[setting - 0xf0];
}

static const char *
see_transparency(unsigned char setting)
{
	switch (setting) {
	    case XAT_DEFAULT:
		return "default";
	    case XAT_OR:
		return "or";
	    case XAT_XOR:
		return "xor";
	    case XAT_OPAQUE:
		return "opaque";
	    default:
		return unknown(setting);
	}
}

static const char *
see_validation(unsigned char setting)
{
	static char buf[64];
	const char *paren = "(";

	(void) strcpy(buf, "");
	if (setting & XAV_FILL) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "fill");
		paren = ",";
	}
	if (setting & XAV_ENTRY) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "entry");
		paren = ",";
	}
	if (setting & XAV_TRIGGER) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "trigger");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");
	else
		(void) strcpy(buf, "(none)");
	return buf;
}

static const char *
see_outline(unsigned char setting)
{
	static char buf[64];
	const char *paren = "(";

	(void) strcpy(buf, "");
	if (setting & XAO_UNDERLINE) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "underline");
		paren = ",";
	}
	if (setting & XAO_RIGHT) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "right");
		paren = ",";
	}
	if (setting & XAO_OVERLINE) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "overline");
		paren = ",";
	}
	if (setting & XAO_LEFT) {
		(void) strcat(buf, paren);
		(void) strcat(buf, "left");
		paren = ",";
	}
	if (strcmp(paren, "("))
		(void) strcat(buf, ")");
	else
		(void) strcpy(buf, "(none)");
	return buf;
}

const char *
see_efa(unsigned char efa, unsigned char value)
{
	static char buf[64];

	switch (efa) {
	    case XA_ALL:
		(void) sprintf(buf, " all(%x)", value);
		break;
	    case XA_3270:
		(void) sprintf(buf, " 3270%s", see_attr(value));
		break;
	    case XA_VALIDATION:
		(void) sprintf(buf, " validation%s", see_validation(value));
		break;
	    case XA_OUTLINING:
		(void) sprintf(buf, " outlining(%s)", see_outline(value));
		break;
	    case XA_HIGHLIGHTING:
		(void) sprintf(buf, " highlighting(%s)", see_highlight(value));
		break;
	    case XA_FOREGROUND:
		(void) sprintf(buf, " foreground(%s)", see_color(value));
		break;
	    case XA_CHARSET:
		(void) sprintf(buf, " charset(%x)", value);
		break;
	    case XA_BACKGROUND:
		(void) sprintf(buf, " background(%s)", see_color(value));
		break;
	    case XA_TRANSPARENCY:
		(void) sprintf(buf, " transparency(%s)", see_transparency(value));
		break;
	    default:
		(void) sprintf(buf, " %s[0x%x]", unknown(efa), value);
	}
	return buf;
}

const char *
see_efa_only(unsigned char efa)
{
	switch (efa) {
	    case XA_ALL:
		return "all";
	    case XA_3270:
		return "3270";
	    case XA_VALIDATION:
		return "validation";
	    case XA_OUTLINING:
		return "outlining";
	    case XA_HIGHLIGHTING:
		return "highlighting";
	    case XA_FOREGROUND:
		return "foreground";
	    case XA_CHARSET:
		return "charset";
	    case XA_BACKGROUND:
		return "background";
	    case XA_TRANSPARENCY:
		return "transparency";
	    default:
		return unknown(efa);
	}
}

const char *
see_qcode(unsigned char id)
{
	static char buf[64];

	switch (id) {
	    case QR_CHARSETS:
		return "CharacterSets";
	    case QR_IMP_PART:
		return "ImplicitPartition";
	    case QR_SUMMARY:
		return "Summary";
	    case QR_USABLE_AREA:
		return "UsableArea";
	    case QR_COLOR:
		return "Color";
	    case QR_HIGHLIGHTING:
		return "Highlighting";
	    case QR_REPLY_MODES:
		return "ReplyModes";
	    case QR_ALPHA_PART:
		return "AlphanumericPartitions";
	    case QR_DDM:
		return "DistributedDataManagement";
	    default:
		(void) sprintf(buf, "unknown[0x%x]", id);
		return buf;
	}
}

/* Data Stream trace print, handles line wraps */

static char *tdsbuf = CN;
#define TDS_LEN	75

static void
trace_ds_s(char *s)
{
	int len = strlen(s);
	Boolean nl = False;

	if (tracef == NULL)
		return;

	if (s && s[len-1] == '\n') {
		len--;
		nl = True;
	}
	while (dscnt + len >= 75) {
		int plen = 75-dscnt;

		(void) fprintf(tracef, "%.*s ...\n... ", plen, s);
		dscnt = 4;
		s += plen;
		len -= plen;
	}
	if (len) {
		(void) fprintf(tracef, "%.*s", len, s);
		dscnt += len;
	}
	if (nl) {
		(void) fprintf(tracef, "\n");
		dscnt = 0;
	}
}

void
trace_ds(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	(void) vsprintf(tdsbuf, fmt, args);
	trace_ds_s(tdsbuf);
	va_end(args);
}
