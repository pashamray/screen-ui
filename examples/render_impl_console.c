#include "render.h"
#include "theme_default.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define COLS   60
#define ROWS   40
#define LOG_W  240
#define LOG_H  320

typedef struct { char ch; uint8_t fg; uint8_t bg; } Cell;

static Cell fb[ROWS][COLS];

static void fb_clear(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            fb[r][c] = (Cell){ ' ', 7U, 0U };
        }
    }
}

static uint8_t rgb565_to_ansi(uint16_t color) {
    if (color == 0x0000U) {return 0U; }
    if (color == 0xFFFFU) {return 15U; }
    if (color == 0x07FFU) {return 14U; }
    if (color == 0xFFE0U) {return 11U; }
    if (color == 0x2104U) {return 8U; }
    if (color == 0x001FU) {return 4U; }
    if (color == 0xF800U) {return 1U; }
    if (color == 0x07E0U) {return 2U; }
    return 7U;
}

/* ── Gfx: fill ────────────────────────────────────────────────────────────── */
static void console_fill(const Ctx *ctx, int16_t lx, int16_t ly, int16_t lw, int16_t lh, uint16_t color) {
    uint8_t bg = rgb565_to_ansi(color);
    int cx1 = (int)ctx->ox; int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw; int cy2 = cy1 + (int)ctx->ch;

    int ax = (int)ctx->ox + (int)lx; int ay = (int)ctx->oy + (int)ly;
    int aw = (int)lw; int ah = (int)lh;

    if ((ax >= cx2) || (ay >= cy2) || ((ax + aw) <= cx1) || ((ay + ah) <= cy1)) {
return;
    }
    if (ax < cx1) { aw -= (cx1 - ax); ax = cx1; }
    if (ay < cy1) { ah -= (cy1 - ay); ay = cy1; }
    if ((ax + aw) > cx2) { aw = cx2 - ax; }
    if ((ay + ah) > cy2) { ah = cy2 - ay; }

    {
        int x = ax * COLS / LOG_W; int y = ay * ROWS / LOG_H;
        int w = aw * COLS / LOG_W; int h = ah * ROWS / LOG_H;
        if (w < 1) { w = 1; } if (h < 1) { h = 1; }
        for (int r = y; r < (y + h); r++) {
            for (int c = x; c < (x + w); c++) {
                if ((r >= 0) && (r < ROWS) && (c >= 0) && (c < COLS)) {
                    fb[r][c] = (Cell){ ' ', 7U, bg };
                }
            }
        }
    }
}

/* ── Gfx: text ────────────────────────────────────────────────────────────── */
static void console_text(const Ctx *ctx, int16_t lx, int16_t ly, const char *text,
                          const struct Font *f, uint16_t fg, uint16_t bg) {
    (void)f; /* console maps pixels to cells — font metrics ignored */
    int ax = (int)ctx->ox + (int)lx; int ay = (int)ctx->oy + (int)ly;
    int cy1 = (int)ctx->oy; int cy2 = cy1 + (int)ctx->ch;
    int cx1 = (int)ctx->ox; int cx2 = cx1 + (int)ctx->cw;

    int x = ax * COLS / LOG_W; int y = ay * ROWS / LOG_H;
    if ((y < 0) || (y >= ROWS)) {return; }
    if ((ay < cy1) || (ay >= cy2)) {return; }

    uint8_t afg = rgb565_to_ansi(fg);
    uint8_t abg = rgb565_to_ansi(bg);
    for (int i = 0; (text[i] != '\0') && ((x + i) < COLS); i++) {
        int lxi = ax + (i * (LOG_W / COLS));
        if ((lxi < cx1) || (lxi >= cx2)) { continue; }
        if ((x + i) >= 0) {
            fb[y][x + i] = (Cell){ text[i], afg, abg };
        }
    }
}

