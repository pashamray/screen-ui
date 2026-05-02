find_package(SDL2 REQUIRED)

add_executable(screen-ui-console
    main.c
    layouts.c
    render_impl_console.c
)
target_link_libraries(screen-ui-console screen-ui-core)
target_compile_options(screen-ui-console PRIVATE -Wall -Wextra)

add_executable(screen-ui-sdl
    main.c
    layouts.c
    render_impl_sdl.c
    ../fonts/font_8x8.c
    ../fonts/font_mono16.c
)
target_link_libraries(screen-ui-sdl screen-ui-core SDL2::SDL2)
target_compile_options(screen-ui-sdl PRIVATE -Wall -Wextra)
