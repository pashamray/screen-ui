#include "layouts.h"
#include "render.h"
#include <stdio.h>

// ── themes ───────────────────────────────────────────────────────────────────

static const Theme theme_dark = {
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

static const Theme theme_light = {
    .screen_bg      = 0xFFFF,
    .text_fg        = 0x0000,
    .widget_bg      = 0xC618,
    .border_normal  = 0x8410,
    .border_focused = 0x041F,
    .border_editing = 0xF800,
    .border_subtle  = 0xD69A,
    .cursor_fg      = 0xFFFF,
    .cursor_bg      = 0x041F,
    .hint_fg        = 0x8410,
};

static const Theme *const themes[] = { &theme_dark, &theme_light };

static void apply_theme(int idx) {
    render_set_theme(themes[idx]);
}

// ── mutable state ────────────────────────────────────────────────────────────

static int brightness = 80;
static int theme_idx  = 0;
static int wifi_on    = 1;
static int bt_on      = 0;
static int battery    = 75;
static int signal_lvl = 60;
static int demo_val   = 40;

static int volume      = 70;
static int eq_idx      = 0;
static int sleep_idx   = 1;
static int time_fmt    = 0;
static int lang_idx    = 0;
static int unit_idx    = 0;

static const char *const eq_names[]    = { "Flat", "Bass", "Treble", "Voice" };
static const char *const sleep_names[] = { "1 min", "5 min", "10 min", "Never" };
static const char *const fmt_names[]   = { "24h", "12h" };
static const char *const lang_names[]  = { "English", "Русский", "Deutsch", "French" };
static const char *const unit_names[]  = { "Metric", "Imperial" };

// dynamic home screen sensor values
static int  temp_val  = 23;
static int  hum_val   = 55;
static int  pres_val  = 1013;
static char temp_buf[24] = "Temperature: 23 C";
static char hum_buf[24]  = "Humidity:    55 %";
static char pres_buf[24] = "Pressure: 1013 hPa";
static char wifi_buf[16]       = "WiFi: On";
static char brightness_buf[24] = "Bright: 80%";

static const char *const theme_names[]   = { "Dark", "Light" };
static const char *const toggle_names[]  = { "Disabled", "Enabled" };

// W_EDIT buffers
static char wifi_ssid[20] = "Home_Net";
static char bt_name[20]   = "STM32-Device";

// ── forward declarations ─────────────────────────────────────────────────────

static void go_home(void);
static void go_settings(void);
static void go_display(void);
static void go_network(void);
static void go_wifi(void);
static void go_bluetooth(void);
static void go_system(void);
static void go_about(void);
static void go_reset(void);
static void go_border_demo(void);
static void go_demo_list(void);
static void go_fonts_demo(void);

// ── key handlers (cancel = navigate back) ───────────────────────────────────

static int on_key_back_to_home(RenderKey key)
    { if (key == RENDER_KEY_CANCEL) { go_home();     return 1; } return 0; }
static int on_key_back_to_settings(RenderKey key)
    { if (key == RENDER_KEY_CANCEL) { go_settings(); return 1; } return 0; }
static int on_key_back_to_network(RenderKey key)
    { if (key == RENDER_KEY_CANCEL) { go_network();  return 1; } return 0; }
static int on_key_back_to_system(RenderKey key)
    { if (key == RENDER_KEY_CANCEL) { go_system();   return 1; } return 0; }

// ── Home ────────────────────────────────────────────────────────────────────

static int home_on_key(RenderKey key) {
    if (key == RENDER_KEY_INC || key == RENDER_KEY_DEC) {
        if (key == RENDER_KEY_INC) {
           brightness = brightness < 100 ? brightness + 10 : 100;
        }
        if (key == RENDER_KEY_DEC) {
           brightness = brightness > 10 ? brightness - 10 : 10;
        }
        snprintf(brightness_buf, sizeof(brightness_buf), "Bright: %d%%", brightness);
        render_refresh();
        return 1;
    }
    return 0;
}

const Layout home_layout = LAYOUT(home_on_key,
    { .type  = W_LABEL,
      .text  = "HOME SCREEN",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type  = W_LABEL,
      .text  = wifi_buf,
      .align = ALIGN_TOP_RIGHT,
      .x     = 0, .y = 4 },

    { .type  = W_LABEL,
      .text  = temp_buf,
      .align = ALIGN_CENTER,
      .y     = -20 },

    { .type  = W_LABEL,
      .text  = hum_buf,
      .align = ALIGN_CENTER,
      .y     = 0 },

    { .type  = W_LABEL,
      .text  = pres_buf,
      .align = ALIGN_CENTER,
      .y     = 20 },

    { .type  = W_LABEL,
      .text  = brightness_buf,
      .align = ALIGN_CENTER,
      .y     = 46 },

    { .type  = W_LABEL,
      .text  = "Battery",
      .align = ALIGN_CENTER,
      .x     = -20, .y = 62 },

    { .type  = W_PROGRESS,
      .align = ALIGN_CENTER,
      .x = -20, .w = 150, .h = 12,
      .y     = 82,
      .value = &battery,
      .min   = 0,
      .max   = 100 },

    { .type  = W_LABEL,
      .text  = "Sig",
      .align = ALIGN_TOP_LEFT,
      .x = 214, .y = 94 },

    { .type  = W_PROGRESS,
      .align = ALIGN_TOP_LEFT,
      .x = 218, .y = 106,
      .w = 14, .h = 72,
      .value = &signal_lvl,
      .min   = 0,
      .max   = 100 },

    { .type     = W_BTN,
      .text     = "Settings",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = go_settings },
);

// ── Settings (level 1) ──────────────────────────────────────────────────────

const Layout settings_layout = LAYOUT(on_key_back_to_home,
    { .type  = W_LABEL,
      .text  = "SETTINGS",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type     = W_BTN,
      .text     = "Display",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -45,
      .on_click = go_display },

    { .type     = W_BTN,
      .text     = "Network",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -15,
      .on_click = go_network },

    { .type     = W_BTN,
      .text     = "System",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 15,
      .on_click = go_system },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 45,
      .on_click = go_home },
);

