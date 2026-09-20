# Builds the third-party libraries of the runtime archive as static
# libraries for one target triple. Run once per target, from CI or by hand.
# Libraries are added in the chapters that need them: the thread pool
# runtime, raylib, miniaudio, Mbed TLS, PCRE2.

cmake_minimum_required(VERSION 3.21)

# The pinned clang compiles the libraries, as it compiles antic.
include(../tools/pinned-compiler.cmake)

project(antic-runtime LANGUAGES C)

set(ANTIC_RUNTIME_TARGET "" CACHE STRING
    "Target directory name, for example macos-arm64 or windows-x86_64")

if(ANTIC_RUNTIME_TARGET STREQUAL "")
    message(FATAL_ERROR "Set ANTIC_RUNTIME_TARGET, for example -DANTIC_RUNTIME_TARGET=macos-arm64")
endif()

set(ANTIC_RUNTIME_OUTPUT
    "${CMAKE_BINARY_DIR}/runtime/lib/${ANTIC_RUNTIME_TARGET}")
file(MAKE_DIRECTORY "${ANTIC_RUNTIME_OUTPUT}")

# DESIGN: every native library of the runtime archive is built for the
# default level of its target, as docs/anti-language-additions.md settles
# under "CPU levels". The levels come from tools/cpu-levels, the file
# src/cpu.c holds the same table for antic, and the test cpu_levels_pin
# compares the two. A library added here takes ANTIC_RUNTIME_C_FLAGS.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/cpu-levels" antic_cpu_lines
    REGEX "^(level|default) ")
foreach(line IN LISTS antic_cpu_lines)
    string(REPLACE " " ";" fields "${line}")
    list(GET fields 0 kind)
    list(GET fields 1 key)
    if(kind STREQUAL "level")
        list(GET fields 4 ANTIC_CPU_MARCH_${key})
    else()
        list(GET fields 2 ANTIC_CPU_LEVEL_${key})
    endif()
endforeach()

set(ANTIC_RUNTIME_LEVEL "${ANTIC_CPU_LEVEL_${ANTIC_RUNTIME_TARGET}}")
if(ANTIC_RUNTIME_LEVEL STREQUAL "")
    message(FATAL_ERROR
        "no default processor level for ${ANTIC_RUNTIME_TARGET} in tools/cpu-levels")
endif()
set(ANTIC_RUNTIME_C_FLAGS "-march=${ANTIC_CPU_MARCH_${ANTIC_RUNTIME_LEVEL}}")
add_compile_options("${ANTIC_RUNTIME_C_FLAGS}")
message(STATUS
    "Runtime libraries for ${ANTIC_RUNTIME_TARGET} at level ${ANTIC_RUNTIME_LEVEL}")
