# Widget List (ListLayout) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a scrollable `ListLayout` type to screen-ui — a vertical menu list where rows are labels, separators, buttons, or editable values, fully integrated with the existing `render.c` focus/edit state machine.

**Architecture:** `ListLayout` is a static array of `ListItem` (no x/y geometry — rows auto-stack). `render_list()` mirrors `render_screen()` and slots into the same global state in `render.c` (`focus_item_idx`, `edit_mode`, `value_backup`). All existing `render_focus_*` functions gain a `current_list` branch. Platform backends implement two new `Render` callbacks: `draw_list_item` and `draw_list_separator`.

**Tech Stack:** C11, CMake, SDL2 (visual testing), ANSI console (secondary backend). No test framework — verification is `cmake --build` succeeding plus manual visual run.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `widget/widget_list.h` | **Create** | `ListItemType`, `ListItem`, `ListLayout`, `LIST_LAYOUT` macro |
| `render/render.h` | **Modify** | Add `render_list()` decl; add `draw_list_item`, `draw_list_separator` to `Render` struct |
| `render/render.c` | **Modify** | List-mode state, `draw_list()`, `render_list()`, list branches in all `render_focus_*` |
| `examples/render_impl_sdl.c` | **Modify** | Implement `sdl_draw_list_item`, `sdl_draw_list_separator`; register in `Render` |
| `examples/render_impl_console.c` | **Modify** | Implement `console_draw_list_item`, `console_draw_list_separator`; register in `Render` |
| `examples/layouts.h` | **Modify** | Add `#include "widget_list.h"`; declare `demo_list` |
| `examples/layouts.c` | **Modify** | Define `demo_list`; add "List Demo" button in `system_layout` |

---

### Task 1: Create `widget/widget_list.h`

**Files:**
- Create: `widget/widget_list.h`

- [ ] **Step 1: Write the header**

```c
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
    void             (*on_click)(void);
    void             (*on_change)(int value);
    int               *value;
    int                min, max, step;
    const char *const *options;
    uint8_t            options_count;
    const WidgetColors *colors;  // NULL → theme default
} ListItem;

typedef struct {
    const ListItem *items;
    uint8_t         count;
    uint8_t         row_h;  // row height in pixels; 0 = DEFAULT_ROW_H (20)
} ListLayout;

#define LIST_LAYOUT(...) { \
    .items = (const ListItem[]){ __VA_ARGS__ }, \
    .count = (uint8_t)(sizeof((const ListItem[]){ __VA_ARGS__ }) / sizeof(ListItem)) \
}
```

- [ ] **Step 2: Verify it compiles in isolation**

```bash
cd /home/ps/projects/screen-ui
cmake --build build 2>&1 | head -30
```

Expected: build succeeds (no new source files, just a header).

- [ ] **Step 3: Commit**

```bash
git add widget/widget_list.h
git commit -m "feat: add widget_list.h — ListItem, ListLayout, LIST_LAYOUT"
```

---

### Task 2: Extend `render/render.h`

**Files:**
- Modify: `render/render.h`

- [ ] **Step 1: Add `#include "widget_list.h"` and the two new `Render` callbacks**

In `render/render.h`, add the include after the existing `#include "widget.h"`:

```c
#include "widget_list.h"
```

Add the two new function pointers to the `Render` struct, after `draw_edit`:

```c
    void (*draw_list_item)     (const Render *r,
                                int16_t x, int16_t y, uint16_t w, uint16_t h,
                                const ListItem *item, const Theme *t,
                                int focused, int editing);
    void (*draw_list_separator)(const Render *r,
                                int16_t x, int16_t y, uint16_t w,
                                const Theme *t);
```

Add the new public function declaration after `render_refresh`:

```c
void  render_list(const ListLayout *list);
```

- [ ] **Step 2: Build to verify the header change is clean**

```bash
cmake --build build 2>&1 | head -30
```

Expected: build succeeds (implementations not yet written, but no uses yet).

- [ ] **Step 3: Commit**

