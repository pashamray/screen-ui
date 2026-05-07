#include "view.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

/* ── font helpers ─────────────────────────────────────────────────────────── */

static int vd_char_idx(const Font *f, uint8_t code) {
    uint8_t result;
    if ((code < f->first) || (code >= (uint8_t)(f->first + f->count))) {
        result = ((f->first <= (uint8_t)' ') && ((uint8_t)' ' < (uint8_t)(f->first + f->count)))
                 ? (uint8_t)' ' : f->first;
    } else {
        result = code;
    }
    return ((int)result - (int)f->first);
}

static int16_t vd_text_width(const char *text, const Font *f) {
    if (f == NULL) { return 0; }
    int16_t w = 0;
    for (const char *p = text; *p != '\0'; p++) {
        int idx = vd_char_idx(f, (uint8_t)*p);
        w = (int16_t)(w + ((f->widths != NULL) ? (int16_t)f->widths[idx] : (int16_t)f->w));
    }
    return w;
}

/* ── widget draw callbacks ────────────────────────────────────────────────── */

static void vd_draw_label(const Ctx *ctx, int16_t x, int16_t y, const Widget *w)
{
    const Font *f = ctx->font;
    uint16_t fg   = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg   = (ctx->panel_bg != 0U) ? ctx->panel_bg : ctx->t->screen_bg;
    int16_t  tx   = (w->w > 0) ? x : (int16_t)(x - (vd_text_width(w->text, f) / 2));
    ctx->gfx->text(ctx, tx, y, w->text, f, fg, bg);
}

static void vd_draw_progress(const Ctx *ctx, int16_t x, int16_t y, const Widget *w)
{
    uint16_t bg   = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t fill = (w->colors != NULL) ? w->colors->fg : ctx->t->border_focused;

    ctx->gfx->fill(ctx, x, y, w->w, w->h, bg);
    ctx->gfx->border(ctx, x, y, w->w, w->h, ctx->t->border_subtle, 1U);

    if ((w->value != NULL) && (w->max > w->min)) {
        int range = w->max - w->min;
        int val;
        if (*w->value < w->min)       { val = w->min; }
        else if (*w->value > w->max)  { val = w->max; }
        else                          { val = *w->value; }

        if (w->h > w->w) {
            int16_t fh = (int16_t)(((val - w->min) * (w->h - 2)) / range);
            if (fh > 0) {
                ctx->gfx->fill(ctx, (int16_t)(x + 1), (int16_t)((y + w->h) - 1 - fh),
                               (int16_t)(w->w - 2), fh, fill);
            }
        } else {
            int16_t fw = (int16_t)(((val - w->min) * (w->w - 2)) / range);
            if (fw > 0) {
                ctx->gfx->fill(ctx, (int16_t)(x + 1), (int16_t)(y + 1),
                               fw, (int16_t)(w->h - 2), fill);
            }
        }
    }
}

static void vd_draw_edit(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused)
{
    const Font *f = ctx->font;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;
    uint16_t border_col = (focused != 0) ? ctx->t->border_focused : ctx->t->border_normal;
    ctx->gfx->fill(ctx, x, y, w->w, w->h, bg);
    ctx->gfx->border(ctx, x, y, w->w, w->h, border_col, 1U);
    {
        char disp[48];
        const char *val = ((w->buf != NULL) && (w->buf[0] != '\0')) ? w->buf : "...";
        (void)snprintf(disp, sizeof(disp), "%s: %s", w->text, val);
        int16_t tw = vd_text_width(disp, f);
        int16_t tx = (int16_t)(x + ((w->w - tw) / 2));
        int16_t ty = (f != NULL) ? (int16_t)(y + ((w->h - (int16_t)f->h) / 2)) : y;
        ctx->gfx->text(ctx, tx, ty, disp, f, fg, bg);
    }
}

static void vd_draw_btn(const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused)
{
    const Font *f = ctx->font;
    uint16_t fg  = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t bg  = (w->colors != NULL) ? w->colors->bg :
                   ((focused != 0) ? ctx->t->widget_bg : ctx->t->screen_bg);
    uint16_t brd = (focused != 0) ? ctx->t->border_focused : ctx->t->border_subtle;

    ctx->gfx->fill(ctx, x, y, w->w, w->h, bg);
    ctx->gfx->border(ctx, x, y, w->w, w->h, brd, 1U);
    {
        int16_t tw = vd_text_width(w->text, f);
        int16_t tx = (int16_t)(x + ((w->w - tw) / 2));
        int16_t ty = (f != NULL) ? (int16_t)(y + ((w->h - (int16_t)f->h) / 2)) : y;
        ctx->gfx->text(ctx, tx, ty, w->text, f, fg, bg);
    }
}

