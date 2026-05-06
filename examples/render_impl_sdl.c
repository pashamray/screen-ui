#include "render.h"
#include "fonts.h"
#include "layouts.h"
#include "theme_default.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* cppcheck-suppress misra-c2012-5.6 */
/* cppcheck-suppress misra-c2012-5.7 */
typedef struct { uint16_t fg; uint16_t bg; } DrawStyle;

static SDL_Window   *window;
static SDL_Renderer *sdl;
/* cppcheck-suppress misra-c2012-8.9 */
static int           scale;
static const Font   *s_font = &font_terminus20;

/* ── RGB565 → RGB888 ──────────────────────────────────────────────────────── */
static void rgb565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (uint8_t)((c >> 11U) << 3U);
    *g = (uint8_t)(((c >> 5U) & 0x3FU) << 2U);
    *b = (uint8_t)((c & 0x1FU) << 3U);
}

/* ── primitives (ctx-relative coords; clipped to ctx region) ─────────────── */
/* cppcheck-suppress misra-c2012-5.9 */
static void my_fill_rect(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    int ax = (int)ctx->ox + (int)x;
    int ay = (int)ctx->oy + (int)y;
    int aw = (int)w;
    int ah = (int)h;

    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;

    if ((ax >= cx2) || (ay >= cy2) || ((ax + aw) <= cx1) || ((ay + ah) <= cy1)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (ax < cx1) { aw -= (cx1 - ax); ax = cx1; }
    if (ay < cy1) { ah -= (cy1 - ay); ay = cy1; }
    if ((ax + aw) > cx2) { aw = cx2 - ax; }
    if ((ay + ah) > cy2) { ah = cy2 - ay; }

    {
        uint8_t cr;
        uint8_t cg;
        uint8_t cb;
        rgb565(color, &cr, &cg, &cb);
        /* cppcheck-suppress misra-c2012-17.3 */
        SDL_SetRenderDrawColor(sdl, cr, cg, cb, 255U);
        {
            SDL_Rect rect = { ax, ay, aw, ah };
            /* cppcheck-suppress misra-c2012-17.3 */
            (void)SDL_RenderFillRect(sdl, &rect);
        }
    }
}

static int char_idx(const Font *f, uint8_t code) {
    uint8_t result;
    if ((code < f->first) || (code >= (uint8_t)(f->first + f->count))) {
        result = ((f->first <= (uint8_t)' ') && ((uint8_t)' ' < (uint8_t)(f->first + f->count))) ? (uint8_t)' ' : f->first;
    } else {
        result = code;
    }
    return ((int)result - (int)f->first);
}

static int16_t text_width(const char *text, const Font *f) {
    int16_t w = 0;
    for (const char *p = text; *p != '\0'; p++) {
        int idx = char_idx(f, (uint8_t)*p);
        w = (int16_t)(w + ((f->widths != NULL) ? (int16_t)f->widths[idx] : (int16_t)f->w));
    }
    return w;
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_draw_text(const Ctx *ctx, int16_t x, int16_t y, const char *text, const DrawStyle *s,
                         const Font *f) {
    int ax  = (int)ctx->ox + (int)x;
    int ay  = (int)ctx->oy + (int)y;
    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;

    uint8_t fr;
    uint8_t fg;
    uint8_t fb;
    uint8_t br;
    uint8_t bg_;
    uint8_t bb;
    rgb565(s->fg, &fr, &fg, &fb);
    rgb565(s->bg, &br, &bg_, &bb);

    /* background fill — clipped */
    {
        int bx = ax;
        int by = ay;
        int bw = (int)text_width(text, f);
        int bh = (int)f->h;
        if ((bx < cx2) && (by < cy2) && ((bx + bw) > cx1) && ((by + bh) > cy1)) {
            int cbx = bx;
            int cby = by;
            int cbw = bw;
            int cbh = bh;
            if (cbx < cx1) { cbw -= (cx1 - cbx); cbx = cx1; }
            if (cby < cy1) { cbh -= (cy1 - cby); cby = cy1; }
            if ((cbx + cbw) > cx2) { cbw = cx2 - cbx; }
            if ((cby + cbh) > cy2) { cbh = cy2 - cby; }
            /* cppcheck-suppress misra-c2012-17.3 */
            SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255U);
            {
                SDL_Rect bg_r = { cbx, cby, cbw, cbh };
                /* cppcheck-suppress misra-c2012-17.3 */
                (void)SDL_RenderFillRect(sdl, &bg_r);
            }
        }
    }

    /* glyphs — each pixel clipped individually */
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_SetRenderDrawColor(sdl, fr, fg, fb, 255U);
    {
        int16_t cx = (int16_t)ax;
        for (const char *p = text; *p != '\0'; p++) {
            int idx = char_idx(f, (uint8_t)*p);
            const uint8_t *glyph = &f->data[idx * (int)f->h * (int)f->stride];
            int advance = (f->widths != NULL) ? (int)f->widths[idx] : (int)f->w;
            for (int row = 0; row < (int)f->h; row++) {
                for (int bi = 0; bi < (int)f->stride; bi++) {
                    uint8_t bits = glyph[(row * (int)f->stride) + bi];
                    if (bits == 0U) {
                        continue;
                    }
                    for (uint8_t col = 0U; col < 8U; col++) {
                        if (((bits >> col) & 1U) != 0U) {
                            int px = (int)cx + (bi * 8) + (int)col;
                            int py = ay + row;
                            if ((px >= cx1) && (px < cx2) && (py >= cy1) && (py < cy2)) {
                                SDL_Rect pxr = { px, py, 1, 1 };
                                /* cppcheck-suppress misra-c2012-17.3 */
                                (void)SDL_RenderFillRect(sdl, &pxr);
                            }
                        }
                    }
                }
            }
            cx = (int16_t)(cx + (int16_t)advance);
        }
    }
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_draw_border(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t t) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int ax  = (int)ctx->ox + (int)x;
    int ay  = (int)ctx->oy + (int)y;
    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;
    rgb565(color, &r, &g, &b);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_SetRenderDrawColor(sdl, r, g, b, 255U);
    {
        int it = (int)t;
        int iw = (int)w;
        int ih = (int)h;

        /* Helper macro to clamp and draw a border rect */
        /* top */
        {
            int bx = ax; int by = ay; int bw = iw; int bh = it;
            if ((bx < cx2) && (by < cy2) && ((bx+bw) > cx1) && ((by+bh) > cy1)) {
                if (bx < cx1) { bw -= (cx1-bx); bx = cx1; }
                if (by < cy1) { bh -= (cy1-by); by = cy1; }
                if ((bx+bw) > cx2) { bw = cx2-bx; }
                if ((by+bh) > cy2) { bh = cy2-by; }
                SDL_Rect rr = { bx, by, bw, bh };
                /* cppcheck-suppress misra-c2012-17.3 */
                (void)SDL_RenderFillRect(sdl, &rr);
            }
        }
        /* bottom */
        {
            int bx = ax; int by = (ay + ih) - it; int bw = iw; int bh = it;
            if ((bx < cx2) && (by < cy2) && ((bx+bw) > cx1) && ((by+bh) > cy1)) {
                if (bx < cx1) { bw -= (cx1-bx); bx = cx1; }
                if (by < cy1) { bh -= (cy1-by); by = cy1; }
                if ((bx+bw) > cx2) { bw = cx2-bx; }
                if ((by+bh) > cy2) { bh = cy2-by; }
                SDL_Rect rr = { bx, by, bw, bh };
                /* cppcheck-suppress misra-c2012-17.3 */
                (void)SDL_RenderFillRect(sdl, &rr);
            }
        }
        /* left */
        {
            int bx = ax; int by = ay; int bw = it; int bh = ih;
            if ((bx < cx2) && (by < cy2) && ((bx+bw) > cx1) && ((by+bh) > cy1)) {
                if (bx < cx1) { bw -= (cx1-bx); bx = cx1; }
                if (by < cy1) { bh -= (cy1-by); by = cy1; }
                if ((bx+bw) > cx2) { bw = cx2-bx; }
                if ((by+bh) > cy2) { bh = cy2-by; }
                SDL_Rect rr = { bx, by, bw, bh };
                /* cppcheck-suppress misra-c2012-17.3 */
                (void)SDL_RenderFillRect(sdl, &rr);
            }
        }
        /* right */
        {
            int bx = (ax + iw) - it; int by = ay; int bw = it; int bh = ih;
            if ((bx < cx2) && (by < cy2) && ((bx+bw) > cx1) && ((by+bh) > cy1)) {
                if (bx < cx1) { bw -= (cx1-bx); bx = cx1; }
                if (by < cy1) { bh -= (cy1-by); by = cy1; }
                if ((bx+bw) > cx2) { bw = cx2-bx; }
                if ((by+bh) > cy2) { bh = cy2-by; }
                SDL_Rect rr = { bx, by, bw, bh };
                /* cppcheck-suppress misra-c2012-17.3 */
                (void)SDL_RenderFillRect(sdl, &rr);
            }
        }
    }
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_flush(void) {
    static uint32_t frames = 0U;
    char title[64];
    frames++;
    /* cppcheck-suppress misra-c2012-21.6 */
    /* cppcheck-suppress misra-c2012-17.7 */
    (void)snprintf(title, sizeof(title), "screen-ui  [redraws: %u]", frames);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_SetWindowTitle(window, title);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_RenderPresent(sdl);
}


/* ── frame-level hooks ──────────────────────────────────────────────────── */

static void menu_begin_frame(const Ctx *ctx) {
    uint8_t br;
    uint8_t bg_;
    uint8_t bb;
    rgb565(ctx->t->screen_bg, &br, &bg_, &bb);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255U);
    /* cppcheck-suppress misra-c2012-17.3 */
    (void)SDL_RenderClear(sdl);
}

static void sdl_draw_edit_overlay(const Ctx *ctx, const Widget *w, int cursor) {
    const Font *f      = s_font;
    char       *buf    = w->buf;
    int         maxlen = (int)w->buf_len - 1;
    int         slen   = (int)strlen(buf);
    int         fh     = (int)f->h;

    /* cppcheck-suppress misra-c2012-10.8 */
    uint16_t wfg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    /* cppcheck-suppress misra-c2012-10.8 */
    uint16_t wbg = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;

    my_fill_rect(ctx, 0, 0, (int16_t)ctx->r->screen_w, (int16_t)ctx->r->screen_h, ctx->t->screen_bg);

    {
        DrawStyle s_title = { .fg = ctx->t->text_fg, .bg = ctx->t->screen_bg };
        /* cppcheck-suppress misra-c2012-10.8 */
        int16_t tx = (int16_t)((int16_t)(ctx->r->screen_w / 2U) - (text_width(w->text, f) / 2));
        my_draw_text(ctx, tx, 16, w->text, &s_title, f);
    }

    {
        int16_t fw = (int16_t)((maxlen * (int)f->w) + 8);
        /* cppcheck-suppress misra-c2012-10.8 */
        int16_t fx = (int16_t)((int16_t)(ctx->r->screen_w  / 2U) - (fw / 2));
        /* cppcheck-suppress misra-c2012-10.8 */
        int16_t fy = (int16_t)((int16_t)(ctx->r->screen_h  / 2U) - ((int16_t)fh / 2) - 4);
        my_fill_rect(ctx, fx, fy, fw, (int16_t)(fh + 8), wbg);
        my_draw_border(ctx, fx, fy, fw, (int16_t)(fh + 8), ctx->t->border_focused, 1U);

        int16_t ecx = (int16_t)(fx + 4);
        int16_t ecy = (int16_t)(fy + 4);

        DrawStyle s_norm = { .fg = wfg,                .bg = wbg               };
        DrawStyle s_cur  = { .fg = ctx->t->cursor_fg,  .bg = ctx->t->cursor_bg };

        int16_t cursor_x = ecx;
        if (cursor > 0) {
            char part[64];
            if (cursor < 64) {
                (void)memcpy(part, buf, (size_t)cursor);
                part[cursor] = '\0';
                cursor_x = (int16_t)(ecx + text_width(part, f));
                my_draw_text(ctx, ecx, ecy, part, &s_norm, f);
            }
        }
        {
            char cur_ch[2];
            cur_ch[0] = (cursor < slen) ? buf[cursor] : ' ';
            cur_ch[1] = '\0';
            int16_t after_x = (int16_t)(cursor_x + text_width(cur_ch, f));
            my_draw_text(ctx, cursor_x, ecy, cur_ch, &s_cur, f);
            if (cursor < slen) {
                my_draw_text(ctx, after_x, ecy, &buf[cursor + 1], &s_norm, f);
            }
        }
    }

    {
        DrawStyle s_hint = { .fg = ctx->t->hint_fg, .bg = ctx->t->screen_bg };
        const char *hint = "Enter=confirm  Esc=cancel";
        /* cppcheck-suppress misra-c2012-10.8 */
        int16_t hx = (int16_t)((int16_t)(ctx->r->screen_w / 2U) - (text_width(hint, f) / 2));
        my_draw_text(ctx, hx, (int16_t)((int16_t)ctx->r->screen_h - 20), hint, &s_hint, f);
    }
}

/* ── widget callbacks ────────────────────────────────────────────────────── */

static void menu_draw_label(const Ctx *ctx, int16_t x, int16_t y, const Widget *w)
{
    const Font *f  = s_font;
    uint16_t fg    = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg    = (ctx->panel_bg != 0U) ? ctx->panel_bg : ctx->t->screen_bg;
    int16_t tx     = (w->w > 0) ? x : (int16_t)(x - (text_width(w->text, f) / 2));
    DrawStyle s    = { .fg = fg, .bg = bg };
    my_draw_text(ctx, tx, y, w->text, &s, f);
}

static void menu_draw_progress(const Ctx *ctx, int16_t x, int16_t y, const Widget *w)
{
    uint16_t bg   = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t fill = (w->colors != NULL) ? w->colors->fg : ctx->t->border_focused;

    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, ctx->t->border_subtle, 1U);

    if ((w->value != NULL) && (w->max > w->min)) {
        int range = w->max - w->min;
        int val;
        if (*w->value < w->min) {
            val = w->min;
        } else if (*w->value > w->max) {
            val = w->max;
        } else {
            val = *w->value;
        }
        if (w->h > w->w) {
            int16_t fh = (int16_t)(((val - w->min) * (w->h - 2)) / range);
            if (fh > 0) {
                my_fill_rect(ctx, (int16_t)(x + 1), (int16_t)((y + w->h) - 1 - fh),
                             (int16_t)(w->w - 2), fh, fill);
            }
        } else {
            int16_t fw = (int16_t)(((val - w->min) * (w->w - 2)) / range);
            if (fw > 0) {
                my_fill_rect(ctx, (int16_t)(x + 1), (int16_t)(y + 1),
                             fw, (int16_t)(w->h - 2), fill);
            }
        }
    }
}

