/*
 * Copyright 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* 
 * HuskP.h
 *	Private definitions for Husk widget
 */

/* Husk Widget Private Data */

#include "Husk.h"
#include <X11/CompositeP.h>

/* New fields for the Husk widget class record */
typedef struct {int empty;} HuskClassPart;

/* Full class record declaration */
typedef struct _HuskClassRec {
	CoreClassPart		core_class;
	CompositeClassPart	composite_class;
	HuskClassPart		husk_class;
} HuskClassRec;

extern HuskClassRec huskClassRec;

/* New fields for the Husk widget record */
typedef struct {int empty;} HuskPart;

/* Full instance record declaration */

typedef struct _HuskRec {
	CorePart		core;
	CompositePart  		composite;
	HuskPart		husk;
} HuskRec;
