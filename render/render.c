#include "render.h"
#include <stddef.h>
#include <string.h>

static const Render  *R;
static const Layout  *current_layout = NULL;
static int            focus_item_idx = -1;

/* 0 = normal, 1 = value edit (W_VALUE / LI_VALUE inside W_LIST), 2 = string edit (W_EDIT) */
static int  edit_mode   = 0;
static int  edit_cursor = 0;
static char edit_backup[64];
static int  value_backup = 0;

static const Theme *T = NULL;
static int dirty = 0;

static const ListLayout *current_list = NULL;
static int               scroll_top   = 0;

/* ── W_LIST embedded state ───────────────────────────────────────────────── */
static int list_item_focus  = -1;
static int list_item_scroll = 0;

#define DEFAULT_ROW_H 20
#define HISTORY_MAX   16

typedef struct {
    const Layout     *layout;
    const ListLayout *list;
    int               focus_idx;
    int               scroll;
    int               list_item_focus;
    int               list_item_scroll;
} HistoryEntry;

static HistoryEntry history[HISTORY_MAX];
static int          history_top = 0;

void render_set(const Render *r)      { R = r; }
void render_set_theme(const Theme *t) { T = t; }

/* ── context helpers ────────────────────────────────────────────────────── */

static Ctx screen_ctx(void) {
    Ctx c;
    c.r  = R;
    c.t  = T;
    c.ox = 0;
    c.oy = 0;
    c.cw = (int16_t)R->screen_w;
    c.ch = (int16_t)R->screen_h;
    return c;
}

