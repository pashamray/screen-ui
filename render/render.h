#pragma once
#include <stdint.h>
#include "widget.h"
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
};

void  render_set(const Render *r);
void  render_screen(const Layout *layout);
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

void  render_screen_load(void *root);
void *render_screen_create(void);
void  render_screen_destroy(void *root);
