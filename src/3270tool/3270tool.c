/*
 * Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
 * Copyright 1988, 1989 by Robert Viduya.
 *
 *                         All Rights Reserved
 */

/*
 *	3270tool.c
 *		This module initializes the suntools environment, sets up
 *		the frame and starts everything going.  All of this is
 *		done in the main procedure.
 */

#include <suntool/sunview.h>
#include <stdio.h>

static unsigned short ibm_image[] = {
#include "3270.icon"
};
mpr_static (ibm_pixrect, 64, 64, 1, ibm_image);

Icon		ibm_icon;
Frame		frame;
int		net_sock;

extern Notify_value	net_input ();


main (argc, argv)
int	argc;
char	*argv[];
{
    ibm_icon = icon_create (ICON_IMAGE, &ibm_pixrect, 0);
    frame = window_create (
	NULL, FRAME,
	FRAME_NO_CONFIRM,		TRUE,
	FRAME_ICON,			ibm_icon,
	FRAME_LABEL,			"3270tool: Copyright 1988,1989 Robert Viduya/Georgia Tech Research Corporation",
	FRAME_SUBWINDOWS_ADJUSTABLE,	FALSE,
	FRAME_ARGC_PTR_ARGV,		&argc, argv,
	0
    );
    if (argc <= 1) {
	(void) fprintf (stderr, "usage: 3270tool ibm-host\n");
	exit (1);
    }
    net_sock = connect_net (argv[1]);
    screen_init ();
    key_panel_init ();
    menu_init ();
    selection_init ();
    (void) notify_set_input_func (&net_sock, net_input, net_sock);
    window_main_loop (frame);
    (void) shutdown (net_sock, 2);
    (void) close (net_sock);
    selection_exit ();
    exit (0);
}