```bash
git add render/render.h
git commit -m "feat: add render_list() and list draw callbacks to Render struct"
```

---

### Task 3: Implement list infrastructure in `render/render.c`

This task adds `current_list`, `scroll_top`, `draw_list()`, `render_list()`, and updates `render_screen()` and `render_refresh()`.

**Files:**
- Modify: `render/render.c`

- [ ] **Step 1: Add list-mode state variables**

After the existing static variables at the top of `render/render.c` (after `static const Theme *T = NULL;`), add:

```c
static const ListLayout *current_list = NULL;
static int               scroll_top   = 0;

#define DEFAULT_ROW_H 20
```

- [ ] **Step 2: Add `list_is_selectable()` helper**

Add after `is_selectable()`:

```c
static int list_is_selectable(const ListItem *item) {
    return item->type == LI_BTN || item->type == LI_VALUE;
}
```

- [ ] **Step 3: Add `draw_list()` static function**

Add after `draw_layout()`:

```c
static void draw_list(const ListLayout *list) {
    if (R->begin_frame) R->begin_frame(R, T);

    uint8_t row_h   = list->row_h ? list->row_h : DEFAULT_ROW_H;
    int     visible = (int)(R->screen_h / row_h);
    int     end     = scroll_top + visible;
    if (end > (int)list->count) end = (int)list->count;

    for (int i = scroll_top; i < end; i++) {
        const ListItem *item    = &list->items[i];
        int16_t         y       = (int16_t)((i - scroll_top) * row_h);
        int             focused = (i == focus_item_idx);
        int             editing = focused && (edit_mode == 1);

        if (item->type == LI_SEPARATOR) {
            if (R->draw_list_separator)
                R->draw_list_separator(R, 0, y, R->screen_w, T);
        } else {
            if (R->draw_list_item)
                R->draw_list_item(R, 0, y, R->screen_w, row_h,
                                  item, T, focused, editing);
        }
    }
    if (R->flush) R->flush();
}
```

- [ ] **Step 4: Add `render_list()` public function**

Add after `render_refresh()`:

```c
void render_list(const ListLayout *list) {
    current_layout = NULL;
    current_list   = list;
    edit_mode      = 0;
    scroll_top     = 0;
    focus_item_idx = -1;
    for (int i = 0; i < (int)list->count; i++) {
        if (list_is_selectable(&list->items[i])) { focus_item_idx = i; break; }
    }
    draw_list(list);
}
```

- [ ] **Step 5: Update `render_screen()` to clear `current_list`**

In the existing `render_screen()` function, add `current_list = NULL;` as the very first line:

```c
void render_screen(const Layout *layout) {
    current_list = NULL;          // ← add this line
    edit_mode = 0;
    current_layout = layout;
    focus_item_idx = -1;
    for (int i = 0; i < (int)layout->count; i++) {
        if (is_selectable(&layout->items[i])) { focus_item_idx = i; break; }
    }
    draw_layout(layout);
}
```

- [ ] **Step 6: Update `render_refresh()` to handle list mode**

Replace the existing `render_refresh()` with:

```c
void render_refresh(void) {
    if (current_list)        draw_list(current_list);
    else if (current_layout) draw_layout(current_layout);
}
```

- [ ] **Step 7: Build to verify**

```bash
cmake --build build 2>&1 | head -30
```

Expected: build succeeds.

- [ ] **Step 8: Commit**

```bash
git add render/render.c
git commit -m "feat: add draw_list, render_list, list-mode state to render.c"
```

---

### Task 4: Add list focus navigation to `render/render.c`

All `render_focus_*` functions gain a `if (current_list)` branch at the top.

**Files:**
- Modify: `render/render.c`

- [ ] **Step 1: Add scroll-adjust helper macro**

Add a `static inline` helper just before `render_focus_next`:

