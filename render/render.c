#include "render.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const Render  *R;
static const Layout  *current_layout = NULL;
static int            focus_item_idx = -1;

// 0 = normal, 1 = value edit (W_VALUE), 2 = string edit (W_EDIT)
static int  edit_mode   = 0;
static int  edit_cursor = 0;
static char edit_backup[64];
static int  value_backup = 0;

const Theme theme_default = {
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

static const Theme *T = &theme_default;

void render_set(const Render *r)      { R = r; }
void render_set_theme(const Theme *t) { T = t; }

// ── helpers ───────────────────────────────────────────────────────────────────

static int is_selectable(const Widget *w) {
    return w->on_click != NULL || w->type == W_VALUE || w->type == W_EDIT;
}

static void resolve(const Widget *w, int16_t *ox, int16_t *oy) {
    int16_t x = w->x, y = w->y;
    int16_t sw = R->screen_w, sh = R->screen_h;
    int16_t ww = w->w,        wh = w->h;

    switch (w->align) {
        case ALIGN_CENTER:       x += sw/2 - ww/2; y += sh/2 - wh/2; break;
        case ALIGN_TOP_MID:      x += sw/2 - ww/2;                    break;
        case ALIGN_TOP_LEFT:                                           break;
        case ALIGN_TOP_RIGHT:    x += sw - ww;                        break;
        case ALIGN_BOTTOM_MID:   x += sw/2 - ww/2; y += sh - wh;     break;
        case ALIGN_BOTTOM_LEFT:                     y += sh - wh;     break;
        case ALIGN_BOTTOM_RIGHT: x += sw - ww;      y += sh - wh;     break;
        default: break;
    }
    *ox = x; *oy = y;
}

// ── edit overlay ─────────────────────────────────────────────────────────────

static void draw_edit_overlay(void) {
    const Widget *w      = &current_layout->items[focus_item_idx];
    char         *buf    = w->buf;
    int           maxlen = w->buf_len - 1;
    int           slen   = (int)strlen(buf);
    uint8_t       cw     = R->char_w;
    uint8_t       fh     = R->char_h;

    uint16_t wfg = w->colors ? w->colors->fg : T->text_fg;
    uint16_t wbg = w->colors ? w->colors->bg : T->widget_bg;

    R->fill_rect(0, 0, R->screen_w, R->screen_h, T->screen_bg);

    DrawStyle s_title = { .fg = T->text_fg, .bg = T->screen_bg };
    int tlen = (int)strlen(w->text);
    int16_t tx = (int16_t)(R->screen_w / 2 - tlen * cw / 2);
    R->draw_text(tx, 16, w->text, &s_title);

    int16_t fw = (int16_t)(maxlen * cw + 8);
    int16_t fx = (int16_t)(R->screen_w / 2 - fw / 2);
    int16_t fy = (int16_t)(R->screen_h / 2 - fh / 2 - 4);
    R->fill_rect(fx, fy, fw, fh + 8, wbg);
    R->draw_border(fx, fy, fw, fh + 8, T->border_focused, 1);

    int16_t cx = (int16_t)(fx + 4);
    int16_t cy = (int16_t)(fy + 4);

    DrawStyle s_normal = { .fg = wfg,         .bg = wbg          };
    DrawStyle s_cursor = { .fg = T->cursor_fg, .bg = T->cursor_bg };

    if (edit_cursor > 0) {
        char part[64];
        memcpy(part, buf, (size_t)edit_cursor);
        part[edit_cursor] = '\0';
        R->draw_text(cx, cy, part, &s_normal);
    }
    char cur_ch[2] = { edit_cursor < slen ? buf[edit_cursor] : ' ', '\0' };
    R->draw_text((int16_t)(cx + edit_cursor * cw), cy, cur_ch, &s_cursor);
    if (edit_cursor < slen)
        R->draw_text((int16_t)(cx + (edit_cursor + 1) * cw), cy,
                     buf + edit_cursor + 1, &s_normal);

    DrawStyle s_hint = { .fg = T->hint_fg, .bg = T->screen_bg };
    const char *hint = "Enter=confirm  Esc=cancel";
    int hlen = (int)strlen(hint);
    int16_t hx = (int16_t)(R->screen_w / 2 - hlen * cw / 2);
    R->draw_text(hx, (int16_t)(R->screen_h - 20), hint, &s_hint);

    if (R->flush) R->flush();
}

// ── draw ─────────────────────────────────────────────────────────────────────

static void draw_layout(const Layout *layout) {
    if (edit_mode == 2 && focus_item_idx >= 0 &&
        layout->items[focus_item_idx].type == W_EDIT) {
        draw_edit_overlay();
        return;
    }

    R->fill_rect(0, 0, R->screen_w, R->screen_h, T->screen_bg);

    for (uint8_t i = 0; i < layout->count; i++) {
        const Widget *w = &layout->items[i];
        int16_t x, y;
        resolve(w, &x, &y);
        int focused   = (focus_item_idx == (int)i);
        int val_edit  = focused && (edit_mode == 1);

        uint8_t cw = R->char_w;
        uint8_t fh = R->char_h;

        switch (w->type) {
            case W_LABEL: {
                uint16_t fg = w->colors ? w->colors->fg : T->text_fg;
                int tlen = (int)strlen(w->text);
                int16_t tx = (w->w > 0) ? x : (int16_t)(x - tlen * cw / 2);
                DrawStyle s = { .fg = fg, .bg = T->screen_bg };
                R->draw_text(tx, y, w->text, &s);
                break;
            }
            case W_BTN: {
                uint16_t fg = w->colors ? w->colors->fg : T->text_fg;
                uint16_t bg = w->colors ? w->colors->bg : T->widget_bg;
                uint16_t border_col = focused ? T->border_focused : T->border_normal;
                R->fill_rect(x, y, w->w, w->h, bg);
                R->draw_border(x, y, w->w, w->h, border_col, 1);
                int tlen = (int)strlen(w->text);
                int16_t tx = (int16_t)(x + (w->w - tlen * cw) / 2);
                int16_t ty = (int16_t)(y + (w->h - fh) / 2);
                DrawStyle s = { .fg = fg, .bg = bg };
                R->draw_text(tx, ty, w->text, &s);
                break;
            }
            case W_VALUE: {
                uint16_t fg = w->colors ? w->colors->fg : T->text_fg;
                uint16_t bg = w->colors ? w->colors->bg : T->widget_bg;
                char disp[48];
                if (w->options && w->value)
                    snprintf(disp, sizeof(disp), "%s: %s",
                             w->text, w->options[*w->value]);
                else if (w->value)
                    snprintf(disp, sizeof(disp), "%s: %d", w->text, *w->value);
                else
                    snprintf(disp, sizeof(disp), "%s: --", w->text);

                uint16_t border_col = val_edit ? T->border_editing :
                                      focused  ? T->border_focused : T->border_subtle;
                R->fill_rect(x, y, w->w, w->h, bg);
                R->draw_border(x, y, w->w, w->h, border_col, 1);

                int tlen = (int)strlen(disp);
                int16_t tx = (int16_t)(x + (w->w - tlen * cw) / 2);
                int16_t ty = (int16_t)(y + (w->h - fh) / 2);
                DrawStyle s  = { .fg = fg, .bg = bg };
                DrawStyle sa = { .fg = val_edit ? T->border_editing : T->border_focused,
                                 .bg = bg };
                R->draw_text(tx, ty, disp, &s);
                if (focused) {
                    R->draw_text((int16_t)(x + cw), ty,
                                 val_edit ? "[" : "<", &sa);
                    R->draw_text((int16_t)(x + w->w - 2 * cw), ty,
                                 val_edit ? "]" : ">", &sa);
                }
                break;
            }
            case W_EDIT: {
                uint16_t fg = w->colors ? w->colors->fg : T->text_fg;
                uint16_t bg = w->colors ? w->colors->bg : T->widget_bg;
                uint16_t border_col = focused ? T->border_focused : T->border_normal;
                R->fill_rect(x, y, w->w, w->h, bg);
                R->draw_border(x, y, w->w, w->h, border_col, 1);
                char disp[48];
                const char *val = (w->buf && w->buf[0]) ? w->buf : "...";
                snprintf(disp, sizeof(disp), "%s: %s", w->text, val);
                int tlen = (int)strlen(disp);
                int16_t tx = (int16_t)(x + (w->w - tlen * cw) / 2);
                int16_t ty = (int16_t)(y + (w->h - fh) / 2);
                DrawStyle s = { .fg = fg, .bg = bg };
                R->draw_text(tx, ty, disp, &s);
                break;
            }
            default: break;
        }
    }
    if (R->flush) R->flush();
}

// ── API ───────────────────────────────────────────────────────────────────────

void render_screen(const Layout *layout) {
    edit_mode = 0;
    current_layout = layout;
    focus_item_idx = -1;
    for (int i = 0; i < (int)layout->count; i++) {
        if (is_selectable(&layout->items[i])) { focus_item_idx = i; break; }
    }
    draw_layout(layout);
}

void render_refresh(void) {
    if (current_layout) draw_layout(current_layout);
}

void render_focus_next(void) {
    if (!current_layout) return;
    if (edit_mode == 1) return;    // in value-edit, rotation changes the value, not focus
    if (edit_mode == 2) {          // in string-edit — move cursor right
        const Widget *w = &current_layout->items[focus_item_idx];
        if (w->buf && edit_cursor < (int)strlen(w->buf))
            { edit_cursor++; render_refresh(); }
        return;
    }
    int n = (int)current_layout->count;
    for (int i = 1; i <= n; i++) {
        int idx = (focus_item_idx + i) % n;
        if (is_selectable(&current_layout->items[idx]))
            { focus_item_idx = idx; return; }
    }
}

void render_focus_prev(void) {
    if (!current_layout) return;
    if (edit_mode == 1) return;
    if (edit_mode == 2) {
        if (edit_cursor > 0) { edit_cursor--; render_refresh(); }
        return;
    }
    int n = (int)current_layout->count;
    for (int i = 1; i <= n; i++) {
        int idx = (focus_item_idx - i + n) % n;
        if (is_selectable(&current_layout->items[idx]))
            { focus_item_idx = idx; return; }
    }
}

void render_focus_inc(void) {
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
    }
    // in normal mode: no-op — must enter edit via activate first
}

void render_focus_dec(void) {
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
    }
}

void render_focus_activate(void) {
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];

    if (edit_mode == 1 || edit_mode == 2) {
        // confirm any active edit mode
        edit_mode = 0;
        render_refresh();
        return;
    }
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
    if (!current_layout || focus_item_idx < 0) return;
    const Widget *w = &current_layout->items[focus_item_idx];
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