// ── Display (level 2) — inline W_VALUE ──────────────────────────────────────
// Pattern: edit value in-place (←→ without navigation)

const Layout display_layout = LAYOUT(on_key_back_to_settings,
    { .type  = W_LABEL,
      .text  = "DISPLAY",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type          = W_VALUE,
      .text          = "Brightness",
      .align         = ALIGN_CENTER,
      .w = 192, .h  = 24,
      .y             = -20,
      .value         = &brightness,
      .min           = 10,
      .max           = 100,
      .step          = 10 },

    { .type           = W_VALUE,
      .text           = "Theme",
      .align          = ALIGN_CENTER,
      .w = 192, .h   = 24,
      .y              = 10,
      .value          = &theme_idx,
      .options        = theme_names,
      .options_count  = 2,
      .on_change      = apply_theme },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = go_settings },
);

// ── Network (level 2) — navigate to sub-screens ─────────────────────────────
// Pattern: dedicated edit screen per section

const Layout network_layout = LAYOUT(on_key_back_to_settings,
    { .type  = W_LABEL,
      .text  = "NETWORK",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type     = W_BTN,
      .text     = "WiFi",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -32,
      .on_click = go_wifi },

    { .type     = W_BTN,
      .text     = "Bluetooth",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 0,
      .on_click = go_bluetooth },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 32,
      .on_click = go_settings },
);

// ── WiFi (level 3) — W_VALUE + W_EDIT ───────────────────────────────────────

const Layout wifi_layout = LAYOUT(on_key_back_to_network,
    { .type  = W_LABEL,
      .text  = "WIFI",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type           = W_VALUE,
      .text           = "Status",
      .align          = ALIGN_CENTER,
      .w = 180, .h   = 24,
      .y              = -35,
      .value          = &wifi_on,
      .options        = toggle_names,
      .options_count  = 2 },

    { .type    = W_EDIT,
      .text    = "SSID",
      .align   = ALIGN_CENTER,
      .w = 180, .h = 24,
      .y       = -5,
      .buf     = wifi_ssid,
      .buf_len = sizeof(wifi_ssid) },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = go_network },
);

// ── Bluetooth (level 3) — W_VALUE + W_EDIT ──────────────────────────────────

const Layout bluetooth_layout = LAYOUT(on_key_back_to_network,
    { .type  = W_LABEL,
      .text  = "BLUETOOTH",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type           = W_VALUE,
      .text           = "Status",
      .align          = ALIGN_CENTER,
      .w = 180, .h   = 24,
      .y              = -35,
      .value          = &bt_on,
      .options        = toggle_names,
      .options_count  = 2 },

    { .type    = W_EDIT,
      .text    = "Name",
      .align   = ALIGN_CENTER,
      .w = 180, .h = 24,
      .y       = -5,
      .buf     = bt_name,
      .buf_len = sizeof(bt_name) },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = go_network },
);

// ── System (level 2) ────────────────────────────────────────────────────────

const Layout system_layout = LAYOUT(on_key_back_to_settings,
    { .type  = W_LABEL,
      .text  = "SYSTEM",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type     = W_BTN,
      .text     = "About",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -80,
      .on_click = go_about },

    { .type     = W_BTN,
      .text     = "Border Demo",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -48,
      .on_click = go_border_demo },

    { .type     = W_BTN,
      .text     = "List Demo",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = -16,
      .on_click = go_demo_list },

    { .type     = W_BTN,
      .text     = "Font Demo",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 16,
      .on_click = go_fonts_demo },

    { .type     = W_BTN,
      .text     = "Reset",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 48,
      .on_click = go_reset },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_CENTER,
      .w = 128, .h = 24,
      .y = 80,
      .on_click = go_settings },
);

