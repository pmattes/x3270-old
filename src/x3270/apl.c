/*
 * Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	apl.c
 *		This module handles APL-specific actions.
 */

#include <X11/Intrinsic.h>
#define XK_APL
#define XK_GREEK
#define XK_TECHNICAL
#include <X11/keysym.h>


/*
 * APL keysym translation.
 *
 * This code looks a little odd because of how an APL font is implemented.
 * An APL font has APL graphics in place of the various accented letters and
 * special symbols in a regular font.  APL keysym translation consists of
 * taking the keysym name for an APL symbol (these names are meaningful only to
 * x3270) and translating it into the keysym for the regular symbol that the
 * desired APL symbol _replaces_.
 *
 * For example, an APL font has the APL "jot" symbol where a regular font has
 * the "registered" symbol.  So we take the keysym name "jot" and translate it
 * into the keysym XK_registered.  When the XK_registered symbol is displayed
 * with an APL font, it appears as a "jot".
 *
 * The specification of which APL symbols replace which regular symbols is in
 * IBM GA27-3831, 3174 Establishment Controller Character Set Reference.
 *
 * Clear as mud, right?
 */

static struct {
	char *name;
	KeySym keysym;
} axl[] = {
	{ "Aunderbar",	XK_nobreakspace },
	{ "Bunderbar",	XK_acircumflex },
	{ "Cunderbar",	XK_adiaeresis },
	{ "Dunderbar",	XK_agrave },
	{ "Eunderbar",	XK_aacute },
	{ "Funderbar",	XK_atilde },
	{ "Gunderbar",	XK_aring },
	{ "Hunderbar",	XK_ccedilla },
	{ "Iunderbar",	XK_ntilde },
	{ "Junderbar",	XK_eacute },
	{ "Kunderbar",	XK_ecircumflex },
	{ "Lunderbar",	XK_ediaeresis },
	{ "Munderbar",	XK_egrave },
	{ "Nunderbar",	XK_iacute },
	{ "Ounderbar",	XK_icircumflex },
	{ "Punderbar",	XK_idiaeresis },
	{ "Qunderbar",	XK_igrave },
	{ "Runderbar",	XK_ssharp },
	{ "Sunderbar",	XK_Acircumflex },
	{ "Tunderbar",	XK_Adiaeresis },
	{ "Uunderbar",	XK_Agrave },
	{ "Vunderbar",	XK_Aacute },
	{ "Wunderbar",	XK_Atilde },
	{ "Xunderbar",	XK_Aring },
	{ "Yunderbar",	XK_Ccedilla },
	{ "Zunderbar",	XK_Ntilde },
	{ "diamond",	XK_oslash },
	{ "upcaret",	XK_Eacute },
	{ "diaeresis",	XK_Ecircumflex },
	{ "quadjot",	XK_Ediaeresis },
	{ "iotaunderbar",	XK_Egrave },
	{ "epsilonunderbar",	XK_Iacute },
	{ "lefttack",	XK_Icircumflex },
	{ "righttack",	XK_Idiaeresis },
	{ "downcaret",	XK_Igrave },
	{ "tilde",	XK_Ooblique },
	{ "uparrow",	XK_guillemotleft },
	{ "downarrow",	XK_guillemotright },
	{ "notgreater",	XK_eth },
	{ "upstile",	XK_yacute },
	{ "downstile",	XK_thorn },
	{ "rightarrow",	XK_plusminus },
	{ "quad",	XK_degree },
	{ "rightshoe",	XK_ordfeminine },
	{ "leftshoe",	XK_masculine },
	{ "splat",	XK_ae },
	{ "circle",	XK_cedilla },
	{ "plusminus",	XK_AE },
	{ "leftarrow",	XK_currency },
	{ "overbar",	XK_mu },
	{ "upshoe",	XK_exclamdown },
	{ "downshoe",	XK_questiondown },
	{ "downtack",	XK_ETH },
	{ "notless",	XK_THORN },
	{ "jot",	XK_registered },
	{ "alpha",	XK_asciicircum },
	{ "epsilon",	XK_sterling },
	{ "iota",	XK_yen },
	{ "rho",	XK_periodcentered },
	{ "omega",	XK_copyright },
	{ "multiply",	XK_paragraph },
	{ "slope",	XK_onequarter },
	{ "divide",	XK_onehalf },
	{ "del",	XK_bracketleft },
	{ "delta",	XK_bracketright },
	{ "uptack",	XK_macron },
	{ "notequal",	XK_acute },
	{ "stile",	XK_multiply },
	{ "upcarettilde",	XK_hyphen },
	{ "downcarettilde",	XK_ocircumflex },
	{ "squad",	XK_odiaeresis },
	{ "circlestile",	XK_ograve },
	{ "slopequad",	XK_oacute },
	{ "circleslope",	XK_otilde },
	{ "downtackup",	XK_onesuperior },
	{ "quotedot",	XK_ucircumflex },
	{ "delstile",	XK_udiaeresis },
	{ "deltastile",	XK_ugrave },
	{ "quadquote",	XK_uacute },
	{ "upshoejot",	XK_ydiaeresis },
	{ "slashbar",	XK_twosuperior },
	{ "slopebar",	XK_Ocircumflex },
	{ "diaeresisdot",	XK_Odiaeresis },
	{ "circlebar",	XK_Ograve },
	{ "quaddivide",	XK_Oacute },
	{ "uptackjot",	XK_Otilde },
	{ "deltilde",	XK_Ucircumflex },
	{ "deltaunderbar",	XK_Udiaeresis },
	{ "circlestar",	XK_Ugrave },
	{ "downtackjot",	XK_Uacute },
	{ "bracketleft",	XK_Yacute },
	{ "bracketright", 	XK_diaeresis },
	{ 0, 0 }
};

/*
 * Translation from APL ksysym names to indirect APL keysyms.
 */
KeySym
APLStringToKeysym(s)
char *s;
{
	register int i;

	if (strncmp(s, "apl_", 4))
		return NoSymbol;
	s += 4;
	for (i = 0; axl[i].name; i++)
		if (!strcmp(axl[i].name, s))
			return axl[i].keysym;
	return NoSymbol;
}
