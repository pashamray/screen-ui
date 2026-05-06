#pragma once
#include "widget.h"
#include "widget_list.h"

void layouts_init(void);   /* set up initial screen (home + static panels) */
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
