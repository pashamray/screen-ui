#include "render.h"
#include "fonts.h"
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
        .fill_rect   = my_fill_rect,
        .draw_text   = my_draw_text,
        .draw_border = my_draw_border,
        .flush       = my_flush,
        .screen_w    = (uint16_t)screen_w,
        .screen_h    = (uint16_t)screen_h,
        .char_w      = font_8x8.w,
        .char_h      = font_8x8.h,
    };
    render_set(&r);
}

void sdl_wait_key(void) {
    SDL_Event e;
    int was_str_edit = 0;

    while (SDL_WaitEvent(&e)) {
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

void render_init(void)     { render_impl_sdl_init(240, 320, 2); }
void render_wait_key(void) { sdl_wait_key(); }
void render_quit(void)     { sdl_quit(); }

void *render_screen_create(void)        { return (void*)1; }
void  render_screen_destroy(void *root) { (void)root; }
void  render_screen_load(void *root)    { (void)root; }
