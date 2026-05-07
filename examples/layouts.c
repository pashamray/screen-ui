#include "layouts.h"
#include "render.h"
#include "fonts.h"
#include <stdio.h>

/* ── fonts ──────────────────────────────────────────────────────────────── */

static int font_idx = 4;

static const Font *const font_list[] = {
    &font_terminus12, &font_terminus14, &font_terminus16,
    &font_terminus18, &font_terminus20, &font_terminus22, &font_terminus24,
};
static const char *const font_names[] = {
    "12", "14", "16", "18", "20", "22", "24",
};

static void apply_font(int idx) { render_set_font(font_list[idx]); }

/* ── themes ─────────────────────────────────────────────────────────────── */

static const Theme theme_dark = {
    .screen_bg      = 0x0000U,
    .text_fg        = 0xFFFFU,
    .widget_bg      = 0x2104U,
    .border_normal  = 0xFFFFU,
    .border_focused = 0x07FFU,
    .border_editing = 0xFFE0U,
    .border_subtle  = 0x4208U,
    .cursor_fg      = 0x0000U,
    .cursor_bg      = 0x07FFU,
    .hint_fg        = 0x8410U,
};

static const Theme theme_light = {
    .screen_bg      = 0xFFFFU,
    .text_fg        = 0x0000U,
    .widget_bg      = 0xC618U,
    .border_normal  = 0x8410U,
    .border_focused = 0x041FU,
    .border_editing = 0xF800U,
    .border_subtle  = 0xD69AU,
    .cursor_fg      = 0xFFFFU,
    .cursor_bg      = 0x041FU,
    .hint_fg        = 0x8410U,
};

static const Theme *const themes[] = { &theme_dark, &theme_light };

static void apply_theme(int idx) {
    render_set_theme(themes[idx]);
}

/* ── mutable state ──────────────────────────────────────────────────────── */

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
static const char *const lang_names[]  = { "English", "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9", "Deutsch", "French" };
static const char *const unit_names[]  = { "Metric", "Imperial" };

/* dynamic home screen sensor values */
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

/* W_EDIT buffers */
static char wifi_ssid[20] = "Home_Net";
static char bt_name[20]   = "STM32-Device";

/* ── forward declarations ─────────────────────────────────────────────────── */

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
static void go_scan_demo(void);

/* ── shared screen header ───────────────────────────────────────────────── */

/* Title text updated by open_screen() before each render_screen_at() call.
   The Widget holds a pointer to this buffer, so changing the contents is safe. */
static char header_title[32] = "";

static const Layout screen_header = LAYOUT_BG_BB(NULL, 0x001FU, 0x07FFU,
    { .type  = W_LABEL, .text = header_title,
      .align = ALIGN_TOP_MID, .y = 4 },
);

/* Open a sub-screen with the standard blue header bar.
   All content layouts occupy y=24..320 (296 px high). */
static void open_screen(const Layout *layout, const char *title) {
    (void)snprintf(header_title, sizeof(header_title), "%s", title);
    render_screen_at(layout, 0, 24, 240, 296);
    render_add_static(&screen_header, 0, 0, 240, 24);
}

/* ── key handlers (cancel = navigate back) ──────────────────────────────── */

/* ── Home header / footer (static panels) ──────────────────────────────── */

static const Layout home_header = LAYOUT_BG_BB(NULL, 0x001FU, 0x07FFU,
    { .type  = W_LABEL, .text = "HOME",
      .align = ALIGN_TOP_MID, .y = 4 },
    { .type  = W_LABEL, .text = wifi_buf,
      .align = ALIGN_TOP_RIGHT, .x = -4, .y = 4 },
);

static const Layout home_footer = LAYOUT_WIDGET_BG_BT(NULL, 0x4208U,
    { .type  = W_LABEL, .text = "screen-ui",
      .align = ALIGN_TOP_MID, .y = 4 },
);

/* ── Home ─────────────────────────────────────────────────────────────────── */

static int home_on_key(RenderKey key) {
    if ((key == RENDER_KEY_INC) || (key == RENDER_KEY_DEC)) {
        if (key == RENDER_KEY_INC) {
            brightness = (brightness < 100) ? (brightness + 10) : 100;
        }
        if (key == RENDER_KEY_DEC) {
            brightness = (brightness > 10) ? (brightness - 10) : 10;
        }
        snprintf(brightness_buf, sizeof(brightness_buf), "Bright: %d%%", brightness);
        return 1;
    }
    return 0;
}

const Layout home_layout = LAYOUT(home_on_key,
    /* sensors — ALIGN_CENTER: center of ctx (oy=24, ch=272) = abs 24+136 = 160,
       same as center of full 320px screen, so y-offsets are unchanged */
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

    /* signal — ALIGN_TOP_LEFT: subtract header height (24) to keep same absolute pos */
    { .type  = W_LABEL,
      .text  = "Sig",
      .align = ALIGN_TOP_LEFT,
      .x = 214, .y = 70 },

    { .type  = W_PROGRESS,
      .align = ALIGN_TOP_LEFT,
      .x = 218, .y = 82,
      .w = 14, .h = 72,
      .value = &signal_lvl,
      .min   = 0,
      .max   = 100 },

    /* Settings — ALIGN_BOTTOM_MID: aligns to content bottom (y=296), not screen bottom */
    { .type     = W_BTN,
      .text     = "Settings",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -4,
      .on_click = go_settings },
);

