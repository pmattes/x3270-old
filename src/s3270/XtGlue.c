/*
 * Copyright 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* s3270 glue for missing Xt code */

#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

#include <sys/time.h>
#include <sys/types.h>
#if defined(SEPARATE_SELECT_H) /*[*/
#include <sys/select.h>
#endif /*]*/

void
XtError(const String s)
{
	fprintf(stderr, "Error: %s\n", s);
	exit(1);
}

void
XtWarning(const String s)
{
	fprintf(stderr, "Warning: %s\n", s);
}

XtPointer
XtMalloc(unsigned len)
{
	char *r;

	r = malloc(len);
	if (r == (char *)NULL)
		XtError("Out of memory");
	return (XtPointer)r;
}

XtPointer
XtCalloc(unsigned nelem, unsigned elsize)
{
	char *r;

	r = malloc(nelem * elsize);
	if (r == (char *)NULL)
		XtError("Out of memory");
	return (XtPointer)memset(r, '\0', nelem * elsize);
}

XtPointer
XtRealloc(XtPointer p, unsigned len)
{
	p = realloc(p, len);
	if (p == (XtPointer)NULL)
		XtError("Out of memory");
	return p;
}

void
XtFree(XtPointer p)
{
	free((char *)p);
}

String
XtNewString(const String s)
{
	return (String) strcpy(XtMalloc(strlen(s) + 1), s);
}

