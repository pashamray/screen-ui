#include "screen_example.h"
#include "screen_manager.h"
#include "render.h"

enum { ID_VALUE = 1 };

static const Layout layout = LAYOUT(
    { .type  = W_LABEL,
      .text  = "Example Screen",
      .align = ALIGN_TOP_MID,
      .y     = 20 },

    { .type  = W_LABEL,
      .text  = "--",
      .align = ALIGN_CENTER,
      .id    = ID_VALUE },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 100, .h = 40, .y = -20,
      .on_click = screen_back },
);

static void create(Screen *self) {
    render_screen(&layout);
}

static void on_show(Screen *self) {
    // refresh data on every show
}

Screen screen_example = {
    .create  = create,
    .on_show = on_show,
};
