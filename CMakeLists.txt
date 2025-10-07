cmake_minimum_required(VERSION 3.15)
project(toml_parser C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/toml.c
    src/main.c
)

add_executable(toml_parser ${SOURCES})

if(MSVC)
    target_compile_options(toml_parser PRIVATE /W4)
else()
    target_compile_options(toml_parser PRIVATE -Wall -Wextra -pedantic)
endif()