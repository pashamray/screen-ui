#include "render.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// font_8x8 and font_mono16 are defined in fonts/ (shared with SDL target)

#define COLS   60
#define ROWS   40
#define LOG_W  240
#define LOG_H  320

typedef struct { char ch; uint8_t fg; uint8_t bg; } Cell;

static Cell fb[ROWS][COLS];

static void fb_clear(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            fb[r][c] = (Cell){ ' ', 7, 0 };
}

static uint8_t rgb565_to_ansi(uint16_t color) {
    if (color == 0x0000) return 0;
    if (color == 0xFFFF) return 15;
    if (color == 0x07FF) return 14;  // cyan — focused
    if (color == 0xFFE0) return 11;  // yellow — value edit
    if (color == 0x2104) return 8;
    if (color == 0x001F) return 4;
    if (color == 0xF800) return 1;
    if (color == 0x07E0) return 2;
    return 7;
}

static void my_fill_rect(int16_t lx, int16_t ly, int16_t lw, int16_t lh, uint16_t color) {
    uint8_t bg = rgb565_to_ansi(color);
    int x = lx * COLS / LOG_W, y = ly * ROWS / LOG_H;
    int w = lw * COLS / LOG_W, h = lh * ROWS / LOG_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
                fb[r][c] = (Cell){ ' ', 7, bg };
}

static void my_draw_text(int16_t lx, int16_t ly, const char *text, const DrawStyle *s) {
    int x = lx * COLS / LOG_W, y = ly * ROWS / LOG_H;
    if (y < 0 || y >= ROWS) return;
    uint8_t fg = rgb565_to_ansi(s->fg);
    uint8_t bg = rgb565_to_ansi(s->bg);
    for (int i = 0; text[i] && x + i < COLS; i++)
        if (x + i >= 0)
            fb[y][x + i] = (Cell){ text[i], fg, bg };
}

static void my_draw_border(int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                            uint16_t color, uint8_t thickness) {
    (void)thickness;
    uint8_t fg = rgb565_to_ansi(color);
    int x = lx * COLS / LOG_W, y = ly * ROWS / LOG_H;
    int w = lw * COLS / LOG_W, h = lh * ROWS / LOG_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    for (int c = x; c < x + w && c < COLS; c++) {
        if (y >= 0 && y < ROWS)
            fb[y][c]     = (Cell){ (c == x || c == x+w-1) ? '+' : '-', fg, fb[y][c].bg };
        if (y+h-1 >= 0 && y+h-1 < ROWS)
            fb[y+h-1][c] = (Cell){ (c == x || c == x+w-1) ? '+' : '-', fg, fb[y+h-1][c].bg };
    }
    for (int r = y + 1; r < y + h - 1 && r < ROWS; r++) {
        if (x >= 0 && x < COLS)
            fb[r][x]     = (Cell){ '|', fg, fb[r][x].bg };
        if (x+w-1 >= 0 && x+w-1 < COLS)
            fb[r][x+w-1] = (Cell){ '|', fg, fb[r][x+w-1].bg };
    }
}

static void my_flush(void) {
    printf("\033[2J\033[H");
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            Cell *cell = &fb[r][c];
            printf("\033[38;5;%dm\033[48;5;%dm%c", cell->fg, cell->bg, cell->ch);
        }
        printf("\033[0m\n");
    }
    fflush(stdout);
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

static void console_begin_frame(const Render *r, const Theme *t) {
    (void)r; (void)t;
    fb_clear();
}

// ── widget callbacks ─────────────────────────────────────────────────────────

static void console_draw_label(const Render *r, int16_t x, int16_t y,
                                const Widget *w, const Theme *t) {
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    int tlen = (int)strlen(w->text);
    int16_t tx = (w->w > 0) ? x : (int16_t)(x - tlen * (LOG_W / COLS) / 2);
    DrawStyle s = { .fg = fg, .bg = t->screen_bg };
    my_draw_text(tx, y, w->text, &s);
}

static void console_draw_btn(const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t, int focused) {
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg = w->colors ? w->colors->bg : t->widget_bg;
    uint16_t border_col = focused ? t->border_focused : t->border_normal;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1);
    int tlen = (int)strlen(w->text);
    int16_t tx = (int16_t)(x + (w->w - tlen * (LOG_W / COLS)) / 2);
    int16_t ty = (int16_t)(y + (w->h - (LOG_H / ROWS)) / 2);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, w->text, &s);
}

