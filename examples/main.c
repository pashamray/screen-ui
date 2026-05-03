#include "render.h"
#include "layouts.h"

int main(void) {
    render_init();
    render_screen(&home_layout);
    render_wait_key();
    render_quit();
    return 0;
}
