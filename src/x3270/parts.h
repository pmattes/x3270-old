/*
 * Copyright 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	parts.h
 *		#defines for the optional parts of x3270.
 */

#define X3270_ANSI	1	/* ANSI mode */
#define X3270_APL	1	/* APL keysym support */
#define X3270_DISPLAY	1	/* X-windows display */
#define X3270_FT	1	/* IND$FILE file transfer */
#define X3270_KEYPAD	1	/* pop-up keypad */
#define X3270_LOCAL_PROCESS 1	/* -e <cmd> support on Linux and BSD */
#define X3270_MENUS	1	/* menu bar */
#define X3270_PRINTER	1	/* printer session support */
#define X3270_SCRIPT	1	/* scripting */
#define X3270_TN3270E	1	/* TN3270E support */
#define X3270_TRACE	1	/* data stream and X event tracing */

#if defined(X3270_TN3270E) && !defined(X3270_ANSI) /*[*/
#define X3270_ANSI	1	/* RFC2355 requires NVT mode */
#endif /*]*/
