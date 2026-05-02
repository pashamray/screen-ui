cmake_minimum_required(VERSION 3.16)
project(screen-ui C)

set(CMAKE_C_STANDARD 11)

add_library(screen-ui-core STATIC
    render/render.c
)
target_include_directories(screen-ui-core PUBLIC . render widget fonts theme)
target_compile_options(screen-ui-core PRIVATE -Wall -Wextra)

add_subdirectory(examples)