static void vd_draw_value(const Ctx *ctx, int16_t x, int16_t y, const Widget *w,
                           int focused, int editing)
{
    const Font *f = ctx->font;
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

    ctx->gfx->fill(ctx, x, y, w->w, w->h, bg);
    ctx->gfx->border(ctx, x, y, w->w, w->h, border_col, 1U);
    {
        char disp[48];
        if ((w->options != NULL) && (w->value != NULL)) {
            (void)snprintf(disp, sizeof(disp), "%s: %s", w->text, w->options[*w->value]);
        } else if (w->value != NULL) {
            (void)snprintf(disp, sizeof(disp), "%s: %d", w->text, *w->value);
        } else {
            (void)snprintf(disp, sizeof(disp), "%s: --", w->text);
        }

        int16_t tw = vd_text_width(disp, f);
        int16_t tx = (int16_t)(x + ((w->w - tw) / 2));
        int16_t ty = (f != NULL) ? (int16_t)(y + ((w->h - (int16_t)f->h) / 2)) : y;
        ctx->gfx->text(ctx, tx, ty, disp, f, fg, bg);

        if ((editing != 0) && (f != NULL)) {
            int16_t aw = vd_text_width("<", f);
            ctx->gfx->text(ctx, (int16_t)(x + (aw / 2)), ty, "<", f,
                           ctx->t->border_editing, bg);
            ctx->gfx->text(ctx, (int16_t)((x + w->w) - aw - (aw / 2)), ty, ">", f,
                           ctx->t->border_editing, bg);
        }
    }
}

static void vd_draw_list_item(const Ctx *ctx, int16_t x, int16_t y,
                               uint16_t row_w, uint16_t row_h,
                               const ListItem *item, int focused, int editing)
{
    const Font *f   = ctx->font;
    uint16_t fg     = (item->colors != NULL) ? item->colors->fg : ctx->t->text_fg;
    uint16_t bg     = (item->colors != NULL) ? item->colors->bg : ctx->t->screen_bg;
    uint16_t row_bg = (focused != 0) ? ctx->t->widget_bg : bg;
    int16_t  cw     = (f != NULL) ? (int16_t)f->w : 8;
    int16_t  ch     = (f != NULL) ? (int16_t)f->h : 8;
    int16_t  ty     = (int16_t)(y + (((int16_t)row_h - ch) / 2));
    int16_t  pad    = 8;

    ctx->gfx->fill(ctx, x, y, (int16_t)row_w, (int16_t)row_h, row_bg);

    if (item->type == LI_LABEL) {
        ctx->gfx->text(ctx, (int16_t)(x + pad), ty, item->text, f, ctx->t->hint_fg, row_bg);
        return;
    }

    int16_t tx = (int16_t)(x + pad);

    if (focused != 0) {
        ctx->gfx->fill(ctx, x, y, 3, (int16_t)row_h, ctx->t->border_focused);
    }

    if (item->type == LI_BTN) {
        ctx->gfx->text(ctx, tx, ty, item->text, f, fg, row_bg);
        return;
    }

    if (item->type == LI_SUBMENU) {
        ctx->gfx->text(ctx, tx, ty, item->text, f, fg, row_bg);
        ctx->gfx->text(ctx, (int16_t)(x + (int16_t)row_w - (2 * cw)), ty, ">", f, fg, row_bg);
        if (item->hint != NULL) {
            int16_t hw = vd_text_width(item->hint, f);
            int16_t hx = (int16_t)(x + (int16_t)row_w - hw - (3 * cw));
            if (hx > tx) {
                ctx->gfx->text(ctx, hx, ty, item->hint, f, ctx->t->hint_fg, row_bg);
            }
        }
        return;
    }

    if (item->type == LI_CHECK) {
        ctx->gfx->text(ctx, tx, ty, item->text, f, fg, row_bg);
        {
            const char *chk = ((item->value != NULL) && (*item->value != 0)) ? "[x]" : "[ ]";
            int16_t cx = (int16_t)(x + (int16_t)row_w - (4 * cw));
            ctx->gfx->text(ctx, cx, ty, chk, f, fg, row_bg);
        }
        return;
    }

    /* LI_VALUE */
    ctx->gfx->text(ctx, tx, ty, item->text, f, fg, row_bg);
    {
        char val_str[32];
        if ((item->options != NULL) && (item->value != NULL)) {
            int idx = *item->value;
            if (idx < 0) { idx = 0; }
            if (idx >= (int)item->options_count) { idx = (int)item->options_count - 1; }
            (void)snprintf(val_str, sizeof(val_str), "%s", item->options[idx]);
        } else if (item->value != NULL) {
            (void)snprintf(val_str, sizeof(val_str), "%d", *item->value);
        } else {
            (void)snprintf(val_str, sizeof(val_str), "--");
        }

        int16_t vlen = (int16_t)strlen(val_str);
        int16_t vx   = (int16_t)(x + (int16_t)row_w - ((vlen + 2) * cw));
        if (vx < (tx + cw)) { vx = (int16_t)(tx + cw); }
        ctx->gfx->text(ctx, vx, ty, val_str, f, fg, row_bg);

        if (editing != 0) {
            ctx->gfx->text(ctx, (int16_t)(vx - cw), ty, "<", f, ctx->t->border_editing, row_bg);
            ctx->gfx->text(ctx, (int16_t)(x + (int16_t)row_w - (2 * cw)), ty, ">", f,
                           ctx->t->border_editing, row_bg);
        }
    }
}