```c
static void list_scroll_adjust(uint8_t row_h) {
    int visible = (int)(R->screen_h / (row_h ? row_h : DEFAULT_ROW_H));
    if (focus_item_idx < scroll_top)
        scroll_top = focus_item_idx;
    if (focus_item_idx >= scroll_top + visible)
        scroll_top = focus_item_idx - visible + 1;
}
```

- [ ] **Step 2: Update `render_focus_next()`**

Add at the very top of `render_focus_next()`, before the existing `if (!current_layout) return;`:

```c
    if (current_list) {
        if (edit_mode == 1) return;
        int n = (int)current_list->count;
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx + i) % n;
            if (list_is_selectable(&current_list->items[idx])) {
                focus_item_idx = idx;
                list_scroll_adjust(current_list->row_h);
                return;
            }
        }
        return;
    }
```

- [ ] **Step 3: Update `render_focus_prev()`**

Add at the very top of `render_focus_prev()`, before the existing `if (!current_layout) return;`:

```c
    if (current_list) {
        if (edit_mode == 1) return;
        int n = (int)current_list->count;
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx - i + n) % n;
            if (list_is_selectable(&current_list->items[idx])) {
                focus_item_idx = idx;
                list_scroll_adjust(current_list->row_h);
                return;
            }
        }
        return;
    }
```

- [ ] **Step 4: Update `render_focus_inc()`**

Add at the very top of `render_focus_inc()`, before the existing `if (!current_layout || focus_item_idx < 0) return;`:

```c
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1 && item->type == LI_VALUE && item->value) {
            if (item->options) {
                *item->value = (*item->value + 1) % item->options_count;
            } else {
                int s = item->step ? item->step : 1;
                int v = *item->value + s;
                *item->value = (v > item->max) ? item->max : v;
            }
            if (item->on_change) item->on_change(*item->value);
            render_refresh();
        }
        return;
    }
```

- [ ] **Step 5: Update `render_focus_dec()`**

Add at the very top of `render_focus_dec()`, before the existing `if (!current_layout || focus_item_idx < 0) return;`:

```c
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1 && item->type == LI_VALUE && item->value) {
            if (item->options) {
                *item->value = (*item->value - 1 + item->options_count) % item->options_count;
            } else {
                int s = item->step ? item->step : 1;
                int v = *item->value - s;
                *item->value = (v < item->min) ? item->min : v;
            }
            if (item->on_change) item->on_change(*item->value);
            render_refresh();
        }
        return;
    }
```

- [ ] **Step 6: Update `render_focus_activate()`**

Add at the very top of `render_focus_activate()`, before the existing `if (!current_layout || focus_item_idx < 0) return;`:

```c
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1) {
            edit_mode = 0;
            render_refresh();
            return;
        }
        if (item->type == LI_VALUE && item->value) {
            value_backup = *item->value;
            edit_mode    = 1;
            render_refresh();
            return;
        }
        if (item->type == LI_BTN && item->on_click) {
            item->on_click();
        }
        return;
    }
```

- [ ] **Step 7: Update `render_cancel()`**

Add at the very top of `render_cancel()`, before the existing `if (!current_layout || focus_item_idx < 0) return;`:

```c
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1 && item->value) {
            *item->value = value_backup;
            if (item->on_change) item->on_change(*item->value);
        }
        edit_mode = 0;
        render_refresh();
        return;
    }
```

- [ ] **Step 8: Build to verify**

```bash
cmake --build build 2>&1 | head -40
```

Expected: build succeeds with no warnings.

- [ ] **Step 9: Commit**

```bash
git add render/render.c
git commit -m "feat: add list-mode branches to all render_focus_* functions"
```

---

### Task 5: Implement list callbacks in `render_impl_sdl.c`

**Files:**
- Modify: `examples/render_impl_sdl.c`

- [ ] **Step 1: Add `sdl_draw_list_item()`**

Add after `menu_draw_value()`, before the lifecycle section:

