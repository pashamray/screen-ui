#pragma once
#include <stdint.h>
#include "ctx.h"
#include "view.h"
#include "widget.h"
#include "widget_list.h"
#include "theme.h"

#define STATIC_PANELS_MAX 4U

/* ── engine initialisation ───────────────────────────────────────────────── */
void render_set_gfx  (const Gfx   *g);
void render_set_view (const View  *v); /* NULL → use &view_default */
void render_set_theme(const Theme *t);
void render_set_font (const Font  *f); /* update active font; marks dirty */

/* ── screen-level entry points (mutually exclusive; share focus/edit state) */
void  render_screen   (const Layout *layout);
void  render_screen_at(const Layout *layout, int16_t x, int16_t y, int16_t w, int16_t h);
void  render_list     (const ListLayout *list);
void  render_list_at  (const ListLayout *list, int16_t x, int16_t y, int16_t w, int16_t h);

/* ── static panels — drawn every frame without focus ─────────────────────── */
void  render_add_static    (const Layout *layout, int16_t x, int16_t y, int16_t w, int16_t h);
void  render_remove_statics(void);

/* ── navigation history ──────────────────────────────────────────────────── */
void  render_back    (void);
int   render_can_back(void);

/* ── frame management ────────────────────────────────────────────────────── */
void  render_refresh   (void);   /* draw if dirty, then clear dirty flag */
void  render_mark_dirty(void);
int   render_is_dirty  (void);

const Layout     *render_current_layout(void);
const ListLayout *render_current_list  (void);

/* ── input events ────────────────────────────────────────────────────────── */
void  render_next    (void);
void  render_prev    (void);
void  render_activate(void);
void  render_cancel  (void);
void  render_inc     (void);
void  render_dec     (void);

/* ── edit mode queries ───────────────────────────────────────────────────── */
int   render_in_value_edit(void);
int   render_in_edit_mode (void);

/* ── W_EDIT character input (SDL / keyboard) ─────────────────────────────── */
void  render_edit_char  (char c);
void  render_edit_delete(void);

/* ── W_EDIT console helpers ──────────────────────────────────────────────── */
WidgetType render_focused_type(void);
void       render_edit_set    (const char *str);

/* ── backend contract (implemented per platform) ─────────────────────────── */
extern void render_init    (void);
extern void render_wait_key(void);
extern void render_quit    (void);
