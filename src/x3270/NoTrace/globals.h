/*
 * Copyright 1990 by Jeff Sparkes.
 * Modifications Copyright 1993, 1994, 1995 by Paul Mattes.
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

/*
 * Prerequisite #includes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>

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

#if defined(_IBMR2) || defined(_SEQUENT_) /*[*/
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

/* Toggles */

enum toggle_type { TT_INITIAL, TT_TOGGLE, TT_FINAL };
struct toggle {
	Boolean value;		/* toggle value */
	Boolean changed;	/* has the value changed since init */
	Widget w[2];		/* the menu item widgets */
	char *label[2];		/* labels */
	void (*upcall)();	/* change value */
};
#define MONOCASE	0
#define ALT_CURSOR	1
#define CURSOR_BLINK	2
#define SHOW_TIMING	3
#define CURSOR_POS	4
#define DS_TRACE	5
#define SCROLL_BAR	6
#define LINE_WRAP	7
#define BLANK_FILL	8
#define SCREEN_TRACE	9
#define EVENT_TRACE	10
#define MARGINED_PASTE	11

#define N_TOGGLES	12

#define toggled(ix)		(appres.toggle[ix].value)
#define toggle_toggle(t) \
	{ (t)->value = !(t)->value; (t)->changed = True; }

/* Application resources */

typedef struct {
	/* Basic colors */
	Pixel	foreground;
	Pixel	background;

	/* Options (not toggles) */
	Boolean mono;
	Boolean extended;
	Boolean m3279;
	Boolean visual_bell;
	Boolean	keypad_on;
	Boolean menubar;
	Boolean active_icon;
	Boolean label_icon;
	Boolean	apl_mode;
	Boolean	once;
	Boolean invert_kpshift;
	Boolean scripted;
	Boolean modified_sel;
	Boolean use_cursor_color;
	Boolean reconnect;
	Boolean do_confirms;
	Boolean numeric_lock;
	Boolean allow_resize;
	Boolean secure;
	Boolean no_other;
	Boolean oerr_lock;

	/* Named resources */
	char	*keypad;
	char	*efontname;
	char	*bfontname;
	char	*afontname;
	char	*model;
	char	*key_map;
	char	*compose_map;
	char	*hostsfile;
	char	*ad_version;
	char	*port;
	char	*charset;
	char	*termname;
	char	*font_list;
	char	*debug_font;
	char	*icon_font;
	char	*icon_label_font;
	char	*macros;
	char	*trace_dir;
	int	save_lines;
	char	*normal_name;
	char	*select_name;
	char	*bold_name;
	char	*colorbg_name;
	char	*keypadbg_name;
	char	*selbg_name;
	char	*cursor_color_name;
	char    *color_name[16];
	int	bell_volume;

	/* Toggles */
	struct toggle toggle[N_TOGGLES];

	/* Simple widget resources */
	XtTranslations	base_translations;
	Cursor	wait_mcursor;
	Cursor	locked_mcursor;

	/* Line-mode TTY parameters */
	Boolean	icrnl;
	Boolean	inlcr;
	char	*erase;
	char	*kill;
	char	*werase;
	char	*rprnt;
	char	*lnext;
	char	*intr;
	char	*quit;
	char	*eof;

} AppRes, *AppResptr;

/* Simple global variables */

extern int		COLS;
extern int		ROWS;
extern int		actioncount;
extern XtActionsRec	actions[];
extern Boolean		ansi_host;
extern AppRes		appres;
extern XtAppContext	appcontext;
extern Atom		a_command;
extern Atom		a_delete_me;
extern Atom		a_save_yourself;
extern Atom		a_state;
extern XFontStruct      **bfontinfo;
extern char		*bfontname;
extern Pixel		colorbg_pixel;
extern char		*current_host;
extern unsigned short	current_port;
extern Boolean		*debugging_font;
extern int		depth;
extern Display		*display;
extern XFontStruct      **efontinfo;
extern char		*efontname;
extern Boolean		error_popup_visible;
extern Boolean		ever_3270;
extern Boolean		exiting;
extern Boolean		extended_ds_host;
extern Boolean		flipped;
extern char		*full_current_host;
extern Pixmap		gray;
extern Boolean		is_olwm;
extern Pixel		keypadbg_pixel;
extern Boolean		*latin1_font;
extern int		maxCOLS;
extern int		maxROWS;
extern int		model_num;
extern char		*programname;
extern Boolean		reconnect_disabled;
extern XrmDatabase	rdb;
extern Window		root_window;
extern Boolean		scroll_initted;
extern Boolean		shifted;
extern Boolean		*standard_font;
extern Widget		toplevel;
extern char		*ttype_color;
extern char		*ttype_model;
extern char		*ttype_extension;
extern char		*user_icon_name;
extern char		*user_title;