static struct {
	char *name;
	KeySym keysym;
} latin1[] = {
	{ "space", XK_space },
	{ "exclam", XK_exclam },
	{ "quotedbl", XK_quotedbl },
	{ "numbersign", XK_numbersign },
	{ "dollar", XK_dollar },
	{ "percent", XK_percent },
	{ "ampersand", XK_ampersand },
	{ "apostrophe", XK_apostrophe },
	{ "quoteright", XK_quoteright },
	{ "parenleft", XK_parenleft },
	{ "parenright", XK_parenright },
	{ "asterisk", XK_asterisk },
	{ "plus", XK_plus },
	{ "comma", XK_comma },
	{ "minus", XK_minus },
	{ "period", XK_period },
	{ "slash", XK_slash },
	{ "0", XK_0 },
	{ "1", XK_1 },
	{ "2", XK_2 },
	{ "3", XK_3 },
	{ "4", XK_4 },
	{ "5", XK_5 },
	{ "6", XK_6 },
	{ "7", XK_7 },
	{ "8", XK_8 },
	{ "9", XK_9 },
	{ "colon", XK_colon },
	{ "semicolon", XK_semicolon },
	{ "less", XK_less },
	{ "equal", XK_equal },
	{ "greater", XK_greater },
	{ "question", XK_question },
	{ "at", XK_at },
	{ "A", XK_A },
	{ "B", XK_B },
	{ "C", XK_C },
	{ "D", XK_D },
	{ "E", XK_E },
	{ "F", XK_F },
	{ "G", XK_G },
	{ "H", XK_H },
	{ "I", XK_I },
	{ "J", XK_J },
	{ "K", XK_K },
	{ "L", XK_L },
	{ "M", XK_M },
	{ "N", XK_N },
	{ "O", XK_O },
	{ "P", XK_P },
	{ "Q", XK_Q },
	{ "R", XK_R },
	{ "S", XK_S },
	{ "T", XK_T },
	{ "U", XK_U },
	{ "V", XK_V },
	{ "W", XK_W },
	{ "X", XK_X },
	{ "Y", XK_Y },
	{ "Z", XK_Z },
	{ "bracketleft", XK_bracketleft },
	{ "backslash", XK_backslash },
	{ "bracketright", XK_bracketright },
	{ "asciicircum", XK_asciicircum },
	{ "underscore", XK_underscore },
	{ "grave", XK_grave },
	{ "quoteleft", XK_quoteleft },
	{ "a", XK_a },
	{ "b", XK_b },
	{ "c", XK_c },
	{ "d", XK_d },
	{ "e", XK_e },
	{ "f", XK_f },
	{ "g", XK_g },
	{ "h", XK_h },
	{ "i", XK_i },
	{ "j", XK_j },
	{ "k", XK_k },
	{ "l", XK_l },
	{ "m", XK_m },
	{ "n", XK_n },
	{ "o", XK_o },
	{ "p", XK_p },
	{ "q", XK_q },
	{ "r", XK_r },
	{ "s", XK_s },
	{ "t", XK_t },
	{ "u", XK_u },
	{ "v", XK_v },
	{ "w", XK_w },
	{ "x", XK_x },
	{ "y", XK_y },
	{ "z", XK_z },
	{ "braceleft", XK_braceleft },
	{ "bar", XK_bar },
	{ "braceright", XK_braceright },
	{ "asciitilde", XK_asciitilde },
	{ "nobreakspace", XK_nobreakspace },
	{ "exclamdown", XK_exclamdown },
	{ "cent", XK_cent },
	{ "sterling", XK_sterling },
	{ "currency", XK_currency },
	{ "yen", XK_yen },
	{ "brokenbar", XK_brokenbar },
	{ "section", XK_section },
	{ "diaeresis", XK_diaeresis },
	{ "copyright", XK_copyright },
	{ "ordfeminine", XK_ordfeminine },
	{ "guillemotleft", XK_guillemotleft },
	{ "notsign", XK_notsign },
	{ "hyphen", XK_hyphen },
	{ "registered", XK_registered },
	{ "macron", XK_macron },
	{ "degree", XK_degree },
	{ "plusminus", XK_plusminus },
	{ "twosuperior", XK_twosuperior },
	{ "threesuperior", XK_threesuperior },
	{ "acute", XK_acute },
	{ "mu", XK_mu },
	{ "paragraph", XK_paragraph },
	{ "periodcentered", XK_periodcentered },
	{ "cedilla", XK_cedilla },
	{ "onesuperior", XK_onesuperior },
	{ "masculine", XK_masculine },
	{ "guillemotright", XK_guillemotright },
	{ "onequarter", XK_onequarter },
	{ "onehalf", XK_onehalf },
	{ "threequarters", XK_threequarters },
	{ "questiondown", XK_questiondown },
	{ "Agrave", XK_Agrave },
	{ "Aacute", XK_Aacute },
	{ "Acircumflex", XK_Acircumflex },
	{ "Atilde", XK_Atilde },
	{ "Adiaeresis", XK_Adiaeresis },
	{ "Aring", XK_Aring },
	{ "AE", XK_AE },
	{ "Ccedilla", XK_Ccedilla },
	{ "Egrave", XK_Egrave },
	{ "Eacute", XK_Eacute },
	{ "Ecircumflex", XK_Ecircumflex },
	{ "Ediaeresis", XK_Ediaeresis },
	{ "Igrave", XK_Igrave },
	{ "Iacute", XK_Iacute },
	{ "Icircumflex", XK_Icircumflex },
	{ "Idiaeresis", XK_Idiaeresis },
	{ "ETH", XK_ETH },
	{ "Eth", XK_Eth },
	{ "Ntilde", XK_Ntilde },
	{ "Ograve", XK_Ograve },
	{ "Oacute", XK_Oacute },
	{ "Ocircumflex", XK_Ocircumflex },
	{ "Otilde", XK_Otilde },
	{ "Odiaeresis", XK_Odiaeresis },
	{ "multiply", XK_multiply },
	{ "Ooblique", XK_Ooblique },
	{ "Ugrave", XK_Ugrave },
	{ "Uacute", XK_Uacute },
	{ "Ucircumflex", XK_Ucircumflex },
	{ "Udiaeresis", XK_Udiaeresis },
	{ "Yacute", XK_Yacute },
	{ "THORN", XK_THORN },
	{ "Thorn", XK_Thorn },
	{ "ssharp", XK_ssharp },
	{ "agrave", XK_agrave },
	{ "aacute", XK_aacute },
	{ "acircumflex", XK_acircumflex },
	{ "atilde", XK_atilde },
	{ "adiaeresis", XK_adiaeresis },
	{ "aring", XK_aring },
	{ "ae", XK_ae },
	{ "ccedilla", XK_ccedilla },
	{ "egrave", XK_egrave },
	{ "eacute", XK_eacute },
	{ "ecircumflex", XK_ecircumflex },
	{ "ediaeresis", XK_ediaeresis },
	{ "igrave", XK_igrave },
	{ "iacute", XK_iacute },
	{ "icircumflex", XK_icircumflex },
	{ "idiaeresis", XK_idiaeresis },
	{ "eth", XK_eth },
	{ "ntilde", XK_ntilde },
	{ "ograve", XK_ograve },
	{ "oacute", XK_oacute },
	{ "ocircumflex", XK_ocircumflex },
	{ "otilde", XK_otilde },
	{ "odiaeresis", XK_odiaeresis },
	{ "division", XK_division },
	{ "oslash", XK_oslash },
	{ "ugrave", XK_ugrave },
	{ "uacute", XK_uacute },
	{ "ucircumflex", XK_ucircumflex },
	{ "udiaeresis", XK_udiaeresis },
	{ "yacute", XK_yacute },
	{ "thorn", XK_thorn },
	{ "ydiaeresis", XK_ydiaeresis },
	{ (char *)NULL, NoSymbol }
};

