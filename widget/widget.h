#pragma once
#include <stdint.h>

typedef struct {
    uint16_t fg;
    uint16_t bg;
} WidgetColors;

typedef enum {
    W_LABEL,
    W_BTN,
    W_VALUE,    // inline editable: int or enum options
    W_PROGRESS, // read-only bar: value/min/max
    W_EDIT,     // string input
    W_RECT,
    W_IMG,
} WidgetType;

typedef uint8_t WidgetAlign;

// horizontal component (bits 0-1)
#define ALIGN_H_LEFT    0x00u
#define ALIGN_H_MID     0x01u
#define ALIGN_H_RIGHT   0x02u
#define ALIGN_H_MASK    0x03u

// vertical component (bits 2-3)
#define ALIGN_V_TOP     0x00u
#define ALIGN_V_MID     0x04u
#define ALIGN_V_BOTTOM  0x08u
#define ALIGN_V_MASK    0x0Cu

// convenience combinations
#define ALIGN_DEFAULT      0u
#define ALIGN_TOP_LEFT     (ALIGN_V_TOP    | ALIGN_H_LEFT)
#define ALIGN_TOP_MID      (ALIGN_V_TOP    | ALIGN_H_MID)
#define ALIGN_TOP_RIGHT    (ALIGN_V_TOP    | ALIGN_H_RIGHT)
#define ALIGN_MID_LEFT     (ALIGN_V_MID    | ALIGN_H_LEFT)
#define ALIGN_CENTER       (ALIGN_V_MID    | ALIGN_H_MID)
#define ALIGN_MID_RIGHT    (ALIGN_V_MID    | ALIGN_H_RIGHT)
#define ALIGN_BOTTOM_LEFT  (ALIGN_V_BOTTOM | ALIGN_H_LEFT)
#define ALIGN_BOTTOM_MID   (ALIGN_V_BOTTOM | ALIGN_H_MID)
#define ALIGN_BOTTOM_RIGHT (ALIGN_V_BOTTOM | ALIGN_H_RIGHT)

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