/* ── Settings (level 1) ──────────────────────────────────────────────────── */

const Layout settings_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── Display (level 2) — inline W_VALUE ────────────────────────────────── */

const Layout display_layout = LAYOUT(NULL,
    { .type          = W_VALUE,
      .text          = "Brightness",
      .align         = ALIGN_CENTER,
      .w = 192, .h  = 24,
      .y             = -30,
      .value         = &brightness,
      .min           = 10,
      .max           = 100,
      .step          = 10 },

    { .type           = W_VALUE,
      .text           = "Theme",
      .align          = ALIGN_CENTER,
      .w = 192, .h   = 24,
      .y              = 0,
      .value          = &theme_idx,
      .options        = theme_names,
      .options_count  = 2,
      .on_change      = apply_theme },

    { .type           = W_VALUE,
      .text           = "Font",
      .align          = ALIGN_CENTER,
      .w = 192, .h   = 24,
      .y              = 30,
      .value          = &font_idx,
      .options        = font_names,
      .options_count  = 7,
      .on_change      = apply_font },

    { .type     = W_BTN,
      .text     = "Back",
      .align    = ALIGN_BOTTOM_MID,
      .w = 96, .h = 24,
      .y = -16,
      .on_click = render_back },
);

/* ── Network (level 2) ─────────────────────────────────────────────────── */

const Layout network_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── WiFi (level 3) — W_VALUE + W_EDIT ─────────────────────────────────── */

const Layout wifi_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── Bluetooth (level 3) — W_VALUE + W_EDIT ─────────────────────────────── */

const Layout bluetooth_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── System (level 2) ────────────────────────────────────────────────────── */

const Layout system_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── About (level 3) ──────────────────────────────────────────────────────── */

const Layout about_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── Reset confirmation (level 3) ─────────────────────────────────────────── */

static const WidgetColors danger_label = { .fg = 0xF800U };                /* red text */
static const WidgetColors danger_btn   = { .fg = 0xFFFFU, .bg = 0xC000U }; /* white on dark red */

