/*
 * Copyright 1993 Paul Mattes.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 */

/*
 *	ansi.c
 *		ANSI terminal emulation.
 */

#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include "globals.h"

extern unsigned char asc2cg[];
extern int cursor_addr;
extern Boolean is_altbuffer;

static void ansi_scroll();

#define	SC	1	/* save cursor position */
#define RC	2	/* restore cursor position */
#define NL	3	/* new line */
#define UP	4	/* cursor up */
#define	E2	5	/* second level of ESC processing */
#define RS	6	/* reset */
#define IC	7	/* insert chars */
#define DN	8	/* cursor down */
#define RT	9	/* cursor right */
#define LT	10	/* cursor left */
#define CM	11	/* cursor motion */
#define ED	12	/* erase in display */
#define EL	13	/* erase in line */
#define IL	14	/* insert lines */
#define DL	15	/* delete lines */
#define DC	16	/* delete characters */
#define	SG	17	/* set graphic rendition */
#define BL	18	/* ring bell */
#define NP	19	/* new page */
#define BS	20	/* backspace */
#define CR	21	/* carriage return */
#define LF	22	/* line feed */
#define HT	23	/* horizontal tab */
#define E1	24	/* first level of ESC processing */
#define Xx	25	/* undefined control character (nop) */
#define Pc	26	/* printing character */
#define Sc	27	/* semicolon (after ESC [) */
#define Dg	28	/* digit (after ESC [ or ESC [ ?) */
#define RI	29	/* reverse index */
#define DA	30	/* send device attributes */
#define SM	31	/* set mode */
#define RM	32	/* reset mode */
#define DO	33	/* return terminal ID (obsolete) */
#define SR	34	/* device status report */
#define CS	35	/* character set designate */
#define E3	36	/* third level of ESC processing */
#define DS	37	/* DEC private set */
#define DR	38	/* DEC private reset */
#define DV	39	/* DEC private save */
#define DT	40	/* DEC private restore */
#define SS	41	/* set scrolling region */
#define TM	42	/* text mode (ESC ]) */
#define T2	43	/* semicolon (after ESC ]) */
#define TX	44	/* text parameter (after ESC ] n ;) */
#define TB	45	/* text parameter done (ESC ] n ; xxx BEL) */
#define TS	46	/* tab set */
#define TC	47	/* tab clear */

static enum state {
    DATA = 0, ESC = 1, CSDES = 2,
    N1 = 3, DECP = 4, TEXT = 5, TEXT2 = 6
} state = DATA;

static enum state ansi_data_mode();
static enum state dec_save_cursor();
static enum state dec_restore_cursor();
static enum state ansi_newline();
static enum state ansi_cursor_up();
static enum state ansi_esc2();
static enum state ansi_reset();
static enum state ansi_insert_chars();
static enum state ansi_cursor_down();
static enum state ansi_cursor_right();
static enum state ansi_cursor_left();
static enum state ansi_cursor_motion();
static enum state ansi_erase_in_display();
static enum state ansi_erase_in_line();
static enum state ansi_insert_lines();
static enum state ansi_delete_lines();
static enum state ansi_delete_chars();
static enum state ansi_sgr();
static enum state ansi_bell();
static enum state ansi_newpage();
static enum state ansi_backspace();
static enum state ansi_cr();
static enum state ansi_lf();
static enum state ansi_htab();
static enum state ansi_escape();
static enum state ansi_nop();
static enum state ansi_printing();
static enum state ansi_semicolon();
static enum state ansi_digit();
static enum state ansi_reverse_index();
static enum state ansi_send_attributes();
static enum state ansi_set_mode();
static enum state ansi_reset_mode();
static enum state dec_return_terminal_id();
static enum state ansi_status_report();
static enum state ansi_cs_designate();
static enum state ansi_esc3();
static enum state dec_set();
static enum state dec_reset();
static enum state dec_save();
static enum state dec_restore();
static enum state dec_scrolling_region();
static enum state xterm_text_mode();
static enum state xterm_text_semicolon();
static enum state xterm_text();
static enum state xterm_text_do();
static enum state ansi_htab_set();
static enum state ansi_htab_clear();

