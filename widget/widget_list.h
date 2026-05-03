#pragma once
#include "widget.h"

typedef enum {
    LI_LABEL,      // read-only text / section header
    LI_SEPARATOR,  // horizontal divider line
    LI_BTN,        // action button (fires on_click on activate)
    LI_VALUE,      // editable int or enum (same semantics as W_VALUE)
} ListItemType;

typedef struct {
    ListItemType       type;
    const char        *text;
    void             (*on_click)(void);        // LI_BTN only
    void             (*on_change)(int value);  // LI_VALUE only
    int               *value;
    int                min, max, step;
    const char *const *options;
    uint8_t            options_count;  // must be > 0 when options != NULL
    const WidgetColors *colors;  // NULL → theme default
} ListItem;

typedef struct {
    const ListItem *items;
    uint8_t         count;
    uint8_t         row_h;  // row height in pixels; 0 = DEFAULT_ROW_H (20)
    int (*on_key)(RenderKey key);  // NULL = pass through; non-zero = consumed
} ListLayout;

#define LIST_LAYOUT(handler, ...) { \
    .items  = (const ListItem[]){ __VA_ARGS__ }, \
    .count  = (uint8_t)(sizeof((const ListItem[]){ __VA_ARGS__ }) / sizeof(ListItem)), \
    .on_key = (handler) \
}
