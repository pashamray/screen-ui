#pragma once
#include <stdint.h>

typedef struct Font {
    uint8_t        w;
    uint8_t        h;
    uint8_t        stride;   // bytes per glyph row
    const uint8_t *data;     // [128][h * stride]
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
