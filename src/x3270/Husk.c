/*
 * Copyright 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 * Husk.c - Husk composite widget
 *	A "Husk" (a nearly useless shell) is a trivial container widget, a
 *	subclass of the Athena Composite widget with a no-op geometry manager
 *	that allows children to move and resize themselves at will.
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Misc.h>
#include <X11/Xaw/XawInit.h>
#include "HuskP.h"

static void ClassInitialize();
static void Initialize();
static void Realize();
static Boolean SetValues();
static XtGeometryResult GeometryManager();
static void ChangeManaged();
static XtGeometryResult QueryGeometry();

HuskClassRec huskClassRec = {
    { /* core_class fields      */
	/* superclass         */ (WidgetClass) & compositeClassRec,
	/* class_name         */ "Husk",
	/* widget_size        */ sizeof(HuskRec),
	/* class_initialize   */ ClassInitialize,
	/* class_part_init    */ NULL,
	/* class_inited       */ FALSE,
	/* initialize         */ Initialize,
	/* initialize_hook    */ NULL,
	/* realize            */ Realize,
	/* actions            */ NULL,
	/* num_actions	      */ 0,
	/* resources          */ NULL,
	/* num_resources      */ 0,
	/* xrm_class          */ NULLQUARK,
	/* compress_motion    */ TRUE,
	/* compress_exposure  */ TRUE,
	/* compress_enterleave */ TRUE,
	/* visible_interest   */ FALSE,
	/* destroy            */ NULL,
	/* resize             */ NULL,
	/* expose             */ NULL,
	/* set_values         */ SetValues,
	/* set_values_hook    */ NULL,
	/* set_values_almost  */ XtInheritSetValuesAlmost,
	/* get_values_hook    */ NULL,
	/* accept_focus       */ NULL,
	/* version            */ XtVersion,
	/* callback_private   */ NULL,
	/* tm_table           */ NULL,
	/* query_geometry     */ QueryGeometry,
	/* display_accelerator */ XtInheritDisplayAccelerator,
	/* extension          */ NULL
    }, {
	/* composite_class fields */
	/* geometry_manager   */ GeometryManager,
	/* change_managed     */ ChangeManaged,
	/* insert_child	      */ XtInheritInsertChild,
	/* delete_child	      */ XtInheritDeleteChild,
	/* extension          */ NULL
    }, {
	/* Husk class fields */
	/* empty	      */ 0,
    }
};

WidgetClass huskWidgetClass = (WidgetClass)&huskClassRec;

static XtGeometryResult 
QueryGeometry(widget, constraint, preferred)
Widget widget;
XtWidgetGeometry *constraint, *preferred;
{
	return XtGeometryYes;
}

static XtGeometryResult 
GeometryManager(w, request, reply)
Widget w;
XtWidgetGeometry *request;
XtWidgetGeometry *reply;
{
	/* Always succeed. */
	if (!(request->request_mode & XtCWQueryOnly)) {
		if (request->request_mode & CWX)
			w->core.x = request->x;
		if (request->request_mode & CWY)
			w->core.y = request->y;
		if (request->request_mode & CWWidth)
			w->core.width = request->width;
		if (request->request_mode & CWHeight)
			w->core.height = request->height;
		if (request->request_mode & CWBorderWidth)
			w->core.border_width = request->border_width;
	}
	return XtGeometryYes;
}

static void 
ChangeManaged(w)
Widget w;
{
}

static void 
ClassInitialize()
{
	XawInitializeWidgetSet();
}

static void 
Initialize(request, new)
Widget request, new;
{
}

static void 
Realize(w, valueMask, attributes)
register Widget w;
Mask *valueMask;
XSetWindowAttributes *attributes;
{
	attributes->bit_gravity = NorthWestGravity;
	*valueMask |= CWBitGravity;

	XtCreateWindow(w, (unsigned)InputOutput, (Visual *)CopyFromParent,
	       *valueMask, attributes);
}

static Boolean 
SetValues(current, request, new)
Widget current, request, new;
{
	return False;
}
