# tests/runners/run_warning_set.cmake refuses a warning flag in any build
# file outside the two warning files, src/native/CMakeLists.txt among
# them, and accepts a tree whose flags stand in those two. Run with
# cmake -P and these values:
#   RUNNER  tests/runners/run_warning_set.cmake
#   WORK    a directory for the trees
#
# Each case writes a tree, runs the check on it and states whether it
# must accept the tree. A case that must be refused names a text of the
# message, so a refusal for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

set(failures "")
# flag_case(<name> accept|<text of the refusal> [<path>])
# Writes the two warning files with a flag each and, when a path is given,
# a build file there that spells -Wall.
function(flag_case name verdict)
    set(tree "${WORK}/${name}")
    file(WRITE "${tree}/tools/warnings.cmake" "set(flags -Wall)\n")
    file(WRITE "${tree}/src/native/warnings.cmake" "set(flags -Wextra)\n")
    file(WRITE "${tree}/CMakeLists.txt" "project(a C)\n")
    foreach(path IN LISTS ARGN)
        file(WRITE "${tree}/${path}" "target_compile_options(a PRIVATE -Wall)\n")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${tree}"
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept")
        if(NOT status EQUAL 0)
            set(failures "${failures}${name}: refused\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
        return()
    endif()
    # CMake wraps a long message, so the text is read with its spaces and
    # its line ends made one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    string(FIND "${flat}" "${verdict}" at)
    if(at EQUAL -1)
        set(failures
            "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

flag_case(two_files accept)
flag_case(recipe "src/native/zlib.cmake:1: -Wall" src/native/zlib.cmake)
flag_case(native_build_file "src/native/CMakeLists.txt:1: -Wall"
          src/native/CMakeLists.txt)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_warning_set.cmake:\n${failures}")
endif()
