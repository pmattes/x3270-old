/*
 * Copyright 1999, 2000, 2001 by Paul Mattes.
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

#define InputReadMask	0x1
#define InputExceptMask	0x2
#define InputWriteMask	0x4

#define MILLION		1000000L

void (*Warning_redirect)(const char *) = NULL;

void
Error(const char *s)
{
	fprintf(stderr, "Error: %s\n", s);
	exit(1);
}

void
Warning(const char *s)
{
	if (Warning_redirect != NULL)
		(*Warning_redirect)(s);
	else
		fprintf(stderr, "Warning: %s\n", s);
}

void *
Malloc(size_t len)
{
	char *r;

	r = malloc(len);
	if (r == (char *)NULL)
		Error("Out of memory");
	return r;
}

void *
Calloc(size_t nelem, size_t elsize)
{
	char *r;

	r = malloc(nelem * elsize);
	if (r == (char *)NULL)
		Error("Out of memory");
	return memset(r, '\0', nelem * elsize);
}

void *
Realloc(void *p, size_t len)
{
	p = realloc(p, len);
	if (p == NULL)
		Error("Out of memory");
	return p;
}

void
Free(void *p)
{
	if (p != NULL)
		free(p);
}

char *
NewString(const char *s)
{
	return strcpy(Malloc(strlen(s) + 1), s);
}

static struct {
	/*const*/ char *name;	/* not const because of ancient X11 API */
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

	/*
	 * The following are, umm, hacks to allow symbolic names for
	 * control codes.
	 */
	{ "BackSpace", 0x08 },
	{ "Tab", 0x09 },
	{ "Linefeed", 0x0a },
	{ "Return", 0x0d },
	{ "Escape", 0x1b },
	{ "Delete", 0xff },

	{ (char *)NULL, NoSymbol }
};

KeySym
StringToKeysym(char *s)
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

char *
KeysymToString(KeySym k)
{
	int i;

	for (i = 0; latin1[i].name != (char *)NULL; i++) {
		if (latin1[i].keysym == k)
			return latin1[i].name;
	}
	return (char *)NULL;
}

/* Timeouts. */
typedef struct timeout {
	struct timeout *next;
	struct timeval tv;
	void (*proc)(void);
} timeout_t;
#define TN	(timeout_t *)NULL
static timeout_t *timeouts = TN;

unsigned long
AddTimeOut(unsigned long interval, void (*proc)(void))
{
	timeout_t *t_new;
	timeout_t *t;
	timeout_t *prev = TN;

	t_new = (timeout_t *)Malloc(sizeof(timeout_t));
	t_new->proc = proc;
	(void) gettimeofday(&t_new->tv, NULL);
	t_new->tv.tv_sec += interval / 1000L;
	t_new->tv.tv_usec += (interval % 1000L) * 1000L;
	if (t_new->tv.tv_usec > MILLION) {
		t_new->tv.tv_sec += t_new->tv.tv_usec / MILLION;
		t_new->tv.tv_usec %= MILLION;
	}

	/* Find where to insert this item. */
	for (t = timeouts; t != TN; t = t->next) {
		if (t->tv.tv_sec > t_new->tv.tv_sec ||
		    (t->tv.tv_sec == t_new->tv.tv_sec &&
		     t->tv.tv_usec > t_new->tv.tv_usec))
			break;
		prev = t;
	}

	/* Insert it. */
	if (prev == TN) {	/* Front. */
		t_new->next = timeouts;
		timeouts = t_new;
	} else if (t == TN) {	/* Rear. */
		t_new->next = TN;
		prev->next = t_new;
	} else {				/* Middle. */
		t_new->next = t;
		prev->next = t_new;
	}

	return (unsigned long)t_new;
}

void
RemoveTimeOut(unsigned long timer)
{
	timeout_t *t;
	timeout_t *prev = TN;

	for (t = timeouts; t != TN; t = t->next) {
		if (t == (timeout_t *)timer) {
			if (prev != TN)
				prev->next = t->next;
			else
				timeouts = t->next;
			Free(t);
			return;
		}
		prev = t;
	}
}

/* Input events. */ 
typedef struct input {  
        struct input *next;
        int source; 
        int condition;
        void (*proc)(void);
} input_t;          
static input_t *inputs = (input_t *)NULL;
static Boolean inputs_changed = False;

unsigned long
AddInput(int source, void (*fn)(void))
{
	input_t *ip;

	ip = (input_t *)Malloc(sizeof(input_t));
	ip->source = source;
	ip->condition = InputReadMask;
	ip->proc = fn;
	ip->next = inputs;
	inputs = ip;
	inputs_changed = True;
	return (unsigned long)ip;
}

unsigned long
AddExcept(int source, void (*fn)(void))
{
	input_t *ip;

	ip = (input_t *)Malloc(sizeof(input_t));
	ip->source = source;
	ip->condition = InputExceptMask;
	ip->proc = fn;
	ip->next = inputs;
	inputs = ip;
	inputs_changed = True;
	return (unsigned long)ip;
}

unsigned long
AddOutput(int source, void (*fn)(void))
{
	input_t *ip;

	ip = (input_t *)Malloc(sizeof(input_t));
	ip->source = source;
	ip->condition = InputWriteMask;
	ip->proc = fn;
	ip->next = inputs;
	inputs = ip;
	inputs_changed = True;
	return (unsigned long)ip;
}