const Layout reset_layout = LAYOUT(NULL,
    { .type  = W_LABEL,
      .text  = "All data will be lost.",
      .colors = &danger_label,
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

/* ── Border demo (level 3) ─────────────────────────────────────────────────── */

const Layout border_demo_layout = LAYOUT(NULL,
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
      .on_click = render_back },
);

/* ── Scan demo (dynamic list) ──────────────────────────────────────────────── */

#define SCAN_MAX_NETS  6
#define SCAN_BUF_SIZE  (SCAN_MAX_NETS + 1)

static const char *const scan_ssids[SCAN_MAX_NETS] = {
    "HomeNetwork_5G", "DIRECT-Samsung",  "Neighbor_2.4G",
    "iPhone_PS",      "Corp_Guest",      "MikroTik_Office",
};

static ListItem  scan_buf[SCAN_BUF_SIZE];
static uint8_t   scan_total  = 0U;
static uint8_t   scan_nets   = 0U;
static uint8_t   scan_active = 0U;
static int       scan_timer  = 0;

static const ListItem *get_scan_items(uint8_t *out) {
    *out = scan_total;
    return scan_buf;
}

static void leave_scan(void) { scan_active = 0U; render_back(); }

static int on_key_back_to_demo_list(RenderKey key) {
    if (key == RENDER_KEY_CANCEL) {
        leave_scan();
        return 1;
    }
    return 0;
}

static const ListLayout scan_list = {
    .get_items = get_scan_items,
    .on_key    = on_key_back_to_demo_list,
};

static void go_scan_demo(void) {
    scan_timer  = 0;
    scan_nets   = 0U;
    scan_active = 1U;
    scan_buf[0] = (ListItem){ .type = LI_LABEL, .text = "Scanning..." };
    scan_buf[1] = (ListItem){ .type = LI_BTN,   .text = "Back", .on_click = leave_scan };
    scan_total  = 2U;
    (void)snprintf(header_title, sizeof(header_title), "%s", "WIFI SCAN");
    render_list_at(&scan_list, 0, 24, 240, 296);
    render_add_static(&screen_header, 0, 0, 240, 24);
}

/* ── Demo list ──────────────────────────────────────────────────────────────── */

const ListLayout demo_list = LIST_LAYOUT(NULL,
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

    { .type = LI_LABEL,   .text = "NETWORK" },
    { .type = LI_CHECK,   .text = "Wi-Fi",      .value = &wifi_on },
    { .type = LI_CHECK,   .text = "Bluetooth",  .value = &bt_on },
    { .type = LI_SUBMENU, .text = "WiFi setup",  .hint = "WPA2", .on_click = go_wifi },
    { .type = LI_SUBMENU, .text = "BT setup",   .hint = "Off",  .on_click = go_bluetooth },

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

    { .type = LI_BTN, .text = "Scan Demo", .on_click = go_scan_demo },

    { .type = LI_SEPARATOR },

    { .type = LI_BTN, .text = "Back", .on_click = render_back },
);

/* ── W_LIST embedded demo ──────────────────────────────────────────────────── */
/* Pattern: scrollable W_LIST occupying content area + fixed Back button */

static const Layout wlist_demo_layout = LAYOUT(NULL,
    /* List fills from y=0 to y=268 within content ctx (oy=24, ch=296);
       leaves 28 px at the bottom for the Back button */
    { .type = W_LIST, .list = &demo_list,
      .x = 0, .y = 0, .w = 240, .h = 268 },

    { .type     = W_BTN, .text = "Back",
      .align    = ALIGN_BOTTOM_MID, .w = 96, .h = 24, .y = -4,
      .on_click = render_back },
);

/* ── tick ────────────────────────────────────────────────────────────────── */

void layouts_tick(void) {
    static int n = 0;
    int changed = 0;

    static int temp_dir = 1;
    static int hum_dir  = 1;
    static int pres_dir = -1;
    static int sig_dir  = 1;

    n++;
    int is_home = (render_current_layout() == &home_layout) ? 1 : 0;

    if (is_home != 0) {
        if ((n % 8) == 0) {
            temp_val += temp_dir;
            if (temp_val >= 26) {
                temp_dir = -1;
            }
            if (temp_val <= 19) {
                temp_dir =  1;
            }
            snprintf(temp_buf, sizeof(temp_buf), "Temperature: %d C", temp_val);
            changed = 1;
        }
        if ((n % 12) == 0) {
            hum_val += hum_dir;
            if (hum_val >= 65) {
                hum_dir = -1;
            }
            if (hum_val <= 48) {
                hum_dir =  1;
            }
            snprintf(hum_buf, sizeof(hum_buf), "Humidity:    %d %%", hum_val);
            changed = 1;
        }
        if ((n % 20) == 0) {
            pres_val += pres_dir;
            if (pres_val >= 1020) {
                pres_dir = -1;
            }
            if (pres_val <= 1008) {
                pres_dir =  1;
            }
            snprintf(pres_buf, sizeof(pres_buf), "Pressure: %d hPa", pres_val);
            changed = 1;
        }
        if ((n % 40) == 0) {
            battery--;
            if (battery < 0) {
                battery = 100;
            }
            changed = 1;
        }
        if ((n % 5) == 0) {
            signal_lvl += (sig_dir * 2);
            if (signal_lvl >= 95) {
                sig_dir = -1;
            }
            if (signal_lvl <= 20) {
                sig_dir =  1;
            }
            changed = 1;
        }

        {
            static int prev_wifi_on = -1;
            if (wifi_on != prev_wifi_on) {
                snprintf(wifi_buf, sizeof(wifi_buf), "WiFi: %s", (wifi_on != 0) ? "On" : "Off");
                prev_wifi_on = wifi_on;
                changed = 1;
            }
        }
    }

    if (scan_active != 0U) {
        scan_timer++;
        if (((scan_timer % 10) == 0) && (scan_nets < (uint8_t)SCAN_MAX_NETS)) {
            scan_buf[(int)scan_total - 2] = (ListItem){
                .type = LI_BTN, .text = scan_ssids[scan_nets], .on_click = leave_scan,
            };
            scan_nets++;
            if (scan_nets < (uint8_t)SCAN_MAX_NETS) {
                scan_buf[scan_total] = scan_buf[(int)scan_total - 1];
                scan_buf[(int)scan_total - 1] = (ListItem){ .type = LI_LABEL, .text = "Scanning..." };
                scan_total++;
            } else {
                scan_active = 0U;
            }
            changed = 1;
        }
    }

    if (changed != 0) {
        render_mark_dirty();
    }
}

/* ── callbacks ─────────────────────────────────────────────────────────────── */

static void go_home(void) {
    /* header: y=0..24, content: y=24..296, footer: y=296..320 */
    render_screen_at(&home_layout, 0, 24, 240, 272);
    render_add_static(&home_header, 0, 0, 240, 24);
    render_add_static(&home_footer, 0, 296, 240, 24);
}

static void go_settings(void)    { open_screen(&settings_layout,    "SETTINGS"); }
static void go_display(void)     { open_screen(&display_layout,     "DISPLAY"); }
static void go_network(void)     { open_screen(&network_layout,     "NETWORK"); }
static void go_wifi(void)        { open_screen(&wifi_layout,        "WIFI"); }
static void go_bluetooth(void)   { open_screen(&bluetooth_layout,   "BLUETOOTH"); }
static void go_system(void)      { open_screen(&system_layout,      "SYSTEM"); }
static void go_about(void)       { open_screen(&about_layout,       "ABOUT"); }
static void go_reset(void)       { open_screen(&reset_layout,       "RESET DEVICE?"); }
static void go_border_demo(void) { open_screen(&border_demo_layout, "BORDER DEMO"); }
static void go_demo_list(void)   { open_screen(&wlist_demo_layout,  "SETTINGS (W_LIST)"); }

void layouts_init(void) { go_home(); }