static void menu_draw_edit(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused)
{
    const Font *f = s_font;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t border_col = (focused != 0) ? ctx->t->border_focused : ctx->t->border_normal;
    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, border_col, 1U);
    {
        char disp[48];
        const char *val = ((w->buf != NULL) && (w->buf[0] != '\0')) ? w->buf : "...";
        /* cppcheck-suppress misra-c2012-21.6 */
        /* cppcheck-suppress misra-c2012-17.7 */
        (void)snprintf(disp, sizeof(disp), "%s: %s", w->text, val);
        int16_t tx = (int16_t)(x + ((w->w - text_width(disp, f)) / 2));
        int16_t ty = (int16_t)(y + ((w->h - (int16_t)f->h) / 2));
        DrawStyle s = { .fg = fg, .bg = bg };
        my_draw_text(ctx, tx, ty, disp, &s, f);
    }
}

static void menu_draw_btn(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused)
{
    const Font *f = s_font;
    uint16_t fg  = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg  = (w->colors != NULL) ? w->colors->bg : ctx->t->screen_bg;
    uint16_t brd = (focused != 0) ? ctx->t->border_focused : ctx->t->border_subtle;

    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, brd, 1U);

    {
        int16_t tx = (int16_t)(x + ((w->w - text_width(w->text, f)) / 2));
        int16_t ty = (int16_t)(y + ((w->h - (int16_t)f->h) / 2));

        DrawStyle s = { .fg = fg, .bg = bg };
        my_draw_text(ctx, tx, ty, w->text, &s, f);
    }
}