void
RemoveInput(unsigned long id)
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
	Free(ip);
	inputs_changed = True;
}

/*
 * Modify the passed-in parameters so they reflect the values needed for
 * select().
 */
int
select_setup(int *nfds, fd_set *readfds, fd_set *writefds, 
    fd_set *exceptfds, struct timeval **timeout, struct timeval *timebuf)
{
	input_t *ip;
	int r = 0;

	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if ((unsigned long)ip->condition & InputReadMask) {
			FD_SET(ip->source, readfds);
			if (ip->source >= *nfds)
				*nfds = ip->source + 1;
			r = 1;
		}
		if ((unsigned long)ip->condition & InputWriteMask) {
			FD_SET(ip->source, writefds);
			if (ip->source >= *nfds)
				*nfds = ip->source + 1;
			r = 1;
		}
		if ((unsigned long)ip->condition & InputExceptMask) {
			FD_SET(ip->source, exceptfds);
			if (ip->source >= *nfds)
				*nfds = ip->source + 1;
			r = 1;
		}
	}
	if (timeouts != TN) {
		struct timeval now, twait;

		(void) gettimeofday(&now, (void *)NULL);
		twait.tv_sec = timeouts->tv.tv_sec - now.tv_sec;
		twait.tv_usec = timeouts->tv.tv_usec - now.tv_usec;
		if (twait.tv_usec < 0L) {
			twait.tv_sec--;
			twait.tv_usec += MILLION;
		}
		if (twait.tv_sec < 0L)
			twait.tv_sec = twait.tv_usec = 0L;

		if (*timeout == NULL) {
			/* No timeout yet -- we're it. */
			*timebuf = twait;
			*timeout = timebuf;
			r = 1;
		} else if (twait.tv_sec < (*timeout)->tv_sec ||
		           (twait.tv_sec == (*timeout)->tv_sec &&
		            twait.tv_usec < (*timeout)->tv_usec)) {
			/* We're sooner than what they're waiting for. */
			**timeout = twait;
			r = 1;
		}
	}
	return r;
}

/* Event dispatcher. */
Boolean
process_events(Boolean block)
{
	input_t *ip;
	fd_set rfds, wfds, xfds;
	int ns;
	struct timeval now, twait, *tp;
	struct timeout *t;
	Boolean any_events;
	Boolean processed_any = False;

	processed_any = False;
    retry:
	/* If we've processed any input, then don't block again. */
	if (processed_any)
		block = False;
	any_events = False;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);
	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if ((unsigned long)ip->condition & InputReadMask) {
			FD_SET(ip->source, &rfds);
			any_events = True;
		}
		if ((unsigned long)ip->condition & InputWriteMask) {
			FD_SET(ip->source, &wfds);
			any_events = True;
		}
		if ((unsigned long)ip->condition & InputExceptMask) {
			FD_SET(ip->source, &xfds);
			any_events = True;
		}
	}
	if (block) {
		if (timeouts != TN) {
			(void) gettimeofday(&now, (void *)NULL);
			twait.tv_sec = timeouts->tv.tv_sec - now.tv_sec;
			twait.tv_usec = timeouts->tv.tv_usec - now.tv_usec;
			if (twait.tv_usec < 0L) {
				twait.tv_sec--;
				twait.tv_usec += MILLION;
			}
			if (twait.tv_sec < 0L)
				twait.tv_sec = twait.tv_usec = 0L;
			tp = &twait;
			any_events = True;
		} else {
			tp = (struct timeval *)NULL;
		}
	} else {
		twait.tv_sec = twait.tv_usec = 0L;
		tp = &twait;
	}

	if (!any_events)
		return processed_any;
	ns = select(FD_SETSIZE, &rfds, &wfds, &xfds, tp);
	if (ns < 0) {
		Warning("process_events: select() failed");
		return processed_any;
	}
	inputs_changed = False;
	for (ip = inputs; ip != (input_t *)NULL; ip = ip->next) {
		if (((unsigned long)ip->condition & InputReadMask) &&
		    FD_ISSET(ip->source, &rfds)) {
			(*ip->proc)();
			processed_any = True;
			if (inputs_changed)
				goto retry;
		}
		if (((unsigned long)ip->condition & InputWriteMask) &&
		    FD_ISSET(ip->source, &wfds)) {
			(*ip->proc)();
			processed_any = True;
			if (inputs_changed)
				goto retry;
		}
		if (((unsigned long)ip->condition & InputExceptMask) &&
		    FD_ISSET(ip->source, &xfds)) {
			(*ip->proc)();
			processed_any = True;
			if (inputs_changed)
				goto retry;
		}
	}

	/* See what's expired. */
	if (timeouts != TN) {
		(void) gettimeofday(&now, (void *)NULL);
		while ((t = timeouts) != TN) {
			if (t->tv.tv_sec < now.tv_sec ||
			    (t->tv.tv_sec == now.tv_sec &&
			     t->tv.tv_usec < now.tv_usec)) {
				timeouts = t->next;
				(*t->proc)();
				processed_any = True;
				Free(t);
			} else
				break;
		}
	}
	if (inputs_changed)
		goto retry;

	return processed_any;
}