KeySym
XStringToKeysym(const String s)
{
	int i;

	if (strlen(s) == 1 && (*(unsigned char *)s & 0x7f) > ' ')
		return (KeySym)*(unsigned char *)s;
	for (i = 0; latin1[i].name != (char *)NULL; i++) {
		if (!strcmp(s, latin1[i].name))
			return latin1[i].keysym;
	}
	return NoSymbol;
}

/* Timeouts. */
typedef struct timeout {
	struct timeout *next;
	long interval;
	XtTimerCallbackProc proc;
	XtPointer closure;
} timeout_t;
timeout_t *timeouts = (timeout_t *)NULL;

XtIntervalId
XtAppAddTimeOut(XtAppContext app_context, unsigned long interval,
    XtTimerCallbackProc proc, XtPointer closure)
{
	timeout_t *t_new;
	timeout_t *t;
	timeout_t *prev = (timeout_t *)NULL;
	unsigned long accum = 0L;

	t_new = (timeout_t *)XtMalloc(sizeof(timeout_t));
	t_new->proc = proc;
	t_new->closure = closure;

	/* Find where to insert this item. */
	for (t = timeouts; t != (timeout_t *)NULL; t = t->next) {
		if (accum + t->interval > interval)
			break;
		accum += t->interval;
		prev = t;
	}

	/* Insert it. */
	t_new->interval = interval - accum;
	if (prev == (timeout_t *)NULL) {	/* Front. */
		t_new->next = timeouts;
		if (timeouts != (timeout_t *)NULL)
			timeouts->interval -= interval;
		timeouts = t_new;
	} else if (t == (timeout_t *)NULL) {	/* Rear. */
		t_new->next = (timeout_t *)NULL;
		prev->next = t_new;
	} else {				/* Middle. */
		t->interval -= t->interval;
		t_new->next = t;
		prev->next = t_new;
	}

	return (XtIntervalId)t_new;
}

void
XtRemoveTimeOut(XtIntervalId timer)
{
	timeout_t *t;
	timeout_t *prev = (timeout_t *)NULL;

	for (t = timeouts; t != (timeout_t *)NULL; t = t->next) {
		if (t == (timeout_t *)timer) {
			if (t->next != (timeout_t *)NULL)
				t->next->interval += t->interval;
			if (prev != (timeout_t *)NULL)
				prev->next = t->next;
			else
				timeouts = t->next;
			XtFree((XtPointer)t);
			return;
		}
		prev = t;
	}
}

