/*
 * Copyright 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* s3270 version of popupsc.h */

extern void popup_an_errno(int errn, const char *fmt, ...);
extern void popup_an_error(const char *fmt, ...);
extern void action_output(const char *fmt, ...);
