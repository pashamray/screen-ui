#pragma once
#include <stdint.h>

typedef struct Font {
    uint8_t        w;       // max advance width
    uint8_t        h;       // cell height
    uint8_t        stride;  // bytes per glyph row = ceil(w/8)
    uint8_t        first;   // first char code (0 for bitmap fonts, 32 for TTF-generated)
    uint8_t        count;   // number of chars
    const uint8_t *data;    // glyph bitmaps: [count][h*stride], LSB-first per row
    const uint8_t *widths;  // advance widths [count]; NULL = fixed width (.w)
} Font;

extern const Font font_8x8;
extern const Font font_mono16;

extern const Font font_noto10;
extern const Font font_noto12;
extern const Font font_noto16;
extern const Font font_noto20;

extern const Font font_roboto10;
extern const Font font_roboto12;
extern const Font font_roboto16;
extern const Font font_roboto20;
