#pragma once
#include <stdint.h>
#include "widget.h"

typedef struct {
    uint16_t fg;
    uint16_t bg;
} DrawStyle;

typedef struct {
    uint16_t screen_bg;      // canvas fill
    uint16_t text_fg;        // default foreground text
    uint16_t widget_bg;      // button / value / edit background
    uint16_t border_normal;  // unfocused border
    uint16_t border_focused; // focused border
    uint16_t border_editing; // value being edited (encoder)
    uint16_t border_subtle;  // de-emphasised border (W_VALUE idle)
    uint16_t cursor_fg;      // text cursor character foreground
    uint16_t cursor_bg;      // text cursor character background
    uint16_t hint_fg;        // overlay hint text
} Theme;

extern const Theme theme_default;
void render_set_theme(const Theme *t);


typedef struct Render Render;

struct Render {
    // ── drawing primitives ────────────────────────────────────────────────────
    void (*fill_rect)  (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void (*draw_text)  (int16_t x, int16_t y, const char *text, const DrawStyle *style);
    void (*draw_border)(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color, uint8_t thickness);
    void (*flush)      (void);

    uint16_t screen_w;
    uint16_t screen_h;
    uint8_t  char_w;   // logical pixels per character column (text-mode renderers)
    uint8_t  char_h;   // logical pixels per character row

    // ── optional widget overrides (NULL → built-in drawing) ───────────────────
    void (*draw_btn)  (const Render *r, int16_t x, int16_t y,
                       const Widget *w, const Theme *t, int focused);
    void (*draw_value)(const Render *r, int16_t x, int16_t y,
                       const Widget *w, const Theme *t, int focused, int editing);
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
