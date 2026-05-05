#pragma once
#include <stdint.h>

/* cppcheck-suppress misra-c2012-5.6 */
typedef struct ListLayout ListLayout; // forward declaration (defined in widget_list.h)

typedef struct {
    uint16_t fg;
    uint16_t bg;
} WidgetColors;

typedef struct {
    uint8_t     w;    // character cell width  (used by renderer for text layout)
    uint8_t     h;    // character cell height (used by core for alignment math)
    const char *name; // renderer looks this up to select the bitmap font; NULL = default
} WidgetFont;

typedef enum {
    W_LABEL,
    W_BTN,
    W_VALUE,    // inline editable: int or enum options
    W_PROGRESS, // read-only bar: value/min/max
    W_EDIT,     // string input
    W_LIST,     // embedded scrollable list (ListLayout) — combinable with other widgets
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

// convenience combinations — suppress 2.5 (unused macro) for public API macros
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_DEFAULT      0u
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_TOP_LEFT     (ALIGN_V_TOP    | ALIGN_H_LEFT)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_TOP_MID      (ALIGN_V_TOP    | ALIGN_H_MID)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_TOP_RIGHT    (ALIGN_V_TOP    | ALIGN_H_RIGHT)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_MID_LEFT     (ALIGN_V_MID    | ALIGN_H_LEFT)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_CENTER       (ALIGN_V_MID    | ALIGN_H_MID)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_MID_RIGHT    (ALIGN_V_MID    | ALIGN_H_RIGHT)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_BOTTOM_LEFT  (ALIGN_V_BOTTOM | ALIGN_H_LEFT)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_BOTTOM_MID   (ALIGN_V_BOTTOM | ALIGN_H_MID)
/* cppcheck-suppress misra-c2012-2.5 */
#define ALIGN_BOTTOM_RIGHT (ALIGN_V_BOTTOM | ALIGN_H_RIGHT)

typedef struct {
    WidgetType   type;
    const char  *text;
    int16_t      x;
    int16_t      y;
    int16_t      w;
    int16_t      h;
    WidgetAlign  align;
    uint8_t      id;
    void       (*on_click)(void);
    void       (*on_change)(int value);  // W_VALUE: called on every value change

    // W_VALUE
    int               *value;
    int                min;
    int                max;
    int                step;
    const char *const *options;
    uint8_t            options_count;

    // W_EDIT
    char    *buf;
    uint8_t  buf_len;

    // W_LIST
    const ListLayout *list;

    const WidgetColors *colors; // NULL → theme default
    WidgetFont          font;   // zero = renderer default
} Widget;

typedef enum {
    RENDER_KEY_ACTIVATE,
    RENDER_KEY_CANCEL,
    RENDER_KEY_NEXT,
    RENDER_KEY_PREV,
    RENDER_KEY_INC,
    RENDER_KEY_DEC,
} RenderKey;

typedef struct {
    const Widget *items;
    uint8_t       count;
    int (*on_key)(RenderKey key);  // NULL = pass through; non-zero = consumed
} Layout;

#define LAYOUT(handler, ...) { \
    .items  = (const Widget[]){ __VA_ARGS__ }, \
    .count  = sizeof((const Widget[]){ __VA_ARGS__ }) / sizeof(Widget), \
    .on_key = (handler) \
}
