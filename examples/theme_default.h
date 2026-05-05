#pragma once
#include "theme.h"

/* cppcheck-suppress misra-c2012-8.9 */
/* cppcheck-suppress misra-c2012-5.9 */
static const Theme theme_default = {
    .screen_bg      = 0x0000,
    .text_fg        = 0xFFFF,
    .widget_bg      = 0x2104,
    .border_normal  = 0xFFFF,
    .border_focused = 0x07FF,
    .border_editing = 0xFFE0,
    .border_subtle  = 0x4208,
    .cursor_fg      = 0x0000,
    .cursor_bg      = 0x07FF,
    .hint_fg        = 0x8410,
};