static enum state (*ansi_fn[])() = {
/* 0 */		ansi_data_mode,
/* 1 */		dec_save_cursor,
/* 2 */		dec_restore_cursor,
/* 3 */		ansi_newline,
/* 4 */		ansi_cursor_up,
/* 5 */		ansi_esc2,
/* 6 */		ansi_reset,
/* 7 */		ansi_insert_chars,
/* 8 */		ansi_cursor_down,
/* 9 */		ansi_cursor_right,
/* 10 */	ansi_cursor_left,
/* 11 */	ansi_cursor_motion,
/* 12 */	ansi_erase_in_display,
/* 13 */	ansi_erase_in_line,
/* 14 */	ansi_insert_lines,
/* 15 */	ansi_delete_lines,
/* 16 */	ansi_delete_chars,
/* 17 */	ansi_sgr,
/* 18 */	ansi_bell,
/* 19 */	ansi_newpage,
/* 20 */	ansi_backspace,
/* 21 */	ansi_cr,
/* 22 */	ansi_lf,
/* 23 */	ansi_htab,
/* 24 */	ansi_escape,
/* 25 */	ansi_nop,
/* 26 */	ansi_printing,
/* 27 */	ansi_semicolon,
/* 28 */	ansi_digit,
/* 29 */	ansi_reverse_index,
/* 30 */	ansi_send_attributes,
/* 31 */	ansi_set_mode,
/* 32 */	ansi_reset_mode,
/* 33 */	dec_return_terminal_id,
/* 34 */	ansi_status_report,
/* 35 */	ansi_cs_designate,
/* 36 */	ansi_esc3,
/* 37 */	dec_set,
/* 38 */	dec_reset,
/* 39 */	dec_save,
/* 40 */	dec_restore,
/* 41 */	dec_scrolling_region,
/* 42 */	xterm_text_mode,
/* 43 */	xterm_text_semicolon,
/* 44 */	xterm_text,
/* 45 */	xterm_text_do,
/* 46 */	ansi_htab_set,
/* 47 */	ansi_htab_clear,
};

static unsigned char st[7][256] = {
/*
 * State table for base processing (state == DATA)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,BL,BS,HT,LF,LF,NP,CR,Xx,Xx,
/* 10 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,E1,Xx,Xx,Xx,Xx,
/* 20 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 30 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 40 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 50 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 60 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 70 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Xx,
/* 80 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* 90 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* a0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* b0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* c0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* d0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* e0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* f0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc
},

/*
 * State table for ESC processing (state == ESC)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0,CS,CS,CS,CS, 0, 0, 0, 0,
/* 30 */	0, 0, 0, 0, 0, 0, 0,SC,RC, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0,NL, 0, 0,TS, 0, 0, 0, 0,RI, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,E2, 0,TM, 0, 0,
/* 60 */	0, 0, 0,RS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ()*+ C processing (state == CSDES)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ processing (state == N1)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,Sc, 0, 0, 0,E3,
/* 40 */       IC,UP,DN,RT,LT, 0, 0, 0,CM, 0,ED,EL,IL,DL, 0, 0,
/* 50 */       DC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0,DA, 0, 0,CM,TC,SM, 0, 0, 0,RM,SG,SR, 0,
/* 70 */	0, 0,SS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ ? processing (state == DECP)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0,DS, 0, 0, 0,DR, 0, 0, 0,
/* 70 */	0, 0,DT,DV, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] processing (state == TEXT)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,T2, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] n ; processing (state == TEXT2)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */        0, 0, 0, 0, 0, 0, 0,TB, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 30 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 40 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 50 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 60 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 70 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,Xx,
/* 80 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 90 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* a0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* b0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* c0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* d0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* e0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* f0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX
},
};

static int saved_cursor = 0;
#define NN	20
static int n[NN], nx = 0;
#define NT	256
static char text[NT+1];
static int tx = 0;
static int ansi_ch;
static unsigned char ea = 0;
static int insert_mode = 0;
static int auto_newline_mode = 0;
static int appl_cursor = 0;		static int saved_appl_cursor = 0;
static int wraparound_mode = 0;		static int saved_wraparound_mode = 0;
static int rev_wraparound_mode = 0;	static int saved_rev_wraparound_mode = 0;
static Boolean saved_altbuffer = False;
static int scroll_top = -1;
static int scroll_bottom = -1;
static unsigned char tabs[(132+7)/8];

static enum state
ansi_data_mode()
{
	return DATA;
}

static enum state
dec_save_cursor()
{
	saved_cursor = cursor_addr;
	return DATA;
}

static enum state
dec_restore_cursor()
{
	cursor_move(saved_cursor);
	return DATA;
}

static enum state
ansi_newline()
{
	int nc;

	cursor_move(cursor_addr - (cursor_addr % COLS));
	nc = cursor_addr + COLS;
	if (nc < scroll_bottom * COLS)
		cursor_move(nc);
	else
		ansi_scroll();
	return DATA;
}

static enum state
ansi_cursor_up(nn)
int nn;
{
	int rr;

	if (nn < 1)
		nn = 1;
	rr = cursor_addr / COLS;
	if (rr - nn < 0)
		cursor_move(cursor_addr % COLS);
	else
		cursor_move(cursor_addr - (nn * COLS));
	return DATA;
}

static enum state
ansi_esc2()
{
	register int	i;

	for (i = 0; i < NN; i++)
		n[i] = 0;
	nx = 0;
	return N1;
}

static enum state
ansi_reset()
{
	int i;
	static Boolean first = True;

	ea = 0;
	saved_cursor = 0;
	insert_mode = 0;
	auto_newline_mode = 0;
	appl_cursor = 0;
	saved_appl_cursor = 0;
	wraparound_mode = 0;
	saved_wraparound_mode = 0;
	rev_wraparound_mode = 0;
	saved_rev_wraparound_mode = 0;
	saved_altbuffer = False;
	scroll_top = 1;
	scroll_bottom = ROWS;
	for (i = 0; i < sizeof(tabs); i++)
		tabs[i] = 0x01;
	if (!first) {
		ctlr_altbuffer(True);
		ctlr_aclear(0, ROWS * COLS, 1);
		ctlr_altbuffer(False);
		ctlr_aclear(0, ROWS * COLS, 1);
	}
	first = False;
	return DATA;
}

static enum state
ansi_insert_chars(nn)
int nn;
{
	int cc = cursor_addr % COLS;	/* current col */
	int mc = COLS - cc;		/* max chars that can be inserted */
	int ns;				/* chars that are shifting */

	if (nn < 1)
		nn = 1;
	if (nn > mc)
		nn = mc;

	/* Move the surviving chars right */
	ns = mc - nn;
	if (ns)
		ctlr_bcopy(cursor_addr, cursor_addr + nn, ns, 1);

	/* Clear the middle of the line */
	ctlr_aclear(cursor_addr, nn, 1);
	return DATA;
}

