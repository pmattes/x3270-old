/*
 * Copyright 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* s3270 basic X11 and Xt typedefs */

#if !defined(X11_INTRINSIC_H) /*[*/
#define X11_INTRINSIC_H

#define False 0
#define True 1
typedef void *XtAppContext;
typedef void *XtPointer;
typedef void *Widget;
typedef void *XEvent;
typedef void *XtInputId;
typedef void *XtIntervalId;
typedef char Boolean;
typedef char *String;
typedef unsigned int Cardinal;
typedef unsigned long KeySym;
#define Bool int
typedef char *XPointer;
typedef void *XrmDatabase;

typedef void (*XtActionProc)(
    Widget 		/* widget */,
    XEvent*		/* event */,
    String*		/* params */,
    Cardinal*		/* num_params */
);

typedef struct _XtActionsRec{
    String	 string;
    XtActionProc proc;
} XtActionsRec;

#define XtNumber(n)	(sizeof(n)/sizeof((n)[0]))

extern XtPointer XtMalloc(unsigned);
extern XtPointer XtCalloc(unsigned, unsigned);
extern XtPointer XtRealloc(XtPointer, unsigned);
extern void XtFree(XtPointer);
extern String XtNewString(const char *);

typedef void (*XtTimerCallbackProc)(
    XtPointer 		/* closure */,
    XtIntervalId*	/* id */
);
extern XtIntervalId XtAppAddTimeOut(
    XtAppContext 	/* app_context */,
    unsigned long 	/* interval */,
    XtTimerCallbackProc /* proc */,
    XtPointer 		/* closure */
);
extern void XtRemoveTimeOut(
    XtIntervalId 	/* timer */
);

extern void XtWarning(const String);
extern void XtError(const String);


typedef void (*XtInputCallbackProc)(
    XtPointer 		/* closure */,
    int*		/* source */,
    XtInputId*		/* id */
);
typedef unsigned long	XtInputMask;
#define XtInputNoneMask		0L
#define XtInputReadMask		(1L<<0)
#define XtInputWriteMask	(1L<<1)
#define XtInputExceptMask	(1L<<2)
extern XtInputId XtAppAddInput(
    XtAppContext       	/* app_context */,
    int 		/* source */,
    XtPointer 		/* condition */,
    XtInputCallbackProc /* proc */,
    XtPointer 		/* closure */
);

extern void XtRemoveInput(
    XtInputId 		/* id */
);

#define NoSymbol		0L

extern KeySym XStringToKeysym(const String);

#define XtIMAll		0

extern void XtAppProcessEvent(
    XtAppContext 		/* app_context */,
    XtInputMask 		/* mask */
);

/* Xrm stuff. */

typedef struct {
    unsigned int    size;
    XPointer	    addr;
} XrmValue, *XrmValuePtr;

extern Bool XrmGetResource(
    XrmDatabase		/* database */,
    const char*		/* str_name */,
    const char*		/* str_class */,
    char**		/* str_type_return */,
    XrmValue*		/* value_return */
);

#define XtName(widget)	"x3270"

#define XtWindow(widget)	((unsigned long)0)

#endif /*]*/
