/*
 * Copyright 2000, 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* c3270 version of keymapc.h */

extern void keymap_init(void);
extern char *lookup_key(int k);
extern void keymap_dump(void);
extern const char *decode_key(int k, int hint);
