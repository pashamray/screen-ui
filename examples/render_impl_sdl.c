#include "render.h"
#include "fonts.h"
#include "layouts.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window   *window;
static SDL_Renderer *sdl;
static int           scale;

// ── RGB565 → RGB888 ─────────────────────────────────────────────────────────
static void rgb565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (c >> 11) << 3;
    *g = ((c >> 5) & 0x3F) << 2;
    *b = (c & 0x1F) << 3;
}

// ── primitives (logical coordinates; SDL_RenderSetLogicalSize handles scaling) ──
static void my_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    uint8_t r, g, b;
    rgb565(color, &r, &g, &b);
    SDL_SetRenderDrawColor(sdl, r, g, b, 255);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(sdl, &rect);
}

static void my_draw_text(int16_t x, int16_t y, const char *text, const DrawStyle *s) {
    const Font *f = &font_8x8;
    uint8_t fr, fg, fb, br, bg_, bb;
    rgb565(s->fg, &fr, &fg, &fb);
    rgb565(s->bg, &br, &bg_, &bb);
    int len = (int)strlen(text);

    SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255);
    for (int ci = 0; ci < len; ci++) {
        SDL_Rect r = { x + ci * f->w, y, f->w, f->h };
        SDL_RenderFillRect(sdl, &r);
    }
    SDL_SetRenderDrawColor(sdl, fr, fg, fb, 255);
    for (int ci = 0; ci < len; ci++) {
        uint8_t code = (uint8_t)text[ci];
        if (code >= 128) code = ' ';
        const uint8_t *glyph = f->data + (int)code * f->h * f->stride;
        for (int row = 0; row < f->h; row++) {
            for (int bi = 0; bi < f->stride; bi++) {
                uint8_t bits = glyph[row * f->stride + bi];
                if (!bits) continue;
                for (int col = 0; col < 8; col++) {
                    if ((bits >> col) & 1) {
                        SDL_Rect px = { x + ci * f->w + bi * 8 + col, y + row, 1, 1 };
                        SDL_RenderFillRect(sdl, &px);
                    }
                }
            }
        }
    }
}

static void my_draw_border(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t t) {
    uint8_t r, g, b;
    rgb565(color, &r, &g, &b);
    SDL_SetRenderDrawColor(sdl, r, g, b, 255);
    SDL_Rect rects[4] = {
        { x,     y,     w, t },
        { x,     y+h-t, w, t },
        { x,     y,     t, h },
        { x+w-t, y,     t, h },
    };
    SDL_RenderFillRects(sdl, rects, 4);
}

static void my_flush(void) {
    SDL_RenderPresent(sdl);
}

static const Theme theme = {
    .screen_bg      = 0x0000,
    .text_fg        = 0xFFFF,
    .widget_bg      = 0x2104,
    .border_normal  = 0xFFFF,
    .border_focused = 0x07FF,
    .border_editing = 0xFFE0,
    .border_subtle  = 0x4208,
    .cursor_fg      = 0x0000,
    .cursor_bg      = 0x07FF,
    .hint_fg        = 0x8410,
};

// ── frame-level hooks ────────────────────────────────────────────────────────

static void menu_begin_frame(const Render *r, const Theme *t) {
    (void)r;
    uint8_t br, bg_, bb;
    rgb565(t->screen_bg, &br, &bg_, &bb);
    SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255);
    SDL_RenderClear(sdl);
}

static void sdl_draw_edit_overlay(const Render *r, const Widget *w,
                                   const Theme *t, int cursor) {
    char    *buf    = w->buf;
    int      maxlen = w->buf_len - 1;
    int      slen   = (int)strlen(buf);
    int      cw     = font_8x8.w;
    int      fh     = font_8x8.h;

    uint16_t wfg = w->colors ? w->colors->fg : t->text_fg;
    uint16_t wbg = w->colors ? w->colors->bg : t->widget_bg;

    my_fill_rect(0, 0, (int16_t)r->screen_w, (int16_t)r->screen_h, t->screen_bg);

    DrawStyle s_title = { .fg = t->text_fg, .bg = t->screen_bg };
    int16_t tx = (int16_t)(r->screen_w / 2 - (int)strlen(w->text) * cw / 2);
    my_draw_text(tx, 16, w->text, &s_title);

    int16_t fw = (int16_t)(maxlen * cw + 8);
    int16_t fx = (int16_t)(r->screen_w  / 2 - fw / 2);
    int16_t fy = (int16_t)(r->screen_h  / 2 - fh / 2 - 4);
    my_fill_rect(fx, fy, fw, (int16_t)(fh + 8), wbg);
    my_draw_border(fx, fy, fw, (int16_t)(fh + 8), t->border_focused, 1);

    int16_t cx = (int16_t)(fx + 4);
    int16_t cy = (int16_t)(fy + 4);

    DrawStyle s_norm = { .fg = wfg,          .bg = wbg           };
    DrawStyle s_cur  = { .fg = t->cursor_fg,  .bg = t->cursor_bg  };

    if (cursor > 0) {
        char part[64];
        memcpy(part, buf, (size_t)cursor);
        part[cursor] = '\0';
        my_draw_text(cx, cy, part, &s_norm);
    }
    char cur_ch[2] = { cursor < slen ? buf[cursor] : ' ', '\0' };
    my_draw_text((int16_t)(cx + cursor * cw), cy, cur_ch, &s_cur);
    if (cursor < slen)
        my_draw_text((int16_t)(cx + (cursor + 1) * cw), cy,
                     buf + cursor + 1, &s_norm);

    DrawStyle s_hint = { .fg = t->hint_fg, .bg = t->screen_bg };
    const char *hint = "Enter=confirm  Esc=cancel";
    int16_t hx = (int16_t)(r->screen_w / 2 - (int)strlen(hint) * cw / 2);
    my_draw_text(hx, (int16_t)(r->screen_h - 20), hint, &s_hint);
}

