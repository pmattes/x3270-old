/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA 30332.
 *  All Rights Reserved.  GTRC hereby grants public use of this software.
 *  Derivative works based on this software must incorporate this copyright
 *  notice.
 *
 * X11 Port Copyright 1990 by Jeff Sparkes.
 * Additional X11 Modifications Copyright 1993, 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	screen.h
 *
 *		Screen definitions for x3270.
 */

#define fCHAR_WIDTH(f)	((f)->max_bounds.width)
#define CHAR_WIDTH	fCHAR_WIDTH(*efontinfo)
#define fCHAR_HEIGHT(f)	((f)->ascent + (f)->descent)
#define CHAR_HEIGHT	fCHAR_HEIGHT(*efontinfo)

#define HHALO	2	/* number of pixels to pad screen left-right */
#define VHALO	1	/* number of pixels to pad screen top-bottom */

#define X_TO_COL(x_pos)	(((x_pos)-HHALO) / *char_width)
#define Y_TO_ROW(y_pos)	(((y_pos)-VHALO) / *char_height)
#define COL_TO_X(col)	(((col) * *char_width)+HHALO)
#define ROW_TO_Y(row)	((((row) * *char_height) + *char_height)+VHALO)

#define ssX_TO_COL(x_pos) (((x_pos)-HHALO) / ss->char_width)
#define ssY_TO_ROW(y_pos) (((y_pos)-VHALO) / ss->char_height)
#define ssCOL_TO_X(col)	(((col) * ss->char_width)+HHALO)
#define ssROW_TO_Y(row)	((((row) * ss->char_height) + ss->char_height)+VHALO)

#define SGAP	((*efontinfo)->descent+3) 	/* gap between screen
						   and status line */

/* selections */

#define SELECTED(baddr)		(selected[(baddr)/8] & (1 << ((baddr)%8)))
#define SET_SELECT(baddr)	(selected[(baddr)/8] |= (1 << ((baddr)%8)))
#define SEL_BIT			4
#define IMAGE_SEL		(SEL_BIT << 8)