```c
static void sdl_draw_list_item(const Render *r,
                                int16_t x, int16_t y, uint16_t w, uint16_t h,
                                const ListItem *item, const Theme *t,
                                int focused, int editing)
{
    (void)r;
    uint16_t fg     = item->colors ? item->colors->fg : t->text_fg;
    uint16_t bg     = item->colors ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = focused ? t->widget_bg : bg;
    int16_t  cw     = (int16_t)font_8x8.w;
    int16_t  ch     = (int16_t)font_8x8.h;
    int16_t  ty     = (int16_t)(y + ((int16_t)h - ch) / 2);
    int16_t  pad    = 8;

    my_fill_rect(x, y, (int16_t)w, (int16_t)h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = t->hint_fg, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        return;
    }

    uint16_t brd = (item->type == LI_VALUE && editing) ? t->border_editing :
                   focused                              ? t->border_focused :
                                                         t->border_subtle;
    my_draw_border(x, y, (int16_t)w, (int16_t)h, brd, 1);
    DrawStyle s = { .fg = fg, .bg = row_bg };

    if (item->type == LI_BTN) {
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        my_draw_text((int16_t)(x + (int16_t)w - 2 * cw), ty, ">", &s);
        return;
    }

    /* LI_VALUE */
    my_draw_text((int16_t)(x + pad), ty, item->text, &s);

    char val_str[24];
    if (item->options && item->value)
        snprintf(val_str, sizeof(val_str), "%s", item->options[*item->value]);
    else if (item->value)
        snprintf(val_str, sizeof(val_str), "%d", *item->value);
    else
        snprintf(val_str, sizeof(val_str), "--");

    int16_t vlen = (int16_t)strlen(val_str);
    int16_t vx   = (int16_t)(x + (int16_t)w - (vlen + 2) * cw);
    my_draw_text(vx, ty, val_str, &s);

    if (editing) {
        DrawStyle sa = { .fg = t->border_editing, .bg = row_bg };
        my_draw_text((int16_t)(vx - cw), ty, "<", &sa);
        my_draw_text((int16_t)(x + (int16_t)w - cw), ty, ">", &sa);
    }
}
```

- [ ] **Step 2: Add `sdl_draw_list_separator()`**

Add immediately after `sdl_draw_list_item()`:

```c
static void sdl_draw_list_separator(const Render *r,
                                     int16_t x, int16_t y, uint16_t w,
                                     const Theme *t)
{
    (void)r;
    my_fill_rect(x, y, (int16_t)w, 1, t->border_subtle);
}
```

- [ ] **Step 3: Register the new callbacks in `render_impl_sdl_init()`**

In the `static Render r = (Render){ ... };` initialiser inside `render_impl_sdl_init()`, add the two new fields:

```c
        .draw_list_item      = sdl_draw_list_item,
        .draw_list_separator = sdl_draw_list_separator,
```

The full initialiser should now look like:

```c
    static Render r;
    r = (Render){
        .begin_frame         = menu_begin_frame,
        .flush               = my_flush,
        .draw_edit_overlay   = sdl_draw_edit_overlay,
        .screen_w            = (uint16_t)screen_w,
        .screen_h            = (uint16_t)screen_h,
        .draw_label          = menu_draw_label,
        .draw_btn            = menu_draw_btn,
        .draw_value          = menu_draw_value,
        .draw_progress       = menu_draw_progress,
        .draw_edit           = menu_draw_edit,
        .draw_list_item      = sdl_draw_list_item,
        .draw_list_separator = sdl_draw_list_separator,
    };
```

- [ ] **Step 4: Build**

```bash
cmake --build build 2>&1 | head -40
```

Expected: build succeeds with no warnings.

- [ ] **Step 5: Commit**

```bash
git add examples/render_impl_sdl.c
git commit -m "feat: implement draw_list_item and draw_list_separator for SDL backend"
```

---

### Task 6: Implement list callbacks in `render_impl_console.c`

**Files:**
- Modify: `examples/render_impl_console.c`

- [ ] **Step 1: Add `console_draw_list_item()`**

Add after `console_draw_edit()`, before the `static const Render render_impl_console` definition:

