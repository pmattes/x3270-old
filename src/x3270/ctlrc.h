/*
 * Copyright 1995, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	ctlrc.h
 *		Global declarations for ctlr.c.
 */

void ctlr_aclear(int baddr, int count, int clear_ea);
void ctlr_add(int baddr, unsigned char c, unsigned char cs);
void ctlr_add_bg(int baddr, unsigned char color);
void ctlr_add_fg(int baddr, unsigned char color);
void ctlr_add_gr(int baddr, unsigned char gr);
void ctlr_altbuffer(Boolean alt);
Boolean ctlr_any_data(void);
void ctlr_bcopy(int baddr_from, int baddr_to, int count, int move_ea);
void ctlr_changed(int bstart, int bend);
void ctlr_clear(Boolean can_snap);
void ctlr_erase(Boolean alt);
void ctlr_erase_all_unprotected(void);
void ctlr_init(unsigned cmask);
void ctlr_read_buffer(unsigned char aid_byte);
void ctlr_read_modified(unsigned char aid_byte, Boolean all);
void ctlr_reinit(unsigned cmask);
void ctlr_scroll(void);
void ctlr_shrink(void);
void ctlr_snap_buffer(void);
Boolean ctlr_snap_modes(void);
void ctlr_write(unsigned char buf[], int buflen, Boolean erase);
struct ea *fa2ea(unsigned char *fa);
unsigned char *get_field_attribute(register int baddr);
Boolean get_bounded_field_attribute(register int baddr, register int bound,
    unsigned char *fa_out);
void mdt_clear(unsigned char *fa);
void mdt_set(unsigned char *fa);
int next_unprotected(int baddr0);
int process_ds(unsigned char *buf, int buflen);
void ps_process(void);
void set_rows_cols(int mn, int ovc, int ovr);
void ticking_start(Boolean anyway);
void toggle_nop(struct toggle *t, enum toggle_type tt);
void toggle_showTiming(struct toggle *t, enum toggle_type tt);