// ── widget callbacks ─────────────────────────────────────────────────────────

static void menu_draw_label(const Render *r, int16_t x, int16_t y,
                            const Widget *w, const Theme *t)
{
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    int tlen = (int)strlen(w->text);
    int16_t tx = (w->w > 0) ? x : (int16_t)(x - tlen * font_8x8.w / 2);
    DrawStyle s = { .fg = fg, .bg = t->screen_bg };
    my_draw_text(tx, y, w->text, &s);
}

static void menu_draw_progress(const Render *r, int16_t x, int16_t y,
                               const Widget *w, const Theme *t)
{
    (void)r;
    uint16_t bg   = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t fill = w->colors ? w->colors->fg : t->border_focused;

    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, t->border_subtle, 1);

    if (w->value && w->max > w->min) {
        int range = w->max - w->min;
        int val   = *w->value < w->min ? w->min :
                    *w->value > w->max ? w->max : *w->value;
        if (w->h > w->w) {
            int16_t fh = (int16_t)((val - w->min) * (w->h - 2) / range);
            if (fh > 0)
                my_fill_rect((int16_t)(x + 1), (int16_t)(y + w->h - 1 - fh),
                             (int16_t)(w->w - 2), fh, fill);
        } else {
            int16_t fw = (int16_t)((val - w->min) * (w->w - 2) / range);
            if (fw > 0)
                my_fill_rect((int16_t)(x + 1), (int16_t)(y + 1),
                             fw, (int16_t)(w->h - 2), fill);
        }
    }
}

static void menu_draw_edit(const Render *r, int16_t x, int16_t y,
                           const Widget *w, const Theme *t, int focused)
{
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t border_col = focused ? t->border_focused : t->border_normal;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1);
    char disp[48];
    const char *val = (w->buf && w->buf[0]) ? w->buf : "...";
    snprintf(disp, sizeof(disp), "%s: %s", w->text, val);
    int tlen = (int)strlen(disp);
    int16_t tx = (int16_t)(x + (w->w - tlen * font_8x8.w) / 2);
    int16_t ty = (int16_t)(y + (w->h - font_8x8.h) / 2);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);
}

static void menu_draw_btn(const Render *r, int16_t x, int16_t y,
                          const Widget *w, const Theme *t, int focused)
{
    (void)r;
    uint16_t fg  = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg  = w->colors ? w->colors->bg : t->screen_bg;
    uint16_t brd = focused ? t->border_focused : t->border_subtle;

    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, brd, 1);

    int tlen = (int)strlen(w->text);
    int16_t tx = (int16_t)(x + (w->w - tlen * font_8x8.w) / 2);
    int16_t ty = (int16_t)(y + (w->h - font_8x8.h) / 2);

    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, w->text, &s);
}

static void menu_draw_value(const Render *r, int16_t x, int16_t y,
                            const Widget *w, const Theme *t, int focused, int editing)
{
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t border_col = editing ? t->border_editing :
                          focused ? t->border_focused : t->border_subtle;

    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1);

    char disp[48];
    if (w->options && w->value)
        snprintf(disp, sizeof(disp), "%s: %s", w->text, w->options[*w->value]);
    else if (w->value)
        snprintf(disp, sizeof(disp), "%s: %d", w->text, *w->value);
    else
        snprintf(disp, sizeof(disp), "%s: --", w->text);

    int tlen = (int)strlen(disp);
    int16_t tx = (int16_t)(x + (w->w - tlen * font_8x8.w) / 2);
    int16_t ty = (int16_t)(y + (w->h - font_8x8.h) / 2);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);

    if (editing) {
        DrawStyle sa = { .fg = t->border_editing, .bg = bg };
        my_draw_text((int16_t)(x + font_8x8.w),             ty, "<", &sa);
        my_draw_text((int16_t)(x + w->w - 2 * font_8x8.w),  ty, ">", &sa);
    }
}