/* Data types and complex global variables */

/*   window placement enumeration */
enum placement { Center, Bottom, Left, Right };
extern enum kp_placement {
	kp_right, kp_left, kp_bottom, kp_integral
} kp_placement;
extern enum placement *CenterP;
extern enum placement *BottomP;
extern enum placement *LeftP;
extern enum placement *RightP;

/*   translation lists */
struct trans_list {
	char			*name;
	struct trans_list	*next;
};
extern struct trans_list *trans_list;

/*   connection state */
enum cstate {
	NOT_CONNECTED,		/* no socket, unknown mode */
	PENDING,		/* connection pending */
	CONNECTED_INITIAL,	/* connected in ANSI mode */
	CONNECTED_3270		/* connected in 3270 mode */
};
extern enum cstate cstate;

#define PCONNECTED	((int)cstate >= (int)PENDING)
#define HALF_CONNECTED	(cstate == PENDING)
#define CONNECTED	((int)cstate >= (int)CONNECTED_INITIAL)
#define IN_3270		(cstate == CONNECTED_3270)
#define IN_ANSI		(cstate == CONNECTED_INITIAL)

/*   mouse-cursor state */
enum mcursor_state { LOCKED, NORMAL, WAIT };

/*   spelled-out tty control character */
struct ctl_char {
	char *name;
	char value[3];
};

/*   keyboard modifer bitmap */
#define ShiftKeyDown	0x01
#define MetaKeyDown	0x02
#define AltKeyDown	0x04

/*   macro definition */
struct macro_def {
	char			*name;
	char			*action;
	struct macro_def	*next;
};
extern struct macro_def *macro_defs;

/*   font list */
struct font_list {
	char			*label;
	char			*font;
	struct font_list	*next;
};
extern struct font_list *font_list;
extern int font_count;

/*   types of internal actions */
enum iaction {
	IA_STRING, IA_PASTE, IA_REDRAW,
	IA_KEYPAD, IA_DEFAULT, IA_KEY,
	IA_MACRO, IA_SCRIPT, IA_PEEK,
	IA_TYPEAHEAD
};
extern enum iaction ia_cause;

/*   list of screen-resizing fonts */
struct rsfont {
	struct rsfont *next;
	char *name;
	int width;
	int height;
	int total_width;	/* transient */
	int total_height;	/* transient */
	int area;		/* transient */
};
extern struct rsfont *rsfonts;

/*   toggle names */
struct toggle_name {
	char *name;
	int index;
};
extern struct toggle_name toggle_names[];

/*   extended attributes */
struct ea {
	unsigned char fg;	/* foreground color (0x00 or 0xf<n>) */
	unsigned char gr;	/* ANSI graphics rendition bits */
};
#define GR_BLINK	0x01
#define GR_REVERSE	0x02
#define GR_UNDERLINE	0x04
#define GR_INTENSIFY	0x08

/*   keyboard lock states */
extern unsigned int kybdlock;
#define KL_OERR_MASK		0x000f
#define  KL_OERR_PROTECTED	1
#define  KL_OERR_NUMERIC	2
#define  KL_OERR_OVERFLOW	3
#define	KL_NOT_CONNECTED	0x0010
#define	KL_AWAITING_FIRST	0x0020
#define	KL_OIA_TWAIT		0x0040
#define	KL_OIA_LOCKED		0x0080
#define	KL_DEFERRED_UNLOCK	0x0100
#define KL_ENTER_INHIBIT	0x0200
#define KL_SCROLLED		0x0400

/* Shorthand macros */

#define CN	((char *) NULL)
#define PN	((XtPointer) NULL)

/* Portability macros */