```c
static void console_draw_list_item(const Render *r,
                                    int16_t x, int16_t y, uint16_t w, uint16_t h,
                                    const ListItem *item, const Theme *t,
                                    int focused, int editing)
{
    (void)r;
    uint16_t fg     = item->colors ? item->colors->fg : t->text_fg;
    uint16_t bg     = item->colors ? item->colors->bg : t->screen_bg;
    uint16_t row_bg = focused ? t->widget_bg : bg;
    int      cw     = LOG_W / COLS;
    int      ch     = LOG_H / ROWS;
    int16_t  ty     = (int16_t)(y + ((int16_t)h - ch) / 2);
    int16_t  pad    = 8;

    my_fill_rect(x, y, (int16_t)w, (int16_t)h, row_bg);

    if (item->type == LI_LABEL) {
        DrawStyle s = { .fg = t->hint_fg, .bg = row_bg };
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        return;
    }

    uint16_t brd = (item->type == LI_VALUE && editing) ? t->border_editing :
                   focused                              ? t->border_focused :
                                                         t->border_subtle;
    my_draw_border(x, y, (int16_t)w, (int16_t)h, brd, 1);
    DrawStyle s = { .fg = fg, .bg = row_bg };

    if (item->type == LI_BTN) {
        my_draw_text((int16_t)(x + pad), ty, item->text, &s);
        my_draw_text((int16_t)(x + (int16_t)w - 2 * cw), ty, ">", &s);
        return;
    }

    /* LI_VALUE */
    my_draw_text((int16_t)(x + pad), ty, item->text, &s);

    char val_str[24];
    if (item->options && item->value)
        snprintf(val_str, sizeof(val_str), "%s", item->options[*item->value]);
    else if (item->value)
        snprintf(val_str, sizeof(val_str), "%d", *item->value);
    else
        snprintf(val_str, sizeof(val_str), "--");

    int16_t vlen = (int16_t)strlen(val_str);
    int16_t vx   = (int16_t)(x + (int16_t)w - (vlen + 2) * cw);
    my_draw_text(vx, ty, val_str, &s);

    if (editing) {
        DrawStyle sa = { .fg = t->border_editing, .bg = row_bg };
        my_draw_text((int16_t)(vx - cw), ty, "<", &sa);
        my_draw_text((int16_t)(x + (int16_t)w - cw), ty, ">", &sa);
    }
}
```

- [ ] **Step 2: Add `console_draw_list_separator()`**

Add immediately after `console_draw_list_item()`:

```c
static void console_draw_list_separator(const Render *r,
                                         int16_t x, int16_t y, uint16_t w,
                                         const Theme *t)
{
    (void)r;
    my_fill_rect(x, y, (int16_t)w, 1, t->border_subtle);
}
```

- [ ] **Step 3: Register in `render_impl_console`**

In the `static const Render render_impl_console` struct, add the two new fields:

```c
static const Render render_impl_console = {
    .begin_frame         = console_begin_frame,
    .flush               = my_flush,
    .screen_w            = LOG_W,
    .screen_h            = LOG_H,
    .draw_label          = console_draw_label,
    .draw_btn            = console_draw_btn,
    .draw_value          = console_draw_value,
    .draw_progress       = console_draw_progress,
    .draw_edit           = console_draw_edit,
    .draw_list_item      = console_draw_list_item,
    .draw_list_separator = console_draw_list_separator,
};
```

- [ ] **Step 4: Build**

```bash
cmake --build build 2>&1 | head -40
```

Expected: build succeeds with no warnings.

- [ ] **Step 5: Commit**

```bash
git add examples/render_impl_console.c
git commit -m "feat: implement draw_list_item and draw_list_separator for console backend"
```

---

### Task 7: Add demo `ListLayout` to `layouts.c` / `layouts.h`

**Files:**
- Modify: `examples/layouts.h`
- Modify: `examples/layouts.c`

- [ ] **Step 1: Update `layouts.h`**

