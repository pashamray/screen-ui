# Widget List (ListLayout) Design

## Overview

Add a `ListLayout` type to screen-ui — a scrollable vertical menu list where rows can be
labels, separators, action buttons, or editable values. The feature slots into the existing
`render.c` focus/edit state machine with minimal disruption; all platform backends continue
to work unchanged.

## Data Model

### New file: `widget/widget_list.h`

```c
typedef enum {
    LI_LABEL,      // read-only text / section header
    LI_SEPARATOR,  // horizontal divider line
    LI_BTN,        // action button (fires on_click on activate)
    LI_VALUE,      // editable int or enum (same semantics as W_VALUE)
} ListItemType;

typedef struct {
    ListItemType       type;
    const char        *text;
    void             (*on_click)(void);
    void             (*on_change)(int value);
    int               *value;
    int                min, max, step;
    const char *const *options;
    uint8_t            options_count;
    const WidgetColors *colors;   // NULL → theme default
} ListItem;

typedef struct {
    const ListItem *items;
    uint8_t         count;
    uint8_t         row_h;   // row height in pixels; 0 = use DEFAULT_ROW_H
} ListLayout;

#define LIST_LAYOUT(...) { \
    .items = (const ListItem[]){ __VA_ARGS__ }, \
    .count = sizeof((const ListItem[]){ __VA_ARGS__ }) / sizeof(ListItem) \
}
```

`ListItem` has no `x/y/align/w/h/id/buf` — all positioning is automatic.

Selectable items: `LI_BTN` and `LI_VALUE`. `LI_LABEL` and `LI_SEPARATOR` are skipped
during focus navigation.

## render.c Integration

### New static state (alongside existing globals)

```c
static const ListLayout *current_list = NULL;
static int               scroll_top   = 0;
```

When `render_list()` is called, `current_layout` is set to NULL (and vice versa for
`render_screen()`), so the two modes are mutually exclusive.

### New public API in `render.h`

```c
void render_list(const ListLayout *list);
```

Behaviour mirrors `render_screen()`:
- Resets `edit_mode = 0`, `scroll_top = 0`
- Sets `focus_item_idx` to the first selectable item (`LI_BTN` or `LI_VALUE`)
- Calls `draw_list()` then flush

### Existing `render_focus_*` functions

All functions gain an `if (current_list)` branch. Behaviour:

| Function | List behaviour |
|---|---|
| `render_focus_next` | Move to next selectable item; adjust `scroll_top` |
| `render_focus_prev` | Move to previous selectable item; adjust `scroll_top` |
| `render_focus_inc`  | Same as current W_VALUE logic (only in `edit_mode == 1`) |
| `render_focus_dec`  | Same as current W_VALUE logic (only in `edit_mode == 1`) |
| `render_focus_activate` | `LI_BTN` → call `on_click`; `LI_VALUE` → enter `edit_mode = 1` |
| `render_cancel` | Restore `value_backup`; clear `edit_mode` |

### Scroll logic

```
visible_rows = screen_h / row_h

if focus_idx < scroll_top:
    scroll_top = focus_idx
if focus_idx >= scroll_top + visible_rows:
    scroll_top = focus_idx - visible_rows + 1
```

## Rendering

### New `Render` callbacks

```c
void (*draw_list_item)(const Render *r,
                       int16_t x, int16_t y, uint16_t w, uint16_t h,
                       const ListItem *item, const Theme *t,
                       int focused, int editing);

void (*draw_list_separator)(const Render *r,
                            int16_t x, int16_t y, uint16_t w,
                            const Theme *t);
```

A single `draw_list_item` callback covers `LI_LABEL`, `LI_BTN`, and `LI_VALUE` because all
share the same geometry (text left, value/indicator right). The `item->type` field tells the
backend what to render on the right side.

### `draw_list()` loop (inside `render.c`)

```
row_h   = list->row_h ?: DEFAULT_ROW_H   // e.g. 20 px
visible = screen_h / row_h

begin_frame()
for i in [scroll_top .. min(scroll_top + visible, count)):
    y = (i - scroll_top) * row_h
    if item[i].type == LI_SEPARATOR:
        draw_list_separator(x=0, y, w=screen_w)
    else:
        draw_list_item(x=0, y, w=screen_w, h=row_h,
                       focused = (i == focus_idx),
                       editing = (edit_mode == 1 && i == focus_idx))
flush()
```

An optional narrow scrollbar on the right edge may be drawn when `count > visible`.

## Usage Example

```c
// in layouts.c
static const ListLayout settings_list = LIST_LAYOUT(
    { .type = LI_LABEL, .text = "DISPLAY" },
    { .type = LI_VALUE, .text = "Brightness",
      .value = &brightness, .min = 10, .max = 100, .step = 10 },
    { .type = LI_VALUE, .text = "Theme",
      .value = &theme_idx, .options = theme_names, .options_count = 2,
      .on_change = apply_theme },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "NETWORK" },
    { .type = LI_BTN, .text = "WiFi",      .on_click = go_wifi },
    { .type = LI_BTN, .text = "Bluetooth", .on_click = go_bluetooth },

    { .type = LI_SEPARATOR },

    { .type = LI_BTN, .text = "Back", .on_click = go_home },
);

static void go_settings_list(void) { render_list(&settings_list); }
```

No changes required to platform input routing (`render_impl_sdl.c`,
`render_impl_console.c`).

## Files Affected

| File | Change |
|---|---|
| `widget/widget_list.h` | **New** — `ListItem`, `ListLayout`, `LIST_LAYOUT` macro |
| `render/render.h` | Add `render_list()`; add `draw_list_item`, `draw_list_separator` to `Render` struct |
| `render/render.c` | Add list mode: `current_list`, `scroll_top`, `render_list()`, `draw_list()`, list branches in all `render_focus_*` |
| `examples/render_impl_sdl.c` | Implement `draw_list_item`, `draw_list_separator` |
| `examples/render_impl_console.c` | Implement `draw_list_item`, `draw_list_separator` |
| `examples/layouts.c` / `layouts.h` | Add example `ListLayout` and navigation callback |

## Out of Scope

- Nested lists (a list item that opens another list is handled by `on_click` navigating to a new list screen — no special support needed)
- Horizontal scrolling of long item text
- Touch/pointer input