static void menu_draw_value(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused, int editing)
{
    const Font *f = s_font;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t border_col;
    if (editing != 0) {
        border_col = ctx->t->border_editing;
    } else if (focused != 0) {
        border_col = ctx->t->border_focused;
    } else {
        border_col = ctx->t->border_subtle;
    }

    my_fill_rect(ctx, x, y, w->w, w->h, bg);
    my_draw_border(ctx, x, y, w->w, w->h, border_col, 1U);

    {
        char disp[48];
        if ((w->options != NULL) && (w->value != NULL)) {
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(disp, sizeof(disp), "%s: %s", w->text, w->options[*w->value]);
        } else if (w->value != NULL) {
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(disp, sizeof(disp), "%s: %d", w->text, *w->value);
        } else {
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(disp, sizeof(disp), "%s: --", w->text);
        }

        int16_t tx = (int16_t)(x + ((w->w - text_width(disp, f)) / 2));
        int16_t ty = (int16_t)(y + ((w->h - (int16_t)f->h) / 2));
        DrawStyle s = { .fg = fg, .bg = bg };
        my_draw_text(ctx, tx, ty, disp, &s, f);

        if (editing != 0) {
            int16_t aw = text_width("<", f);
            DrawStyle sa = { .fg = ctx->t->border_editing, .bg = bg };
            my_draw_text(ctx, (int16_t)(x + (aw / 2)),                              ty, "<", &sa, f);
            my_draw_text(ctx, (int16_t)((x + w->w) - aw - (aw / 2)), ty, ">", &sa, f);
        }
    }
}

