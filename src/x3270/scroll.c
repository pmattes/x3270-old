/*
 * Copyright 1994 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	scroll.c
 *		Scrollbar support
 */

#include <X11/Intrinsic.h>
#include "globals.h"

/* Externals */
extern unsigned char *screen_buf;
extern unsigned char *ea_buf;

/* Globals */
unsigned char **buf_save = (unsigned char **)NULL;
unsigned char **ea_save = (unsigned char **)NULL;
int	n_saved = 0;
int	scroll_next = 0;
float	thumb_top = 0.0;
float	thumb_shown = 1.0;
int	scrolled_back = 0;
Boolean	need_saving = True;
Boolean	scroll_initted = False;

/* Statics */
static void sync_scroll();
static char *sbuf = (char *)NULL;
static int sa_bufsize;

/*
 * Initialize (or re-initialize) the scrolling parameters and save area.
 */
void
scroll_init()
{
	register int i;
	int sa_size;
	unsigned char *s;

	if (sbuf) {
		XtFree(sbuf);
		XtFree((char *)buf_save);
		XtFree((char *)ea_save);
	}
	sa_size = appres.save_lines + maxROWS;
	buf_save = (unsigned char **)XtCalloc(sizeof(unsigned char *), sa_size);
	ea_save = (unsigned char **)XtCalloc(sizeof(unsigned char *), sa_size);
	sa_bufsize = 2 * sa_size * maxCOLS;
	sbuf = XtMalloc(sa_bufsize);
	s = (unsigned char *)sbuf;
	for (i = 0; i < sa_size; s += maxCOLS, i++)
		buf_save[i] = s;
	for (i = 0; i < sa_size; s += maxCOLS, i++)
		ea_save[i] = s;
	scroll_reset();
	scroll_initted = True;
}

/*
 * Reset the scrolling parameters and erase the save area.
 */
void
scroll_reset()
{
	(void) memset(sbuf, 0, sa_bufsize);
	scroll_next = 0;
	n_saved = 0;
	scrolled_back = 0;
	thumb_top = 0.0;
	thumb_shown = 1.0;
	need_saving = True;
	screen_set_thumb(thumb_top, thumb_shown);
	enable_cursor(True);
}

/*
 * Save <n> lines of data from the top of the screen.
 */
void
scroll_save(n)
int n;
{
	int i;

	if (scrolled_back)
		sync_scroll(0);
	for (i = 0; i < n; i++) {
		MEMORY_MOVE((char *)buf_save[scroll_next],
		    (char *)screen_buf+(i*COLS), COLS);
		MEMORY_MOVE((char *)ea_save[scroll_next],
		    (char *)ea_buf+(i*COLS), COLS);
		scroll_next = (scroll_next + 1) % appres.save_lines;
		if (n_saved < appres.save_lines)
			n_saved++;
	}
	thumb_top = ((float)n_saved / (float)(appres.save_lines + ROWS));
	thumb_shown = 1.0 - thumb_top;
	screen_set_thumb(thumb_top, thumb_shown);
}

/*
 * Jump to the bottom of the scroll buffer.
 */
void
scroll_to_bottom()
{
	if (scrolled_back) {
		sync_scroll(0);
		screen_set_thumb(thumb_top, thumb_shown);
	}
	need_saving = True;
}

/*
 * Save the current screen image, if it hasn't been saved since last updated.
 */
void
save_image()
{
	int i;

	if (!need_saving)
		return;
	for (i = 0; i < ROWS; i++) {
		MEMORY_MOVE((char *)buf_save[appres.save_lines+i],
		            (char *)screen_buf + (i*COLS),
		            COLS);
		MEMORY_MOVE((char *)ea_save[appres.save_lines+i],
		            (char *)ea_buf + (i*COLS),
		            COLS);
	}
	need_saving = False;
}

/*
 * Redraw the display so it begins back <sb> lines.
 */
static void
sync_scroll(sb)
int sb;
{
	int i;
	int scroll_first;
	extern Boolean screen_changed;

	scroll_first = (scroll_next+appres.save_lines-sb) % appres.save_lines;

	/* Update the screen. */
	for (i = 0; i < ROWS; i++)
		if (i < sb) {
			MEMORY_MOVE((char *)screen_buf + (i*COLS),
				    (char *)buf_save[(scroll_first+i) % appres.save_lines],
				    COLS);
			MEMORY_MOVE((char *)ea_buf + (i*COLS),
				    (char *)ea_save[(scroll_first+i) % appres.save_lines],
				    COLS);
		} else {
			MEMORY_MOVE((char *)screen_buf + (i*COLS),
				    (char *)buf_save[appres.save_lines+i-sb],
				    COLS);
			MEMORY_MOVE((char *)ea_buf + (i*COLS),
				    (char *)ea_save[appres.save_lines+i-sb],
				    COLS);
		}

	/* Don't even try to keep the cursor consistent. */
	if (sb)
		enable_cursor(False);
	else if (CONNECTED)
		enable_cursor(True);

	scrolled_back = sb;
	screen_changed = True;
}

/*
 * Callback for "scroll" action (moving the thumb to a particular spot).
 */
void
scroll_proc(n, total)
int n;
int total;
{
	float pct;
	int nss;

	if (!n_saved)
		return;
	if (n < 0)
		pct = (float)(-n) / (float)total;
	else
		pct = (float)n / (float)total;
	/*printf("scroll_proc(%d, %d) %f\n", n, total, pct);*/
	nss = pct * thumb_shown * n_saved;
	if (!nss)
		nss = 1;
	save_image();
	if (n > 0) {	/* scroll forward */
		if (nss > scrolled_back)
			sync_scroll(0);
		else
			sync_scroll(scrolled_back - nss);
	} else {	/* scroll back */
		if (scrolled_back + nss > n_saved)
			sync_scroll(n_saved);
		else
			sync_scroll(scrolled_back + nss);
	}

	screen_set_thumb((float)(n_saved - scrolled_back) / (float)(appres.save_lines + ROWS),
	    thumb_shown);
}

/*
 * Callback for "jump" action (incrementing the thumb in one direction).
 */
void
jump_proc(top)
float top;
{
	/*printf("jump_proc(%f)\n", top);*/
	if (!n_saved) {
		screen_set_thumb(thumb_top, thumb_shown);
		return;
	}
	if (top > thumb_top) {	/* too far down */
		screen_set_thumb(thumb_top, thumb_shown);
		sync_scroll(0);
	} else {
		save_image();
		sync_scroll((int)((1.0 - top) * n_saved));
	}
}

/*
 * Resynchronize the thumb (called when the scrollbar is turned on).
 */
void
rethumb()
{
	screen_set_thumb(thumb_top, thumb_shown);
}
