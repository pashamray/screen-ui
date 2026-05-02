#include "screen_manager.h"
#include "render.h"

#define STACK_DEPTH 4

static Screen *stack[STACK_DEPTH];
static int     top = -1;

void screen_show(Screen *s) {
    if (top >= 0 && stack[top]->on_hide)
        stack[top]->on_hide(stack[top]);

    if (!s->root && s->create)
        s->create(s);

    stack[++top] = s;
    render_screen_load(s->root);

    if (s->on_show) s->on_show(s);
}

void screen_back(void) {
    if (top <= 0) return;

    Screen *cur = stack[top--];
    if (cur->on_hide)  cur->on_hide(cur);
    if (cur->destroy)  cur->destroy(cur);

    Screen *prev = stack[top];
    render_screen_load(prev->root);
    if (prev->on_show) prev->on_show(prev);
}

Screen *screen_current(void) {
    return top >= 0 ? stack[top] : NULL;
}
