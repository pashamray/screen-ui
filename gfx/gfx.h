#pragma once
#include <stdint.h>

/* Forward declarations — completed by ctx.h and fonts.h respectively. */
struct Ctx;
struct Font;

typedef struct Gfx {
    void (*fill)  (const struct Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void (*text)  (const struct Ctx *ctx, int16_t x, int16_t y, const char *s,
                   const struct Font *f, uint16_t fg, uint16_t bg);
    void (*border)(const struct Ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color, uint8_t thickness);
    void (*begin) (const struct Ctx *ctx);
    void (*flush) (void);
    uint16_t screen_w;
    uint16_t screen_h;
} Gfx;
