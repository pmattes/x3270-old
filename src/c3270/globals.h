/*
 * Modifications Copyright 1993, 1994, 1995, 1996, 1999, 2000 by Paul Mattes.
 * Copyright 1990 by Jeff Sparkes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	globals.h
 *		Common definitions for x3270.
 */

/* Optional parts. */
#include "parts.h"
#if defined(X3270_TN3270E) && !defined(X3270_ANSI) /*[*/
#define X3270_ANSI	1	/* RFC2355 requires NVT mode */
#endif /*]*/

/*
 * OS-specific #defines.
 */

/*
 * SEPARATE_SELECT_H
 *   The definitions of the data structures for select() are in <select.h>.
 * NO_SYS_TIME_H
 *   Don't #include <sys/time.h>.
 * NO_MEMORY_H
 *   Don't #include <memory.h>.
 * LOCAL_TELNET_H
 *   #include a local copy of "telnet.h" rather then <arpa/telnet.h>
 * SELECT_INT
 *   select() takes (int *) arguments rather than (fd_set *) arguments.
 * BLOCKING_CONNECT_ONLY
 *   Use only blocking sockets.
 */
#if defined(sco) /*[*/
#define BLOCKING_CONNECT_ONLY	1
#define NO_SYS_TIME_H		1
#endif /*]*/

#if defined(_IBMR2) || defined(_SEQUENT_) || defined(__QNX__) /*[*/
#define SEPARATE_SELECT_H	1
#endif /*]*/

#if defined(apollo) /*[*/
#define BLOCKING_CONNECT_ONLY	1
#define NO_MEMORY_H		1
#endif /*]*/

#if defined(hpux) /*[*/
#define SELECT_INT		1
#define LOCAL_TELNET_H		1
#endif /*]*/

/*
 * Compiler-specific #defines.
 */

/* 'unused' explicitly flags an unused parameter */
#if defined(__GNUC__) /*[*/
#define unused __attribute__((__unused__))
#else /*][*/
#define unused /* nothing */
#endif /*]*/



/*
 * Prerequisite #includes.
 */
#include <stdio.h>			/* Unix standard I/O library */
#include <stdlib.h>			/* Other Unix library functions */
#include <unistd.h>			/* Unix system calls */
#include <ctype.h>			/* Character classes */
#include <string.h>			/* String manipulations */
#if !defined(NO_MEMORY_H) /*[*/
#include <memory.h>			/* Block moves and compares */
#endif /*]*/
#include <sys/types.h>			/* Basic system data types */
#if !defined(NO_SYS_TIME_H) /*[*/
#include <sys/time.h>			/* System time-related data types */
#endif /*]*/
#include "localdefs.h"

#if defined(X3270_LOCAL_PROCESS) /*[*/
#if defined(__FreeBSD__) /*[*/
#include <termios.h>
#include <libutil.h>
#define LOCAL_PROCESS    1
#endif /*]*/
#if defined(__linux__) /*[*/
#include <termios.h>
#include <pty.h>
#define LOCAL_PROCESS    1
#endif /*]*/
#endif /*]*/

#if defined(__FreeBSD__) /*[*/
#define HAS_ASPRINTF 1
#endif /*]*/

/* Simple global variables */

extern int		COLS;
extern int		ROWS;
#if defined(X3270_DISPLAY) /*[*/
extern Atom		a_3270, a_registry, a_iso8859, a_ISO8859, a_encoding,
				a_1;
extern XtAppContext	appcontext;
#endif /*]*/
extern const char	*build;
extern int		children;
extern char		*connected_lu;
extern char		*connected_type;
extern char		*current_host;
extern unsigned short	current_port;
extern Boolean		*debugging_font;
extern char		*efontname;
extern Boolean		ever_3270;
extern Boolean		exiting;
extern Boolean		*extended_3270font;
extern Boolean		flipped;
extern char		*full_current_host;
extern char		full_model_name[];
extern char		*hostname;
extern Boolean		*latin1_font;
extern char		luname[];
#if defined(LOCAL_PROCESS) /*[*/
extern Boolean		local_process;
#endif /*]*/
extern int		maxCOLS;
extern int		maxROWS;
extern char		*model_name;
extern int		model_num;
extern Boolean		non_tn3270e_host;
extern int		ov_cols, ov_rows;
extern Boolean		passthru_host;
extern char		*programname;
extern char		*reconnect_host;
extern Boolean		auto_reconnect_disabled;
extern int		screen_depth;
extern Boolean		scroll_initted;
extern Boolean		shifted;
extern Boolean		*standard_font;
extern Boolean		std_ds_host;
extern char		*termtype;
extern Widget		toplevel;

#if defined(X3270_DISPLAY) /*[*/
extern Atom		a_delete_me;
extern Atom		a_save_yourself;
extern Atom		a_state;
extern Pixel		colorbg_pixel;
extern Display		*display;
extern Pixmap		gray;
extern Pixel		keypadbg_pixel;
extern XrmDatabase	rdb;
extern Window		root_window;
#endif /*]*/