static Ctx sub_ctx(int16_t ox, int16_t oy, int16_t cw, int16_t ch) {
    Ctx c;
    c.r  = R;
    c.t  = T;
    c.ox = ox;
    c.oy = oy;
    c.cw = cw;
    c.ch = ch;
    return c;
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static int list_is_selectable(const ListItem *item) {
    /* cppcheck-suppress misra-c2012-12.1 */
    return ((item->type == LI_BTN)
        || (item->type == LI_VALUE)
        || (item->type == LI_SUBMENU)
        || (item->type == LI_CHECK)) ? 1 : 0;
}

static void change_value(int *val, int delta,
                         int min, int max, int step,
                         const char *const *opts, uint8_t opts_count,
                         void (*on_change)(int)) {
    if (opts != NULL) {
        *val = (*val + delta + (int)opts_count) % (int)opts_count;
    } else {
        int s = (step != 0) ? step : 1;
        int v = *val + (delta * s);
        if (v < min) {
            *val = min;
        } else if (v > max) {
            *val = max;
        } else {
            *val = v;
        }
    }
    if (on_change != NULL) {
        on_change(*val);
    }
}

static const ListItem *list_items(const ListLayout *list, int *out_count) {
    if (list->get_items != NULL) {
        uint8_t c = 0U;
        const ListItem *items = list->get_items(&c);
        if (items == NULL) {
            c = 0U;
        }
        *out_count = (int)c;
        /* cppcheck-suppress misra-c2012-15.5 */
        return items;
    }
    *out_count = (int)list->count;
    return list->items;
}

static int is_selectable(const Widget *w) {
    /* cppcheck-suppress misra-c2012-12.1 */
    return ((w->on_click != NULL)
        || (w->type == W_VALUE)
        || (w->type == W_EDIT)
        || (w->type == W_LIST)) ? 1 : 0;
}

static void push_history(void) {
    if ((current_layout == NULL) && (current_list == NULL)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (history_top >= HISTORY_MAX) {
        for (int i = 0; i < (HISTORY_MAX - 1); i++) {
            history[i] = history[i + 1];
        }
        history_top = HISTORY_MAX - 1;
    }
    {
        int ht = history_top;
        history_top = ht + 1;
        history[ht] = (HistoryEntry){
            .layout           = current_layout,
            .list             = current_list,
            .focus_idx        = focus_item_idx,
            .scroll           = scroll_top,
            .list_item_focus  = list_item_focus,
            .list_item_scroll = list_item_scroll,
        };
    }
}

static void clamp_list_focus(const ListItem *items, int n) {
    if (n == 0) {
        focus_item_idx = -1;
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (focus_item_idx >= n) {
        focus_item_idx = -1;
        for (int i = n - 1; i >= 0; i--) {
            if (list_is_selectable(&items[i]) != 0) {
                focus_item_idx = i;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
        }
    }
}

static int dispatch_key(RenderKey key) {
    if (current_list != NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return (current_list->on_key != NULL) ? current_list->on_key(key) : 0;
    }
    if (current_layout != NULL) {
        if (focus_item_idx >= 0) {
            const Widget *w = &current_layout->items[focus_item_idx];
            if ((w->type == W_LIST) && (w->list != NULL) && (w->list->on_key != NULL)) {
                if (w->list->on_key(key) != 0) {
                    /* cppcheck-suppress misra-c2012-15.5 */
                    return 1;
                }
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return (current_layout->on_key != NULL) ? current_layout->on_key(key) : 0;
    }
    return 0;
}

static void resolve(const Widget *w, int16_t *ox, int16_t *oy) {
    int16_t x  = w->x;
    int16_t y  = w->y;
    int16_t sw = R->screen_w;
    int16_t sh = R->screen_h;
    int16_t ww = w->w;
    int16_t wh = (w->h != 0) ? w->h : (int16_t)w->font.h;

    switch (w->align & ALIGN_H_MASK) {
        case ALIGN_H_MID:   x = (int16_t)(x + (sw/2) - (ww/2)); break;
        case ALIGN_H_RIGHT: x = (int16_t)(x + sw - ww);          break;
        default: break;
    }
    switch (w->align & ALIGN_V_MASK) {
        case ALIGN_V_MID:    y = (int16_t)(y + (sh/2) - (wh/2)); break;
        case ALIGN_V_BOTTOM: y = (int16_t)(y + sh - wh);          break;
        default: break;
    }
    *ox = x; *oy = y;
}

/* ── W_LIST helpers ─────────────────────────────────────────────────────── */

static void wlist_scroll_adjust(const Widget *w) {
    uint8_t row_h   = (w->list->row_h != 0U) ? w->list->row_h : (uint8_t)DEFAULT_ROW_H;
    int     visible = (int)w->h / (int)row_h;
    if (list_item_focus < list_item_scroll) {
        list_item_scroll = list_item_focus;
    }
    if (list_item_focus >= (list_item_scroll + visible)) {
        list_item_scroll = (list_item_focus - visible) + 1;
    }
}

static void enter_list_widget(const Widget *w) {
    list_item_focus  = -1;
    list_item_scroll = 0;
    if (w->list == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    int n;
    const ListItem *items = list_items(w->list, &n);
    for (int i = 0; i < n; i++) {
        if (list_is_selectable(&items[i]) != 0) {
            list_item_focus = i;
            break;
        }
    }
}

static void leave_list_widget(void) {
    list_item_focus  = -1;
    list_item_scroll = 0;
}

/* Returns focused LI_VALUE item if W_LIST is active and in value-edit mode. */
static const ListItem *active_wlist_item(void) {
    if ((current_layout == NULL) || (focus_item_idx < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return NULL;
    }
    const Widget *w = &current_layout->items[focus_item_idx];
    if ((w->type != W_LIST) || (w->list == NULL) || (list_item_focus < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return NULL;
    }
    int n;
    const ListItem *items = list_items(w->list, &n);
    if (list_item_focus >= n) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return NULL;
    }
    return &items[list_item_focus];
}

/* ── draw ──────────────────────────────────────────────────────────────── */

static void draw_wlist(const Widget *w, int16_t wx, int16_t wy, int focused) {
    if (w->list == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    Ctx ctx = sub_ctx(wx, wy, (int16_t)w->w, (int16_t)w->h);
    int n;
    const ListItem *items = list_items(w->list, &n);
    uint8_t row_h   = (w->list->row_h != 0U) ? w->list->row_h : (uint8_t)DEFAULT_ROW_H;
    int     visible = (int)w->h / (int)row_h;
    int     end     = list_item_scroll + visible;
    if (end > n) {
        end = n;
    }

    for (int j = list_item_scroll; j < end; j++) {
        const ListItem *item      = &items[j];
        int16_t         iy        = (int16_t)((j - list_item_scroll) * (int)row_h);
        int             item_foc  = ((focused != 0) && (j == list_item_focus)) ? 1 : 0;
        int             item_edit = ((item_foc != 0) && (edit_mode == 1)) ? 1 : 0;

        if (item->type == LI_SEPARATOR) {
            if (R->draw_list_separator != NULL) {
                R->draw_list_separator(&ctx, 0, iy, (uint16_t)w->w, row_h);
            }
        } else {
            if (R->draw_list_item != NULL) {
                R->draw_list_item(&ctx, 0, iy, (uint16_t)w->w, row_h,
                                  item, item_foc, item_edit);
            }
        }
    }
}

static void draw_layout(const Layout *layout) {
    Ctx ctx = screen_ctx();
    if ((edit_mode == 2) && (focus_item_idx >= 0) &&
        (layout->items[focus_item_idx].type == W_EDIT)) {
        if (R->draw_edit_overlay != NULL) {
            R->draw_edit_overlay(&ctx, &layout->items[focus_item_idx], edit_cursor);
        }
        if (R->flush != NULL) {
            R->flush();
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    if (R->begin_frame != NULL) {
        R->begin_frame(&ctx);
    }

    for (uint8_t i = 0U; i < layout->count; i++) {
        const Widget *w = &layout->items[i];
        int16_t x;
        int16_t y;
        resolve(w, &x, &y);
        int focused  = (focus_item_idx == (int)i) ? 1 : 0;
        int val_edit = ((focused != 0) && (edit_mode == 1)) ? 1 : 0;
        switch (w->type) {
            case W_LABEL:
                if (R->draw_label != NULL) {
                    R->draw_label(&ctx, x, y, w);
                }
                break;
            case W_BTN:
                if (R->draw_btn != NULL) {
                    R->draw_btn(&ctx, x, y, w, focused);
                }
                break;
            case W_VALUE:
                if (R->draw_value != NULL) {
                    R->draw_value(&ctx, x, y, w, focused, val_edit);
                }
                break;
            case W_PROGRESS:
                if (R->draw_progress != NULL) {
                    R->draw_progress(&ctx, x, y, w);
                }
                break;
            case W_EDIT:
                if (R->draw_edit != NULL) {
                    R->draw_edit(&ctx, x, y, w, focused);
                }
                break;
            case W_LIST:
                draw_wlist(w, x, y, focused);
                break;
            default:
                break;
        }
    }
    if (R->flush != NULL) {
        R->flush();
    }
}

static void draw_list(const ListLayout *list) {
    Ctx ctx = screen_ctx();
    if (R->begin_frame != NULL) {
        R->begin_frame(&ctx);
    }

    int n;
    const ListItem *items = list_items(list, &n);
    clamp_list_focus(items, n);
    uint8_t row_h   = (list->row_h != 0U) ? list->row_h : (uint8_t)DEFAULT_ROW_H;
    int     visible = (int)R->screen_h / (int)row_h;
    int     end     = scroll_top + visible;
    if (end > n) {
        end = n;
    }

    for (int i = scroll_top; i < end; i++) {
        const ListItem *item    = &items[i];
        int16_t         y       = (int16_t)((i - scroll_top) * (int)row_h);
        int             focused = (i == focus_item_idx) ? 1 : 0;
        int             editing = ((focused != 0) && (edit_mode == 1)) ? 1 : 0;

        if (item->type == LI_SEPARATOR) {
            if (R->draw_list_separator != NULL) {
                R->draw_list_separator(&ctx, 0, y, R->screen_w, row_h);
            }
        } else {
            if (R->draw_list_item != NULL) {
                R->draw_list_item(&ctx, 0, y, R->screen_w, row_h,
                                  item, focused, editing);
            }
        }
    }
    if (R->flush != NULL) {
        R->flush();
    }
}

/* ── API ─────────────────────────────────────────────────────────────────── */

void render_screen(const Layout *layout) {
    push_history();
    current_list     = NULL;
    edit_mode        = 0;
    current_layout   = layout;
    focus_item_idx   = -1;
    list_item_focus  = -1;
    list_item_scroll = 0;
    for (int i = 0; i < (int)layout->count; i++) {
        if (is_selectable(&layout->items[i]) != 0) {
            focus_item_idx = i;
            if (layout->items[i].type == W_LIST) {
                enter_list_widget(&layout->items[i]);
            }
            break;
        }
    }
    dirty = 1;
}

void render_refresh(void) {
    if (dirty == 0) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    dirty = 0;
    if (current_list != NULL) {
        draw_list(current_list);
    } else if (current_layout != NULL) {
        draw_layout(current_layout);
        /* cppcheck-suppress misra-c2012-15.7 */
    } else {
        /* no action */
    }
}

/* cppcheck-suppress misra-c2012-8.7 */
void render_mark_dirty(void)         { dirty = 1; }
/* cppcheck-suppress misra-c2012-8.7 */
int  render_is_dirty(void)           { return dirty; }
/* cppcheck-suppress misra-c2012-8.7 */
const Layout     *render_current_layout(void) { return current_layout; }
/* cppcheck-suppress misra-c2012-8.7 */
const ListLayout *render_current_list(void)   { return current_list; }

void render_list(const ListLayout *list) {
    if (list == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    push_history();
    current_layout   = NULL;
    current_list     = list;
    edit_mode        = 0;
    scroll_top       = 0;
    focus_item_idx   = -1;
    list_item_focus  = -1;
    list_item_scroll = 0;
    int n;
    const ListItem *items = list_items(list, &n);
    for (int i = 0; i < n; i++) {
        if (list_is_selectable(&items[i]) != 0) {
            focus_item_idx = i;
            break;
        }
    }
    dirty = 1;
}

/* cppcheck-suppress misra-c2012-8.7 */
int render_can_back(void) { return (history_top > 0) ? 1 : 0; }

void render_back(void) {
    if (history_top == 0) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        int ht = history_top - 1;
        history_top = ht;
        HistoryEntry e   = history[ht];
        edit_mode        = 0;
        focus_item_idx   = e.focus_idx;
        scroll_top       = e.scroll;
        list_item_focus  = e.list_item_focus;
        list_item_scroll = e.list_item_scroll;
        if (e.list != NULL) {
            current_layout = NULL;
            current_list   = e.list;
        } else {
            current_list   = NULL;
            current_layout = e.layout;
        }
    }
    dirty = 1;
}

static void list_scroll_adjust(uint8_t row_h) {
    uint8_t effective_row_h = (row_h != 0U) ? row_h : (uint8_t)DEFAULT_ROW_H;
    int visible = (int)R->screen_h / (int)effective_row_h;
    if (focus_item_idx < scroll_top) {
        scroll_top = focus_item_idx;
    }
    if (focus_item_idx >= (scroll_top + visible)) {
        scroll_top = (focus_item_idx - visible) + 1;
    }
}

/* ── internal focus functions ─────────────────────────────────────────────── */

static void focus_next(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        if (edit_mode == 1) {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, -1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (focus_item_idx < 0) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx + i) % n;
            if (list_is_selectable(&items[idx]) != 0) {
                focus_item_idx = idx;
                list_scroll_adjust(current_list->row_h);
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (edit_mode == 1) {
        const Widget *w = (focus_item_idx >= 0) ? &current_layout->items[focus_item_idx] : NULL;
        if ((w != NULL) && (w->type == W_VALUE) && (w->value != NULL)) {
            change_value(w->value, -1, w->min, w->max, w->step,
                         w->options, w->options_count, w->on_change);
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            const ListItem *item = active_wlist_item();
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, -1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (edit_mode == 2) {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((w->buf != NULL) && (edit_cursor < (int)strlen(w->buf))) {
            edit_cursor++;
            dirty = 1;
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    /* Navigate within W_LIST if focused */
    if (focus_item_idx >= 0) {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((w->type == W_LIST) && (w->list != NULL)) {
            int n;
            const ListItem *items = list_items(w->list, &n);
            int start = (list_item_focus < 0) ? 0 : (list_item_focus + 1);
            for (int i = start; i < n; i++) {
                if (list_is_selectable(&items[i]) != 0) {
                    list_item_focus = i;
                    wlist_scroll_adjust(w);
                    dirty = 1;
                    /* cppcheck-suppress misra-c2012-15.5 */
                    return;
                }
            }
            /* reached end of list, leave it and fall through to next widget */
            leave_list_widget();
        }
    }

    {
        int n = (int)current_layout->count;
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx + i) % n;
            if (is_selectable(&current_layout->items[idx]) != 0) {
                focus_item_idx = idx;
                if (current_layout->items[idx].type == W_LIST) {
                    enter_list_widget(&current_layout->items[idx]);
                }
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
        }
    }
}

static void focus_prev(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        if (edit_mode == 1) {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, +1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (focus_item_idx < 0) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx - i + n) % n;
            if (list_is_selectable(&items[idx]) != 0) {
                focus_item_idx = idx;
                list_scroll_adjust(current_list->row_h);
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (edit_mode == 1) {
        const Widget *w = (focus_item_idx >= 0) ? &current_layout->items[focus_item_idx] : NULL;
        if ((w != NULL) && (w->type == W_VALUE) && (w->value != NULL)) {
            change_value(w->value, +1, w->min, w->max, w->step,
                         w->options, w->options_count, w->on_change);
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            const ListItem *item = active_wlist_item();
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, +1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (edit_mode == 2) {
        if (edit_cursor > 0) {
            edit_cursor--;
            dirty = 1;
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }

    /* Navigate within W_LIST if focused */
    if (focus_item_idx >= 0) {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((w->type == W_LIST) && (w->list != NULL)) {
            int n;
            const ListItem *items = list_items(w->list, &n);
            int start = (list_item_focus < 0) ? (n - 1) : (list_item_focus - 1);
            for (int i = start; i >= 0; i--) {
                if (list_is_selectable(&items[i]) != 0) {
                    list_item_focus = i;
                    wlist_scroll_adjust(w);
                    dirty = 1;
                    /* cppcheck-suppress misra-c2012-15.5 */
                    return;
                }
            }
            /* reached top of list, leave it and fall through to prev widget */
            leave_list_widget();
        }
    }

    {
        int n = (int)current_layout->count;
        for (int i = 1; i <= n; i++) {
            int idx = (focus_item_idx - i + n) % n;
            if (is_selectable(&current_layout->items[idx]) != 0) {
                focus_item_idx = idx;
                if (current_layout->items[idx].type == W_LIST) {
                    const Widget *lw = &current_layout->items[idx];
                    list_item_focus  = -1;
                    list_item_scroll = 0;
                    if (lw->list != NULL) {
                        int ln;
                        const ListItem *litems = list_items(lw->list, &ln);
                        for (int j = ln - 1; j >= 0; j--) {
                            if (list_is_selectable(&litems[j]) != 0) {
                                list_item_focus = j;
                                wlist_scroll_adjust(lw);
                                break;
                            }
                        }
                    }
                }
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
        }
    }
}

static void focus_inc(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (edit_mode == 1) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, +1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = (focus_item_idx >= 0) ? &current_layout->items[focus_item_idx] : NULL;
        if (w == NULL) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (edit_mode == 2) {
            if ((w->buf != NULL) && (edit_cursor < (int)strlen(w->buf))) {
                char c = w->buf[edit_cursor];
                w->buf[edit_cursor] = (c < '~') ? (char)(c + 1) : ' ';
                dirty = 1;
            }
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if ((edit_mode == 1) && (w->type == W_VALUE) && (w->value != NULL)) {
            change_value(w->value, +1, w->min, w->max, w->step,
                         w->options, w->options_count, w->on_change);
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if ((edit_mode == 1) && (w->type == W_LIST)) {
            const ListItem *item = active_wlist_item();
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, +1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
    }
}

static void focus_dec(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (edit_mode == 1) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, -1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = (focus_item_idx >= 0) ? &current_layout->items[focus_item_idx] : NULL;
        if (w == NULL) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (edit_mode == 2) {
            if ((w->buf != NULL) && (edit_cursor < (int)strlen(w->buf))) {
                char c = w->buf[edit_cursor];
                w->buf[edit_cursor] = (c > ' ') ? (char)(c - 1) : '~';
                dirty = 1;
            }
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if ((edit_mode == 1) && (w->type == W_VALUE) && (w->value != NULL)) {
            change_value(w->value, -1, w->min, w->max, w->step,
                         w->options, w->options_count, w->on_change);
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if ((edit_mode == 1) && (w->type == W_LIST)) {
            const ListItem *item = active_wlist_item();
            if ((item != NULL) && (item->type == LI_VALUE) && (item->value != NULL)) {
                change_value(item->value, -1, item->min, item->max, item->step,
                             item->options, item->options_count, item->on_change);
                dirty = 1;
            }
        }
    }
}

static void focus_activate(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (edit_mode == 1)) {
                edit_mode = 0;
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            if (item == NULL) {
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            if ((item->type == LI_VALUE) && (item->value != NULL)) {
                value_backup = *item->value;
                edit_mode    = 1;
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            if ((item->type == LI_CHECK) && (item->value != NULL)) {
                *item->value = (*item->value != 0) ? 0 : 1;
                if (item->on_change != NULL) {
                    item->on_change(*item->value);
                }
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            if (((item->type == LI_BTN) || (item->type == LI_SUBMENU)) && (item->on_click != NULL)) {
                item->on_click();
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = (focus_item_idx >= 0) ? &current_layout->items[focus_item_idx] : NULL;

        /* Confirm value / string edit */
        if ((w != NULL) && ((edit_mode == 1) || (edit_mode == 2))) {
            edit_mode = 0;
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (w == NULL) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }

        if ((w->type == W_VALUE) && (w->value != NULL)) {
            value_backup = *w->value;
            edit_mode = 1;
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (w->type == W_EDIT) {
            edit_mode = 2;
            edit_cursor = (w->buf != NULL) ? (int)strlen(w->buf) : 0;
            if (w->buf != NULL) {
                /* cppcheck-suppress misra-c2012-17.7 */
                strncpy(edit_backup, w->buf, sizeof(edit_backup) - 1U);
                edit_backup[sizeof(edit_backup) - 1U] = '\0';
            }
            dirty = 1;
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if ((w->type == W_LIST) && (w->list != NULL)) {
            int n;
            const ListItem *items;
            if (list_item_focus < 0) {
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            items = list_items(w->list, &n);
            if (list_item_focus >= n) {
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            {
                const ListItem *item = &items[list_item_focus];
                if ((item->type == LI_VALUE) && (item->value != NULL)) {
                    value_backup = *item->value;
                    edit_mode    = 1;
                    dirty = 1;
                    /* cppcheck-suppress misra-c2012-15.5 */
                    return;
                }
                if ((item->type == LI_CHECK) && (item->value != NULL)) {
                    *item->value = (*item->value != 0) ? 0 : 1;
                    if (item->on_change != NULL) {
                        item->on_change(*item->value);
                    }
                    dirty = 1;
                    /* cppcheck-suppress misra-c2012-15.5 */
                    return;
                }
                if (((item->type == LI_BTN) || (item->type == LI_SUBMENU)) && (item->on_click != NULL)) {
                    item->on_click();
                }
            }
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        if (w->on_click != NULL) {
            w->on_click();
        }
    }
}

static void focus_cancel(void) {
    if (current_list != NULL) {
        int n;
        const ListItem *items = list_items(current_list, &n);
        clamp_list_focus(items, n);
        {
            const ListItem *item = (focus_item_idx >= 0) ? &items[focus_item_idx] : NULL;
            if ((item != NULL) && (edit_mode == 1) && (item->value != NULL)) {
                *item->value = value_backup;
                if (item->on_change != NULL) {
                    item->on_change(*item->value);
                }
                edit_mode = 0;
                dirty = 1;
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            if (edit_mode == 0) {
                render_back();
            }
        }
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (current_layout == NULL) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (edit_mode == 0) {
        render_back();
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    if (focus_item_idx < 0) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((edit_mode == 1) && (w->type == W_VALUE) && (w->value != NULL)) {
            *w->value = value_backup;
            if (w->on_change != NULL) {
                w->on_change(*w->value);
            }
        }
        if ((edit_mode == 1) && (w->type == W_LIST)) {
            const ListItem *item = active_wlist_item();
            if ((item != NULL) && (item->value != NULL)) {
                *item->value = value_backup;
                if (item->on_change != NULL) {
                    item->on_change(*item->value);
                }
            }
        }
        if ((edit_mode == 2) && (w->buf != NULL)) {
            int maxcopy = (int)w->buf_len - 1;
            /* cppcheck-suppress misra-c2012-17.7 */
            strncpy(w->buf, edit_backup, (size_t)maxcopy);
            w->buf[(size_t)maxcopy] = '\0';
        }
    }
    edit_mode = 0;
    dirty = 1;
}

/* ── public event API ────────────────────────────────────────────────────── */

void render_next(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_NEXT) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_next();
}

void render_prev(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_PREV) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_prev();
}

void render_activate(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_ACTIVATE) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_activate();
}

void render_cancel(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_CANCEL) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_cancel();
}

void render_inc(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_INC) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_inc();
}

void render_dec(void) {
    if ((edit_mode == 0) && (dispatch_key(RENDER_KEY_DEC) != 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    focus_dec();
}

/* cppcheck-suppress misra-c2012-8.7 */
int render_in_value_edit(void) { return (edit_mode == 1) ? 1 : 0; }
int render_in_edit_mode(void)  { return (edit_mode == 2) ? 1 : 0; }

void render_edit_char(char c) {
    if ((edit_mode != 2) || (current_layout == NULL) || (focus_item_idx < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = &current_layout->items[focus_item_idx];
        if (w->buf == NULL) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            int len = (int)strlen(w->buf);
            if (len >= ((int)w->buf_len - 1)) {
                /* cppcheck-suppress misra-c2012-15.5 */
                return;
            }
            {
                int move_len = len - edit_cursor + 1;
                /* cppcheck-suppress misra-c2012-17.7 */
                memmove(&w->buf[edit_cursor + 1], &w->buf[edit_cursor],
                        (size_t)move_len);
            }
            w->buf[edit_cursor] = c;
            edit_cursor++;
            dirty = 1;
        }
    }
}

void render_edit_delete(void) {
    if ((edit_mode != 2) || (current_layout == NULL) || (focus_item_idx < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((w->buf == NULL) || (edit_cursor == 0)) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            int len = (int)strlen(w->buf);
            int move_len = len - edit_cursor + 1;
            /* cppcheck-suppress misra-c2012-17.7 */
            memmove(&w->buf[edit_cursor - 1], &w->buf[edit_cursor],
                    (size_t)move_len);
            edit_cursor--;
            dirty = 1;
        }
    }
}

WidgetType render_focused_type(void) {
    if ((current_layout == NULL) || (focus_item_idx < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return W_LABEL;
    }
    return current_layout->items[focus_item_idx].type;
}

void render_edit_set(const char *str) {
    if ((current_layout == NULL) || (focus_item_idx < 0)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return;
    }
    {
        const Widget *w = &current_layout->items[focus_item_idx];
        if ((w->type != W_EDIT) || (w->buf == NULL)) {
            /* cppcheck-suppress misra-c2012-15.5 */
            return;
        }
        {
            int maxcopy = (int)w->buf_len - 1;
            /* cppcheck-suppress misra-c2012-17.7 */
            strncpy(w->buf, str, (size_t)maxcopy);
            w->buf[(size_t)maxcopy] = '\0';
        }
        edit_cursor = (int)strlen(w->buf);
    }
}
