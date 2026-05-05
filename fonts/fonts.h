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
/* cppcheck-suppress misra-c2012-5.6 */
} Font;

extern const Font font_terminus12;
extern const Font font_terminus14;
extern const Font font_terminus16;
extern const Font font_terminus18;
extern const Font font_terminus20;
extern const Font font_terminus22;
extern const Font font_terminus24;
