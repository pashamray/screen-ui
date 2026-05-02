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

static const Render render_impl_console = {
    .fill_rect   = my_fill_rect,
    .draw_text   = my_draw_text,
    .draw_border = my_draw_border,
    .flush       = my_flush,
    .screen_w    = LOG_W,
    .screen_h    = LOG_H,
    .char_w      = LOG_W / COLS,   // 4 logical pixels per column
    .char_h      = LOG_H / ROWS,   // 8 logical pixels per row
};

void console_render_init(void) {
    fb_clear();
    render_set(&render_impl_console);
}

void render_init(void)     { console_render_init(); }
void render_wait_key(void) {
    char buf[64];
    for (;;) {
        if (render_in_value_edit())
            printf("\033[%d;0H\033[0m[h/l=change  Enter=confirm  z=cancel  q=quit] ", ROWS + 1);
        else
            printf("\033[%d;0H\033[0m[j/k=nav  Enter=edit/select  q=quit]          ", ROWS + 1);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) return;
        char key = buf[0];

        if (key == 'q' || key == 'Q') return;

        if (render_in_value_edit()) {
            // ── W_VALUE edit mode (analogous to encoder rotation) ──────────
            if (key == 'l' || key == 'L') { render_focus_inc(); render_refresh(); }
            else if (key == 'h' || key == 'H') { render_focus_dec(); render_refresh(); }
            else if (key == '\n') render_focus_activate();
            else if (key == 'z' || key == 'Z') render_cancel();
        } else {
            // ── normal navigation mode ─────────────────────────────────────
            if (key == 'j' || key == 'J') { render_focus_next(); render_refresh(); }
            else if (key == 'k' || key == 'K') { render_focus_prev(); render_refresh(); }
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
                    render_focus_activate();
                }
            }
        }
    }
}
void render_quit(void)     { printf("\033[?25h\033[0m\n"); }

void *render_screen_create(void)        { fb_clear(); return (void*)fb; }
void  render_screen_destroy(void *root) { (void)root; }
void  render_screen_load(void *root)    { (void)root; }