// ── About (level 3) ─────────────────────────────────────────────────────────

const Layout about_layout = LAYOUT(on_key_back_to_system,
    { .type  = W_LABEL,
      .text  = "ABOUT",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type  = W_LABEL,
      .text  = "FW:  v1.0.0",
      .align = ALIGN_CENTER,
      .y     = -20 },

    { .type  = W_LABEL,
      .text  = "MCU: STM32G431",
      .align = ALIGN_CENTER,
      .y     = 0 },

    { .type  = W_LABEL,
      .text  = "LCD: ST7789 240x240",
      .align = ALIGN_CENTER,
      .y     = 20 },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = go_system },
);

// ── Reset confirmation (level 3) ────────────────────────────────────────────

static const WidgetColors danger_label = { .fg = 0xF800 };                // red text
static const WidgetColors danger_btn   = { .fg = 0xFFFF, .bg = 0xC000 }; // white on dark red

const Layout reset_layout = LAYOUT(on_key_back_to_system,
    { .type   = W_LABEL,
      .text   = "RESET DEVICE?",
      .colors = &danger_label,
      .align  = ALIGN_TOP_MID,
      .y      = 4 },

    { .type  = W_LABEL,
      .text  = "All data will be lost.",
      .align = ALIGN_CENTER,
      .y     = -20 },

    { .type     = W_BTN,
      .text     = "Yes",
      .colors   = &danger_btn,
      .align    = ALIGN_CENTER,
      .w = 80, .h = 24,
      .x = -50, .y = 20,
      .on_click = go_home },

    { .type     = W_BTN,
      .text     = "No",
      .align    = ALIGN_CENTER,
      .w = 80, .h = 24,
      .x = 50, .y = 20,
      .on_click = go_system },
);

// ── Border demo (level 3) ────────────────────────────────────────────────────

const Layout border_demo_layout = LAYOUT(on_key_back_to_system,
    { .type  = W_LABEL,
      .text  = "BORDER DEMO",
      .align = ALIGN_TOP_MID,
      .y     = 4 },

    { .type     = W_BTN,
      .text     = "Button A",
      .align    = ALIGN_CENTER,
      .w = 160, .h = 24,
      .y        = -60,
      .on_click = go_border_demo },

    { .type     = W_BTN,
      .text     = "Button B",
      .align    = ALIGN_CENTER,
      .w = 160, .h = 24,
      .y        = -30,
      .on_click = go_border_demo },

    { .type  = W_VALUE,
      .text  = "Value",
      .align = ALIGN_CENTER,
      .w = 160, .h = 24,
      .y     = 0,
      .value = &demo_val,
      .min   = 0, .max = 100, .step = 10 },

    { .type  = W_PROGRESS,
      .align = ALIGN_CENTER,
      .w = 160, .h = 12,
      .y     = 36,
      .value = &demo_val,
      .min   = 0, .max = 100 },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y        = -16,
      .on_click = go_system },
);

// ── Demo list ────────────────────────────────────────────────────────────────

const ListLayout demo_list = LIST_LAYOUT(on_key_back_to_system,
    { .type = LI_LABEL, .text = "DISPLAY" },
    { .type = LI_VALUE, .text = "Brightness",
      .value = &brightness, .min = 10, .max = 100, .step = 10 },
    { .type = LI_VALUE, .text = "Theme",
      .value = &theme_idx, .options = theme_names, .options_count = 2,
      .on_change = apply_theme },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "AUDIO" },
    { .type = LI_VALUE, .text = "Volume",
      .value = &volume, .min = 0, .max = 100, .step = 5 },
    { .type = LI_VALUE, .text = "Equalizer",
      .value = &eq_idx, .options = eq_names, .options_count = 4 },
    { .type = LI_BTN,   .text = "Test tone" },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "NETWORK" },
    { .type = LI_BTN, .text = "WiFi",      .on_click = go_wifi },
    { .type = LI_BTN, .text = "Bluetooth", .on_click = go_bluetooth },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "POWER" },
    { .type = LI_VALUE, .text = "Sleep after",
      .value = &sleep_idx, .options = sleep_names, .options_count = 4 },
    { .type = LI_BTN,   .text = "Sleep now" },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "LOCALE" },
    { .type = LI_VALUE, .text = "Language",
      .value = &lang_idx, .options = lang_names, .options_count = 4 },
    { .type = LI_VALUE, .text = "Units",
      .value = &unit_idx, .options = unit_names, .options_count = 2 },
    { .type = LI_VALUE, .text = "Time format",
      .value = &time_fmt, .options = fmt_names, .options_count = 2 },

    { .type = LI_SEPARATOR },

    { .type = LI_LABEL, .text = "SYSTEM" },
    { .type = LI_BTN, .text = "About",  .on_click = go_about },
    { .type = LI_BTN, .text = "Reset",  .on_click = go_reset },

    { .type = LI_SEPARATOR },

    { .type = LI_BTN, .text = "Back", .on_click = go_system },
);