static enum state
ansi_cursor_down(nn)
int nn;
{
	int rr;

	if (nn < 1)
		nn = 1;
	rr = cursor_addr / COLS;
	if (rr + nn >= ROWS)
		cursor_move((ROWS-1)*COLS + (cursor_addr%COLS));
	else
		cursor_move(cursor_addr + (nn * COLS));
	return DATA;
}

static enum state
ansi_cursor_right(nn)
int nn;
{
	int cc;

	if (nn < 1)
		nn = 1;
	cc = cursor_addr % COLS;
	if (cc == COLS-1)
		return DATA;
	if (cc + nn >= COLS)
		nn = COLS - 1 - cc;
	cursor_move(cursor_addr + nn);
	return DATA;
}

static enum state
ansi_cursor_left(nn)
int nn;
{
	int cc;

	if (nn < 1)
		nn = 1;
	cc = cursor_addr % COLS;
	if (!cc)
		return DATA;
	if (nn > cc)
		nn = cc;
	cursor_move(cursor_addr - nn);
	return DATA;
}

static enum state
ansi_cursor_motion(n1, n2)
int n1, n2;
{
	if (n1 < 1) n1 = 1;
	if (n1 > ROWS) n1 = ROWS;
	if (n2 < 1) n2 = 1;
	if (n2 > COLS) n2 = COLS;
	cursor_move((n1 - 1) * COLS + (n2 - 1));
	return DATA;
}

static enum state
ansi_erase_in_display(nn)
int nn;
{
	switch (nn) {
	    case 0:	/* below */
		ctlr_aclear(cursor_addr, (ROWS * COLS) - cursor_addr, 1);
		break;
	    case 1:	/* above */
		ctlr_aclear(0, cursor_addr + 1, 1);
		break;
	    case 2:	/* all (without moving cursor) */
		ctlr_aclear(0, ROWS * COLS, 1);
		break;
	}
	return DATA;
}