static void vd_draw_list_separator(const Ctx *ctx, int16_t x, int16_t y,
                                    uint16_t row_w, uint16_t row_h)
{
    int16_t mid = (int16_t)(y + ((int16_t)row_h / 2));
    ctx->gfx->fill(ctx, x, mid, (int16_t)row_w, 1, ctx->t->border_subtle);
}

static void vd_draw_edit_overlay(const Ctx *ctx, const Widget *w, int cursor)
{
    const Font *f   = ctx->font;
    char       *buf = w->buf;
    int         maxlen = (int)w->buf_len - 1;
    int         slen   = (int)strlen(buf);
    int16_t     fh     = (f != NULL) ? (int16_t)f->h : 8;
    uint16_t wfg = (w->colors != NULL) ? w->colors->fg : ctx->t->text_fg;
    uint16_t wbg = (w->colors != NULL) ? w->colors->bg : ctx->t->widget_bg;

    /* clear screen */
    ctx->gfx->fill(ctx, 0, 0, (int16_t)ctx->gfx->screen_w, (int16_t)ctx->gfx->screen_h,
                   ctx->t->screen_bg);
    /* title */
    {
        int16_t tx = (int16_t)((int16_t)(ctx->gfx->screen_w / 2U) - (vd_text_width(w->text, f) / 2));
        ctx->gfx->text(ctx, tx, 16, w->text, f, ctx->t->text_fg, ctx->t->screen_bg);
    }
    /* input box */
    {
        int16_t fw = (int16_t)((maxlen * (int)((f != NULL) ? f->w : 8U)) + 8);
        int16_t fx = (int16_t)((int16_t)(ctx->gfx->screen_w / 2U) - (fw / 2));
        int16_t fy = (int16_t)((int16_t)(ctx->gfx->screen_h / 2U) - (fh / 2) - 4);
        ctx->gfx->fill(ctx, fx, fy, fw, (int16_t)(fh + 8), wbg);
        ctx->gfx->border(ctx, fx, fy, fw, (int16_t)(fh + 8), ctx->t->border_focused, 1U);

        int16_t ecx = (int16_t)(fx + 4);
        int16_t ecy = (int16_t)(fy + 4);
        int16_t cursor_x = ecx;

        if (cursor > 0) {
            char part[64];
            if (cursor < 64) {
                (void)memcpy(part, buf, (size_t)cursor);
                part[cursor] = '\0';
                cursor_x = (int16_t)(ecx + vd_text_width(part, f));
                ctx->gfx->text(ctx, ecx, ecy, part, f, wfg, wbg);
            }
        }
        {
            char cur_ch[2];
            cur_ch[0] = (cursor < slen) ? buf[cursor] : ' ';
            cur_ch[1] = '\0';
            int16_t after_x = (int16_t)(cursor_x + vd_text_width(cur_ch, f));
            ctx->gfx->text(ctx, cursor_x, ecy, cur_ch, f, ctx->t->cursor_fg, ctx->t->cursor_bg);
            if (cursor < slen) {
                ctx->gfx->text(ctx, after_x, ecy, &buf[cursor + 1], f, wfg, wbg);
            }
        }
    }
    /* hint */
    {
        const char *hint = "Enter=confirm  Esc=cancel";
        int16_t hx = (int16_t)((int16_t)(ctx->gfx->screen_w / 2U) - (vd_text_width(hint, f) / 2));
        ctx->gfx->text(ctx, hx, (int16_t)((int16_t)ctx->gfx->screen_h - 20), hint, f,
                       ctx->t->hint_fg, ctx->t->screen_bg);
    }
}

const View view_default = {
    .draw_label          = vd_draw_label,
    .draw_btn            = vd_draw_btn,
    .draw_value          = vd_draw_value,
    .draw_progress       = vd_draw_progress,
    .draw_edit           = vd_draw_edit,
    .draw_list_item      = vd_draw_list_item,
    .draw_list_separator = vd_draw_list_separator,
    .draw_edit_overlay   = vd_draw_edit_overlay,
};
