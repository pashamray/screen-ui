#include "render.h"
#include "theme_default.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* cppcheck-suppress misra-c2012-5.6 */
/* cppcheck-suppress misra-c2012-5.7 */
typedef struct { uint16_t fg; uint16_t bg; } DrawStyle;

/* font_8x8 and font_mono16 are defined in fonts/ (shared with SDL target) */

#define COLS   60
#define ROWS   40
#define LOG_W  240
#define LOG_H  320

/* cppcheck-suppress misra-c2012-7.3 */
typedef struct { char ch; uint8_t fg; uint8_t bg; } Cell;

/* cppcheck-suppress misra-c2012-8.9 */
/* cppcheck-suppress misra-c2012-7.3 */
static Cell fb[ROWS][COLS];

static void fb_clear(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[r][c] = (Cell){ ' ', 7U, 0U };
        }
    }
}

static uint8_t rgb565_to_ansi(uint16_t color) {
    if (color == 0x0000U) { /* cppcheck-suppress misra-c2012-15.5 */ return 0U; }
    if (color == 0xFFFFU) { /* cppcheck-suppress misra-c2012-15.5 */ return 15U; }
    if (color == 0x07FFU) { /* cppcheck-suppress misra-c2012-15.5 */ return 14U; } /* cyan — focused */
    if (color == 0xFFE0U) { /* cppcheck-suppress misra-c2012-15.5 */ return 11U; } /* yellow — value edit */
    if (color == 0x2104U) { /* cppcheck-suppress misra-c2012-15.5 */ return 8U; }
    if (color == 0x001FU) { /* cppcheck-suppress misra-c2012-15.5 */ return 4U; }
    if (color == 0xF800U) { /* cppcheck-suppress misra-c2012-15.5 */ return 1U; }
    if (color == 0x07E0U) { /* cppcheck-suppress misra-c2012-15.5 */ return 2U; }
    return 7U;
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_fill_rect(int16_t lx, int16_t ly, int16_t lw, int16_t lh, uint16_t color) {
    uint8_t bg = rgb565_to_ansi(color);
    int x = (int)lx * COLS / LOG_W;
    int y = (int)ly * ROWS / LOG_H;
    int w = (int)lw * COLS / LOG_W;
    int h = (int)lh * ROWS / LOG_H;
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    for (int r = y; r < (y + h); r++) {
        for (int c = x; c < (x + w); c++) {
            if ((r >= 0) && (r < ROWS) && (c >= 0) && (c < COLS)) {
                /* cppcheck-suppress misra-c2012-7.3 */
                fb[r][c] = (Cell){ ' ', 7U, bg };
            }
        }
    }
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_draw_text(int16_t lx, int16_t ly, const char *text, const DrawStyle *s) {
    int x = (int)lx * COLS / LOG_W;
    int y = (int)ly * ROWS / LOG_H;
    if ((y < 0) || (y >= ROWS)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    uint8_t fg = rgb565_to_ansi(s->fg);
    uint8_t bg = rgb565_to_ansi(s->bg);
    for (int i = 0; (text[i] != '\0') && ((x + i) < COLS); i++) {
        if ((x + i) >= 0) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[y][x + i] = (Cell){ text[i], fg, bg };
        }
    }
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_draw_border(int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                            uint16_t color, uint8_t thickness) {
    (void)thickness;
    uint8_t fg = rgb565_to_ansi(color);
    int x = (int)lx * COLS / LOG_W;
    int y = (int)ly * ROWS / LOG_H;
    int w = (int)lw * COLS / LOG_W;
    int h = (int)lh * ROWS / LOG_H;
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }

    for (int c = x; (c < (x + w)) && (c < COLS); c++) {
        if ((y >= 0) && (y < ROWS)) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[y][c] = (Cell){ ((c == x) || (c == (x + w - 1))) ? '+' : '-', fg, fb[y][c].bg };
        }
        if (((y + h - 1) >= 0) && ((y + h - 1) < ROWS)) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[y + h - 1][c] = (Cell){ ((c == x) || (c == (x + w - 1))) ? '+' : '-', fg, fb[y + h - 1][c].bg };
        }
    }
    for (int r = (y + 1); (r < (y + h - 1)) && (r < ROWS); r++) {
        if ((x >= 0) && (x < COLS)) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[r][x] = (Cell){ '|', fg, fb[r][x].bg };
        }
        if (((x + w - 1) >= 0) && ((x + w - 1) < COLS)) {
            /* cppcheck-suppress misra-c2012-7.3 */
            fb[r][x + w - 1] = (Cell){ '|', fg, fb[r][x + w - 1].bg };
        }
    }
}

