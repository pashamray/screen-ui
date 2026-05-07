#include "render.h"
#include "fonts.h"
#include "layouts.h"
#include "theme_default.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static SDL_Window   *window;
static SDL_Renderer *sdl;
static int           scale;

/* ── RGB565 → RGB888 ──────────────────────────────────────────────────────── */
static void rgb565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (uint8_t)((c >> 11U) << 3U);
    *g = (uint8_t)(((c >> 5U) & 0x3FU) << 2U);
    *b = (uint8_t)((c & 0x1FU) << 3U);
}

/* ── Gfx: fill ────────────────────────────────────────────────────────────── */
static void sdl_fill(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    int ax = (int)ctx->ox + (int)x;
    int ay = (int)ctx->oy + (int)y;
    int aw = (int)w;
    int ah = (int)h;

    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;

    if ((ax >= cx2) || (ay >= cy2) || ((ax + aw) <= cx1) || ((ay + ah) <= cy1)) {
        return;
    }
    if (ax < cx1) { aw -= (cx1 - ax); ax = cx1; }
    if (ay < cy1) { ah -= (cy1 - ay); ay = cy1; }
    if ((ax + aw) > cx2) { aw = cx2 - ax; }
    if ((ay + ah) > cy2) { ah = cy2 - ay; }

    {
        uint8_t cr; uint8_t cg; uint8_t cb;
        rgb565(color, &cr, &cg, &cb);
        SDL_SetRenderDrawColor(sdl, cr, cg, cb, 255U);
        {
            SDL_Rect rect = { ax, ay, aw, ah };
            (void)SDL_RenderFillRect(sdl, &rect);
        }
    }
}

/* ── Gfx: text ────────────────────────────────────────────────────────────── */
static int sdl_char_idx(const Font *f, uint8_t code) {
    uint8_t result;
    if ((code < f->first) || (code >= (uint8_t)(f->first + f->count))) {
        result = ((f->first <= (uint8_t)' ') && ((uint8_t)' ' < (uint8_t)(f->first + f->count)))
                 ? (uint8_t)' ' : f->first;
    } else {
        result = code;
    }
    return ((int)result - (int)f->first);
}

