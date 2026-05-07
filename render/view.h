#pragma once
#include "ctx.h"
#include "widget.h"
#include "widget_list.h"

/* Platform-independent widget rendering vtable.
 * view_default provides a complete implementation built on Gfx primitives.
 * Individual callbacks may be NULL — engine skips them silently. */
typedef struct {
    void (*draw_label)         (const Ctx *ctx, int16_t x, int16_t y, const Widget *w);
    void (*draw_btn)           (const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused);
    void (*draw_value)         (const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused, int editing);
    void (*draw_progress)      (const Ctx *ctx, int16_t x, int16_t y, const Widget *w);
    void (*draw_edit)          (const Ctx *ctx, int16_t x, int16_t y, const Widget *w, int focused);
    void (*draw_list_item)     (const Ctx *ctx, int16_t x, int16_t y,
                                uint16_t row_w, uint16_t row_h,
                                const ListItem *item, int focused, int editing);
    void (*draw_list_separator)(const Ctx *ctx, int16_t x, int16_t y,
                                uint16_t row_w, uint16_t row_h);
    void (*draw_edit_overlay)  (const Ctx *ctx, const Widget *w, int cursor);
} View;

extern const View view_default;
