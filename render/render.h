#pragma once
#include <stdint.h>
#include "widget.h"
#include "widget_list.h"
#include "theme.h"

typedef struct {
    uint16_t fg;
    uint16_t bg;
} DrawStyle;

void render_set_theme(const Theme *t);


typedef struct Render Render;

struct Render {
    // ── frame-level hooks ─────────────────────────────────────────────────────
    void (*begin_frame)      (const Render *r, const Theme *t);
    void (*flush)            (void);
    // called instead of the normal frame when W_EDIT is in string-input mode
    void (*draw_edit_overlay)(const Render *r, const Widget *w, const Theme *t, int cursor);

    uint16_t screen_w;
    uint16_t screen_h;

    // ── widget draw callbacks ─────────────────────────────────────────────────
    void (*draw_label)   (const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t);
    void (*draw_btn)     (const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t, int focused);
    void (*draw_value)   (const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t, int focused, int editing);
    void (*draw_progress)(const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t);
    void (*draw_edit)    (const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t, int focused);
    // ── list callbacks ───────────────────────────────────────────────────────
    void (*draw_list_item)     (const Render *r,
                                int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                const ListItem *item, const Theme *t,
                                int focused, int editing);
    void (*draw_list_separator)(const Render *r,
                                int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                const Theme *t);
};

void  render_set(const Render *r);
// ── screen-level entry points (mutually exclusive; share focus/edit state) ───
void  render_screen(const Layout *layout);
void  render_list(const ListLayout *list);
void  render_refresh(void);

// widget focus navigation
void  render_focus_next(void);
void  render_focus_prev(void);
// change W_VALUE / move W_EDIT cursor
void  render_focus_inc(void);
void  render_focus_dec(void);
// activate focused widget (W_BTN on_click / enter W_EDIT / confirm edit)
void  render_focus_activate(void);

// edit mode queries
int   render_in_value_edit(void);   // W_VALUE: encoder rotation changes value
int   render_in_edit_mode(void);    // W_EDIT: string input active
void  render_cancel(void);          // cancel any edit mode and restore backup

// W_EDIT — direct character input (SDL / keyboard platforms)
void  render_edit_char(char c);
void  render_edit_delete(void);

// W_EDIT — query focused widget type and set value (console platform)
WidgetType render_focused_type(void);
void       render_edit_set(const char *str);

// ── backend contract (must be implemented by each renderer) ──────────────────
extern void  render_init(void);       // init hardware / window, call render_set() + render_set_theme()
extern void  render_wait_key(void);   // platform event loop — blocks until exit
extern void  render_quit(void);       // release resources

extern void  render_screen_load(void *root);
extern void *render_screen_create(void);
extern void  render_screen_destroy(void *root);
