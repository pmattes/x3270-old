/*
 * Copyright 1993, 1994, 1995, 1999, 2000, 2001 by Paul Mattes.
 * Parts Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	util.c
 *		Utility functions for x3270
 */

#include "globals.h"
#include <pwd.h>
#include <stdarg.h>
#include "resources.h"

#include "utilc.h"

#define my_isspace(c)	isspace((unsigned char)c)

/*
 * Cheesy internal version of sprintf that allocates its own memory.
 */
static char *
xs_vsprintf(const char *fmt, va_list args)
{
	char *r;
#if defined(HAVE_VASPRINTF) /*[*/
	(void) vasprintf(&r, fmt, args);
	if (r == CN)
		Error("Out of memory");
	return r;
#else /*][*/
	char buf[16384];
	int nc;

	nc = vsprintf(buf, fmt, args);
	if (nc > sizeof(buf))
		Error("Internal buffer overflow");
	r = Malloc(nc + 1);
	return strcpy(r, buf);
#endif /*]*/
}

/*
 * Common helper functions to insert strings, through a template, into a new
 * buffer.
 * 'format' is assumed to be a printf format string with '%s's in it.
 */
char *
xs_buffer(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
	va_end(args);
	return r;
}

/* Common uses of xs_buffer. */
void
xs_warning(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
	va_end(args);
	Warning(r);
	Free(r);
}

void
xs_error(const char *fmt, ...)
{
	va_list args;
	char *r;

	va_start(args, fmt);
	r = xs_vsprintf(fmt, args);
	va_end(args);
	Error(r);
	Free(r);
}

/* Prettyprinter for strings with unprintable data. */
void
fcatv(FILE *f, char *s)
{
	char c;

	while ((c = *s++)) {
		switch (c) {
		    case '\n':
			(void) fprintf(f, "\\n");
			break;
		    case '\t':
			(void) fprintf(f, "\\t");
			break;
		    case '\b':
			(void) fprintf(f, "\\b");
			break;
		    default:
			if ((c & 0x7f) < ' ')
				(void) fprintf(f, "\\%03o", c & 0xff);
			else
				fputc(c, f);
			break;
		}
	}
}

/*
 * Definition resource splitter, for resources of the repeating form:
 *	left: right\n
 *
 * Can be called iteratively to parse a list.
 * Returns 1 for success, 0 for EOF, -1 for error.
 *
 * Note: Modifies the input string.
 */
