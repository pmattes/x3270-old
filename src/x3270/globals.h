/*
 * Copyright 1990 Jeff Sparkes.
 * Copyright 1993 Paul Mattes.
 *
 *	All rights reserved.
 */

/*
 *	globals.h
 *		Common definitions for x3270.
 */

struct toggle {
	Boolean value;		/* toggle value */
	Widget w[2];		/* the menu item widgets */
	char *label[2];		/* labels */
	void (*upcall)();	/* change value */
};
#define MONOCASE	0
#define ALTCURSOR	1
#define BLINK		2
#define TIMING		3
#define CURSORP		4
#define TRACE3270	5
#define TRACETN		6

#define N_TOGGLES	7

typedef struct {
	Pixel	foreground;
	Pixel	background;
	Pixel	normal;
	Pixel	select;
	Pixel	bold;
	Pixel	colorbg;
	Pixel	keypadbg;
	Pixel	selbg;
	Boolean mono;
	Boolean visual_bell;
	Boolean	keypad_on;
	Boolean menubar;
	Boolean active_icon;
	Boolean label_icon;
	Boolean	apl_mode;
	Boolean	once;
	XtTranslations	default_translations;
	Cursor	wait_mcursor;
	Cursor	locked_mcursor;
	char	*keypad;
	char	*efontname;
	char	*bfontname;
	char	*model;
	char	*key_map;
	char	*compose_map;
	char	*hostsfile;
	char	*ad_version;
	char	*port;
	char	*charset;
	char	*termname;
	char	*large_font;
	char	*small_font;
	char	*icon_font;
	char	*icon_label_font;
	struct toggle toggle[N_TOGGLES];

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

extern int		COLS;
extern int		ROWS;
extern int		actioncount;
extern XtActionsRec	actions[];
extern AppRes		appres;
extern XtAppContext	appcontext;
extern Atom		a_delete_me;
extern Font		bfont;
extern XFontStruct      **bfontinfo;
extern char		*bfontname;
extern char		*current_host;
extern int		depth;
extern Display		*display;
extern Font		efont;
extern XFontStruct      **efontinfo;
extern char		*efontname;
extern Boolean		error_popup_visible;
extern Boolean		exiting;
extern char		*full_current_host;
extern Boolean		*latin1_font;
extern Pixmap		gray;
extern Boolean		keypad;
extern int		maxCOLS;
extern int		maxROWS;
extern int		model_num;
extern char		*programname;
extern int		root_window;
extern Boolean		shifted;
extern Boolean		*standard_font;
extern Widget		toplevel;

#define toggled(ix)		(appres.toggle[ix].value)
#define toggle_toggle(t)	((t)->value = !(t)->value)

enum placement { Center, Bottom, Right };
extern enum kp_placement { kp_right, kp_bottom, kp_integral } kp_placement;

extern struct trans_list {
	char			*name;
	char			*translations;
	struct trans_list	*next;
} *trans_list;

extern enum cstate {
	NOT_CONNECTED,		/* no socket, unknown mode */
	PENDING,		/* connection pending */
	CONNECTED_INITIAL,	/* connected in ANSI mode */
	CONNECTED_3270		/* connected in 3270 mode */
} cstate;
extern Boolean		ansi_host;
extern Boolean		ever_3270;

#define PCONNECTED	(cstate >= PENDING)
#define HALF_CONNECTED	(cstate == PENDING)
#define CONNECTED	(cstate >= CONNECTED_INITIAL)
#define IN_3270		(cstate == CONNECTED_3270)
#define IN_ANSI		(cstate == CONNECTED_INITIAL && ansi_host)

struct ctl_char {
	char *name;
	char value[3];
};

#define ShiftKeyDown	0x01
#define MetaKeyDown	0x02
#define AltKeyDown	0x04

/* Replacement for memcpy that handles overlaps */

#ifndef MEMORY_MOVE
extern char *MEMORY_MOVE();
#endif

/* Global Functions */

/* ansi.c */
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

/* apl.c */
extern KeySym APLStringToKeySym();

/* ctlr.c */
extern void ctlr_aclear();
extern void ctlr_add();
extern void ctlr_add_ea();
extern void ctlr_altbuffer();
extern void ctlr_bcopy();
extern void ctlr_clear();
extern void ctlr_erase();
extern void ctlr_init();
extern void ctlr_read_modified();
extern unsigned char get_extended_attribute();
extern unsigned char *get_field_attribute();
extern void mdt_clear();
extern void mdt_set();
extern int next_unprotected();
extern int process_ds();
extern void ps_process();
extern void ps_set();
extern char *rcba();
extern void set_rows_cols();
extern void ticking_start();
extern void ticking_stop();
extern void toggle_nop();
extern void toggle_timing();
extern void toggle_tracetn();

/* keypad.c */
extern void keypad_at_startup();
extern Widget keypad_init();
extern void keypad_popup_init();
extern void keypad_shift();

/* kybd.c */
extern int emulate_input();
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
extern void key_PA1();
extern void key_PA2();
extern void key_PA3();
extern void key_PF1();
extern void key_PF10();
extern void key_PF11();
extern void key_PF12();
extern void key_PF13();
extern void key_PF14();
extern void key_PF15();
extern void key_PF16();
extern void key_PF17();
extern void key_PF18();
extern void key_PF19();
extern void key_PF2();
extern void key_PF20();
extern void key_PF21();
extern void key_PF22();
extern void key_PF23();
extern void key_PF24();
extern void key_PF3();
extern void key_PF4();
extern void key_PF5();
extern void key_PF6();
extern void key_PF7();
extern void key_PF8();
extern void key_PF9();
extern void key_Reset();
extern void key_Right();
extern void key_Shift();
extern void key_SysReq();
extern void key_Up();
extern void kybd_connect();
extern int state_from_keymap();

/* menubar.c */
extern void handle_menu();
extern void hostfile_init();
extern int hostfile_lookup();
extern void menubar_connect();
extern void menubar_gone();
extern Dimension menubar_init();
extern void menubar_newmode();
extern void menubar_resize();
extern void menubar_retoggle();

/* popups.c */
extern Widget create_form_popup();
extern void error_popup_init();
extern void place_popup();
extern void popup_an_errno();
extern void popup_an_error();
extern void popup_options();
extern void popup_popup();

/* screen.c */
extern void aicon_font_init();
extern void aicon_init();
extern void aicon_size();
extern void cursor_move();
extern void delete_window();
extern void do_toggle();
extern void enter_leave();
extern void focus_change();
extern void keymap_event();
extern char *load_fixed_font();
extern void mcursor_normal();
extern void mcursor_waiting();
extern void mcursor_locked();
extern void quit_event();
extern void print_text();
extern void print_window();
extern void redraw();
extern void ring_bell();
extern void screen_change_model();
extern void screen_connect();
extern void screen_disp();
extern void screen_init();
extern int screen_newfont();
extern void screen_showkeypad();
extern void set_aicon_label();
extern void set_font();
extern void set_font_globals();
extern void set_translations();
extern void shift_event();
extern void state_event();

/* select.c */
extern void MoveCursor();
extern Boolean area_is_selected();
extern void move_select();
extern void select_end();
extern void select_extend();
extern void select_start();
extern void start_extend();
extern void unselect();

/* status.c */
extern void status_compose();
extern void status_connect();
extern void status_ctlr_done();
extern void status_ctlr_init();
extern void status_cursor_pos();
extern void status_disp();
extern void status_init();
extern void status_insert_mode();
extern void status_numeric();
extern void status_overflow();
extern void status_protected();
extern void status_reset();
extern void status_shift_mode();
extern void status_syswait();
extern void status_timing();
extern void status_touch();
extern void status_twait();
extern void status_uncursor_pos();
extern void status_untiming();

/* telnet.c */
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

/* x3270.c */
extern char *get_message();
extern char *get_resource();
extern void invert_icon();
extern void relabel();
extern void set_toplevel_translations();
extern int x_connect();
extern void x_connected();
extern void x_disconnect();
extern void x_except_off();
extern void x_except_on();
extern void x_in3270();
extern char *xs_buffer();
extern char *xs_buffer2();
extern void xs_warning();
extern void xs_warning2();
