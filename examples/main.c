#include "render.h"
#include "layouts.h"

int main(void) {
    render_init();
    layouts_init();
    render_wait_key();
    render_quit();
    return 0;
}