int
split_dresource(char **st, char **left, char **right)
{
	char *s = *st;
	char *t;
	Boolean quote;

	/* Skip leading white space. */
	while (my_isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* There must be a left-hand side. */
	if (*s == ':')
		return -1;

	/* Scan until an unquoted colon is found. */
	*left = s;
	for (; *s && *s != ':' && *s != '\n'; s++)
		if (*s == '\\' && *(s+1) == ':')
			s++;
	if (*s != ':')
		return -1;

	/* Stip white space before the colon. */
	for (t = s-1; my_isspace(*t); t--)
		*t = '\0';

	/* Terminate the left-hand side. */
	*(s++) = '\0';

	/* Skip white space after the colon. */
	while (*s != '\n' && my_isspace(*s))
		s++;

	/* There must be a right-hand side. */
	if (!*s || *s == '\n')
		return -1;

	/* Scan until an unquoted newline is found. */
	*right = s;
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (my_isspace(*t))
		*t-- = '\0';

	/* Done. */
	return 1;
}

#if defined(X3270_DISPLAY) /*[*/
/*
 * List resource splitter, for lists of elements speparated by newlines.
 *
 * Can be called iteratively.
 * Returns 1 for success, 0 for EOF, -1 for error.
 */
int
split_lresource(char **st, char **value)
{
	char *s = *st;
	char *t;
	Boolean quote;

	/* Skip leading white space. */
	while (my_isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* Save starting point. */
	*value = s;

	/* Scan until an unquoted newline is found. */
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (my_isspace(*t))
		*t-- = '\0';

	/* Done. */
	return 1;
}
#endif /*]*/

const char *
get_message(const char *key)
{
	static char namebuf[128];
	char *r;

	(void) sprintf(namebuf, "%s.%s", ResMessage, key);
	if ((r = get_resource(namebuf)) != CN)
		return r;
	else {
		(void) sprintf(namebuf, "[missing \"%s\" message]", key);
		return namebuf;
	}
}

#define ex_getenv getenv

/* Variable and tilde substitution functions. */
static char *
var_subst(const char *s)
{
	enum { VS_BASE, VS_QUOTE, VS_DOLLAR, VS_BRACE, VS_VN, VS_VNB, VS_EOF }
	    state = VS_BASE;
	char c;
	int o_len = strlen(s) + 1;
	char *ob;
	char *o;
	const char *vn_start = CN;

	if (strchr(s, '$') == CN)
		return NewString(s);

	o_len = strlen(s) + 1;
	ob = Malloc(o_len);
	o = ob;
#	define LBR	'{'
#	define RBR	'}'

	while (state != VS_EOF) {
		c = *s;
		switch (state) {
		    case VS_BASE:
			if (c == '\\')
			    state = VS_QUOTE;
			else if (c == '$')
			    state = VS_DOLLAR;
			else
			    *o++ = c;
			break;
		    case VS_QUOTE:
			if (c == '$') {
				*o++ = c;
				o_len--;
			} else {
				*o++ = '\\';
				*o++ = c;
			}
			state = VS_BASE;
			break;
		    case VS_DOLLAR:
			if (c == LBR)
				state = VS_BRACE;
			else if (isalpha(c) || c == '_') {
				vn_start = s;
				state = VS_VN;
			} else {
				*o++ = '$';
				*o++ = c;
				state = VS_BASE;
			}
			break;
		    case VS_BRACE:
			if (isalpha(c) || c == '_') {
				vn_start = s;
				state = VS_VNB;
			} else {
				*o++ = '$';
				*o++ = LBR;
				*o++ = c;
				state = VS_BASE;
			}
			break;
		    case VS_VN:
		    case VS_VNB:
			if (!(isalnum(c) || c == '_')) {
				int vn_len;
				char *vn;
				char *vv;

				vn_len = s - vn_start;
				if (state == VS_VNB && c != RBR) {
					*o++ = '$';
					*o++ = LBR;
					(void) strncpy(o, vn_start, vn_len);
					o += vn_len;
					state = VS_BASE;
					continue;	/* rescan */
				}
				vn = Malloc(vn_len + 1);
				(void) strncpy(vn, vn_start, vn_len);
				vn[vn_len] = '\0';
				if ((vv = ex_getenv(vn))) {
					*o = '\0';
					o_len = o_len
					    - 1			/* '$' */
					    - (state == VS_VNB)	/* { */
					    - vn_len		/* name */
					    - (state == VS_VNB)	/* } */
					    + strlen(vv);
					ob = Realloc(ob, o_len);
					o = strchr(ob, '\0');
					(void) strcpy(o, vv);
					o += strlen(vv);
				}
				Free(vn);
				if (state == VS_VNB) {
					state = VS_BASE;
					break;
				} else {
					/* Rescan this character */
					state = VS_BASE;
					continue;
				}
			}
			break;
		    case VS_EOF:
			break;
		}
		s++;
		if (c == '\0')
			state = VS_EOF;
	}
	return ob;
}

/*
 * Do tilde (home directory) substitution on a string.  Returns a malloc'd
 * result.
 */
static char *
tilde_subst(const char *s)
{
	char *slash;
	const char *name;
	const char *rest;
	struct passwd *p;
	char *r;
	char *mname = CN;

	/* Does it start with a "~"? */
	if (*s != '~')
		return NewString(s);

	/* Terminate with "/". */
	slash = strchr(s, '/');
	if (slash) {
		int len = slash - s;

		mname = Malloc(len + 1);
		(void) strncpy(mname, s, len);
		mname[len] = '\0';
		name = mname;
		rest = slash;
	} else {
		name = s;
		rest = strchr(name, '\0');
	}

	/* Look it up. */
	if (!strcmp(name, "~"))	/* this user */
		p = getpwuid(getuid());
	else			/* somebody else */
		p = getpwnam(name + 1);

	/* Free any temporary copy. */
	if (mname != CN)
		Free(mname);

	/* Substitute and return. */
	if (p == (struct passwd *)NULL)
		r = NewString(s);
	else {
		r = Malloc(strlen(p->pw_dir) + strlen(rest) + 1);
		(void) strcpy(r, p->pw_dir);
		(void) strcat(r, rest);
	}
	return r;
}

char *
do_subst(const char *s, Boolean do_vars, Boolean do_tilde)
{
	if (!do_vars && !do_tilde)
		return NewString(s);

	if (do_vars) {
		char *t;

		t = var_subst(s);
		if (do_tilde) {
			char *u;

			u = tilde_subst(t);
			Free(t);
			return u;
		}
		return t;
	}

	return tilde_subst(s);
}

/*
 * ctl_see
 *	Expands a character in the manner of "cat -v".
 */
char *
ctl_see(int c)
{
	static char	buf[64];
	char	*p = buf;

	c &= 0xff;
	if ((c & 0x80) && (c <= 0xa0)) {
		*p++ = 'M';
		*p++ = '-';
		c &= 0x7f;
	}
	if (c >= ' ' && c != 0x7f) {
		*p++ = c;
	} else {
		*p++ = '^';
		if (c == 0x7f) {
			*p++ = '?';
		} else {
			*p++ = c + '@';
		}
	}
	*p = '\0';
	return buf;
}

#if defined(X3270_DISPLAY) /*[*/

/* Glue between x3270 and the X libraries. */

/*
 * A way to work around problems with Xt resources.  It seems to be impossible
 * to get arbitrarily named resources.  Someday this should be hacked to
 * add classes too.
 */
char *
get_resource(const char *name)
{
	XrmValue value;
	char *type;
	char *str;
	char *r = CN;

	str = xs_buffer("%s.%s", XtName(toplevel), name);
	if ((XrmGetResource(rdb, str, 0, &type, &value) == True) && *value.addr)
		r = value.addr;
	XtFree(str);
	return r;
}

/*
 * Input callbacks.
 */
typedef void voidfn(void);
static void
io_fn(XtPointer closure, int *source unused, XtInputId *id unused)
{
	voidfn *fn = (voidfn *)closure;

	(*fn)();
}

unsigned long
AddInput(int sock, voidfn *fn)
{
	return XtAppAddInput(appcontext, sock, (XtPointer) XtInputReadMask,
		io_fn, (XtPointer)fn);
}

unsigned long
AddExcept(int sock, voidfn *fn)
{
	return XtAppAddInput(appcontext, sock, (XtPointer) XtInputExceptMask,
		io_fn, (XtPointer)fn);
}

void
RemoveInput(unsigned long cookie)
{
	XtRemoveInput((XtInputId)cookie);
}

/*
 * Timer callbacks.
 */
static void
to_fn(XtPointer closure, XtIntervalId *id)
{
	voidfn *fn = (voidfn *)closure;

	(*fn)();
}

unsigned long
AddTimeOut(unsigned long msec, voidfn *fn)
{
	return (unsigned long) XtAppAddTimeOut(appcontext, msec, to_fn,
		 (XtPointer)fn);
}

void
RemoveTimeOut(unsigned long cookie)
{
	XtRemoveTimeOut((XtIntervalId)cookie);
}

KeySym
StringToKeysym(char *s)
{
	return XStringToKeysym(s);
}
#endif /*]*/
