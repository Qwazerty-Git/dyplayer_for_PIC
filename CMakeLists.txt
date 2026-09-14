cmake_minimum_required(VERSION 3.10)

add_library(dyplayer STATIC
    src/DYPlayer_PIC.c
)

target_include_directories(dyplayer
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)