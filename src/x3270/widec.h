/*
 * Copyright 2002 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * x3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	widec.h
 *		Global declarations for wide.c.
 */

extern int wide_init(const char *csname);
extern void wchar_to_dbcs(unsigned char c1, unsigned char c2,
    unsigned char ebc[]);
extern void dbcs_to_wchar(unsigned char ebc1, unsigned char ebc2,
    unsigned char c[]);
extern void dbcs_to_unicode16(unsigned char ebc1, unsigned char ebc2,
    unsigned char c[]);
