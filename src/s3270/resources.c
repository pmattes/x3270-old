/*
 * Copyright 1999, 2000, 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

#include <stdio.h>
#include <string.h>
#include "localdefs.h"

/* s3270 substitute Xt resource database. */

static struct {
	char *name;
	char *value;
} rdb[] = {
	{ "charset.us-intl", "\n" },
	{ "codepage.us", "37" },
	{ "charset.us", "\n" },
	{ "codepage.us", "37" },
	{ "charset.apl", "\n" },
	{ "codepage.apl", "37" },
	{ "charset.bracket",
"0xad: [		\n	0xba: Yacute		\n\
0xbd: ]			\n	0xbb: diaeresis		\n" },
	{ "codepage.bracket", "37" },
	{ "charset.oldibm",
"0xad: [		\n	0xba: Yacute		\n\
0xbd: ]			\n	0xbb: diaeresis		\n" },
	{ "codepage.oldibm", "37" },
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
	{ "codepage.german", "273" },
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
	{ "codepage.finnish", "278" },
	{ "charset.uk",
"0x4a: $		\n	0x5b: sterling		\n\
0xa1: macron		\n	0xb0: cent		\n\
0xb1: [			\n	0xba: ^			\n\
0xbc: ~			\n" },
	{ "codepage.uk", "285" },
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
	{ "codepage.norwegian", "277" },
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
	{ "codepage.french", "297" },
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
	{ "codepage.icelandic", "871" },
	{ "charset.belgian",
"0x4a: [		\n	0x4f: !			\n\
0x5a: ]			\n	0x5f: ^			\n\
0xb0: cent		\n	0xba: notsign		\n\
0xbb: bar		\n" },
	{ "codepage.belgian", "500" },
	{ "charset.turkish",
"0x48: {		\n	0x4a: 0xc7		\n\
0x4f: !			\n	0x5a: 0xd0		\n\
0x5b: 0xdd		\n	0x5f: ^			\n\
0x68: [			\n	0x6a: 0xfe		\n\
0x79: 0xfd		\n	0x7b: 0xd6		\n\
0x7c: 0xde		\n	0x7f: 0xdc		\n\
0x8c: }			\n	0x8d: 0x91		\n\
0x8e: 0xa6		\n	0xa1: 0xf6		\n\
0xac: ]			\n	0xad: $			\n\
0xae: @			\n	0xb0: 0xa2		\n\
0xba: 0xac		\n	0xbb: |			\n\
0xbe: 0x92		\n	0xc0: 0xe7		\n\
0xcc: ~			\n	0xd0: 0xf0		\n\
0xdc: 0x5c		\n	0xe0: 0xfc		\n\
0xec: #			\n	0xfc: \"" },
	{ "codepage.turkish", "0x04800402" },
	{ "displayCharset.turkish", "iso8859-9" },
	{ "charset.iso-hebrew",
"0x41: 0xe0		\n	0x42: 0xe1		\n\
0x43: 0xe2		\n	0x44: 0xe3		\n\
0x45: 0xe4		\n	0x46: 0xe5		\n\
0x47: 0xe6		\n	0x48: 0xe7		\n\
0x49: 0xe8		\n	0x51: 0xe9		\n\
0x52: 0xea		\n	0x53: 0xea		\n\
0x54: 0xec		\n	0x55: 0xec		\n\
0x56: 0xee		\n	0x57: 0xef		\n\
0x58: 0xf0		\n	0x59: 0xf1		\n\
0x62: 0xf2		\n	0x63: 0xf3		\n\
0x64: 0xf4		\n	0x65: 0xf5		\n\
0x66: 0xf6		\n	0x67: 0xf7		\n\
0x68: 0xf8		\n	0x69: 0xf9		\n\
0x71: 0xfa" },
	{ "codepage.iso-hebrew", "424" },
	{ "displayCharset.iso-hebrew", "iso8859-8" },
#if defined(C3270) /*[*/
	{ "composeMap.latin1", "c + bar		= cent			\n\
c + slash	= cent			\n\
L + minus	= sterling		\n\
Y + equal	= yen			\n\
S + S		= section		\n\
C + O		= copyright		\n\
a + underscore	= ordfeminine		\n\
less + less	= guillemotleft		\n\
R + O		= registered		\n\
plus + minus	= plusminus		\n\
o + underscore	= masculine		\n\
greater + greater = guillemotright	\n\
1 + 4		= onequarter		\n\
1 + 2		= onehalf		\n\
3 + 4		= threequarters		\n\
bar + bar	= brokenbar		\n\
A + grave	= Agrave		\n\
A + apostrophe	= Aacute		\n\
A + asciicircum	= Acircumflex		\n\
A + asciitilde	= Atilde		\n\
A + quotedbl	= Adiaeresis		\n\
A + asterisk	= Aring			\n\
A + E		= AE			\n\
C + comma	= Ccedilla		\n\
E + grave	= Egrave		\n\
E + apostrophe	= Eacute		\n\
E + asciicircum	= Ecircumflex		\n\
E + quotedbl	= Ediaeresis		\n\
I + grave	= Igrave		\n\
I + apostrophe	= Iacute		\n\
I + asciicircum	= Icircumflex		\n\
I + quotedbl	= Idiaeresis		\n\
N + asciitilde	= Ntilde		\n\
O + grave	= Ograve		\n\
O + apostrophe	= Oacute		\n\
O + asciicircum	= Ocircumflex		\n\
O + asciitilde	= Otilde		\n\
O + quotedbl	= Odiaeresis		\n\
O + slash	= Ooblique		\n\
U + grave	= Ugrave		\n\
U + apostrophe	= Uacute		\n\
U + asciicircum	= Ucircumflex		\n\
U + quotedbl	= Udiaeresis		\n\
Y + apostrophe	= Yacute		\n\
s + s		= ssharp		\n\
a + grave	= agrave		\n\
a + apostrophe	= aacute		\n\
a + asciicircum	= acircumflex		\n\
a + asciitilde	= atilde		\n\
a + quotedbl	= adiaeresis		\n\
a + asterisk	= aring			\n\
a + e		= ae			\n\
c + comma	= ccedilla		\n\
e + grave	= egrave		\n\
e + apostrophe	= eacute		\n\
e + asciicircum	= ecircumflex		\n\
e + quotedbl	= ediaeresis		\n\
i + grave	= igrave		\n\
i + apostrophe	= iacute		\n\
i + asciicircum	= icircumflex		\n\
i + quotedbl	= idiaeresis		\n\
n + asciitilde	= ntilde		\n\
o + grave	= ograve		\n\
o + apostrophe	= oacute		\n\
o + asciicircum	= ocircumflex		\n\
o + asciitilde	= otilde		\n\
o + quotedbl	= odiaeresis		\n\
o + slash	= oslash		\n\
u + grave	= ugrave		\n\
u + apostrophe	= uacute		\n\
u + asciicircum	= ucircumflex		\n\
u + quotedbl	= udiaeresis		\n\
y + apostrophe	= yacute		\n\
y + quotedbl	= ydiaeresis		\n" },
	{ "printer.command",	"lpr" },
	{ "printer.assocCommandLine", "pr3287 -assoc %L% -command \"%C%\" %H%" },
	{ "printer.luCommandLine", "pr3287 -command \"%C%\" %L%@%H%" },
	{ "printTextCommand",	"lpr" },
#endif /*]*/
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
#if defined(C3270) /*[*/
	{ "message.hour",	"hour" },
	{ "message.hours",	"hours" },
	{ "message.minute",	"minute" },
	{ "message.byte",	"byte" },
	{ "message.bytes",	"bytes" },
	{ "message.characterSet",	"EBCDIC character set:" },
	{ "message.charMode",	"NVT character mode" },
	{ "message.columns",	"columns" },
	{ "message.connectedTo",	"Connected to:" },
	{ "message.connectionPending",	"Connection pending to:" },
	{ "message.defaultCharacterSet",	"Default (us) EBCDIC character set" },
	{ "message.dsMode",	"3270 mode" },
	{ "message.extendedDs",	"extended data stream" },
	{ "message.fullColor",	"color" },
	{ "message.keyboardMap",	"Keyboard map:" },
	{ "message.lineMode",	"NVT line mode" },
	{ "message.luName",	"LU name:" },
	{ "message.minute",	"minute" },
	{ "message.minutes",	"minutes" },
	{ "message.model",	"Model" },
	{ "message.mono",	"monochrome" },
	{ "message.notConnected",	"Not connected" },
	{ "message.port",	"Port:" },
	{ "message.Received",	"Received" },
	{ "message.received",	"received" },
	{ "message.record",	"record" },
	{ "message.records",	"records" },
	{ "message.rows",	"rows" },
	{ "message.second",	"second" },
	{ "message.seconds",	"seconds" },
	{ "message.sent",	"Sent" },
	{ "message.specialCharacters",	"Special characters:" },
	{ "message.sscpMode",	"SSCP-LU mode" },
	{ "message.standardDs",	"standard data stream" },
	{ "message.terminalName",	"Terminal name:" },
	{ "message.tn3270eNoOpts",	"No TN3270E options" },
	{ "message.tn3270eOpts",	"TN3270E options:" },
#endif /*]*/
	{ (char *)NULL, (char *)NULL }
};

static struct dresource {
	struct dresource *next;
	const char *name;
	char *value;
} *drdb = NULL, **drdb_next = &drdb;

void
add_resource(const char *name, char *value)
{
	struct dresource *d;

	for (d = drdb; d != NULL; d = d->next) {
		if (!strcmp(d->name, name)) {
			d->value = value;
			return;
		}
	}
	d = Malloc(sizeof(struct dresource));
	d->next = NULL;
	d->name = name;
	d->value = value;
	*drdb_next = d;
	drdb_next = &d->next;
}

char *
get_resource(const char *name)
{
	struct dresource *d;
	int i;

	for (d = drdb; d != NULL; d = d->next) {
		if (!strcmp(d->name, name))
			return d->value;
	}

	for (i = 0; rdb[i].name != (char *)NULL; i++) {
		if (!strcmp(rdb[i].name, name)) {
			return rdb[i].value;
		}
	}
	return NULL;
}
