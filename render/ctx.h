#pragma once
#include <stdint.h>
#include "gfx.h"
#include "theme.h"
#include "fonts.h"

/* Self-contained drawing context: carries hardware backend, theme, font, and clip region.
 * Every draw callback receives a Ctx* — no global renderer state needed. */
typedef struct Ctx {
    const Gfx   *gfx;
    const Theme *t;
    const Font  *font;
    int16_t      ox;       /* absolute x origin on screen */
    int16_t      oy;       /* absolute y origin on screen */
    int16_t      cw;       /* clip width  */
    int16_t      ch;       /* clip height */
    uint16_t     panel_bg; /* current panel background (0 = theme screen_bg) */
} Ctx;

/* Derive a sub-context: same gfx/theme/font, new absolute origin and dimensions. */
static inline Ctx ctx_sub(const Ctx *p, int16_t ox, int16_t oy, int16_t w, int16_t h) {
    Ctx c      = *p;
    c.ox       = ox;
    c.oy       = oy;
    c.cw       = w;
    c.ch       = h;
    c.panel_bg = 0U;
    return c;
}
