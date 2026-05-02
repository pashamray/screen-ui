#pragma once
#include <stdint.h>

typedef struct {
    uint16_t screen_bg;      // canvas fill
    uint16_t text_fg;        // default foreground text
    uint16_t widget_bg;      // button / value / edit background
    uint16_t border_normal;  // unfocused border
    uint16_t border_focused; // focused border
    uint16_t border_editing; // value being edited (encoder)
    uint16_t border_subtle;  // de-emphasised border (W_VALUE idle)
    uint16_t cursor_fg;      // text cursor character foreground
    uint16_t cursor_bg;      // text cursor character background
    uint16_t hint_fg;        // overlay hint text
} Theme;