static enum state
ansi_erase_in_line(nn)
int nn;
{
	int nc = cursor_addr % COLS;

	switch (nn) {
	    case 0:	/* to right */
		ctlr_aclear(cursor_addr, COLS - nc, 1);
		break;
	    case 1:	/* to left */
		ctlr_aclear(cursor_addr - nc, nc+1, 1);
		break;
	    case 2:	/* all */
		ctlr_aclear(cursor_addr - nc, COLS, 1);
		break;
	}
	return DATA;
}

static enum state
ansi_insert_lines(nn)
int nn;
{
	int rr = cursor_addr / COLS;	/* current row */
	int mr = scroll_bottom - rr;	/* rows left at and below this one */
	int ns;				/* rows that are shifting */

	/* If outside of the scrolling region, do nothing */
	if (rr < scroll_top - 1 || rr >= scroll_bottom)
		return DATA;

	if (nn < 1)
		nn = 1;
	if (nn > mr)
		nn = mr;
	
	/* Move the victims down */
	ns = mr - nn;
	if (ns)
		ctlr_bcopy(rr * COLS, (rr + nn) * COLS, ns * COLS, 1);

	/* Clear the middle of the screen */
	ctlr_aclear(rr * COLS, nn * COLS, 1);
	return DATA;
}

static enum state
ansi_delete_lines(nn)
int nn;
{
	int rr = cursor_addr / COLS;	/* current row */
	int mr = scroll_bottom - rr;	/* max rows that can be deleted */
	int ns;				/* rows that are shifting */

	/* If outside of the scrolling region, do nothing */
	if (rr < scroll_top - 1 || rr >= scroll_bottom)
		return DATA;

	if (nn < 1)
		nn = 1;
	if (nn > mr)
		nn = mr;

	/* Move the surviving rows up */
	ns = mr - nn;
	if (ns)
		ctlr_bcopy((rr + nn) * COLS, rr * COLS, ns * COLS, 1);

	/* Clear the rest of the screen */
	ctlr_aclear((rr + ns) * COLS, nn * COLS, 1);
	return DATA;
}

static enum state
ansi_delete_chars(nn)
int nn;
{
	int cc = cursor_addr % COLS;	/* current col */
	int mc = COLS - cc;		/* max chars that can be deleted */
	int ns;				/* chars that are shifting */

	if (nn < 1)
		nn = 1;
	if (nn > mc)
		nn = mc;

	/* Move the surviving chars left */
	ns = mc - nn;
	if (ns)
		ctlr_bcopy(cursor_addr + nn, cursor_addr, ns, 1);

	/* Clear the end of the line */
	ctlr_aclear(cursor_addr + ns, nn, 1);
	return DATA;
}

static enum state
ansi_sgr(nn)
int nn;
{
	ea = nn % 0xf;
	return DATA;
}

static enum state
ansi_bell()
{
	ring_bell();
	return DATA;
}

static enum state
ansi_newpage()
{
	ctlr_clear();
	return DATA;
}

static enum state
ansi_backspace()
{
	if (rev_wraparound_mode) {
		if (cursor_addr > (scroll_top - 1) * COLS)
			cursor_move(cursor_addr - 1);
	} else {
		if (cursor_addr % COLS)
			cursor_move(cursor_addr - 1);
	}
	return DATA;
}

static enum state
ansi_cr()
{
	if (cursor_addr % COLS)
		cursor_move(cursor_addr - (cursor_addr % COLS));
	if (auto_newline_mode)
		(void) ansi_lf();
	return DATA;
}

static enum state
ansi_lf()
{
	int nc = cursor_addr + COLS;

	/* If we're below the scrolling region, don't scroll. */
	if ((cursor_addr / COLS) >= scroll_bottom) {
		if (nc < ROWS * COLS)
			cursor_move(nc);
		return DATA;
	}

	if (nc < scroll_bottom * COLS)
		cursor_move(nc);
	else
		ansi_scroll();
	return DATA;
}

static enum state
ansi_htab()
{
	int col = cursor_addr % COLS;
	int i;

	if (col == COLS-1)
		return DATA;
	for (i = col+1; i < COLS-1; i++)
		if (tabs[i/8] & 1<<(i%8))
			break;
	cursor_move(cursor_addr - col + i);
	return DATA;
}

static enum state
ansi_escape()
{
	return ESC;
}

static enum state
ansi_nop()
{
	return DATA;
}

