#include "render.h"
#include <stddef.h>
#include <string.h>

static const Render  *R;
static const Layout  *current_layout = NULL;
static int            focus_item_idx = -1;

// 0 = normal, 1 = value edit (W_VALUE), 2 = string edit (W_EDIT)
static int  edit_mode   = 0;
static int  edit_cursor = 0;
static char edit_backup[64];
static int  value_backup = 0;

static const Theme *T = NULL;

static const ListLayout *current_list = NULL;
static int               scroll_top   = 0;

#define DEFAULT_ROW_H 20

void render_set(const Render *r)      { R = r; }
void render_set_theme(const Theme *t) { T = t; }

// ── helpers ───────────────────────────────────────────────────────────────────

static int is_selectable(const Widget *w) {
    return w->on_click != NULL || w->type == W_VALUE || w->type == W_EDIT;
}

static int list_is_selectable(const ListItem *item) {
    return item->type == LI_BTN || item->type == LI_VALUE;
}

static int dispatch_key(RenderKey key) {
    if (current_list)   return current_list->on_key   ? current_list->on_key(key)   : 0;
    if (current_layout) return current_layout->on_key ? current_layout->on_key(key) : 0;
    return 0;
}

static void resolve(const Widget *w, int16_t *ox, int16_t *oy) {
    int16_t x  = w->x, y  = w->y;
    int16_t sw = R->screen_w, sh = R->screen_h;
    int16_t ww = w->w,        wh = w->h;

    switch (w->align & ALIGN_H_MASK) {
        case ALIGN_H_MID:   x += sw/2 - ww/2; break;
        case ALIGN_H_RIGHT: x += sw - ww;      break;
        default: break;
    }
    switch (w->align & ALIGN_V_MASK) {
        case ALIGN_V_MID:    y += sh/2 - wh/2; break;
        case ALIGN_V_BOTTOM: y += sh - wh;      break;
        default: break;
    }
    *ox = x; *oy = y;
}

// ── draw ─────────────────────────────────────────────────────────────────────

static void draw_layout(const Layout *layout) {
    if (edit_mode == 2 && focus_item_idx >= 0 &&
        layout->items[focus_item_idx].type == W_EDIT) {
        if (R->draw_edit_overlay)
            R->draw_edit_overlay(R, &layout->items[focus_item_idx], T, edit_cursor);
        if (R->flush) R->flush();
        return;
    }

    if (R->begin_frame) R->begin_frame(R, T);

    for (uint8_t i = 0; i < layout->count; i++) {
        const Widget *w = &layout->items[i];
        int16_t x, y;
        resolve(w, &x, &y);
        int focused  = (focus_item_idx == (int)i);
        int val_edit = focused && (edit_mode == 1);
        switch (w->type) {
            case W_LABEL:    if (R->draw_label)    R->draw_label(R, x, y, w, T);                    break;
            case W_BTN:      if (R->draw_btn)      R->draw_btn(R, x, y, w, T, focused);             break;
            case W_VALUE:    if (R->draw_value)    R->draw_value(R, x, y, w, T, focused, val_edit); break;
            case W_PROGRESS: if (R->draw_progress) R->draw_progress(R, x, y, w, T);                 break;
            case W_EDIT:     if (R->draw_edit)     R->draw_edit(R, x, y, w, T, focused);            break;
            default: break;
        }
    }
    if (R->flush) R->flush();
}

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
                R->draw_list_separator(R, 0, y, R->screen_w, row_h, T);
        } else {
            if (R->draw_list_item)
                R->draw_list_item(R, 0, y, R->screen_w, row_h,
                                  item, T, focused, editing);
        }
    }
    if (R->flush) R->flush();
}

// ── API ───────────────────────────────────────────────────────────────────────

void render_screen(const Layout *layout) {
    current_list = NULL;
    edit_mode = 0;
    current_layout = layout;
    focus_item_idx = -1;
    for (int i = 0; i < (int)layout->count; i++) {
        if (is_selectable(&layout->items[i])) { focus_item_idx = i; break; }
    }
    draw_layout(layout);
}

void render_refresh(void) {
    if (current_list)        draw_list(current_list);
    else if (current_layout) draw_layout(current_layout);
}

