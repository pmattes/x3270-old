/*
 * Copyright 1995, 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	screenc.h
 *		Global declarations for screen.c.
 */

extern Dimension main_width, main_height;

extern void blink_start(void);
extern void cursor_move(int baddr);
extern unsigned display_height(void);
extern unsigned display_heightMM(void);
extern unsigned display_width(void);
extern unsigned display_widthMM(void);
extern void enable_cursor(Boolean on);
extern void font_init(void);
extern void icon_init(void);
extern const char *load_fixed_font(const char *name);
extern void mcursor_locked(void);
extern void mcursor_normal(void);
extern void mcursor_waiting(void);
extern void PA_ConfigureNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_EnterLeave_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_Expose_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_Focus_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_GraphicsExpose_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_KeymapNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_StateChanged_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_VisibilityNotify_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void PA_WMProtocols_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void Redraw_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void ring_bell(void);
extern void save_00translations(Widget w, XtTranslations *t00);
extern void screen_change_model(int mn, int ovc, int ovr);
extern void screen_disp(void);
extern void screen_extended(Boolean extended);
extern void screen_flip(void);
extern GC screen_gc(int color);
extern void screen_init(void);
extern GC screen_invgc(int color);
extern void screen_m3279(Boolean m3279);
extern void screen_newcharset(char *csname);
extern void screen_newfont(char *fontname, Boolean do_popup);
extern void screen_newscheme(char *s);
extern Boolean screen_obscured(void);
extern void screen_scroll(void);
extern void screen_set_keymap(void);
extern void screen_set_temp_keymap(XtTranslations trans);
extern void screen_set_thumb(float top, float shown);
extern void screen_showikeypad(Boolean on);
extern void SetFont_action(Widget w, XEvent *event, String *params,
    Cardinal *num_params);
extern void set_aicon_label(char *l);
extern void set_translations(Widget w, XtTranslations *t00, XtTranslations *t0);
extern void shift_event(int event_state);
extern void toggle_altCursor(struct toggle *t, enum toggle_type tt);
extern void toggle_cursorBlink(struct toggle *t, enum toggle_type tt);
extern void toggle_cursorPos(struct toggle *t, enum toggle_type tt);
extern void toggle_monocase(struct toggle *t, enum toggle_type tt);
extern void toggle_scrollBar(struct toggle *t, enum toggle_type tt);