/* Input events. */
typedef struct input {
	struct input *next;
	int source;
	XtPointer condition;
	XtInputCallbackProc proc;
	XtPointer closure;
} input_t;
input_t *inputs = (input_t *)NULL;
Boolean inputs_changed = False;

XtInputId
XtAppAddInput(XtAppContext app_context, int source, XtPointer condition,
    XtInputCallbackProc proc, XtPointer closure)
{
	input_t *ip;

	ip = (input_t *)XtMalloc(sizeof(input_t));
	ip->source = source;
	ip->condition = condition;
	ip->proc = proc;
	ip->closure = closure;
	ip->next = inputs;
	inputs = ip;
	inputs_changed = True;
	return (XtInputId)ip;
}

void
XtRemoveInput(XtInputId id)
{
	input_t *ip;
	input_t *prev = (input_t *)NULL;

	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if (ip == (input_t *)id)
			break;
		prev = ip;
	}
	if (ip == (input_t *)NULL)
		return;
	if (prev != (input_t *)NULL)
		prev->next = ip->next;
	else
		inputs = ip->next;
	XtFree((XtPointer)ip);
	inputs_changed = True;
}

/* Event dispatcher. */
void
XtAppProcessEvent(XtAppContext app_context, XtInputMask mask)
{
	input_t *ip;
	fd_set rfds, wfds, xfds;
	int ns;
	struct timeval t0, t1, twait, *tp;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);
	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if ((unsigned long)ip->condition & XtInputReadMask)
			FD_SET(ip->source, &rfds);
		if ((unsigned long)ip->condition & XtInputWriteMask)
			FD_SET(ip->source, &wfds);
		if ((unsigned long)ip->condition & XtInputExceptMask)
			FD_SET(ip->source, &xfds);
	}
	if (timeouts != (struct timeout *)NULL) {
		(void) gettimeofday(&t0, (void *)NULL);
		twait.tv_sec = timeouts->interval / 1000L;
		twait.tv_usec = (timeouts->interval % 1000L) * 1000L;
		tp = &twait;
	} else {
		tp = (struct timeval *)NULL;
	}
	ns = select(FD_SETSIZE, &rfds, &wfds, &xfds, tp);
	if (ns < 0) {
		XtWarning("XtAppProcessEvent: select() failed");
		return;
	}
    retry:
	inputs_changed = False;
	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if (((unsigned long)ip->condition & XtInputReadMask) &&
		    FD_ISSET(ip->source, &rfds)) {
			(*ip->proc)(ip->closure, &ip->source, (XtInputId *)ip);
			if (inputs_changed)
				goto retry;
		}
		if (((unsigned long)ip->condition & XtInputWriteMask) &&
		    FD_ISSET(ip->source, &wfds)) {
			(*ip->proc)(ip->closure, &ip->source, (XtInputId *)ip);
			if (inputs_changed)
				goto retry;
		}
		if (((unsigned long)ip->condition & XtInputExceptMask) &&
		    FD_ISSET(ip->source, &xfds)) {
			(*ip->proc)(ip->closure, &ip->source, (XtInputId *)ip);
			if (inputs_changed)
				goto retry;
		}
	}
	if (timeouts) {
		/* Subtract the elapsed time from the lead element(s). */
		long msec;
		timeout_t *t;

		(void) gettimeofday(&t1, (void *)NULL);
		msec = ((t1.tv_sec - t0.tv_sec)) * 1000L + 
		       ((t1.tv_usec / 1000L) - (t0.tv_usec / 1000L));
		while ((t = timeouts) != (timeout_t *)NULL &&
		       msec >= t->interval) {
			msec -= t->interval;
			timeouts = t->next;
			(*t->proc)(t->closure, (XtIntervalId *)t);
			XtFree((XtPointer)t);
		}
	}
}
