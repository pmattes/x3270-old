/*
 * (from) $XConsortium: SmeBSBP.h,v 1.6 89/12/11 15:20:15 kit Exp $
 *
 * Modifications Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */

/* 
 * CmeP.h - Private definitions for Cme object
 * (from) SmeP.h - Private definitions for Sme object
 * 
 */

#ifndef _XawCmeBSBP_h
#define _XawCmeBSBP_h

/***********************************************************************
 *
 * Cme Object Private Data
 *
 ***********************************************************************/

#include "CmeP.h"
#include "CmeBSB.h"

/************************************************************
 *
 * New fields for the Cme Object class record.
 *
 ************************************************************/

typedef struct _CmeBSBClassPart {
  XtPointer extension;
} CmeBSBClassPart;

/* Full class record declaration */
typedef struct _CmeBSBClassRec {
    RectObjClassPart       rect_class;
    CmeClassPart     cme_class;
    CmeBSBClassPart  cme_bsb_class;
} CmeBSBClassRec;

extern CmeBSBClassRec cmeBSBClassRec;

/* New fields for the Cme Object record */
typedef struct {
    /* resources */
    String label;		/* The entry label. */
    int vert_space;		/* extra vert space to leave, as a percentage
				   of the font height of the label. */
    Pixmap left_bitmap, right_bitmap; /* bitmaps to show. */
    Dimension left_margin, right_margin; /* left and right margins. */
    Pixel foreground;		/* foreground color. */
    XFontStruct * font;		/* The font to show label in. */
    XtJustify justify;		/* Justification for the label. */
    String menu_name;		/* The submenu to pop up. */

/* private resources. */

    Boolean set_values_area_cleared; /* Remember if we need to unhighlight. */
    GC norm_gc;			/* noral color gc. */
    GC rev_gc;			/* reverse color gc. */
    GC norm_gray_gc;		/* Normal color (grayed out) gc. */
    GC invert_gc;		/* gc for flipping colors. */
    Boolean ticking;		/* is the pop-up timer ticking? */
    XtIntervalId id;		/* pop-up timer id */

    Dimension left_bitmap_width; /* size of each bitmap. */
    Dimension left_bitmap_height;
    Dimension right_bitmap_width;
    Dimension right_bitmap_height;

} CmeBSBPart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _CmeBSBRec {
  ObjectPart         object;
  RectObjPart        rectangle;
  CmePart	     cme;
  CmeBSBPart   cme_bsb;
} CmeBSBRec;

/************************************************************
 *
 * Private declarations.
 *
 ************************************************************/

#endif /* _XawCmeBSBP_h */