static enum state
ansi_printing()
{
	int nc;

	if (insert_mode)
		(void) ansi_insert_chars(1);
	ctlr_add(cursor_addr, asc2cg[ansi_ch]);
	ctlr_add_ea(cursor_addr, ea);
	if (wraparound_mode) {
		nc = cursor_addr + 1;
		if (nc < scroll_bottom * COLS)
			cursor_move(nc);
		else if (cursor_addr / COLS >= scroll_bottom)
			cursor_move(cursor_addr / COLS * COLS);
		else {
			ansi_scroll();
			cursor_move(nc - COLS);
		}
	} else {
		if ((cursor_addr % COLS) != (COLS - 1))
			cursor_move(cursor_addr + 1);
	}
	return DATA;
}

static enum state
ansi_semicolon()
{
	if (nx >= NN)
		return DATA;
	nx++;
	return state;
}

static enum state
ansi_digit()
{
	n[nx] = (n[nx] * 10) + (ansi_ch - '0');
	return state;
}

static enum state
ansi_reverse_index()
{
	int rr = cursor_addr / COLS;	/* current row */
	int np = (scroll_top - 1) - rr;	/* number of rows in the scrolling
					   region, above this line */
	int ns;				/* number of rows to scroll */
	int nn = 1;			/* number of rows to index */

	/* If the cursor is above the scrolling region, do a simple margined
	   cursor up.  */
	if (np < 0) {
		(void) ansi_cursor_up(nn);
		return DATA;
	}

	/* Split the number of lines to scroll into ns */
	if (nn > np) {
		ns = nn - np;
		nn = np;
	} else
		ns = 0;

	/* Move the cursor up without scrolling */
	if (nn)
		(void) ansi_cursor_up(nn);

	/* Insert lines at the top for backward scroll */
	if (ns)
		(void) ansi_insert_lines(ns);

	return DATA;
}

static enum state
ansi_send_attributes(nn)
int nn;
{
	if (!nn)
		net_sends("\033[?1;2c");
	return DATA;
}

static enum state
dec_return_terminal_id()
{
	return ansi_send_attributes(0);
}

static enum state
ansi_set_mode(nn)
int nn;
{
	switch (nn) {
	    case 4:
		insert_mode = 1;
		break;
	    case 20:
		auto_newline_mode = 1;
		break;
	}
	return DATA;
}

static enum state
ansi_reset_mode(nn)
int nn;
{
	switch (nn) {
	    case 4:
		insert_mode = 0;
		break;
	    case 20:
		auto_newline_mode = 0;
		break;
	}
	return DATA;
}

static enum state
ansi_status_report(nn)
int nn;
{
	static char cpr[11];

	switch (nn) {
	    case 5:
		net_sends("\033[0n");
		break;
	    case 6:
		(void) sprintf(cpr, "\033[%d;%dR",
		    (cursor_addr/COLS) + 1, (cursor_addr%COLS) + 1);
		net_sends(cpr);
		break;
	}
	return DATA;
}

static enum state
ansi_cs_designate()
{
	return CSDES;
}

static enum state
ansi_esc3()
{
	return DECP;
}

static enum state
dec_set()
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i]) {
		    case 1:	/* application cursor keys */
			appl_cursor = 1;
			break;
		    case 7:	/* wraparound mode */
			wraparound_mode = 1;
			break;
		    case 45:	/* reverse-wraparound mode */
			rev_wraparound_mode = 1;
			break;
		    case 47:	/* alt buffer */
			ctlr_altbuffer(True);
			break;
		}
	return DATA;
}

static enum state
dec_reset()
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i]) {
		    case 1:	/* normal cursor keys */
			appl_cursor = 0;
			break;
		    case 7:	/* no wraparound mode */
			wraparound_mode = 0;
			break;
		    case 45:	/* no reverse-wraparound mode */
			rev_wraparound_mode = 0;
			break;
		    case 47:	/* alt buffer */
			ctlr_altbuffer(False);
			break;
		}
	return DATA;
}

static enum state
dec_save()
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i]) {
		    case 1:	/* application cursor keys */
			saved_appl_cursor = appl_cursor;
			break;
		    case 7:	/* wraparound mode */
			saved_wraparound_mode = wraparound_mode;
			break;
		    case 45:	/* reverse-wraparound mode */
			saved_rev_wraparound_mode = rev_wraparound_mode;
			break;
		    case 47:	/* alt buffer */
			saved_altbuffer = is_altbuffer;
			break;
		}
	return DATA;
}

