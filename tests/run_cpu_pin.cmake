# The level table of antic and the one the build reads are the same.
# src/antic/cpu.c holds it for the compiler and tools/cpu-levels for CMake, so
# this test compares `antic --print-cpu-levels` with that file. Run with
# cmake -P and these values:
#   ANTIC   the antic executable
#   LEVELS  tools/cpu-levels

execute_process(COMMAND "${ANTIC}" --print-cpu-levels
    RESULT_VARIABLE status OUTPUT_VARIABLE printed ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic --print-cpu-levels failed\n${err}")
endif()
file(STRINGS "${LEVELS}" lines REGEX "^(level|default) ")
set(pinned "")
foreach(line IN LISTS lines)
    string(APPEND pinned "${line}\n")
endforeach()
if(NOT printed STREQUAL pinned)
    message(FATAL_ERROR
        "antic --print-cpu-levels differs from ${LEVELS}\nantic:\n${printed}"
        "file:\n${pinned}")
endif()