static void sdl_draw_list_item(const Ctx *ctx,
                                int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                const ListItem *item, int focused, int editing)
{
    uint16_t fg     = (item->colors != NULL) ? item->colors->fg : ctx->t->text_fg;
    uint16_t bg     = (item->colors != NULL) ? item->colors->bg : ctx->t->screen_bg;
    uint16_t row_bg = (focused != 0) ? ctx->t->widget_bg : bg;
    int16_t  cw     = (int16_t)s_font->w;
    int16_t  ch     = (int16_t)s_font->h;
    int16_t  ty     = (int16_t)(y + (((int16_t)row_h - ch) / 2));
    int16_t  pad    = 8;

    my_fill_rect(ctx, x, y, (int16_t)row_w, (int16_t)row_h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = ctx->t->hint_fg, .bg = row_bg };
        my_draw_text(ctx, (int16_t)(x + pad), ty, item->text, &s, s_font);
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    int16_t  tx = (int16_t)(x + pad + ((focused != 0) ? cw : 0));
    DrawStyle s  = { .fg = fg, .bg = row_bg };

    if (focused != 0) {
        DrawStyle sc = { .fg = ctx->t->border_focused, .bg = row_bg };
        my_draw_text(ctx, (int16_t)(x + pad), ty, ">", &sc, s_font);
    }

    if (item->type == LI_BTN) {
        my_draw_text(ctx, tx, ty, item->text, &s, s_font);
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    if (item->type == LI_SUBMENU) {
        my_draw_text(ctx, tx, ty, item->text, &s, s_font);
        my_draw_text(ctx, (int16_t)(x + (int16_t)row_w - (2 * cw)), ty, ">", &s, s_font);
        if (item->hint != NULL) {
            DrawStyle sh = { .fg = ctx->t->hint_fg, .bg = row_bg };
            int16_t hw = text_width(item->hint, s_font);
            int16_t hx = (int16_t)(x + (int16_t)row_w - hw - (3 * cw));
            if (hx > tx) {
                my_draw_text(ctx, hx, ty, item->hint, &sh, s_font);
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    if (item->type == LI_CHECK) {
        my_draw_text(ctx, tx, ty, item->text, &s, s_font);
        {
            const char *chk = ((item->value != NULL) && (*item->value != 0)) ? "[x]" : "[ ]";
            int16_t cx = (int16_t)(x + (int16_t)row_w - (4 * cw));
            my_draw_text(ctx, cx, ty, chk, &s, s_font);
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    /* LI_VALUE */
    my_draw_text(ctx, tx, ty, item->text, &s, s_font);

    {
        char val_str[32];
        if ((item->options != NULL) && (item->value != NULL)) {
            int idx = *item->value;
            if (idx < 0) {
                idx = 0;
            }
            if (idx >= (int)item->options_count) {
                idx = (int)item->options_count - 1;
            }
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(val_str, sizeof(val_str), "%s", item->options[idx]);
        } else if (item->value != NULL) {
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(val_str, sizeof(val_str), "%d", *item->value);
        } else {
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            (void)snprintf(val_str, sizeof(val_str), "--");
        }

        int16_t vlen = (int16_t)strlen(val_str);
        int16_t vx   = (int16_t)(x + (int16_t)row_w - ((vlen + 2) * cw));
        if (vx < (tx + cw)) {
            vx = (int16_t)(tx + cw);
        }
        my_draw_text(ctx, vx, ty, val_str, &s, s_font);

        if (editing != 0) {
            DrawStyle sa = { .fg = ctx->t->border_editing, .bg = row_bg };
            my_draw_text(ctx, (int16_t)(vx - cw), ty, "<", &sa, s_font);
            my_draw_text(ctx, (int16_t)(x + (int16_t)row_w - (2 * cw)), ty, ">", &sa, s_font);
        }
    }
}

static void sdl_draw_list_separator(const Ctx *ctx,
                                     int16_t x, int16_t y, uint16_t row_w, uint16_t row_h)
{
    int16_t mid = (int16_t)(y + ((int16_t)row_h / 2));
    my_fill_rect(ctx, x, mid, (int16_t)row_w, 1, ctx->t->border_subtle);
}

static void sdl_draw_fill(const Ctx *ctx, uint16_t color) {
    my_fill_rect(ctx, 0, 0, ctx->cw, ctx->ch, color);
}

/* ── lifecycle ────────────────────────────────────────────────────────────── */
/* cppcheck-suppress misra-c2012-8.4 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_impl_sdl_init(int screen_w, int screen_h, int sc) {
    scale = sc;
    /* cppcheck-suppress misra-c2012-17.3 */
    (void)SDL_Init(SDL_INIT_VIDEO);
    /* cppcheck-suppress misra-c2012-17.3 */
    window = SDL_CreateWindow("screen-ui",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        screen_w * scale, screen_h * scale, 0);
    /* cppcheck-suppress misra-c2012-17.3 */
    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    /* cppcheck-suppress misra-c2012-17.3 */
    sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    /* cppcheck-suppress misra-c2012-17.3 */
    (void)SDL_RenderSetLogicalSize(sdl, screen_w, screen_h);

    {
        static Render r;
        r = (Render){
            .begin_frame      = menu_begin_frame,
            .flush            = my_flush,
            .draw_edit_overlay = sdl_draw_edit_overlay,
            .screen_w         = (uint16_t)screen_w,
            .screen_h         = (uint16_t)screen_h,
            .draw_label       = menu_draw_label,
            .draw_btn         = menu_draw_btn,
            .draw_value       = menu_draw_value,
            .draw_progress    = menu_draw_progress,
            .draw_edit        = menu_draw_edit,
            .draw_list_item      = sdl_draw_list_item,
            .draw_list_separator = sdl_draw_list_separator,
            .draw_fill           = sdl_draw_fill,
        };
        render_set(&r);
        render_set_theme(&theme_default);
    }
}

/* cppcheck-suppress misra-c2012-8.4 */
/* cppcheck-suppress misra-c2012-8.7 */
void sdl_wait_key(void) {
    SDL_Event e;
    int was_str_edit = 0;
    int keep_running = 1;

    /* cppcheck-suppress misra-c2012-15.4 */
    while (keep_running != 0) {
        /* ── tick: advance animations and redraw every frame ────────── */
        /* cppcheck-suppress misra-c2012-17.3 */
        if (SDL_WaitEventTimeout(&e, 16) == 0) {
            layouts_tick();
            render_refresh();   /* draws if dirty (set by engine events or layouts_tick) */
            continue;
        }
        /* cppcheck-suppress misra-c2012-17.3 */
        if (e.type == (uint32_t)SDL_QUIT) {
            keep_running = 0;
            continue;
        }

        {
            int str_edit = render_in_edit_mode();
            if ((str_edit != 0) && (was_str_edit == 0)) {
                /* cppcheck-suppress misra-c2012-17.3 */
                SDL_StartTextInput();
            }
            if ((str_edit == 0) && (was_str_edit != 0)) {
                /* cppcheck-suppress misra-c2012-17.3 */
                SDL_StopTextInput();
            }
            was_str_edit = str_edit;

            /* ── input: only change state (engine sets dirty internally) ─── */
            if (str_edit != 0) {
                /* cppcheck-suppress misra-c2012-17.3 */
                if (e.type == (uint32_t)SDL_TEXTINPUT) {
                    render_edit_char(e.text.text[0]);
                /* cppcheck-suppress misra-c2012-17.3 */
                } else if (e.type == (uint32_t)SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                        case SDLK_BACKSPACE: render_edit_delete(); break;
                        case SDLK_LEFT:      render_prev();        break;
                        case SDLK_RIGHT:     render_next();        break;
                        case SDLK_RETURN:    render_activate();    break;
                        case SDLK_ESCAPE:    render_cancel();      break;
                        default: break;
                    }
                } else {
                    /* no action */
                }
            } else {
                /* cppcheck-suppress misra-c2012-17.3 */
                if (e.type != (uint32_t)SDL_KEYDOWN) {
                    continue;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_DOWN:   render_next();    break;
                    case SDLK_UP:     render_prev();    break;
                    case SDLK_LEFT:   render_dec();     break;
                    case SDLK_RIGHT:  render_inc();     break;
                    case SDLK_RETURN:
                    case SDLK_SPACE:  render_activate(); break;
                    case SDLK_ESCAPE: render_cancel();   break;
                    default: break;
                    /* cppcheck-suppress misra-c2012-15.7 */
                }
            }
        }
        /* dirty flag will be drawn on the next tick */
    }
}

/* cppcheck-suppress misra-c2012-8.4 */
/* cppcheck-suppress misra-c2012-8.7 */
void sdl_quit(void) {
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_DestroyRenderer(sdl);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_DestroyWindow(window);
    /* cppcheck-suppress misra-c2012-17.3 */
    SDL_Quit();
}

/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_init(void)     { render_impl_sdl_init(240, 320, 2); }
/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_wait_key(void) { sdl_wait_key(); }
/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_quit(void)     { sdl_quit(); }

/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_set_font(const Font *f) { s_font = f; render_mark_dirty(); }