/*   Replacement for memcpy that handles overlaps */

#if XtSpecificationRelease >= 5 /*[*/
#include <X11/Xfuncs.h>
#undef MEMORY_MOVE
#define MEMORY_MOVE(to,from,cnt)	bcopy(from,to,cnt)
#else /*][*/
#if !defined(MEMORY_MOVE) /*[*/
extern char *MEMORY_MOVE();
#endif /*]*/
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

/* Global Functions */

/*   ansi.c */
extern void ansi_init();
extern void ansi_process();
extern void ansi_send_clear();
extern void ansi_send_down();
extern void ansi_send_home();
extern void ansi_send_left();
extern void ansi_send_pa();
extern void ansi_send_pf();
extern void ansi_send_right();
extern void ansi_send_up();
extern void toggle_lineWrap();

/*   apl.c */
extern KeySym APLStringToKeysym();

/*   ctlr.c */
extern void ctlr_aclear();
extern void ctlr_add();
extern void ctlr_add_color();
extern void ctlr_add_gr();
extern void ctlr_altbuffer();
extern Boolean ctlr_any_data();
extern void ctlr_bcopy();
extern void ctlr_clear();
extern void ctlr_connect();
extern void ctlr_erase();
extern void ctlr_erase_all_unprotected();
extern void ctlr_init();
extern void ctlr_read_buffer();
extern void ctlr_read_modified();
extern void ctlr_shrink();
extern void ctlr_snap_buffer();
extern Boolean ctlr_snap_modes();
extern void ctlr_write();
extern struct ea *fa2ea();
extern Boolean get_bounded_field_attribute();
extern unsigned char *get_field_attribute();
extern void mdt_clear();
extern void mdt_set();
extern int next_unprotected();
extern int process_ds();
extern void ps_process();
extern void ps_set();
extern void set_rows_cols();
extern void ticking_start();
extern void ticking_stop();
extern void toggle_nop();
extern void toggle_showTiming();

/*   keypad.c */
extern Dimension keypad_qheight();
extern Dimension min_keypad_width();
extern void keypad_first_up();
extern Widget keypad_init();
extern void keypad_popup_init();
extern Dimension keypad_qheight();
extern void keypad_set_keymap();
extern void keypad_shift();
extern void move_keypad();

/*   kybd.c */
extern void add_xk();
extern void debug_action();
extern void do_reset();
extern int emulate_input();
extern void enq_ta();
extern void internal_action();
extern void key_AID();
extern void key_Attn();
extern void key_BTab();
extern void key_Clear();
extern void key_CursorSelect();
extern void key_Delete();
extern void key_Down();
extern void key_Dup();
extern void key_Enter();
extern void key_EraseEOF();
extern void key_EraseInput();
extern void key_FTab();
extern void key_FieldMark();
extern void key_Home();
extern void key_Insert();
extern void key_Left();
extern void key_MonoCase();
extern void key_PA();
extern void key_PF();
extern void key_Reset();
extern void key_Right();
extern void key_Shift();
extern void key_SysReq();
extern void key_Up();
extern void kybd_connect();
extern void kybd_inhibit();
extern void kybd_scroll_lock();
extern void kybdlock_set();
extern void kybdlock_clr();
extern XtTranslations lookup_tt();
extern Boolean run_ta();
extern int state_from_keymap();

/*   macros.c */
extern void ansi_text_fn();
extern void ascii_fn();
extern void ascii_field_fn();
extern void close_script_fn();
extern void continue_script_fn();
extern void ebcdic_fn();
extern void ebcdic_field_fn();
extern void execute_fn();
extern void execute_action_option();
extern void macro_command();
extern void macros_init();
extern void pause_script_fn();
extern Boolean scripting();
extern void script_connect_wait();
extern void script_continue();
extern void script_error();
extern void script_init();
extern void script_input();
extern void script_store();
extern void wait_fn();

/*   menubar.c */
extern void Connect();
extern void Disconnect();
extern void handle_menu();
extern void hostfile_init();
extern int hostfile_lookup();
extern void menubar_connect();
extern void menubar_gone();
extern void menubar_init();
extern void menubar_keypad_changed();
extern void menubar_newmode();
extern Dimension menubar_qheight();
extern void menubar_resize();
extern void menubar_retoggle();
extern void Reconnect();