// ── Font demo ────────────────────────────────────────────────────────────────

static const WidgetColors hint_color = { .fg = 0x8410 };

const Layout fonts_demo_layout = LAYOUT(on_key_back_to_system,
    { .type = W_LABEL, .text = "FONT SIZES",
      .align = ALIGN_TOP_MID, .y = 2 },

    { .type = W_LABEL, .text = "Noto Sans",
      .colors = &hint_color, .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 14 },
    { .type = W_LABEL, .text = "10: Abc Def 012 !?",
      .font = { .w = 9, .h = 10, .name = "noto10" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 24 },
    { .type = W_LABEL, .text = "12: Abc Def 012 !?",
      .font = { .w = 11, .h = 12, .name = "noto12" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 36 },
    { .type = W_LABEL, .text = "16: Abc Def 012",
      .font = { .w = 15, .h = 16, .name = "noto16" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 50 },
    { .type = W_LABEL, .text = "20: Abc 012",
      .font = { .w = 18, .h = 20, .name = "noto20" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 68 },

    { .type = W_LABEL, .text = "Roboto",
      .colors = &hint_color, .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 92 },
    { .type = W_LABEL, .text = "10: Abc Def 012 !?",
      .font = { .w = 9, .h = 12, .name = "roboto10" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 102 },
    { .type = W_LABEL, .text = "12: Abc Def 012 !?",
      .font = { .w = 10, .h = 14, .name = "roboto12" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 116 },
    { .type = W_LABEL, .text = "16: Abc Def 012",
      .font = { .w = 14, .h = 18, .name = "roboto16" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 132 },
    { .type = W_LABEL, .text = "20: Abc 012",
      .font = { .w = 16, .h = 22, .name = "roboto20" },
      .align = ALIGN_TOP_LEFT, .w = 1, .x = 4, .y = 152 },

    { .type = W_BTN, .text = "Back",
      .align = ALIGN_BOTTOM_MID, .w = 96, .h = 24, .y = -16,
      .on_click = go_system },
);

// ── tick ─────────────────────────────────────────────────────────────────────

void layouts_tick(void) {
    static int n = 0;
    n++;

    static int temp_dir = 1;
    static int hum_dir  = 1;
    static int pres_dir = -1;
    static int sig_dir  = 1;

    if (n % 8 == 0) {
        temp_val += temp_dir;
        if (temp_val >= 26) temp_dir = -1;
        if (temp_val <= 19) temp_dir =  1;
        snprintf(temp_buf, sizeof(temp_buf), "Temperature: %d C", temp_val);
    }
    if (n % 12 == 0) {
        hum_val += hum_dir;
        if (hum_val >= 65) hum_dir = -1;
        if (hum_val <= 48) hum_dir =  1;
        snprintf(hum_buf, sizeof(hum_buf), "Humidity:    %d %%", hum_val);
    }
    if (n % 20 == 0) {
        pres_val += pres_dir;
        if (pres_val >= 1020) pres_dir = -1;
        if (pres_val <= 1008) pres_dir =  1;
        snprintf(pres_buf, sizeof(pres_buf), "Pressure: %d hPa", pres_val);
    }
    if (n % 40 == 0) {
        battery--;
        if (battery < 0) battery = 100;
    }
    if (n % 5 == 0) {
        signal_lvl += sig_dir * 2;
        if (signal_lvl >= 95) sig_dir = -1;
        if (signal_lvl <= 20) sig_dir =  1;
    }

    snprintf(wifi_buf, sizeof(wifi_buf), "WiFi: %s", wifi_on ? "On" : "Off");
}

// ── callbacks ────────────────────────────────────────────────────────────────

static void go_home(void)      { render_screen(&home_layout); }
static void go_settings(void)  { render_screen(&settings_layout); }
static void go_display(void)   { render_screen(&display_layout); }
static void go_network(void)   { render_screen(&network_layout); }
static void go_wifi(void)      { render_screen(&wifi_layout); }
static void go_bluetooth(void) { render_screen(&bluetooth_layout); }
static void go_system(void)    { render_screen(&system_layout); }
static void go_about(void)       { render_screen(&about_layout); }
static void go_reset(void)       { render_screen(&reset_layout); }
static void go_border_demo(void) { render_screen(&border_demo_layout); }
static void go_demo_list(void)   { render_list(&demo_list); }
static void go_fonts_demo(void)  { render_screen(&fonts_demo_layout); }
