#include "render.h"

// Include your display driver here
// #include "st7789.h"
// #include "font.h"

static void my_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // ST7789_SetWindow(x, y, x+w-1, y+h-1);
    // for (uint32_t i = 0; i < (uint32_t)w*h; i++) ST7789_WritePixel(color);
}

static void my_draw_text(int16_t x, int16_t y, const char *text, const DrawStyle *s) {
    // font_draw(x, y, text, s->fg, s->bg);
}

static void my_draw_border(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t t) {
    my_fill_rect(x,     y,     w, t, color);
    my_fill_rect(x,     y+h-t, w, t, color);
    my_fill_rect(x,     y,     t, h, color);
    my_fill_rect(x+w-t, y,     t, h, color);
}

static const Render st7789_render = {
    .fill_rect   = my_fill_rect,
    .draw_text   = my_draw_text,
    .draw_border = my_draw_border,
    .flush       = NULL,
    .screen_w    = 240,
    .screen_h    = 240,
};

void render_impl_st7789_init(void) {
    // ST7789_Init();
    render_set(&st7789_render);
}
