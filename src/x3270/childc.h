/*
 * Copyright 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	childc.h
 *		Global declarations for child.c.
 */

#if defined(X3270_DISPLAY) || defined(C3270) /*[*/
extern int fork_child(void);
extern void child_ignore_output(void);
#else /*][*/
#define fork_child()	fork()
#define child_ignore_output()
#endif /*]*/
