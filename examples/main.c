#include "render.h"
#include "layouts.h"

void render_init(void);
void render_wait_key(void);
void render_quit(void);

int main(void) {
    render_init();
    render_screen(&home_layout);
    render_wait_key();
    render_quit();
    return 0;
}