static enum state
dec_restore()
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i]) {
		    case 1:	/* application cursor keys */
			appl_cursor = saved_appl_cursor;
			break;
		    case 7:	/* wraparound mode */
			wraparound_mode = saved_wraparound_mode;
			break;
		    case 45:	/* reverse-wraparound mode */
			rev_wraparound_mode = saved_rev_wraparound_mode;
			break;
		    case 47:	/* alt buffer */
			ctlr_altbuffer(saved_altbuffer);
			break;
		}
	return DATA;
}

static enum state
dec_scrolling_region(top, bottom)
int top, bottom;
{
	if (top < 1)
		top = 1;
	if (bottom > ROWS)
		bottom = ROWS;
	if (top <= bottom && (top > 1 || bottom < ROWS)) {
		scroll_top = top;
		scroll_bottom = bottom;
	} else {
		scroll_top = 1;
		scroll_bottom = ROWS;
	}
	return DATA;
}

static enum state
xterm_text_mode()
{
	nx = 0;
	n[0] = 0;
	return TEXT;
}

static enum state
xterm_text_semicolon()
{
	tx = 0;
	return TEXT2;
}

static enum state
xterm_text()
{
	if (tx < NT)
		text[tx++] = ansi_ch;
	return state;
}

static enum state
xterm_text_do()
{
	text[tx] = '\0';

	switch (n[0]) {
	    case 0:	/* icon name and window title */
		XtVaSetValues(toplevel, XtNiconName, text, NULL);
		XtVaSetValues(toplevel, XtNtitle, text, NULL);
		break;
	    case 1:	/* icon name */
		XtVaSetValues(toplevel, XtNiconName, text, NULL);
		break;
	    case 2:	/* window_title */
		XtVaSetValues(toplevel, XtNtitle, text, NULL);
		break;
	    case 50:	/* font */
		(void) screen_newfont(text, False);
		break;
	}
	return DATA;
}

static enum state
ansi_htab_set()
{
	register int col = cursor_addr % COLS;

	tabs[col/8] |= 1<<(col%8);
	return DATA;
}

static enum state
ansi_htab_clear(nn)
int nn;
{
	register int col, i;

	switch (nn) {
	    case 0:
		col = cursor_addr % COLS;
		tabs[col/8] &= ~(1<<(col%8));
		break;
	    case 3:
		for (i = 0; i < sizeof(tabs); i++)
			tabs[i] = 0;
		break;
	}
	return DATA;
}

/*
 * Scroll the screen or the scrolling region.
 */
static void
ansi_scroll()
{
	/* Scroll all but the last line up */
	if (scroll_bottom > scroll_top)
		ctlr_bcopy(scroll_top * COLS,
		    (scroll_top - 1) * COLS,
		    (scroll_bottom - scroll_top) * COLS,
		    1);

	/* Clear the last line */
	ctlr_aclear((scroll_bottom - 1) * COLS, COLS, 1);
}


/*
 * External entry points
 */

void
ansi_init()
{
	(void) ansi_reset();
}

void
ansi_process(c)
unsigned int c;
{
	c &= 0xff;
	ansi_ch = c;

	state = (*ansi_fn[st[state][c]])(n[0], n[1]);
}

void
ansi_send_up()
{
	if (appl_cursor)
		net_sends("\033OA");
	else
		net_sends("\033[A");
}

void
ansi_send_down()
{
	if (appl_cursor)
		net_sends("\033OB");
	else
		net_sends("\033[B");
}

void
ansi_send_right()
{
	if (appl_cursor)
		net_sends("\033OC");
	else
		net_sends("\033[C");
}

void
ansi_send_left()
{
	if (appl_cursor)
		net_sends("\033OD");
	else
		net_sends("\033[D");
}

void
ansi_send_home()
{
	net_sends("\033[H");
}

void
ansi_send_clear()
{
	net_sends("\033[2K");
}

void
ansi_send_pf(nn)
int nn;
{
	static char fn_buf[6];
	static int code[20] = {
		11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24,
		25, 26, 28, 29, 31, 32, 33, 34
	};

	if (nn < 1 || nn > 20)
		return;
	(void) sprintf(fn_buf, "\033[%d~", code[nn-1]);
	net_sends(fn_buf);
}

void
ansi_send_pa(nn)
int nn;
{
	static char fn_buf[4];
	static char code[4] = { 'P', 'Q', 'R', 'S' };

	if (nn < 1 || nn > 4)
		return;
	(void) sprintf(fn_buf, "\033O%c", code[nn-1]);
	net_sends(fn_buf);
}
