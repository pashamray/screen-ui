#pragma once
#include <stdint.h>

typedef struct {
    uint16_t fg;
    uint16_t bg;
} WidgetColors;

typedef enum {
    W_LABEL,
    W_BTN,
    W_VALUE,  // inline editable: int or enum options
    W_EDIT,   // string input
    W_RECT,
    W_IMG,
} WidgetType;

typedef enum {
    ALIGN_DEFAULT,
    ALIGN_CENTER,
    ALIGN_TOP_LEFT,
    ALIGN_TOP_MID,
    ALIGN_TOP_RIGHT,
    ALIGN_BOTTOM_LEFT,
    ALIGN_BOTTOM_MID,
    ALIGN_BOTTOM_RIGHT,
} WidgetAlign;

typedef struct {
    WidgetType   type;
    const char  *text;
    int16_t      x, y;
    int16_t      w, h;
    WidgetAlign  align;
    uint8_t      id;
    void       (*on_click)(void);
    void       (*on_change)(int value);  // W_VALUE: called on every value change

    // W_VALUE
    int               *value;
    int                min, max, step;
    const char *const *options;
    uint8_t            options_count;

    // W_EDIT
    char    *buf;
    uint8_t  buf_len;

    const WidgetColors *colors; // NULL → theme default
} Widget;

typedef struct {
    const Widget *items;
    uint8_t       count;
} Layout;

#define LAYOUT(...) { \
    .items = (const Widget[]){ __VA_ARGS__ }, \
    .count = sizeof((const Widget[]){ __VA_ARGS__ }) / sizeof(Widget) \
}