static void sdl_text(const Ctx *ctx, int16_t x, int16_t y, const char *text,
                     const struct Font *f, uint16_t fg, uint16_t bg) {
    if (f == NULL) {
        return;
    }
    int ax  = (int)ctx->ox + (int)x;
    int ay  = (int)ctx->oy + (int)y;
    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;

    uint8_t fr; uint8_t fg_; uint8_t fb;
    uint8_t br; uint8_t bg_; uint8_t bb;
    rgb565(fg, &fr, &fg_, &fb);
    rgb565(bg, &br, &bg_, &bb);

    /* background fill — clipped */
    {
        int bw = 0;
        for (const char *p = text; *p != '\0'; p++) {
            int idx = sdl_char_idx(f, (uint8_t)*p);
            bw += (f->widths != NULL) ? (int)f->widths[idx] : (int)f->w;
        }
        int bx = ax; int by = ay; int bh = (int)f->h;
        if ((bx < cx2) && (by < cy2) && ((bx + bw) > cx1) && ((by + bh) > cy1)) {
            int cbx = bx; int cby = by; int cbw = bw; int cbh = bh;
            if (cbx < cx1) { cbw -= (cx1 - cbx); cbx = cx1; }
            if (cby < cy1) { cbh -= (cy1 - cby); cby = cy1; }
            if ((cbx + cbw) > cx2) { cbw = cx2 - cbx; }
            if ((cby + cbh) > cy2) { cbh = cy2 - cby; }
            SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255U);
            {
                SDL_Rect bg_r = { cbx, cby, cbw, cbh };
                (void)SDL_RenderFillRect(sdl, &bg_r);
            }
        }
    }

    /* glyphs */
    SDL_SetRenderDrawColor(sdl, fr, fg_, fb, 255U);
    {
        int16_t cx = (int16_t)ax;
        for (const char *p = text; *p != '\0'; p++) {
            int idx = sdl_char_idx(f, (uint8_t)*p);
            const uint8_t *glyph = &f->data[idx * (int)f->h * (int)f->stride];
            int advance = (f->widths != NULL) ? (int)f->widths[idx] : (int)f->w;
            for (int row = 0; row < (int)f->h; row++) {
                for (int bi = 0; bi < (int)f->stride; bi++) {
                    uint8_t bits = glyph[(row * (int)f->stride) + bi];
                    if (bits == 0U) { continue; }
                    for (uint8_t col = 0U; col < 8U; col++) {
                        if (((bits >> col) & 1U) != 0U) {
                            int px = (int)cx + (bi * 8) + (int)col;
                            int py = ay + row;
                            if ((px >= cx1) && (px < cx2) && (py >= cy1) && (py < cy2)) {
                                SDL_Rect pxr = { px, py, 1, 1 };
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

/* ── Gfx: border ──────────────────────────────────────────────────────────── */
static void sdl_border(const Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color, uint8_t t) {
    uint8_t r; uint8_t g; uint8_t b;
    int ax  = (int)ctx->ox + (int)x;
    int ay  = (int)ctx->oy + (int)y;
    int cx1 = (int)ctx->ox;
    int cy1 = (int)ctx->oy;
    int cx2 = cx1 + (int)ctx->cw;
    int cy2 = cy1 + (int)ctx->ch;
    rgb565(color, &r, &g, &b);
    SDL_SetRenderDrawColor(sdl, r, g, b, 255U);
    {
        int it = (int)t; int iw = (int)w; int ih = (int)h;
        /* top */
        { int bx=ax; int by=ay; int bw=iw; int bh=it;
          if ((bx<cx2)&&(by<cy2)&&((bx+bw)>cx1)&&((by+bh)>cy1)){
            if(bx<cx1){bw-=(cx1-bx);bx=cx1;} if(by<cy1){bh-=(cy1-by);by=cy1;}
            if((bx+bw)>cx2){bw=cx2-bx;} if((by+bh)>cy2){bh=cy2-by;}
            SDL_Rect rr={bx,by,bw,bh};
            (void)SDL_RenderFillRect(sdl,&rr); } }
        /* bottom */
        { int bx=ax; int by=(ay+ih)-it; int bw=iw; int bh=it;
          if ((bx<cx2)&&(by<cy2)&&((bx+bw)>cx1)&&((by+bh)>cy1)){
            if(bx<cx1){bw-=(cx1-bx);bx=cx1;} if(by<cy1){bh-=(cy1-by);by=cy1;}
            if((bx+bw)>cx2){bw=cx2-bx;} if((by+bh)>cy2){bh=cy2-by;}
            SDL_Rect rr={bx,by,bw,bh};
            (void)SDL_RenderFillRect(sdl,&rr); } }
        /* left */
        { int bx=ax; int by=ay; int bw=it; int bh=ih;
          if ((bx<cx2)&&(by<cy2)&&((bx+bw)>cx1)&&((by+bh)>cy1)){
            if(bx<cx1){bw-=(cx1-bx);bx=cx1;} if(by<cy1){bh-=(cy1-by);by=cy1;}
            if((bx+bw)>cx2){bw=cx2-bx;} if((by+bh)>cy2){bh=cy2-by;}
            SDL_Rect rr={bx,by,bw,bh};
            (void)SDL_RenderFillRect(sdl,&rr); } }
        /* right */
        { int bx=(ax+iw)-it; int by=ay; int bw=it; int bh=ih;
          if ((bx<cx2)&&(by<cy2)&&((bx+bw)>cx1)&&((by+bh)>cy1)){
            if(bx<cx1){bw-=(cx1-bx);bx=cx1;} if(by<cy1){bh-=(cy1-by);by=cy1;}
            if((bx+bw)>cx2){bw=cx2-bx;} if((by+bh)>cy2){bh=cy2-by;}
            SDL_Rect rr={bx,by,bw,bh};
            (void)SDL_RenderFillRect(sdl,&rr); } }
    }
}

/* ── Gfx: frame hooks ─────────────────────────────────────────────────────── */
static void sdl_begin(const Ctx *ctx) {
    uint8_t br; uint8_t bg_; uint8_t bb;
    rgb565(ctx->t->screen_bg, &br, &bg_, &bb);
    SDL_SetRenderDrawColor(sdl, br, bg_, bb, 255U);
    (void)SDL_RenderClear(sdl);
}

static void sdl_flush(void) {
    static uint32_t frames = 0U;
    char title[64];
    frames++;
    (void)snprintf(title, sizeof(title), "screen-ui  [redraws: %u]", frames);
    SDL_SetWindowTitle(window, title);
    SDL_RenderPresent(sdl);
}

/* ── lifecycle ────────────────────────────────────────────────────────────── */
void render_impl_sdl_init(int screen_w, int screen_h, int sc) {
    scale = sc;
    (void)SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("screen-ui",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        screen_w * scale, screen_h * scale,
        SDL_WINDOW_BORDERLESS);
    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    (void)SDL_RenderSetLogicalSize(sdl, screen_w, screen_h);

    {
        static Gfx gfx;
        gfx = (Gfx){
            .fill     = sdl_fill,
            .text     = sdl_text,
            .border   = sdl_border,
            .begin    = sdl_begin,
            .flush    = sdl_flush,
            .screen_w = (uint16_t)screen_w,
            .screen_h = (uint16_t)screen_h,
        };
        render_set_gfx(&gfx);
        render_set_view(NULL);           /* use view_default */
        render_set_theme(&theme_default);
        render_set_font(&font_terminus20);
    }
}

void sdl_wait_key(void) {
    SDL_Event e;
    int was_str_edit = 0;
    int keep_running = 1;

    while (keep_running != 0) {
        if (SDL_WaitEventTimeout(&e, 16) == 0) {
            layouts_tick();
            render_refresh();
            continue;
        }
        if (e.type == (uint32_t)SDL_QUIT) {
            keep_running = 0;
            continue;
        }

        {
            int str_edit = render_in_edit_mode();
            if ((str_edit != 0) && (was_str_edit == 0)) {
                SDL_StartTextInput();
            }
            if ((str_edit == 0) && (was_str_edit != 0)) {
                SDL_StopTextInput();
            }
            was_str_edit = str_edit;

            if (str_edit != 0) {
                if (e.type == (uint32_t)SDL_TEXTINPUT) {
                    render_edit_char(e.text.text[0]);
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
                if (e.type != (uint32_t)SDL_KEYDOWN) {
                    continue;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_DOWN:   render_next();     break;
                    case SDLK_UP:     render_prev();     break;
                    case SDLK_LEFT:   render_dec();      break;
                    case SDLK_RIGHT:  render_inc();      break;
                    case SDLK_RETURN:
                    case SDLK_SPACE:  render_activate(); break;
                    case SDLK_ESCAPE: render_cancel();   break;
                    default: break;
                }
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