/* cppcheck-suppress misra-c2012-5.9 */
static void my_flush(void) {
    /* cppcheck-suppress misra-c2012-21.6 */
    /* cppcheck-suppress misra-c2012-17.7 */
    /* cppcheck-suppress misra-c2012-4.1 */
    (void)printf("\033[2J\033[H");
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            /* cppcheck-suppress misra-c2012-7.3 */
            const Cell *cell = &fb[r][c];
            /* cppcheck-suppress misra-c2012-21.6 */
            /* cppcheck-suppress misra-c2012-17.7 */
            /* cppcheck-suppress misra-c2012-7.3 */
            /* cppcheck-suppress misra-c2012-4.1 */
            (void)printf("\033[38;5;%dm\033[48;5;%dm%c", (int)cell->fg, (int)cell->bg, cell->ch);
        }
        /* cppcheck-suppress misra-c2012-21.6 */
        /* cppcheck-suppress misra-c2012-17.7 */
        /* cppcheck-suppress misra-c2012-4.1 */
        (void)printf("\033[0m\n");
    }
    /* cppcheck-suppress misra-c2012-21.6 */
    (void)fflush(stdout);
}


/* ── frame-level hooks ──────────────────────────────────────────────────── */

static void console_begin_frame(const Render *r, const Theme *t) {
    (void)r; (void)t;
    fb_clear();
}

/* ── widget callbacks ─────────────────────────────────────────────────────── */

static void console_draw_label(const Render *r, int16_t x, int16_t y,
                                const Widget *w, const Theme *t) {
    (void)r;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : t->text_fg;
    int tlen = (int)strlen(w->text);
    int16_t tx = (w->w > 0) ? x : (int16_t)(x - (tlen * (LOG_W / COLS)) / 2);
    DrawStyle s = { .fg = fg, .bg = t->screen_bg };
    my_draw_text(tx, y, w->text, &s);
}

static void console_draw_btn(const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t, int focused) {
    (void)r;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : t->widget_bg;
    uint16_t border_col = (focused != 0) ? t->border_focused : t->border_normal;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1U);
    int tlen = (int)strlen(w->text);
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t tx = (int16_t)(x + ((w->w - (int16_t)(tlen * (LOG_W / COLS))) / 2));
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t ty = (int16_t)(y + ((w->h - (int16_t)(LOG_H / ROWS)) / 2));
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, w->text, &s);
}

static void console_draw_value(const Render *r, int16_t x, int16_t y,
                                const Widget *w, const Theme *t, int focused, int editing) {
    (void)r;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : t->widget_bg;
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

    uint16_t border_col;
    if (editing != 0) {
        border_col = t->border_editing;
    } else if (focused != 0) {
        border_col = t->border_focused;
    } else {
        border_col = t->border_subtle;
    }
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1U);
    int tlen = (int)strlen(disp);
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t tx = (int16_t)(x + ((w->w - (int16_t)(tlen * (LOG_W / COLS))) / 2));
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t ty = (int16_t)(y + ((w->h - (int16_t)(LOG_H / ROWS)) / 2));
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);
}