Add `#include "widget_list.h"` after the existing `#include "widget.h"`, and add the extern declaration:

```c
#pragma once
#include "widget.h"
#include "widget_list.h"

void layouts_tick(void);

extern const Layout home_layout;
extern const Layout settings_layout;
extern const Layout display_layout;
extern const Layout network_layout;
extern const Layout wifi_layout;
extern const Layout bluetooth_layout;
extern const Layout system_layout;
extern const Layout about_layout;
extern const Layout reset_layout;
extern const Layout border_demo_layout;

extern const ListLayout demo_list;
```

- [ ] **Step 2: Add forward declaration in `layouts.c`**

At the top of `layouts.c`, in the "forward declarations" section add:

```c
static void go_demo_list(void);
```

- [ ] **Step 3: Define `demo_list` in `layouts.c`**

Add after `border_demo_layout` and before the `layouts_tick` function:

```c
// ── Demo list ────────────────────────────────────────────────────────────────

const ListLayout demo_list = LIST_LAYOUT(
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

    { .type = LI_LABEL, .text = "SYSTEM" },
    { .type = LI_BTN, .text = "About",  .on_click = go_about },
    { .type = LI_BTN, .text = "Reset",  .on_click = go_reset },

    { .type = LI_SEPARATOR },

    { .type = LI_BTN, .text = "Back", .on_click = go_home },
);
```

- [ ] **Step 4: Add `go_demo_list` callback**

In the callbacks section at the bottom of `layouts.c`, add:

```c
static void go_demo_list(void) { render_list(&demo_list); }
```

- [ ] **Step 5: Add "List Demo" button to `system_layout`**

Replace the existing `system_layout` definition with one that includes a "List Demo" button. Insert it between "Border Demo" and "Reset":

```c
const Layout system_layout = LAYOUT(
    { .type  = W_LABEL,
      .text  = "SYSTEM",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type     = W_BTN,
      .text     = "About",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -64,
      .on_click = go_about },

    { .type     = W_BTN,
      .text     = "Border Demo",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -32,
      .on_click = go_border_demo },

    { .type     = W_BTN,
      .text     = "List Demo",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 0,
      .on_click = go_demo_list },

    { .type     = W_BTN,
      .text     = "Reset",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 32,
      .on_click = go_reset },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 64,
      .on_click = go_settings },
);
```

- [ ] **Step 6: Add `render_list` include — verify `layouts.c` can see it**

`layouts.c` already includes `"layouts.h"` which includes `"widget_list.h"`. It also includes `"render.h"` for `render_list()`. Verify the top of `layouts.c` has both:

```c
#include "layouts.h"
#include "render.h"
```

No changes needed if both are already present (they are).

- [ ] **Step 7: Build**

```bash
cmake --build build 2>&1 | head -40
```

Expected: build succeeds with no warnings.

- [ ] **Step 8: Run SDL demo and verify visually**

```bash
./build/examples/screen-ui-sdl
```

Navigate to: Home → Settings → System → "List Demo".

Expected checklist:
- [ ] List renders all rows with correct types (dimmer labels, bordered buttons/values)
- [ ] Arrow keys (Up/Down) move focus through selectable items, skipping labels and separators
- [ ] Scroll works: focus never leaves the visible area
- [ ] Enter on a `LI_BTN` fires `on_click` (e.g. "WiFi" navigates to wifi_layout; "Back" returns home)
- [ ] Enter on a `LI_VALUE` enters edit mode (border changes color)
- [ ] In edit mode: Left/Right changes the value; Enter confirms; Escape cancels and restores backup
- [ ] Theme toggle (LI_VALUE with `on_change`) immediately changes the screen theme

- [ ] **Step 9: Run console demo and verify**

```bash
./build/examples/screen-ui-console
```

Navigate to the list demo using `j/k`. Verify the same checklist above applies.

- [ ] **Step 10: Commit**

```bash
git add examples/layouts.h examples/layouts.c
git commit -m "feat: add demo_list ListLayout and List Demo button in system screen"
```