/*   popups.c */
extern void Confirm();
extern void Info();
extern Widget create_form_popup();
extern void error_popup_init();
extern void info_popup_init();
extern void place_popup();
#if defined(__STDC__)
extern void popup_an_info(char *fmt, ...);
extern void popup_an_errno(int err, char *fmt, ...);
extern void popup_an_error(char *fmt, ...);
#else
extern void popup_an_info();
extern void popup_an_errno();
extern void popup_an_error();
#endif
extern void popup_options();
extern void popup_popup();
extern void toplevel_geometry();

/*   save.c */
extern void save_init();
extern void save_yourself();

/*   screen.c */
extern void aicon_font_init();
extern void aicon_size();
extern void blink_start();
extern void configure();
extern void cursor_move();
extern void do_toggle();
extern void enable_cursor();
extern void enter_leave();
extern void focus_change();
extern Boolean fprint_screen();
extern void initialize_toggles();
extern void keymap_event();
extern char *load_fixed_font();
extern void mcursor_normal();
extern void mcursor_waiting();
extern void mcursor_locked();
extern void quit_event();
extern void print_text();
extern void print_text_option();
extern void print_window();
extern void print_window_option();
extern void redraw();
extern void ring_bell();
extern void screen_change_model();
extern void screen_connect();
extern void screen_disp();
extern void screen_flip();
extern void screen_init();
extern int screen_newfont();
extern void screen_set_keymap();
extern void screen_set_thumb();
extern void screen_showikeypad();
extern void set_aicon_label();
extern void set_font();
extern void set_font_globals();
extern void set_translations();
extern void shift_event();
extern void shutdown_toggles();
extern void state_event();
extern void wm_protocols();

/*   scroll.c */
extern void jump_proc();
extern void rethumb();
extern void scroll_init();
extern void scroll_proc();
extern void scroll_reset();
extern void scroll_round();
extern void scroll_save();
extern void scroll_to_bottom();

/*   select.c */
extern void Cut();
extern void MoveCursor();
extern Boolean area_is_selected();
extern void move_select();
extern void select_end();
extern void select_extend();
extern void select_start();
extern void set_select();
extern void start_extend();
extern void unselect();

/*   sf.c */
extern void write_structured_field();

/* status.c */
extern void status_compose();
extern void status_connect();
extern void status_ctlr_done();
extern void status_ctlr_init();
extern void status_cursor_pos();
extern void status_disp();
extern void status_init();
extern void status_insert_mode();
extern void status_kmap();
extern void status_kybdlock();
extern void status_oerr();
extern void status_reset();
extern void status_reverse_mode();
extern void status_scrolled();
extern void status_shift_mode();
extern void status_syswait();
extern void status_timing();
extern void status_touch();
extern void status_twait();
extern void status_typeahead();
extern void status_uncursor_pos();
extern void status_untiming();

/*   telnet.c */
extern void net_add_eor();
extern void net_break();
extern void net_charmode();
extern int net_connect();
extern void net_disconnect();
extern void net_exception();
extern void net_input();
extern void net_linemode();
extern struct ctl_char *net_linemode_chars();
extern void net_output();
extern void net_sendc();
extern void net_sends();
extern void net_send_erase();
extern void net_send_kill();
extern void net_send_werase();
extern Boolean net_snap_options();
extern void trace_netdata();

/*   trace_ds.c */
extern char *see_color();
extern void toggle_dsTrace();
extern void toggle_eventTrace();
extern void toggle_screenTrace();
extern FILE *tracef;
extern void trace_screen();

/*   util.c */
extern char *ctl_see();
extern char *do_subst();
extern char *get_message();
extern char *get_resource();
extern int split_dresource();
extern int split_lresource();
extern char *xs_buffer();
extern char *xs_buffer2();
extern void xs_warning();
extern void xs_warning2();

/*   x3270.c */
extern void invert_icon();
extern void lock_icon();
extern void relabel();
extern void x3270_exit();
extern int x_connect();
extern void x_connected();
extern void x_disconnect();
extern void x_except_off();
extern void x_except_on();
extern Status x_get_window_attributes();
extern void x_in3270();
extern void x_reconnect();
