# screen-ui

A UI screen description library decoupled from any specific rendering backend.
Target platform: STM32G431 + ST7789 (SPI), but renderers are plugged in separately.

## Project structure

```
widget/      — widget types, Font, Layout; no external dependencies
render/      — rendering engine (render.c) and backend interface (render.h)
fonts/       — bitmap fonts: font_8x8, font_mono16 (Liberation Mono 10×16)
layouts/     — application screen definitions
screen/      — screen manager (navigation stack)
tools/       — utilities: gen_font.py (ttf2ugui → Font converter)
examples/    — entry point and renderer implementations
  main.c
  render_impl_console.c   — ANSI terminal renderer
  render_impl_sdl.c       — SDL2 pixel-accurate renderer
  render_impl_st7789.c    — ST7789 template (SPI)
  CMakeLists.txt
CMakeLists.txt            — builds libscreen-ui-core.a + examples
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

### Individual targets

```sh
cmake --build build --target screen-ui-console
cmake --build build --target screen-ui-sdl
```

### Run

```sh
./build/examples/screen-ui-console   # terminal, controls: j/k/Enter/q
./build/examples/screen-ui-sdl       # 480×640 window, controls: ↑↓/Enter/Esc
```

## Widgets

| Type | Description |
|---|---|
| `W_LABEL` | Text label (transparent background) |
| `W_BTN` | Button with border, calls `on_click` |
| `W_VALUE` | Inline int or enum editor (encoder / ←→) |
| `W_EDIT` | String input with cursor |

All widgets support `.font` and `.colors` to override the font and colors per widget.

## Fonts

Fonts are declared in `fonts/fonts.h` and are independent of the renderer:

```c
#include "fonts.h"

extern const Font font_8x8;     // 8×8 px, ASCII
extern const Font font_mono16;  // 10×16 px, Liberation Mono
```

Usage in a widget:

```c
{ .type = W_LABEL, .text = "TITLE", .font = &font_mono16, ... }
```

### Adding a new font

```sh
# 1. Convert TTF with ttf2ugui
ttf2ugui --dump --font=MyFont.ttf --size=16 --chars=32-126

# 2. Convert to our format
python3 tools/gen_font.py MyFont_NxM.c font_my

# 3. Copy result into fonts/
cp MyFont_NxM_font.c fonts/font_my.c

# 4. Add extern to fonts/fonts.h and the file to CMakeLists.txt
```

## Themes

A theme is set via `render_set_theme()` and can be switched from any screen:

```c
static void apply_theme(int idx) {
    render_set_theme(themes[idx]);
}
```

Built-in themes: `theme_dark`, `theme_light` (defined in `layouts/layouts.c`).

## Custom renderer

Implement four primitives and three lifecycle functions:

```c
static void my_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
static void my_draw_text(int16_t x, int16_t y, const char *text, const DrawStyle *s);
static void my_draw_border(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color, uint8_t thickness);
static void my_flush(void);  // may be NULL

void render_init(void) {
    static const Render r = {
        .fill_rect    = my_fill_rect,
        .draw_text    = my_draw_text,
        .draw_border  = my_draw_border,
        .flush        = my_flush,
        .screen_w     = 240,
        .screen_h     = 240,
        .char_w       = font_8x8.w,
        .char_h       = font_8x8.h,
        .default_font = &font_8x8,
    };
    render_set(&r);
}

void render_wait_key(void) { /* platform event loop */ }
void render_quit(void)     { /* release resources */ }
```

`DrawStyle.font` is guaranteed non-NULL — the engine substitutes `default_font` before calling `draw_text`.

Add a new `add_executable` in `examples/CMakeLists.txt` and link against `screen-ui-core`.