void render_list(const ListLayout *list) {
    if (!list) return;
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

static void list_scroll_adjust(uint8_t row_h) {
    int visible = (int)(R->screen_h / (row_h ? row_h : DEFAULT_ROW_H));
    if (focus_item_idx < scroll_top)
        scroll_top = focus_item_idx;
    if (focus_item_idx >= scroll_top + visible)
        scroll_top = focus_item_idx - visible + 1;
}

void render_focus_next(void) {
    if (current_list) {
        if (edit_mode == 1) return;
        if (focus_item_idx < 0) return;
        if (dispatch_key(RENDER_KEY_NEXT)) return;
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
    if (!current_layout) return;
    if (edit_mode == 1) return;
    if (edit_mode == 2) {          // in string-edit — move cursor right
        const Widget *w = &current_layout->items[focus_item_idx];
        if (w->buf && edit_cursor < (int)strlen(w->buf))
            { edit_cursor++; render_refresh(); }
        return;
    }
    if (dispatch_key(RENDER_KEY_NEXT)) return;
    int n = (int)current_layout->count;
    for (int i = 1; i <= n; i++) {
        int idx = (focus_item_idx + i) % n;
        if (is_selectable(&current_layout->items[idx]))
            { focus_item_idx = idx; return; }
    }
}

void render_focus_prev(void) {
    if (current_list) {
        if (edit_mode == 1) return;
        if (focus_item_idx < 0) return;
        if (dispatch_key(RENDER_KEY_PREV)) return;
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
    if (!current_layout) return;
    if (edit_mode == 1) return;
    if (edit_mode == 2) {
        if (edit_cursor > 0) { edit_cursor--; render_refresh(); }
        return;
    }
    if (dispatch_key(RENDER_KEY_PREV)) return;
    int n = (int)current_layout->count;
    for (int i = 1; i <= n; i++) {
        int idx = (focus_item_idx - i + n) % n;
        if (is_selectable(&current_layout->items[idx]))
            { focus_item_idx = idx; return; }
    }
}

void render_focus_inc(void) {
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
        } else if (edit_mode == 0) {
            dispatch_key(RENDER_KEY_INC);
        }
        return;
    }
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];

    if (edit_mode == 2) {   // string edit: cycle character up
        if (w->buf && edit_cursor < (int)strlen(w->buf)) {
            char c = w->buf[edit_cursor];
            w->buf[edit_cursor] = (c < '~') ? c + 1 : ' ';
            render_refresh();
        }
        return;
    }
    if (edit_mode == 1 && w->type == W_VALUE && w->value) {
        if (w->options) {
            *w->value = (*w->value + 1) % w->options_count;
        } else {
            int s = w->step ? w->step : 1;
            int v = *w->value + s;
            *w->value = (v > w->max) ? w->max : v;
        }
        if (w->on_change) w->on_change(*w->value);
        render_refresh();
        return;
    }
    dispatch_key(RENDER_KEY_INC);
}

void render_focus_dec(void) {
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
        } else if (edit_mode == 0) {
            dispatch_key(RENDER_KEY_DEC);
        }
        return;
    }
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];

    if (edit_mode == 2) {
        if (w->buf && edit_cursor < (int)strlen(w->buf)) {
            char c = w->buf[edit_cursor];
            w->buf[edit_cursor] = (c > ' ') ? c - 1 : '~';
            render_refresh();
        }
        return;
    }
    if (edit_mode == 1 && w->type == W_VALUE && w->value) {
        if (w->options) {
            *w->value = (*w->value - 1 + w->options_count) % w->options_count;
        } else {
            int s = w->step ? w->step : 1;
            int v = *w->value - s;
            *w->value = (v < w->min) ? w->min : v;
        }
        if (w->on_change) w->on_change(*w->value);
        render_refresh();
        return;
    }
    dispatch_key(RENDER_KEY_DEC);
}

void render_focus_activate(void) {
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1) {
            edit_mode = 0;
            render_refresh();
            return;
        }
        if (dispatch_key(RENDER_KEY_ACTIVATE)) return;
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
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];

    if (edit_mode == 1 || edit_mode == 2) {
        edit_mode = 0;
        render_refresh();
        return;
    }
    if (dispatch_key(RENDER_KEY_ACTIVATE)) return;
    if (w->type == W_VALUE && w->value) {
        value_backup = *w->value;
        edit_mode = 1;
        render_refresh();
        return;
    }
    if (w->type == W_EDIT) {
        edit_mode = 2;
        edit_cursor = w->buf ? (int)strlen(w->buf) : 0;
        if (w->buf) strncpy(edit_backup, w->buf, sizeof(edit_backup) - 1);
        render_refresh();
        return;
    }
    if (w->on_click) w->on_click();
}

int render_in_value_edit(void) { return edit_mode == 1; }
int render_in_edit_mode(void)  { return edit_mode == 2; }

void render_cancel(void) {
    if (current_list) {
        if (focus_item_idx < 0) return;
        const ListItem *item = &current_list->items[focus_item_idx];
        if (edit_mode == 1 && item->value) {
            *item->value = value_backup;
            if (item->on_change) item->on_change(*item->value);
            edit_mode = 0;
            render_refresh();
        } else if (edit_mode == 0) {
            dispatch_key(RENDER_KEY_CANCEL);
        }
        return;
    }
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];
    if (edit_mode == 0) {
        dispatch_key(RENDER_KEY_CANCEL);
        return;
    }
    if (edit_mode == 1 && w->value) {
        *w->value = value_backup;
        if (w->on_change) w->on_change(*w->value);
    }
    if (edit_mode == 2 && w->buf) {
        strncpy(w->buf, edit_backup, w->buf_len - 1);
        w->buf[w->buf_len - 1] = '\0';
    }
    edit_mode = 0;
    render_refresh();
}

void render_edit_char(char c) {
    if (edit_mode != 2 || !current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];
    if (!w->buf) return;
    int len = (int)strlen(w->buf);
    if (len >= w->buf_len - 1) return;
    memmove(w->buf + edit_cursor + 1, w->buf + edit_cursor,
            (size_t)(len - edit_cursor + 1));
    w->buf[edit_cursor] = c;
    edit_cursor++;
    render_refresh();
}

void render_edit_delete(void) {
    if (edit_mode != 2 || !current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];
    if (!w->buf || edit_cursor == 0) return;
    int len = (int)strlen(w->buf);
    memmove(w->buf + edit_cursor - 1, w->buf + edit_cursor,
            (size_t)(len - edit_cursor + 1));
    edit_cursor--;
    render_refresh();
}

WidgetType render_focused_type(void) {
    if (!current_layout || focus_item_idx < 0) return W_LABEL;
    return current_layout->items[focus_item_idx].type;
}

void render_edit_set(const char *str) {
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];
    if (w->type != W_EDIT || !w->buf) return;
    strncpy(w->buf, str, w->buf_len - 1);
    w->buf[w->buf_len - 1] = '\0';
    edit_cursor = (int)strlen(w->buf);
}
