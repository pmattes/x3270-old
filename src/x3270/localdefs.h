/*
 * Copyright 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	localdefs.h
 *		Local definitions for x3270.
 *
 *		This file contains definitions for environment-specific
 *		facilities, such as memory allocation, I/O registration,
 *		and timers.
 */

/* Use X for this stuff. */
#include <X11/Intrinsic.h>

#define Malloc(n)	XtMalloc(n)
#define Free(p)		XtFree((XtPointer)p)
#define Calloc(n, s)	XtCalloc(n, s)
#define Realloc(p, s)	XtRealloc((XtPointer)p, s)
#define NewString(s)	XtNewString(s)

#define Error(s)	XtError(s)
#define Warning(s)	XtWarning(s)