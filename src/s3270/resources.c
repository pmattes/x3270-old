/*
 * Copyright 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

#include <stdio.h>
#include <string.h>

/* s3270 substitute Xt resource database. */

static struct {
	char *name;
	char *value;
} rdb[] = {
	{ "charset.us-intl", "\n" },
	{ "charset.us", "\n" },
	{ "charset.apl", "\n" },
	{ "charset.bracket",
"0xad: [		\n	0xba: Yacute		\n\
0xbd: ]			\n	0xbb: diaeresis		\n" },
	{ "charset.oldibm",
"0xad: [		\n	0xba: Yacute		\n\
0xbd: ]			\n	0xbb: diaeresis		\n" },
	{ "charset.german",
"0x43: {		\n	0x4a: Adiaeresis	\n\
0x4f: !			\n	0x59: ~			\n\
0x5a: Udiaeresis	\n	0x5f: ^			\n\
0x63: [			\n	0x6a: odiaeresis	\n\
0x7c: section		\n	0xa1: ssharp		\n\
0xb0: cent		\n	0xb5: @			\n\
0xba: notsign		\n	0xbb: bar		\n\
0xc0: adiaeresis	\n	0xcc: brokenbar		\n\
0xd0: udiaeresis	\n	0xdc: }			\n\
0xe0: Odiaeresis	\n	0xec: backslash		\n\
0xfc: ]			\n" },
	{ "charset.finnish",
"0x43: {		\n	0x47: }			\n\
0x4a: section		\n	0x4f: !			\n\
0x51: `			\n	0x5a: currency		\n\
0x5b: Aring		\n	0x5f: ^			\n\
0x63: #			\n	0x67: $			\n\
0x6a: odiaeresis	\n	0x71: backslash		\n\
0x79: eacute		\n	0x7b: Adiaeresis	\n\
0x7c: Odiaeresis	\n	0x9f: ]			\n\
0xa1: udiaeresis	\n	0xb1: cent		\n\
0xb5: [			\n	0xba: notsign		\n\
0xbb: |			\n	0xc0: adiaeresis	\n\
0xcc: brokenbar		\n	0xd0: aring		\n\
0xdc: ~			\n	0xe0: Eacute		\n\
0xec: @			\n" },
	{ "charset.uk",
"0x4a: $		\n	0x5b: sterling		\n\
0xa1: macron		\n	0xb0: cent		\n\
0xb1: [			\n	0xba: ^			\n\
0xbc: ~			\n" },
	{ "charset.norwegian",
"0x47: }		\n	0x4a: #			\n\
0x4f: !			\n	0x5a: currency		\n\
0x5b: Aring		\n	0x5f: ^			\n\
0x67: $			\n	0x6a: oslash		\n\
0x70: brokenbar		\n	0x7b: AE		\n\
0x7c: Ooblique		\n	0x80: @			\n\
0x9c: {			\n	0x9e: [			\n\
0x9f: ]			\n	0xa1: udiaeresis	\n\
0xb0: cent		\n	0xba: notsign		\n\
0xbb: bar		\n	0xc0: ae		\n\
0xd0: aring		\n	0xdc: ~			\n" },
	{ "charset.french",
"0x44: @		\n	0x48: backslash		\n\
0x4a: degree		\n	0x4f: !			\n\
0x51: {			\n	0x54: }			\n\
0x5a: section		\n	0x5f: ^			\n\
0x6a: ugrave		\n	0x79: mu		\n\
0x7b: sterling		\n	0x7c: agrave		\n\
0x90: [			\n	0xa0: grave		\n\
0xa1: diaeresis		\n	0xb0: cent		\n\
0xb1: numbersign	\n	0xb5: ]			\n\
0xba: notsign		\n	0xbb: bar		\n\
0xbd: ~			\n	0xc0: eacute		\n\
0xd0: egrave		\n	0xe0: ccedilla		\n" },
	{ "charset.hebrew",
"0x41: hebrew_aleph	\n	0x42: hebrew_bet	\n\
0x43: hebrew_gimel	\n	0x44: hebrew_dalet	\n\
0x45: hebrew_he		\n	0x46: hebrew_waw	\n\
0x47: hebrew_zain	\n	0x48: hebrew_chet	\n\
0x49: hebrew_tet	\n	0x51: hebrew_yod	\n\
0x52: hebrew_finalkaph	\n	0x53: hebrew_kaph	\n\
0x54: hebrew_lamed	\n	0x55: hebrew_finalmem	\n\
0x56: hebrew_mem	\n	0x57: hebrew_finalnun	\n\
0x58: hebrew_nun	\n	0x59: hebrew_samech	\n\
0x62: hebrew_ayin	\n	0x63: hebrew_finalpe	\n\
0x64: hebrew_pe		\n	0x65: hebrew_finalzade	\n\
0x66: hebrew_zade	\n	0x67: hebrew_qoph	\n\
0x68: hebrew_resh	\n	0x69: hebrew_shin	\n\
0x71: hebrew_taw	\n" },
	{ "charset.icelandic",
"0xa1: odiaeresis	\n	0x5f: Odiaeresis	\n\
0x79: eth		\n	0x7c: Eth		\n\
0xc0: thorn		\n	0x4a: Thorn		\n\
0xd0: ae		\n	0x5a: AE		\n\
0xcc: ~			\n	0x4f: !			\n\
0x8e: {			\n	0x9c: }			\n\
0xae: [			\n	0x9e: ]			\n\
0xac: @			\n	0xbe: \\		\n\
0x7d: '			\n	0x8c: `			\n\
0x6a: |			\n" },
	{ "message.ftComplete",
"Transfer complete, %i bytes transferred\n\
%.2lg Kbytes/sec in %s mode" },
	{ "message.ftUnable",	"Cannot begin transfer" },
	{ "message.ftUserCancel",	"Transfer cancelled by user" },
	{ "message.ftHostCancel",	"Transfer cancelled by host" },
	{ "message.ftCutUnknownFrame",	"Unknown frame type from host" },
	{ "message.ftCutUnknownControl",	"Unknown FT control code from host" },
	{ "message.ftCutRetransmit",	"Transmission error" },
	{ "message.ftCutConversionError",	"Data conversion error" },
	{ "message.ftCutOversize",	"Illegal frame length" },
	{ "message.ftDisconnected",	"Host disconnected, transfer cancelled" },
	{ "message.ftNot3270",	"Not in 3270 mode, transfer cancelled" },
	{ (char *)NULL, (char *)NULL }
};

char *
get_resource(const char *name)
{
	int i;

	for (i = 0; rdb[i].name != (char *)NULL; i++) {
		if (!strcmp(rdb[i].name, name)) {
			return rdb[i].value;
		}
	}
	return NULL;
}