static void sdl_draw_list_item(const Render *r,
                                int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                const ListItem *item, const Theme *t,
                                int focused, int editing)
{
    (void)r;
    uint16_t fg     = item->colors ? item->colors->fg : t->text_fg;
    uint16_t bg     = item->colors ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = focused ? t->widget_bg : bg;
    int16_t  cw     = (int16_t)font_8x8.w;
    int16_t  ch     = (int16_t)font_8x8.h;
    int16_t  ty     = (int16_t)(y + ((int16_t)row_h - ch) / 2);
    int16_t  pad    = 8;

    my_fill_rect(x, y, (int16_t)row_w, (int16_t)row_h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = t->hint_fg, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        return;
    }

    uint16_t brd = (item->type == LI_VALUE && editing) ? t->border_editing :
                   focused                              ? t->border_focused :
                                                         t->border_subtle;
    my_draw_border(x, y, (int16_t)row_w, (int16_t)row_h, brd, 1);
    DrawStyle s = { .fg = fg, .bg = row_bg };

    if (item->type == LI_BTN) {
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        my_draw_text((int16_t)(x + (int16_t)row_w - 2 * cw), ty, ">", &s);
        return;
    }

    /* LI_VALUE */
    my_draw_text((int16_t)(x + pad), ty, item->text, &s);

    char val_str[32];
    if (item->options && item->value) {
        int idx = *item->value;
        if (idx < 0) idx = 0;
        if (idx >= (int)item->options_count) idx = (int)item->options_count - 1;
        snprintf(val_str, sizeof(val_str), "%s", item->options[idx]);
    } else if (item->value) {
        snprintf(val_str, sizeof(val_str), "%d", *item->value);
    } else {
        snprintf(val_str, sizeof(val_str), "--");
    }

    int16_t vlen = (int16_t)strlen(val_str);
    int16_t vx   = (int16_t)(x + (int16_t)row_w - (vlen + 2) * cw);
    if (vx < x + pad) vx = (int16_t)(x + pad);
    my_draw_text(vx, ty, val_str, &s);

    if (editing) {
        DrawStyle sa = { .fg = t->border_editing, .bg = row_bg };
        my_draw_text((int16_t)(vx - cw), ty, "<", &sa);
        my_draw_text((int16_t)(x + (int16_t)row_w - 2 * cw), ty, ">", &sa);
    }
}

static void sdl_draw_list_separator(const Render *r,
                                     int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                     const Theme *t)
{
    (void)r;
    int16_t mid = (int16_t)(y + (int16_t)row_h / 2);
    my_fill_rect(x, mid, (int16_t)row_w, 1, t->border_subtle);
}

// ── lifecycle ────────────────────────────────────────────────────────────────
void render_impl_sdl_init(int screen_w, int screen_h, int sc) {
    scale = sc;
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("screen-ui",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        screen_w * scale, screen_h * scale, 0);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(sdl, screen_w, screen_h);

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
    };
    render_set(&r);
    render_set_theme(&theme);
}

void sdl_wait_key(void) {
    SDL_Event e;
    int was_str_edit = 0;

    while (1) {
        if (!SDL_WaitEventTimeout(&e, 150)) {
            layouts_tick();
            render_refresh();
            continue;
        }
        if (e.type == SDL_QUIT) exit(0);

        int str_edit = render_in_edit_mode();
        int val_edit = render_in_value_edit();

        if (str_edit && !was_str_edit) SDL_StartTextInput();
        if (!str_edit && was_str_edit) SDL_StopTextInput();
        was_str_edit = str_edit;

        if (str_edit) {
            // ── string input mode (W_EDIT) ────────────────────────────────
            if (e.type == SDL_TEXTINPUT) {
                render_edit_char(e.text.text[0]);
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_BACKSPACE: render_edit_delete();    break;
                    case SDLK_LEFT:      render_focus_prev();     break;
                    case SDLK_RIGHT:     render_focus_next();     break;
                    case SDLK_RETURN:    render_focus_activate(); break;
                    case SDLK_ESCAPE:    render_cancel();         break;
                    default: break;
                }
            }
        } else if (val_edit) {
            // ── value edit mode (W_VALUE) — analogous to encoder rotation ─
            if (e.type != SDL_KEYDOWN) continue;
            switch (e.key.keysym.sym) {
                case SDLK_RIGHT:
                case SDLK_UP:    render_focus_inc(); render_refresh(); break;
                case SDLK_LEFT:
                case SDLK_DOWN:  render_focus_dec(); render_refresh(); break;
                case SDLK_RETURN:
                case SDLK_SPACE: render_focus_activate();              break;
                case SDLK_ESCAPE: render_cancel();                     break;
                default: break;
            }
        } else {
            // ── normal navigation mode ────────────────────────────────────
            if (e.type != SDL_KEYDOWN) continue;
            switch (e.key.keysym.sym) {
                case SDLK_DOWN:   render_focus_next(); render_refresh(); break;
                case SDLK_UP:     render_focus_prev(); render_refresh(); break;
                case SDLK_RETURN:
                case SDLK_SPACE:  render_focus_activate();               break;
                case SDLK_ESCAPE: return;
                default: break;
            }
        }
    }
}

void sdl_quit(void) {
    SDL_DestroyRenderer(sdl);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void render_init(void)     { render_impl_sdl_init(240, 320, 3); }
void render_wait_key(void) { sdl_wait_key(); }
void render_quit(void)     { sdl_quit(); }

void *render_screen_create(void)        { return (void*)1; }
void  render_screen_destroy(void *root) { (void)root; }
void  render_screen_load(void *root)    { (void)root; }