static void console_draw_progress(const Render *r, int16_t x, int16_t y,
                                   const Widget *w, const Theme *t) {
    (void)r;
    uint16_t bg   = (w->colors != NULL) ? w->colors->bg : t->widget_bg;
    uint16_t fill = (w->colors != NULL) ? w->colors->fg : t->border_focused;

    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, t->border_subtle, 1U);

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
                my_fill_rect((int16_t)(x + 1), (int16_t)((y + w->h) - 1 - fh),
                             (int16_t)(w->w - 2), fh, fill);
            }
        } else {
            int16_t fw = (int16_t)(((val - w->min) * (w->w - 2)) / range);
            if (fw > 0) {
                my_fill_rect((int16_t)(x + 1), (int16_t)(y + 1),
                             fw, (int16_t)(w->h - 2), fill);
            }
        }
    }
}

static void console_draw_edit(const Render *r, int16_t x, int16_t y,
                               const Widget *w, const Theme *t, int focused) {
    (void)r;
    uint16_t fg = (w->colors != NULL) ? w->colors->fg : t->text_fg;
    uint16_t bg = (w->colors != NULL) ? w->colors->bg : t->widget_bg;
    uint16_t border_col = (focused != 0) ? t->border_focused : t->border_normal;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1U);
    char disp[48];
    const char *val = ((w->buf != NULL) && (w->buf[0] != '\0')) ? w->buf : "...";
    /* cppcheck-suppress misra-c2012-21.6 */
    /* cppcheck-suppress misra-c2012-17.7 */
    (void)snprintf(disp, sizeof(disp), "%s: %s", w->text, val);
    int tlen = (int)strlen(disp);
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t tx = (int16_t)(x + ((w->w - (int16_t)(tlen * (LOG_W / COLS))) / 2));
    /* cppcheck-suppress misra-c2012-10.8 */
    int16_t ty = (int16_t)(y + ((w->h - (int16_t)(LOG_H / ROWS)) / 2));
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);
}

