/*
 * Copyright 1993, 1994, 1995, 1999, 2000, 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	seec.h
 *		Declarations for see.c
 *
 */

#if defined(X3270_TRACE) /*[*/

extern const char *see_aid(unsigned char code);
extern const char *see_attr(unsigned char fa);
extern const char *see_color(unsigned char setting);
extern const char *see_ebc(unsigned char ch);
extern const char *see_efa(unsigned char efa, unsigned char value);
extern const char *see_efa_only(unsigned char efa);
extern const char *see_qcode(unsigned char id);
extern const char *unknown(unsigned char value);

#else /*][*/

#define see_aid 0 &&
#define see_attr 0 &&
#define see_color 0 &&
#define see_ebc 0 &&
#define see_efa 0 &&
#define see_efa_only 0 &&
#define see_qcode 0 &&
#define unknown 0 &&

#endif /*]*/