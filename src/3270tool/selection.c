/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 *
 *                         All Rights Reserved
 */

/*
 *	selection.c
 *		This module handles putting and getting the selection.
 *		Selections in the window are done in a block-mode fashion
 *		instead of the expected line-by-line.  This seems more
 *		appropriate for the displays produced by most IBM software.
 */

#include <suntool/sunview.h>
#include <suntool/canvas.h>
#include <suntool/seln.h>
#include <stdio.h>
#include "3270.h"
#include "3270_enc.h"

bool		seln_isset;
int		row_start, row_end, col_start, col_end;	/* selection area */
Seln_client	s_client;
u_char		prim_seln[ROWS * COLS + ROWS + 1];/* enough for screen + eols */
int		prim_size = 0;
u_char		shelf_seln[ROWS * COLS + ROWS + 1];
int		shelf_size = 0;
extern u_char	asc2cg[128], cg2asc[256];
extern u_char	screen_buf[ROWS * COLS];
extern Pixwin	*pixwin;
extern Pixfont	*ibmfont;
extern int	char_width, char_height, char_base;


Seln_result
read_seln (buffer)
Seln_request	*buffer;
{
    register char	*cp;
    char		*reply;
    int			cgcode;
    Event		event;

    if (*buffer->requester.context == 0) {
	if (buffer == (Seln_request *) NULL
	||  *((Seln_attribute *) buffer->data) != SELN_REQ_CONTENTS_ASCII) {
	    fprintf (stderr, "Selection holder error -- unknown request (in read_seln)\n");
	    return (SELN_FAILED);
	}
	else {
	    reply = buffer->data + sizeof (Seln_attribute);
	    *buffer->requester.context = 1;
	}
    }
    else
	reply = buffer->data;
    bzero (&event, sizeof (event));
    for (cp = reply; *cp; cp++) {
	/* code copied out of canvas_event_proc in kybd.c */
	if ((*cp >= ' ' && *cp <= '~') || (*cp == 0x1B)) {
	    cgcode = asc2cg[*cp];
	    if (cgcode != CG_NULLBLANK)
		(void) key_Character (cgcode);
	}
    }
    return (SELN_SUCCESS);
}


stuff_seln ()
{
    Seln_holder	holder;
    char	context = 0;

    holder = seln_inquire (SELN_PRIMARY);
    if (holder.state == SELN_NONE)	/* no selection */
	return;
    seln_query (&holder, read_seln, &context, SELN_REQ_CONTENTS_ASCII, 0, 0);
}


key_seln (client_data, args)
char		*client_data;
Seln_function_buffer	*args;
{
    Seln_holder	*holder;
    char	context = 0;

    switch (seln_figure_response (args, &holder)) {
	case SELN_REQUEST:	/* GET */
	    seln_query (holder, read_seln, &context, SELN_REQ_CONTENTS_ASCII, 0, 0);
	    break;
	case SELN_SHELVE:	/* PUT */
	    if (seln_acquire (s_client, SELN_SHELF) != SELN_SHELF)
		fprintf (stderr, "can't acquire shelf selection!\n");
	    else {
		shelf_size = prim_size;
		bcopy (prim_seln, shelf_seln, shelf_size);
	    }
	default:		/* ignore the rest */
	    break;
    }
}


Seln_result
reply_seln (item, context, length)
Seln_attribute		item;
Seln_replier_data	*context;
int			length;
{
    u_char	*seln, *destp;
    int		size, needed;

    switch (item) {
	case SELN_REQ_BYTESIZE:
	    if (context->rank == SELN_PRIMARY) {
		*context->response_pointer++ = (char *) prim_size;
		return (SELN_SUCCESS);
	    }
	    else if (context->rank == SELN_SHELF) {
		*context->response_pointer++ = (char *) shelf_size;
		return (SELN_SUCCESS);
	    }
	    else
		return (SELN_DIDNT_HAVE);
	case SELN_REQ_YIELD:
	    if (context->rank == SELN_PRIMARY) {
		clear_seln ();
		return (SELN_SUCCESS);
	    }
	    else if (context->rank == SELN_SHELF) {
		shelf_size = 0;
		return (SELN_SUCCESS);
	    }
	    else
		return (SELN_DIDNT_HAVE);
	case SELN_REQ_CONTENTS_ASCII:
	    if (context->rank == SELN_PRIMARY)
		seln = prim_seln;
	    else if (context->rank == SELN_SHELF)
		seln = shelf_seln;
	    else
		return (SELN_DIDNT_HAVE);
	    if (context->context == NULL)
		context->context = (char *) seln;
	    size = strlen (context->context);
	    destp = (u_char *) context->response_pointer;
	    needed = size + 4;
	    if (size % 4 != 0)
		needed += 4 - size % 4;
	    if (needed <= length) {	/* everything fits */
		strcpy (destp, context->context);
		destp += size;
		while ((int) destp % 4 != 0)
		    *destp++ = '\0';
		context->response_pointer = (char **) destp;
		*context->response_pointer++ = 0;
		return (SELN_SUCCESS);
	    }
	    else {
		strncpy (destp, context->context, length);
		destp += length;
		context->response_pointer = (char **) destp;
		context->context += length;
		return (SELN_CONTINUED);
	    }
	    break;
	case SELN_REQ_END_REQUEST:
	    return (SELN_SUCCESS);
	default:
	    return (SELN_UNRECOGNIZED);
    }
}


set_seln (sbaddr, ebaddr)
int	sbaddr, ebaddr;
{
    register u_char	*cp;
    int			r, c;

    cursor_off ();	/* also clears selection, if any */
    /* make sure order is correct */
    if (ebaddr < sbaddr) {
	r = ebaddr;
	ebaddr = sbaddr;
	sbaddr = r;
    }
    col_start = BA_TO_COL (sbaddr);
    row_start = BA_TO_ROW (sbaddr);
    col_end = BA_TO_COL (ebaddr);
    row_end = BA_TO_ROW (ebaddr);
    pw_writebackground (
	pixwin,
	COL_TO_X (col_start), ROW_TO_Y (row_start),
	COL_TO_X (col_end - col_start + 1), ROW_TO_Y (row_end - row_start + 1),
	PIX_SET ^ PIX_DST
    );
    cp = prim_seln;
    for (r = row_start; r <= row_end; r++) {
	for (c = col_start; c <= col_end; c++)
	    *cp++ = cg2asc[screen_buf[ROWCOL_TO_BA(r,c)]];
	*cp++ = '\n';
    }
    *cp = '\0';
    prim_size = cp - prim_seln;
    cursor_on ();
    seln_isset = TRUE;
    if (seln_acquire (s_client, SELN_PRIMARY) != SELN_PRIMARY)
	fprintf (stderr, "can't acquire primary selection!\n");
}


clear_seln ()
{
    if (!seln_isset)
	return;
    prim_size = 0;
    pw_writebackground (
	pixwin,
	COL_TO_X (col_start), ROW_TO_Y (row_start),
	COL_TO_X (col_end - col_start + 1), ROW_TO_Y (row_end - row_start + 1),
	PIX_SET ^ PIX_DST
    );
    seln_isset = FALSE;
    seln_done (s_client, SELN_PRIMARY);
}


selection_init ()
{
    s_client = seln_create (key_seln, reply_seln, (char *) 0);
    seln_isset = FALSE;
}


selection_exit ()
{
    seln_destroy (s_client);
}
