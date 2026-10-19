# A pin of tools/, a file <name>-pin or sysroot-pins, is read through
# tools/pins.cmake alone: antic_read_pins for all its rows and
# antic_pin_value for one, which refuses a row that is missing or empty.
# The table tools/cpu-levels is read through antic_read_cpu_levels of
# tools/cpu-levels.cmake alone. The check refuses a file(STRINGS) of a pin
# or of the table, the form every reader of a row took, in any build
# file, script of tools/ or src/native/ and runner. A file(READ) of the
# whole text, which a test reads for what the pin must not spell, passes.
# Run with cmake -P and ROOT, the root of the repository.
#
# Three runners read the lines of a pin rather than a value. Two copy
# sysroot-pins into a scratch tree with one block replaced, and look for a
# block that must not be there. run_cpu_pin.cmake compares the rows of
# tools/cpu-levels as text with what antic prints.

cmake_minimum_required(VERSION 3.21)

set(allowed tools/pins.cmake tools/cpu-levels.cmake
    tests/runners/run_pin_readers.cmake
    tests/runners/run_cpu_pin.cmake tests/runners/run_glibc_sysroot.cmake
    tests/runners/run_windows_sysroot.cmake)

file(GLOB files RELATIVE "${ROOT}"
    "${ROOT}/CMakeLists.txt" "${ROOT}/tools/*.cmake"
    "${ROOT}/src/native/*.cmake" "${ROOT}/src/native/CMakeLists.txt"
    "${ROOT}/tests/CMakeLists.txt" "${ROOT}/tests/*.cmake"
    "${ROOT}/tests/runners/*.cmake")
set(reader "file\\(STRINGS[ \t]+\"[^\"]*(-pins?|cpu-levels|CPU_LEVELS\\}|\\$\\{pin\\}|\\$\\{PIN\\}|\\$\\{LEVELS\\})\"")
set(failures "")
foreach(path IN LISTS files)
    if(path IN_LIST allowed)
        continue()
    endif()
    file(STRINGS "${ROOT}/${path}" lines)
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "${reader}")
            string(APPEND failures "\n${path}:${number}: ${line}")
        endif()
    endforeach()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "pins read outside tools/pins.cmake and "
                        "tools/cpu-levels.cmake:${failures}")
endif()