/* Data types and complex global variables */

/*   connection state */
enum cstate {
	NOT_CONNECTED,		/* no socket, unknown mode */
	PENDING,		/* connection pending */
	CONNECTED_INITIAL,	/* connected, no mode yet */
	CONNECTED_ANSI,		/* connected in NVT ANSI mode */
	CONNECTED_3270,		/* connected in old-style 3270 mode */
	CONNECTED_INITIAL_E,	/* connected in TN3270E mode, unnegotiated */
	CONNECTED_NVT,		/* connected in TN3270E mode, NVT mode */
	CONNECTED_SSCP,		/* connected in TN3270E mode, SSCP-LU mode */
	CONNECTED_TN3270E	/* connected in TN3270E mode, 3270 mode */
};
extern enum cstate cstate;

#define PCONNECTED	((int)cstate >= (int)PENDING)
#define HALF_CONNECTED	(cstate == PENDING)
#define CONNECTED	((int)cstate >= (int)CONNECTED_INITIAL)
#define IN_NEITHER	(cstate == CONNECTED_INITIAL)
#define IN_ANSI		(cstate == CONNECTED_ANSI || cstate == CONNECTED_NVT)
#define IN_3270		(cstate == CONNECTED_3270 || cstate == CONNECTED_TN3270E || cstate == CONNECTED_SSCP)
#define IN_SSCP		(cstate == CONNECTED_SSCP)
#define IN_TN3270E	(cstate == CONNECTED_TN3270E)
#define IN_E		(cstate >= CONNECTED_INITIAL_E)

/*   keyboard modifer bitmap */
#define ShiftKeyDown	0x01
#define MetaKeyDown	0x02
#define AltKeyDown	0x04

/*   toggle names */
struct toggle_name {
	const char *name;
	int index;
};
extern struct toggle_name toggle_names[];

/*   extended attributes */
struct ea {
	unsigned char fg;	/* foreground color (0x00 or 0xf<n>) */
	unsigned char bg;	/* background color (0x00 or 0xf<n>) */
	unsigned char gr;	/* ANSI graphics rendition bits */
	unsigned char cs;	/* character set (GE flag, or 0..2) */
};
#define GR_BLINK	0x01
#define GR_REVERSE	0x02
#define GR_UNDERLINE	0x04
#define GR_INTENSIFY	0x08

#define CS_MASK		0x03	/* mask for specific character sets */
#define CS_GE		0x04	/* cs flag for Graphic Escape */

/*
 * Lightpen select test macro, includes configurable override of selectability
 * of highlighted fields.
 */
#define FA_IS_SEL(fa) \
	(FA_IS_SELECTABLE(fa) && \
	 (appres.highlight_select || !FA_IS_INTENSE(fa)))

/*   translation lists */
struct trans_list {
	char			*name;
	char			*pathname;
	Boolean			is_temp;
	Boolean			from_server;
	struct trans_list	*next;
};
extern struct trans_list *trans_list;

/*   font list */
struct font_list {
	char			*label;
	char			*font;
	struct font_list	*next;
};
extern struct font_list *font_list;
extern int font_count;

/*   input key type */
enum keytype { KT_STD, KT_GE };

/*   state changes */
#define ST_HALF_CONNECT	0
#define ST_CONNECT	1
#define ST_3270_MODE	2
#define ST_LINE_MODE	3
#define ST_REMODEL	4
#define ST_PRINTER	5
#define ST_EXITING	6
#define N_ST		7

/* Naming convention for private actions. */
#define PA_PFX	"PA-"

/* Shorthand macros */

#define CN	((char *) NULL)
#define PN	((XtPointer) NULL)

/* Configuration change masks. */
#define NO_CHANGE	0x0000	/* no change */
#define MODEL_CHANGE	0x0001	/* screen dimensions changed */
#define FONT_CHANGE	0x0002	/* emulator font changed */
#define COLOR_CHANGE	0x0004	/* color scheme or 3278/9 mode changed */
#define SCROLL_CHANGE	0x0008	/* scrollbar snapped on or off */
#define CHARSET_CHANGE	0x0010	/* character set changed */
#define ALL_CHANGE	0xffff	/* everything changed */

/* Portability macros */

/*   Replacement for memcpy that handles overlaps */

#if defined(X3270_DISPLAY) /*[*/
#include <X11/Xfuncs.h>
#undef MEMORY_MOVE
#define MEMORY_MOVE(to,from,cnt)	bcopy(from,to,cnt)
#endif /*]*/

/*   Equivalent of setlinebuf */

#if defined(_IOLBF) /*[*/
#define SETLINEBUF(s)	setvbuf(s, (char *)NULL, _IOLBF, BUFSIZ)
#else /*][*/
#define SETLINEBUF(s)	setlinebuf(s)
#endif /*]*/

/*   Motorola version of gettimeofday */

#if defined(MOTOROLA)
#define gettimeofday(tp,tz)	gettimeofday(tp)
#endif