/* ── Gfx: border (ASCII art) ──────────────────────────────────────────────── */
static void console_border(const Ctx *ctx, int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                             uint16_t color, uint8_t thickness) {
    (void)thickness;
    uint8_t fg = rgb565_to_ansi(color);
    int ax = (int)ctx->ox + (int)lx; int ay = (int)ctx->oy + (int)ly;
    int x = ax * COLS / LOG_W; int y = ay * ROWS / LOG_H;
    int w = (int)lw * COLS / LOG_W; int h = (int)lh * ROWS / LOG_H;
    if (w < 1) { w = 1; } if (h < 1) { h = 1; }

    for (int c = x; (c < (x + w)) && (c < COLS); c++) {
        if ((y >= 0) && (y < ROWS)) {
            fb[y][c] = (Cell){ ((c == x) || (c == (x + w - 1))) ? '+' : '-', fg, fb[y][c].bg };
        }
        if (((y + h - 1) >= 0) && ((y + h - 1) < ROWS)) {
            fb[y + h - 1][c] = (Cell){ ((c == x) || (c == (x + w - 1))) ? '+' : '-', fg, fb[y + h - 1][c].bg };
        }
    }
    for (int r = (y + 1); (r < (y + h - 1)) && (r < ROWS); r++) {
        if ((x >= 0) && (x < COLS)) {
            fb[r][x] = (Cell){ '|', fg, fb[r][x].bg };
        }
        if (((x + w - 1) >= 0) && ((x + w - 1) < COLS)) {
            fb[r][x + w - 1] = (Cell){ '|', fg, fb[r][x + w - 1].bg };
        }
    }
}

/* ── Gfx: frame hooks ─────────────────────────────────────────────────────── */
static void console_begin(const Ctx *ctx) { (void)ctx; fb_clear(); }

static void console_flush(void) {
    (void)printf("\033[2J\033[H");
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            const Cell *cell = &fb[r][c];
            (void)printf("\033[38;5;%dm\033[48;5;%dm%c", (int)cell->fg, (int)cell->bg, cell->ch);
        }
        (void)printf("\033[0m\n");
    }
    (void)fflush(stdout);
}

/* Pseudo-font: gives view_default correct cell-based layout metrics.
 * w = LOG_W/COLS = 4 pixels per cell column (matches current console text positioning). */
static const Font console_font = {
    .w      = (uint8_t)(LOG_W / COLS),
    .h      = (uint8_t)(LOG_H / ROWS),
    .stride = 0U,
    .first  = 0U,
    .count  = 0U,
    .data   = NULL,
    .widths = NULL,
};

void console_render_init(void) {
    static Gfx gfx;
    fb_clear();
    gfx = (Gfx){
        .fill     = console_fill,
        .text     = console_text,
        .border   = console_border,
        .begin    = console_begin,
        .flush    = console_flush,
        .screen_w = (uint16_t)LOG_W,
        .screen_h = (uint16_t)LOG_H,
    };
    render_set_gfx(&gfx);
    render_set_view(NULL);           /* use view_default */
    render_set_theme(&theme_default);
    render_set_font(&console_font);
}

void render_init(void)     { console_render_init(); }
void render_wait_key(void) {
    char buf[64];
    for (;;) {
        (void)printf("\033[%d;0H\033[0m[j/k=nav/change  Enter=select  z=back  q=quit]   ", ROWS + 1);
        (void)fflush(stdout);

        if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
            return;
        }
        {
            char key = buf[0];
            if ((key == 'q') || (key == 'Q')) {
                return;
            }
            if ((key == 'j') || (key == 'J')) {
                render_next(); render_refresh();
            } else if ((key == 'k') || (key == 'K')) {
                render_prev(); render_refresh();
            } else if ((key == 'z') || (key == 'Z')) {
                render_cancel(); render_refresh();
            } else if (key == '\n') {
                if (render_focused_type() == W_EDIT) {
                    (void)printf("\033[%d;0H\033[0mNew value: ", ROWS + 1);
                    (void)fflush(stdout);
                    {
                        char val[64];
                        if (fgets(val, (int)sizeof(val), stdin) != NULL) {
                            val[strcspn(val, "\n")] = '\0';
                            render_edit_set(val);
                            render_refresh();
                        }
                    }
                } else {
                    render_activate(); render_refresh();
                }
            } else {
                /* no action */
            }
        }
    }
}
void render_quit(void)     { (void)printf("\033[?25h\033[0m\n"); }