static void console_draw_list_item(const Render *r,
                                    int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                    const ListItem *item, const Theme *t,
                                    int focused, int editing)
{
    (void)r;
    uint16_t fg     = (item->colors != NULL) ? item->colors->fg : t->text_fg;
    uint16_t bg     = (item->colors != NULL) ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = (focused != 0) ? t->widget_bg : bg;
    int      cw     = LOG_W / COLS;
    int      ch     = LOG_H / ROWS;
    int16_t  ty     = (int16_t)(y + (((int16_t)row_h - (int16_t)ch) / 2));
    int16_t  pad    = (int16_t)cw;

    my_fill_rect(x, y, (int16_t)row_w, (int16_t)row_h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = t->hint_fg, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    int16_t  tx = (int16_t)(x + pad + ((focused != 0) ? (int16_t)cw : 0));
    DrawStyle s  = { .fg = fg, .bg = row_bg };

    if (focused != 0) {
        DrawStyle sc = { .fg = t->border_focused, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, ">", &sc);
    }

    if (item->type == LI_BTN) {
        my_draw_text(tx, ty, item->text, &s);
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    if (item->type == LI_SUBMENU) {
        my_draw_text(tx, ty, item->text, &s);
        my_draw_text((int16_t)(x + (int16_t)row_w - (int16_t)(2 * cw)), ty, ">", &s);
        if (item->hint != NULL) {
            DrawStyle sh = { .fg = t->hint_fg, .bg = row_bg };
            int16_t hlen = (int16_t)strlen(item->hint);
            int16_t hx   = (int16_t)(x + (int16_t)row_w - ((hlen + (int16_t)3) * (int16_t)cw));
            if (hx > tx) {
                my_draw_text(hx, ty, item->hint, &sh);
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    if (item->type == LI_CHECK) {
        my_draw_text(tx, ty, item->text, &s);
        const char *chk = ((item->value != NULL) && (*item->value != 0)) ? "[x]" : "[ ]";
        int16_t cx = (int16_t)(x + (int16_t)row_w - (int16_t)(4 * cw));
        my_draw_text(cx, ty, chk, &s);
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    /* LI_VALUE */
    my_draw_text(tx, ty, item->text, &s);

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
        int16_t vx   = (int16_t)(x + (int16_t)row_w - ((vlen + (int16_t)2) * (int16_t)cw));
        if (vx < (tx + (int16_t)cw)) {
            vx = (int16_t)(tx + (int16_t)cw);
        }
        my_draw_text(vx, ty, val_str, &s);

        if (editing != 0) {
            DrawStyle sa = { .fg = t->border_editing, .bg = row_bg };
            my_draw_text((int16_t)(vx - (int16_t)cw), ty, "<", &sa);
            my_draw_text((int16_t)(x + (int16_t)row_w - (int16_t)(2 * cw)), ty, ">", &sa);
        }
    }
}

static void console_draw_list_separator(const Render *r,
                                         int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                         const Theme *t)
{
    (void)r;
    int16_t mid = (int16_t)(y + ((int16_t)row_h / 2));
    my_fill_rect(x, mid, (int16_t)row_w, 1, t->border_subtle);
}

/* cppcheck-suppress misra-c2012-8.9 */
static const Render render_impl_console = {
    .begin_frame  = console_begin_frame,
    .flush        = my_flush,
    .screen_w     = (uint16_t)LOG_W,
    .screen_h     = (uint16_t)LOG_H,
    .draw_label    = console_draw_label,
    .draw_btn      = console_draw_btn,
    .draw_value    = console_draw_value,
    .draw_progress = console_draw_progress,
    .draw_edit     = console_draw_edit,
    .draw_list_item      = console_draw_list_item,
    .draw_list_separator = console_draw_list_separator,
};

/* cppcheck-suppress misra-c2012-8.4 */
/* cppcheck-suppress misra-c2012-8.7 */
void console_render_init(void) {
    fb_clear();
    render_set(&render_impl_console);
    render_set_theme(&theme_default);
}

/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_init(void)     { console_render_init(); }
/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_wait_key(void) {
    char buf[64];
    /* cppcheck-suppress misra-c2012-15.4 */
    for (;;) {
        /* cppcheck-suppress misra-c2012-21.6 */
        /* cppcheck-suppress misra-c2012-17.7 */
        /* cppcheck-suppress misra-c2012-4.1 */
        (void)printf("\033[%d;0H\033[0m[j/k=nav/change  Enter=select  z=back  q=quit]   ", ROWS + 1);
        /* cppcheck-suppress misra-c2012-21.6 */
        /* cppcheck-suppress misra-c2012-17.7 */
        (void)fflush(stdout);

        /* cppcheck-suppress misra-c2012-21.6 */
        if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            char key = buf[0];

            if ((key == 'q') || (key == 'Q')) {
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }

            if ((key == 'j') || (key == 'J')) {
                render_next();
                render_refresh();
            } else if ((key == 'k') || (key == 'K')) {
                render_prev();
                render_refresh();
            } else if ((key == 'z') || (key == 'Z')) {
                render_cancel();
                render_refresh();
            } else if (key == '\n') {
                if (render_focused_type() == W_EDIT) {
                    /* cppcheck-suppress misra-c2012-21.6 */
                    /* cppcheck-suppress misra-c2012-17.7 */
                    /* cppcheck-suppress misra-c2012-4.1 */
                    (void)printf("\033[%d;0H\033[0mNew value: ", ROWS + 1);
                    /* cppcheck-suppress misra-c2012-21.6 */
                    /* cppcheck-suppress misra-c2012-17.7 */
                    (void)fflush(stdout);
                    {
                        char val[64];
                        /* cppcheck-suppress misra-c2012-21.6 */
                        if (fgets(val, (int)sizeof(val), stdin) != NULL) {
                            val[strcspn(val, "\n")] = '\0';
                            render_edit_set(val);
                            render_refresh();
                        }
                    }
                } else {
                    render_activate();
                    render_refresh();
                    /* cppcheck-suppress misra-c2012-15.7 */
                }
            } else {
                /* no action */
            }
        }
    }
}
/* cppcheck-suppress misra-c2012-4.1 */
/* cppcheck-suppress misra-c2012-21.6 */
/* cppcheck-suppress misra-c2012-17.7 */
/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_quit(void)     { (void)printf("\033[?25h\033[0m\n"); }

/* cppcheck-suppress misra-c2012-8.6 */
/* cppcheck-suppress misra-c2012-8.7 */
void render_set_font(const Font *f) { (void)f; }
