/*
 * Copyright 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/* s3270 version of screenc.h */

#define blink_start()
#define cursor_move(baddr)	cursor_addr = (baddr)
#define display_heightMM()	100
#define display_height()	1
#define display_widthMM()	100
#define display_width()		1
#define mcursor_locked()
#define mcursor_normal()
#define mcursor_waiting()
#define ring_bell()
#define screen_disp(erasing)
#define screen_flip()
#define screen_obscured()	False
#define screen_scroll()
