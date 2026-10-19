# The six targets of antic are listed once, by the default rows of
# tools/cpu-levels, which antic_read_cpu_levels of tools/cpu-levels.cmake
# reads into ANTIC_TARGETS. The check refuses a list of all six spelled in
# a build file, a script of tools/ or src/native/ or a runner, where two
# lines in a row name every target. Run with cmake -P and ROOT, the root
# of the repository.
#
# tools/release.sh is a shell script, which reads no CMake. Its list of
# hosts gives the order of the downloads page, and the check requires it
# to hold the six targets of the table and nothing else.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels.cmake")
antic_read_cpu_levels("${ROOT}/tools/cpu-levels")
list(LENGTH ANTIC_TARGETS count)
if(NOT count EQUAL 6)
    message(FATAL_ERROR "tools/cpu-levels names ${count} targets: "
                        "${ANTIC_TARGETS}")
endif()

# The expected output of anti, which names the six targets as it prints
# them, is the one list a runner holds.
set(allowed tests/runners/run_target_lists.cmake
    tests/runners/run_anti_options.cmake)
file(GLOB files RELATIVE "${ROOT}"
    "${ROOT}/CMakeLists.txt" "${ROOT}/tools/*.cmake"
    "${ROOT}/src/native/*.cmake" "${ROOT}/src/native/CMakeLists.txt"
    "${ROOT}/tests/CMakeLists.txt" "${ROOT}/tests/*.cmake"
    "${ROOT}/tests/runners/*.cmake")
set(failures "")
foreach(path IN LISTS files)
    if(path IN_LIST allowed)
        continue()
    endif()
    file(STRINGS "${ROOT}/${path}" lines)
    set(previous "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        string(REGEX REPLACE "#.*" "" code "${line}")
        set(pair "${previous} ${code} ")
        set(all ON)
        foreach(target IN LISTS ANTIC_TARGETS)
            if(NOT pair MATCHES "[ \"(;]${target}[ \";)]")
                set(all OFF)
            endif()
        endforeach()
        if(all)
            string(APPEND failures "\n${path}:${number}")
        endif()
        set(previous "${code}")
    endforeach()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "the six targets spelled outside tools/cpu-levels:"
                        "${failures}")
endif()

file(STRINGS "${ROOT}/tools/release.sh" row REGEX "^hosts='")
string(REGEX REPLACE "^hosts='(.*)'$" "\\1" hosts "${row}")
separate_arguments(hosts UNIX_COMMAND "${hosts}")
set(sorted_hosts ${hosts})
set(sorted_targets ${ANTIC_TARGETS})
list(SORT sorted_hosts)
list(SORT sorted_targets)
if(NOT sorted_hosts STREQUAL sorted_targets)
    message(FATAL_ERROR "tools/release.sh names the hosts ${hosts}, and "
                        "tools/cpu-levels the targets ${ANTIC_TARGETS}")
endif()
