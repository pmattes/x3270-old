/* (from) $XConsortium: Sme.c,v 1.9 91/02/17 16:44:14 rws Exp $ */

/*
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
 */

/*
 * Cme.c - Source code for the generic menu entry
 * (from) Sme.c - Source code for the generic menu entry
 *
 * Date:    September 26, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium 
 *          kit@expo.lcs.mit.edu
 */

#include "globals.h"
#if defined(X3270_MENUS) /*[*/

#include <stdio.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/XawInit.h>
#include "CmeP.h"
#include <X11/Xaw/Cardinals.h>

#define offset(field) XtOffsetOf(CmeRec, cme.field)
static XtResource resources[] = {
  {XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
     offset(callbacks), XtRCallback, (XtPointer)NULL},
};   
#undef offset

/*
 * Semi Public function definitions. 
 */

static void Unhighlight(), Highlight(), Notify(), ClassPartInitialize();
static void Initialize();
static XtGeometryResult QueryGeometry();

#define SUPERCLASS (&rectObjClassRec)

CmeClassRec cmeClassRec = {
  {
    /* superclass         */    (WidgetClass) SUPERCLASS,
    /* class_name         */    "Cme",
    /* size               */    sizeof(CmeRec),
    /* class_initialize   */	XawInitializeWidgetSet,
    /* class_part_initialize*/	ClassPartInitialize,
    /* Class init'ed      */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    NULL,
    /* actions            */    NULL,
    /* num_actions        */    ZERO,
    /* resources          */    resources,
    /* resource_count     */	XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE, 
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/ 	FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    NULL,
    /* expose             */    NULL,
    /* set_values         */    NULL,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */	XtInheritSetValuesAlmost,  
    /* get_values_hook    */	NULL,			
    /* accept_focus       */    NULL,
    /* intrinsics version */	XtVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    NULL,
    /* query_geometry	  */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* Complex Menu Entry Fields */
      
    /* highlight */             Highlight,
    /* unhighlight */           Unhighlight,
    /* notify */		Notify,		
    /* extension */             NULL				
  }
};

WidgetClass cmeObjectClass = (WidgetClass) &cmeClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*	Function Name: ClassPartInitialize
 *	Description: handles inheritance of class functions.
 *	Arguments: class - the widget classs of this widget.
 *	Returns: none.
 */

static void
ClassPartInitialize(class)
WidgetClass class;
{
    CmeObjectClass m_ent, superC;

    m_ent = (CmeObjectClass) class;
    superC = (CmeObjectClass) m_ent->rect_class.superclass;

/* 
 * We don't need to check for null super since we'll get to TextSink
 * eventually.
 */

    if (m_ent->cme_class.highlight == XtInheritHighlight) 
	m_ent->cme_class.highlight = superC->cme_class.highlight;

    if (m_ent->cme_class.unhighlight == XtInheritUnhighlight)
	m_ent->cme_class.unhighlight = superC->cme_class.unhighlight;

    if (m_ent->cme_class.notify == XtInheritNotify) 
	m_ent->cme_class.notify = superC->cme_class.notify;
}

/*      Function Name: Initialize
 *      Description: Initializes the complex menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 * 
 * MENU ENTRIES CANNOT HAVE BORDERS.
 */

/* ARGSUSED */
static void
Initialize(request, new)
Widget request, new;
{
    CmeObject entry = (CmeObject) new;

    entry->rectangle.border_width = 0;
}

/*	Function Name: Highlight
 *	Description: The default highlight proceedure for menu entries.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

/* ARGSUSED */
static void
Highlight(w)
Widget w;
{
/* This space intentionally left blank. */
}

/*	Function Name: Unhighlight
 *	Description: The default unhighlight proceedure for menu entries.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

/* ARGSUSED */
static void
Unhighlight(w)
Widget w;
{
/* This space intentionally left blank. */
}

/*	Function Name: Notify
 *	Description: calls the callback proceedures for this entry.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

static void
Notify(w) 
Widget w;
{
    XtCallCallbacks(w, XtNcallback, NULL);
}

/*	Function Name: QueryGeometry.
 *	Description: Returns the preferred geometry for this widget.
 *	Arguments: w - the menu entry object.
 *                 itended, return - the intended and return geometry info.
 *	Returns: A Geometry Result.
 *
 * See the Intrinsics manual for details on what this function is for.
 * 
 * I just return the height and a width of 1.
 */

static XtGeometryResult
QueryGeometry(w, intended, return_val) 
Widget w;
XtWidgetGeometry *intended, *return_val;
{
    CmeObject entry = (CmeObject) w;
    Dimension width;
    XtGeometryResult ret_val = XtGeometryYes;
    XtGeometryMask mode = intended->request_mode;

    width = 1;			/* we can be really small. */

    if ( ((mode & CWWidth) && (intended->width != width)) ||
	 !(mode & CWWidth) ) {
	return_val->request_mode |= CWWidth;
	return_val->width = width;
	mode = return_val->request_mode;
	
	if ( (mode & CWWidth) && (width == entry->rectangle.width) )
	    return(XtGeometryNo);
	return(XtGeometryAlmost);
    }
    return(ret_val);
}

#endif /*]*/
