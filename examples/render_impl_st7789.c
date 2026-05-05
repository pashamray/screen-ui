#include "render.h"
#include "fonts.h"
#include "theme_default.h"

// Include your display driver here:
// #include "st7789.h"

typedef struct { uint16_t fg; uint16_t bg; } DrawStyle;

static const Font *s_font;

// ── primitives ───────────────────────────────────────────────────────────────

static void my_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // ST7789_SetWindow(x, y, x+w-1, y+h-1);
    // for (uint32_t i = 0; i < (uint32_t)w*h; i++) ST7789_WritePixel(color);
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void my_draw_text(int16_t x, int16_t y, const char *text, const DrawStyle *s,
                          const Font *f) {
    // Render each glyph from f->data using s->fg on s->bg background
    (void)x; (void)y; (void)text; (void)s; (void)f;
}

static void my_draw_border(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t t) {
    my_fill_rect(x,     y,     w, t, color);
    my_fill_rect(x,     y+h-t, w, t, color);
    my_fill_rect(x,     y,     t, h, color);
    my_fill_rect(x+w-t, y,     t, h, color);
}

static void my_flush(void) {
    // DMA transfer or display commit if needed
}

// ── frame hooks ───────────────────────────────────────────────────────────────

static void st7789_begin_frame(const Render *r, const Theme *t) {
    my_fill_rect(0, 0, (int16_t)r->screen_w, (int16_t)r->screen_h, t->screen_bg);
}

// ── widget callbacks ──────────────────────────────────────────────────────────

static void st7789_draw_label(const Render *r, int16_t x, int16_t y,
                               const Widget *w, const Theme *t) {
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    DrawStyle s = { .fg = fg, .bg = t->screen_bg };
    my_draw_text(x, y, w->text, &s, s_font);
}

static void st7789_draw_btn(const Render *r, int16_t x, int16_t y,
                             const Widget *w, const Theme *t, int focused) {
    (void)r;
    uint16_t fg  = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg  = w->colors ? w->colors->bg : t->screen_bg;
    uint16_t brd = focused ? t->border_focused : t->border_subtle;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, brd, 1);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text((int16_t)(x + 4), (int16_t)(y + (w->h - s_font->h) / 2), w->text, &s, s_font);
}

static void st7789_draw_value(const Render *r, int16_t x, int16_t y,
                               const Widget *w, const Theme *t, int focused, int editing) {
    (void)r;
    uint16_t fg  = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg  = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t brd = editing ? t->border_editing : focused ? t->border_focused : t->border_subtle;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, brd, 1);
    char buf[48];
    if (w->options && w->value)
        snprintf(buf, sizeof(buf), "%s: %s", w->text, w->options[*w->value]);
    else if (w->value)
        snprintf(buf, sizeof(buf), "%s: %d", w->text, *w->value);
    else
        snprintf(buf, sizeof(buf), "%s: --", w->text);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text((int16_t)(x + 4), (int16_t)(y + (w->h - s_font->h) / 2), buf, &s, s_font);
}

static void st7789_draw_progress(const Render *r, int16_t x, int16_t y,
                                  const Widget *w, const Theme *t) {
    (void)r;
    uint16_t bg   = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t fill = w->colors ? w->colors->fg : t->border_focused;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, t->border_subtle, 1);
    if (w->value && w->max > w->min) {
        int range = w->max - w->min;
        int val   = *w->value < w->min ? w->min : *w->value > w->max ? w->max : *w->value;
        int16_t fw = (int16_t)((val - w->min) * (w->w - 2) / range);
        if (fw > 0) my_fill_rect((int16_t)(x+1), (int16_t)(y+1), fw, (int16_t)(w->h-2), fill);
    }
}

static void st7789_draw_edit(const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t, int focused) {
    (void)r;
    uint16_t fg  = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg  = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t brd = focused ? t->border_focused : t->border_normal;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, brd, 1);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s: %s", w->text, w->buf ? w->buf : "");
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text((int16_t)(x + 4), (int16_t)(y + (w->h - s_font->h) / 2), buf, &s, s_font);
}

static void st7789_draw_list_item(const Render *r,
                                   int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                   const ListItem *item, const Theme *t,
                                   int focused, int editing) {
    (void)r;
    uint16_t fg     = item->colors ? item->colors->fg : t->text_fg;
    uint16_t bg     = item->colors ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = focused ? t->widget_bg : bg;
    int16_t  cw     = (int16_t)s_font->w;
    int16_t  ty     = (int16_t)(y + ((int16_t)row_h - s_font->h) / 2);
    my_fill_rect(x, y, (int16_t)row_w, (int16_t)row_h, row_bg);
    DrawStyle s = { .fg = fg, .bg = row_bg };
    int16_t tx = (int16_t)(x + cw + (focused ? cw : 0));
    if (focused) {
        DrawStyle sc = { .fg = t->border_focused, .bg = row_bg };
        my_draw_text((int16_t)(x + cw), ty, ">", &sc, s_font);
    }
    my_draw_text(tx, ty, item->text, &s, s_font);
    (void)editing;
}

static void st7789_draw_list_separator(const Render *r,
                                        int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                        const Theme *t) {
    (void)r;
    int16_t mid = (int16_t)(y + (int16_t)row_h / 2);
    my_fill_rect(x, mid, (int16_t)row_w, 1, t->border_subtle);
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

void render_init(void) {
    s_font = &font_terminus16;  // choose a suitable size for your display

    static const Render r = {
        .begin_frame         = st7789_begin_frame,
        .flush               = my_flush,
        .draw_edit_overlay   = NULL,
        .screen_w            = 240,
        .screen_h            = 320,
        .draw_label          = st7789_draw_label,
        .draw_btn            = st7789_draw_btn,
        .draw_value          = st7789_draw_value,
        .draw_progress       = st7789_draw_progress,
        .draw_edit           = st7789_draw_edit,
        .draw_list_item      = st7789_draw_list_item,
        .draw_list_separator = st7789_draw_list_separator,
    };
    render_set(&r);
    render_set_theme(&theme_default);
}

void render_wait_key(void) { /* platform event loop */ }
void render_quit(void)     { /* release resources */ }
void render_set_font(const Font *f) { s_font = f; render_refresh(); }
