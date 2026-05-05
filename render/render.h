#pragma once
#include <stdint.h>
#include "widget.h"
#include "widget_list.h"
#include "theme.h"

/* cppcheck-suppress misra-c2012-5.6 */
typedef struct Font Font;

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
void  render_back(void);            // pop history and restore previous screen
int   render_can_back(void);        // non-zero if there is history to pop
void  render_refresh(void);       // draw if dirty, then clear dirty
void  render_mark_dirty(void);    // mark display as needing redraw (call from dynamic data updates)
int   render_is_dirty(void);
const Layout     *render_current_layout(void);
const ListLayout *render_current_list(void);

// input events — dispatch on_key first (normal mode), then act on focus
void  render_next(void);            // navigate forward / decrement value in edit
void  render_prev(void);            // navigate back   / increment value in edit
void  render_activate(void);        // confirm edit / enter edit / trigger on_click
void  render_cancel(void);          // cancel edit (restore backup) / back via on_key
void  render_inc(void);             // increment value (encoder +)
void  render_dec(void);             // decrement value (encoder −)

// edit mode queries
int   render_in_value_edit(void);   // W_VALUE: encoder rotation changes value
int   render_in_edit_mode(void);    // W_EDIT: string input active

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
extern void  render_set_font(const Font *f); // switch active font and redraw
