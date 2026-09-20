# Refuse a use of a find_program variable above the line that finds it.
#
#   cmake -DROOT=<repository> -P tests/run_find_order.cmake
#
# DESIGN: find_program writes a cache entry, and a build directory that
# already holds one reads the value before the call. A fresh one does
# not, so a use above the call takes an empty string there and the right
# path everywhere else. The generated test then runs a tool named by
# nothing. cpu_check_refuses passed here for months and failed in the
# first export of the release script, which configures a build directory
# that no earlier run filled. The order is what decides it, so the order
# is what this reads.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ROOT)
    message(FATAL_ERROR "usage: cmake -DROOT=<repository> "
                        "-P tests/run_find_order.cmake")
endif()

# The files in the order CMake reads them. add_subdirectory(tests) is
# the last line of CMakeLists.txt, so a name found there is set before
# the first line of tests/CMakeLists.txt.
set(files CMakeLists.txt tests/CMakeLists.txt)
set(findings "")
set(found "")
set(used "")
foreach(file IN LISTS files)
    # One list item per line, empty lines and semicolons included, so
    # that the number a finding names is the number of the editor.
    file(READ "${ROOT}/${file}" text)
    string(REPLACE ";" "\\;" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "find_program\\(([A-Za-z0-9_]+)")
            set(name "${CMAKE_MATCH_1}")
            list(APPEND found "${name}")
            if("${name}" IN_LIST used)
                list(APPEND findings
                     "${file}: ${name} is read above the find_program of line ${number}")
            endif()
        endif()
        string(REGEX MATCHALL "\\$\\{([A-Za-z0-9_]+)\\}" reads "${line}")
        foreach(read IN LISTS reads)
            string(REGEX REPLACE "^\\$\\{(.*)\\}$" "\\1" name "${read}")
            if(NOT "${name}" IN_LIST found)
                list(APPEND used "${name}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(NOT findings STREQUAL "")
    string(REPLACE ";" "\n  " printed "${findings}")
    message(FATAL_ERROR "a fresh build directory reads an empty value:\n  ${printed}")
endif()
