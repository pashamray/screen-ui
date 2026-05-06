#include "render.h"
#include "fonts.h"
#include "theme_default.h"

// Include your display driver here:
// #include "st7789.h"

typedef struct { uint16_t fg; uint16_t bg; } DrawStyle;

static const Font *s_font;

// ── primitives ───────────────────────────────────────────────────────────────

static void my_fill_rect(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // ST7789_SetWindow(ctx->ox+x, ctx->oy+y, ctx->ox+x+w-1, ctx->oy+y+h-1);
    // for (uint32_t i = 0; i < (uint32_t)w*h; i++) ST7789_WritePixel(color);
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void my_draw_text(const Ctx *ctx, int16_t x, int16_t y, const char *text, const DrawStyle *s,
                          const Font *f) {
    // Render each glyph from f->data using s->fg on s->bg background
    (void)ctx; (void)x; (void)y; (void)text; (void)s; (void)f;
}

static void my_draw_border(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t t) {
    my_fill_rect(ctx, x,         y,         w, (int16_t)t, color);
    my_fill_rect(ctx, x,         (int16_t)(y+h-(int16_t)t), w, (int16_t)t, color);
    my_fill_rect(ctx, x,         y, (int16_t)t, h, color);
    my_fill_rect(ctx, (int16_t)(x+w-(int16_t)t), y, (int16_t)t, h, color);
}

static void my_flush(void) {
    // DMA transfer or display commit if needed
}

// ── frame hooks ───────────────────────────────────────────────────────────────

static void st7789_begin_frame(const Ctx *ctx) {
    my_fill_rect(ctx, 0, 0, (int16_t)ctx->r->screen_w, (int16_t)ctx->r->screen_h, ctx->t->screen_bg);
}

// ── widget callbacks ──────────────────────────────────────────────────────────

static void st7789_draw_label(const Ctx *ctx, int16_t x, int16_t y, const Widget *w) {
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    DrawStyle s = { .fg = fg, .bg = ctx->t->screen_bg };
    my_draw_text(ctx, x, y, w->text, &s, s_font);
}

static void st7789_draw_btn(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused) {
    uint16_t fg  = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg  = (w->colors != NULL) ? w->colors->bg : ctx->t->screen_bg;
    uint16_t brd = (focused != 0) ? ctx->t->border_focused : ctx->t->border_subtle;
    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, brd, 1U);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(ctx, (int16_t)(x + 4), (int16_t)(y + ((w->h - (int16_t)s_font->h) / 2)), w->text, &s, s_font);
}

static void st7789_draw_value(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused, int editing) {
    uint16_t fg  = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg  = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t brd;
    if (editing != 0) {
        brd = ctx->t->border_editing;
    } else if (focused != 0) {
        brd = ctx->t->border_focused;
    } else {
        brd = ctx->t->border_subtle;
    }
    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, brd, 1U);
    char buf[48];
    if ((w->options != NULL) && (w->value != NULL)) {
        (void)snprintf(buf, sizeof(buf), "%s: %s", w->text, w->options[*w->value]);
    } else if (w->value != NULL) {
        (void)snprintf(buf, sizeof(buf), "%s: %d", w->text, *w->value);
    } else {
        (void)snprintf(buf, sizeof(buf), "%s: --", w->text);
    }
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(ctx, (int16_t)(x + 4), (int16_t)(y + ((w->h - (int16_t)s_font->h) / 2)), buf, &s, s_font);
}

static void st7789_draw_progress(const Ctx *ctx, int16_t x, int16_t y, const Widget *w) {
    uint16_t bg   = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t fill = (w->colors != NULL) ? w->colors->fg : ctx->t->border_focused;
    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, ctx->t->border_subtle, 1U);
    if ((w->value != NULL) && (w->max > w->min)) {
        int range = w->max - w->min;
        int val   = (*w->value < w->min) ? w->min : ((*w->value > w->max) ? w->max : *w->value);
        int16_t fw = (int16_t)(((val - w->min) * (w->w - 2)) / range);
        if (fw > 0) {
            my_fill_rect(ctx, (int16_t)(x+1), (int16_t)(y+1), fw, (int16_t)(w->h-2), fill);
        }
    }
}

static void st7789_draw_edit(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused) {
    uint16_t fg  = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg  = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t brd = (focused != 0) ? ctx->t->border_focused : ctx->t->border_normal;
    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, brd, 1U);
    char buf[48];
    (void)snprintf(buf, sizeof(buf), "%s: %s", w->text, (w->buf != NULL) ? w->buf : "");
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(ctx, (int16_t)(x + 4), (int16_t)(y + ((w->h - (int16_t)s_font->h) / 2)), buf, &s, s_font);
}

static void st7789_draw_list_item(const Ctx *ctx,
                                   int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                   const ListItem *item, int focused, int editing) {
    (void)editing;
    uint16_t fg     = (item->colors != NULL) ? item->colors->fg : ctx->t->text_fg;
    uint16_t bg     = (item->colors != NULL) ? item->colors->bg : ctx->t->screen_bg;
    uint16_t row_bg = (focused != 0) ? ctx->t->widget_bg : bg;
    int16_t  cw     = (int16_t)s_font->w;
    int16_t  ty     = (int16_t)(y + (((int16_t)row_h - (int16_t)s_font->h) / 2));
    my_fill_rect(ctx, x, y, (int16_t)row_w, (int16_t)row_h, row_bg);
    DrawStyle s = { .fg = fg, .bg = row_bg };
    int16_t tx = (int16_t)(x + cw + ((focused != 0) ? cw : 0));
    if (focused != 0) {
        DrawStyle sc = { .fg = ctx->t->border_focused, .bg = row_bg };
        my_draw_text(ctx, (int16_t)(x + cw), ty, ">", &sc, s_font);
    }
    my_draw_text(ctx, tx, ty, item->text, &s, s_font);
}

static void st7789_draw_list_separator(const Ctx *ctx,
                                        int16_t x, int16_t y, uint16_t row_w, uint16_t row_h) {
    int16_t mid = (int16_t)(y + ((int16_t)row_h / 2));
    my_fill_rect(ctx, x, mid, (int16_t)row_w, 1, ctx->t->border_subtle);
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
