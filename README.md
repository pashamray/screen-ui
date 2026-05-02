# screen-ui

A UI screen description library for embedded systems, decoupled from any specific rendering backend.
Target platform: STM32G431 + ST7789 (SPI), 240×320. Renderers are plugged in separately.

## Project structure

```
widget/      — widget types and Layout definition
render/      — engine: state machine, focus, edit modes, geometry resolution
theme/       — Theme struct (colors)
fonts/       — bitmap fonts: font_8x8, font_mono16 (Liberation Mono 10×16)
screen/      — screen manager (navigation stack)
tools/       — utilities: gen_font.py (TTF → Font converter)
examples/
  layouts.c / layouts.h        — application screen definitions
  render_impl_console.c        — ANSI terminal renderer
  render_impl_sdl.c            — SDL2 pixel-accurate renderer
  render_impl_st7789.c         — ST7789 template (SPI)
  main.c
  CMakeLists.txt
CMakeLists.txt                 — builds libscreen-ui-core.a + examples
```

## Dependencies

| Target | Dependency |
|---|---|
| `screen-ui-console` | none |
| `screen-ui-sdl` | SDL2 ≥ 2.0 |
| STM32 / ST7789 | STM32CubeG4 HAL SPI |

### Installing SDL2

```sh
# Fedora
sudo dnf install SDL2-devel
# Ubuntu / Debian
sudo apt install libsdl2-dev
# macOS
brew install sdl2
```

## Build

```sh
cmake -B build
cmake --build build
```

```sh
cmake --build build --target screen-ui-console
cmake --build build --target screen-ui-sdl
```

### Run

```sh
./build/examples/screen-ui-console   # terminal — j/k/Enter/q
./build/examples/screen-ui-sdl       # 480×640 window — ↑↓/Enter/Esc
```

## Widgets

| Type | Description |
|---|---|
| `W_LABEL` | Text label |
| `W_BTN` | Button, calls `on_click` on activation |
| `W_VALUE` | Inline int or enum editor (encoder / ←→) |
| `W_PROGRESS` | Read-only bar: `value`/`min`/`max`; vertical if `h > w` |
| `W_EDIT` | String input with cursor and full-screen overlay |

All widgets accept `.colors` (`WidgetColors { fg, bg }`) to override theme defaults per widget.

### Layout definition

```c
static int volume = 70;
static int mode   = 0;
static const char *const modes[] = { "Mono", "Stereo", "Surround" };

const Layout audio_layout = LAYOUT(
    { .type  = W_LABEL, .text = "AUDIO",
      .align = ALIGN_TOP_MID, .y = 4 },

    { .type  = W_VALUE, .text = "Volume",
      .align = ALIGN_CENTER, .w = 180, .h = 24, .y = -20,
      .value = &volume, .min = 0, .max = 100, .step = 5 },

    { .type           = W_VALUE, .text = "Mode",
      .align          = ALIGN_CENTER, .w = 180, .h = 24, .y = 10,
      .value          = &mode,
      .options        = modes, .options_count = 3 },

    { .type  = W_PROGRESS,
      .align = ALIGN_CENTER, .w = 160, .h = 12, .y = 40,
      .value = &volume, .min = 0, .max = 100 },
);
```

### Alignment

| Constant | Anchor |
|---|---|
| `ALIGN_CENTER` | Screen center (x, y are offsets from center) |
| `ALIGN_TOP_MID` | Top edge, horizontally centered |
| `ALIGN_TOP_LEFT` / `ALIGN_TOP_RIGHT` | Top corners |
| `ALIGN_BOTTOM_LEFT` / `ALIGN_BOTTOM_MID` / `ALIGN_BOTTOM_RIGHT` | Bottom edge |

## Themes

`Theme` is defined in `theme/theme.h`. Each renderer implementation provides its own theme and sets it during init:

```c
static const Theme theme = {
    .screen_bg      = 0x0000,  // RGB565 black
    .text_fg        = 0xFFFF,
    .widget_bg      = 0x2104,
    .border_normal  = 0xFFFF,
    .border_focused = 0x07FF,  // cyan
    .border_editing = 0xFFE0,  // yellow
    .border_subtle  = 0x4208,
    .cursor_fg      = 0x0000,
    .cursor_bg      = 0x07FF,
    .hint_fg        = 0x8410,
};

void render_init(void) {
    render_set(&r);
    render_set_theme(&theme);
}
```

Themes can be switched at runtime from any screen:

```c
static void apply_theme(int idx) {
    render_set_theme(themes[idx]);
}
```

## Custom renderer

A renderer implements frame-level hooks and per-widget draw callbacks. All visual logic lives in the implementation — the engine only resolves geometry and dispatches.

```c
// frame hooks
static void my_begin_frame(const Render *r, const Theme *t) {
    // clear screen with t->screen_bg
}
static void my_flush(void) {
    // send framebuffer to display
}
// optional: full-screen text input overlay for W_EDIT
static void my_draw_edit_overlay(const Render *r, const Widget *w,
                                  const Theme *t, int cursor) { ... }

// widget callbacks
static void my_draw_label   (const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t) { ... }
static void my_draw_btn     (const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t, int focused) { ... }
static void my_draw_value   (const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t,
                              int focused, int editing) { ... }
static void my_draw_progress(const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t) { ... }
static void my_draw_edit    (const Render *r, int16_t x, int16_t y,
                              const Widget *w, const Theme *t, int focused) { ... }

void render_init(void) {
    static const Render r = {
        .begin_frame       = my_begin_frame,
        .flush             = my_flush,
        .draw_edit_overlay = my_draw_edit_overlay,  // NULL on platforms without keyboard
        .screen_w = 240, .screen_h = 320,
        .draw_label    = my_draw_label,
        .draw_btn      = my_draw_btn,
        .draw_value    = my_draw_value,
        .draw_progress = my_draw_progress,
        .draw_edit     = my_draw_edit,
    };
    render_set(&r);
    render_set_theme(&theme);
}

void render_wait_key(void) { /* platform event loop */ }
void render_quit(void)     { /* release resources */ }
```

Add a new `add_executable` in `examples/CMakeLists.txt` and link against `screen-ui-core`.

## Fonts

Fonts are declared in `fonts/fonts.h` and used directly in renderer implementations:

```c
#include "fonts.h"
// font_8x8    — 8×8 px, ASCII
// font_mono16 — 10×16 px, Liberation Mono
```

### Adding a new font

```sh
# 1. Convert TTF with ttf2ugui
ttf2ugui --dump --font=MyFont.ttf --size=16 --chars=32-126
# 2. Convert to screen-ui format
python3 tools/gen_font.py MyFont_NxM.c font_my
# 3. Copy into fonts/ and add extern to fonts/fonts.h
```

## License

MIT — see [LICENSE](LICENSE).