static void console_draw_value(const Render *r, int16_t x, int16_t y,
                                const Widget *w, const Theme *t, int focused, int editing) {
    (void)r;
    uint16_t fg = w->colors ? w->colors->fg : t->text_fg;
    uint16_t bg = w->colors ? w->colors->bg : t->widget_bg;
    char disp[48];
    if (w->options && w->value)
        snprintf(disp, sizeof(disp), "%s: %s", w->text, w->options[*w->value]);
    else if (w->value)
        snprintf(disp, sizeof(disp), "%s: %d", w->text, *w->value);
    else
        snprintf(disp, sizeof(disp), "%s: --", w->text);

    uint16_t border_col = editing ? t->border_editing :
                          focused ? t->border_focused : t->border_subtle;
    my_fill_rect(x, y, w->w, w->h, bg);
    my_draw_border(x, y, w->w, w->h, border_col, 1);
    int tlen = (int)strlen(disp);
    int16_t tx = (int16_t)(x + (w->w - tlen * (LOG_W / COLS)) / 2);
    int16_t ty = (int16_t)(y + (w->h - (LOG_H / ROWS)) / 2);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);
}

static void console_draw_progress(const Render *r, int16_t x, int16_t y,
                                   const Widget *w, const Theme *t) {
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

static void console_draw_edit(const Render *r, int16_t x, int16_t y,
                               const Widget *w, const Theme *t, int focused) {
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
    int16_t tx = (int16_t)(x + (w->w - tlen * (LOG_W / COLS)) / 2);
    int16_t ty = (int16_t)(y + (w->h - (LOG_H / ROWS)) / 2);
    DrawStyle s = { .fg = fg, .bg = bg };
    my_draw_text(tx, ty, disp, &s);
}

static void console_draw_list_item(const Render *r,
                                    int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                    const ListItem *item, const Theme *t,
                                    int focused, int editing)
{
    (void)r;
    uint16_t fg     = item->colors ? item->colors->fg : t->text_fg;
    uint16_t bg     = item->colors ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = focused ? t->widget_bg : bg;
    int      cw     = LOG_W / COLS;
    int      ch     = LOG_H / ROWS;
    int16_t  ty     = (int16_t)(y + ((int16_t)row_h - ch) / 2);
    int16_t  pad    = (int16_t)cw;

    my_fill_rect(x, y, (int16_t)row_w, (int16_t)row_h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = t->hint_fg, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        return;
    }

    int16_t  tx = (int16_t)(x + pad + (focused ? cw : 0));
    DrawStyle s  = { .fg = fg, .bg = row_bg };

    if (focused) {
        DrawStyle sc = { .fg = t->border_focused, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, ">", &sc);
    }

    if (item->type == LI_BTN) {
        my_draw_text(tx, ty, item->text, &s);
        return;
    }

    /* LI_VALUE */
    my_draw_text(tx, ty, item->text, &s);

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
    if (vx < tx + cw) vx = (int16_t)(tx + cw);
    my_draw_text(vx, ty, val_str, &s);

    if (editing) {
        DrawStyle sa = { .fg = t->border_editing, .bg = row_bg };
        my_draw_text((int16_t)(vx - cw), ty, "<", &sa);
        my_draw_text((int16_t)(x + (int16_t)row_w - 2 * cw), ty, ">", &sa);
    }
}

static void console_draw_list_separator(const Render *r,
                                         int16_t x, int16_t y, uint16_t row_w, uint16_t row_h,
                                         const Theme *t)
{
    (void)r;
    int16_t mid = (int16_t)(y + (int16_t)row_h / 2);
    my_fill_rect(x, mid, (int16_t)row_w, 1, t->border_subtle);
}

static const Render render_impl_console = {
    .begin_frame  = console_begin_frame,
    .flush        = my_flush,
    .screen_w     = LOG_W,
    .screen_h     = LOG_H,
    .draw_label    = console_draw_label,
    .draw_btn      = console_draw_btn,
    .draw_value    = console_draw_value,
    .draw_progress = console_draw_progress,
    .draw_edit     = console_draw_edit,
    .draw_list_item      = console_draw_list_item,
    .draw_list_separator = console_draw_list_separator,
};

void console_render_init(void) {
    fb_clear();
    render_set(&render_impl_console);
    render_set_theme(&theme);
}

void render_init(void)     { console_render_init(); }
void render_wait_key(void) {
    char buf[64];
    for (;;) {
        printf("\033[%d;0H\033[0m[j/k=nav/change  Enter=select  z=back  q=quit]   ", ROWS + 1);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) return;
        char key = buf[0];

        if (key == 'q' || key == 'Q') return;

        if (key == 'j' || key == 'J') { render_next(); render_refresh(); }
        else if (key == 'k' || key == 'K') { render_prev(); render_refresh(); }
        else if (key == 'z' || key == 'Z') render_cancel();
        else if (key == '\n') {
            if (render_focused_type() == W_EDIT) {
                printf("\033[%d;0H\033[0mNew value: ", ROWS + 1);
                fflush(stdout);
                char val[64];
                if (fgets(val, sizeof(val), stdin)) {
                    val[strcspn(val, "\n")] = '\0';
                    render_edit_set(val);
                    render_refresh();
                }
            } else {
                render_activate();
            }
        }
    }
}
void render_quit(void)     { printf("\033[?25h\033[0m\n"); }

void *render_screen_create(void)        { fb_clear(); return (void*)fb; }
void  render_screen_destroy(void *root) { (void)root; }
void  render_screen_load(void *root)    { (void)root; }
