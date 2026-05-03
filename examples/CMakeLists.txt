find_package(SDL2 REQUIRED)

set(FONT_SOURCES
    ../fonts/font_8x8.c
    ../fonts/font_mono16.c
    ../fonts/font_noto10.c
    ../fonts/font_noto12.c
    ../fonts/font_noto16.c
    ../fonts/font_noto20.c
    ../fonts/font_roboto10.c
    ../fonts/font_roboto12.c
    ../fonts/font_roboto16.c
    ../fonts/font_roboto20.c
)

add_executable(screen-ui-console
    main.c
    layouts.c
    render_impl_console.c
    ${FONT_SOURCES}
)
target_link_libraries(screen-ui-console screen-ui-core)
target_compile_options(screen-ui-console PRIVATE -Wall -Wextra)

add_executable(screen-ui-sdl
    main.c
    layouts.c
    render_impl_sdl.c
    ${FONT_SOURCES}
)
target_link_libraries(screen-ui-sdl screen-ui-core SDL2::SDL2)
target_compile_options(screen-ui-sdl PRIVATE -Wall -Wextra)
