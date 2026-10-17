# The level table of antic and the one the build reads are the same.
# src/antic/cpu.c holds it for the compiler and tools/cpu-levels for CMake, so
# this test compares `antic --print-cpu-levels` with that file. Run with
# cmake -P and these values:
#   ANTIC   the antic executable
#   LEVELS  tools/cpu-levels

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

antic_build(OUTPUT printed COMMAND --print-cpu-levels)
file(STRINGS "${LEVELS}" lines REGEX "^(level|default) ")
set(pinned "")
foreach(line IN LISTS lines)
    string(APPEND pinned "${line}\n")
endforeach()
expect_same_text("antic --print-cpu-levels, against ${LEVELS},"
                 "${pinned}" "${printed}")
