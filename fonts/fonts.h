#pragma once
#include <stdint.h>

typedef struct {
    uint8_t        w;
    uint8_t        h;
    uint8_t        stride;   // bytes per glyph row
    const uint8_t *data;     // [128][h * stride]
} Font;

extern const Font font_8x8;
extern const Font font_mono16;
