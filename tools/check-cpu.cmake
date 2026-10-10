# Refuse a package whose runtime levels are not the ones of tools/cpu-levels.
#
#   cmake -DARCHIVE=<anti-*.tar.xz> -DLEVELS=<tools/cpu-levels>
#         -P tools/check-cpu.cmake
#   cmake -DTREE=<directory> -DLEVELS=<tools/cpu-levels>
#         -P tools/check-cpu.cmake
#
# DESIGN: a program built with --cpu below the default of its target links
# the runtime of its own level, so the archive holds one runtime per level
# of every target. A package that lacks one links the wrong library or
# nothing at all, and the machine that finds out is the user's. The levels
# of the build stand in tools/cpu-levels, so the package is read against
# that file rather than against a list written here a second time.
#
# ARCHIVE reads the entries of the tarball and unpacks nothing. TREE reads
# an unpacked package, whose top directory is anti.
#
# DESIGN: the runtime of the glibc link mode of a Linux target stands in
# lib/<target>-glibc/ at the levels of that target, and every package
# carries it since the step glibc of docs/work-order-distribution.md. The
# check reads that directory as it reads the target's own, so a package
# without one glibc level is refused like one without a musl level.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED LEVELS OR (NOT DEFINED ARCHIVE AND NOT DEFINED TREE))
    message(FATAL_ERROR "usage: cmake -DARCHIVE=<file> -DLEVELS=<file> "
                        "-P tools/check-cpu.cmake")
endif()

# The levels of each architecture and the targets, from tools/cpu-levels.
file(STRINGS "${LEVELS}" level_lines REGEX "^level ")
foreach(line IN LISTS level_lines)
    string(REPLACE " " ";" fields "${line}")
    list(GET fields 1 name)
    list(GET fields 2 arch)
    list(APPEND levels_${arch} "${name}")
endforeach()
file(STRINGS "${LEVELS}" default_lines REGEX "^default ")
set(targets "")
foreach(line IN LISTS default_lines)
    string(REPLACE " " ";" fields "${line}")
    list(GET fields 1 target)
    list(APPEND targets "${target}")
endforeach()
if(targets STREQUAL "")
    message(FATAL_ERROR "check-cpu: ${LEVELS} names no target")
endif()
# The directories under lib/: each target, and the glibc twin of a Linux one.
set(runtime_dirs "")
foreach(target IN LISTS targets)
    list(APPEND runtime_dirs "${target}")
    if(target MATCHES "^linux-")
        list(APPEND runtime_dirs "${target}-glibc")
    endif()
endforeach()

if(DEFINED ARCHIVE)
    if(NOT EXISTS "${ARCHIVE}")
        message(FATAL_ERROR "check-cpu: no such file ${ARCHIVE}")
    endif()
    get_filename_component(directory "${ARCHIVE}" DIRECTORY)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${ARCHIVE}"
                    WORKING_DIRECTORY "${directory}"
                    OUTPUT_VARIABLE entries RESULT_VARIABLE listed
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT listed EQUAL 0)
        message(FATAL_ERROR "check-cpu: ${ARCHIVE} does not list\n${err}")
    endif()
    set(what "${ARCHIVE}")
else()
    set(entries "")
    file(GLOB_RECURSE found RELATIVE "${TREE}" "${TREE}/anti/lib/*")
    foreach(name IN LISTS found)
        string(APPEND entries "${name}\n")
    endforeach()
    set(what "${TREE}")
endif()

set(missing "")
set(checked 0)
foreach(target IN LISTS runtime_dirs)
    if(target MATCHES "arm64(-glibc)?$")
        set(wanted "${levels_arm64}")
    else()
        set(wanted "${levels_x86_64}")
    endif()
    set(library "libanti_rt.a")
    if(target MATCHES "^windows-")
        set(library "anti_rt.lib")
    endif()
    foreach(level IN LISTS wanted)
        set(entry "anti/lib/${target}/${level}/${library}")
        if(NOT entries MATCHES "(^|\n)${entry}(\n|$)")
            list(APPEND missing "${entry}")
        endif()
        math(EXPR checked "${checked} + 1")
    endforeach()
endforeach()

# A level the package carries and tools/cpu-levels does not name is a
# runtime that no program of this version asks for.
string(REGEX MATCHALL "anti/lib/[^/\n]+/[^/\n]+/[^/\n]*anti_rt[^/\n]*"
       carried "${entries}")
set(extra "")
foreach(entry IN LISTS carried)
    string(REGEX REPLACE "^anti/lib/([^/]+)/([^/]+)/.*$" "\\1;\\2" pair "${entry}")
    list(GET pair 0 target)
    list(GET pair 1 level)
    if(target MATCHES "arm64(-glibc)?$")
        set(wanted "${levels_arm64}")
    else()
        set(wanted "${levels_x86_64}")
    endif()
    if(NOT target IN_LIST runtime_dirs OR NOT level IN_LIST wanted)
        list(APPEND extra "anti/lib/${target}/${level}")
    endif()
endforeach()
list(REMOVE_DUPLICATES extra)

if(NOT missing STREQUAL "")
    string(REPLACE ";" "\n  " printed "${missing}")
    message(FATAL_ERROR "check-cpu: ${what} lacks the runtime of\n  ${printed}")
endif()
if(NOT extra STREQUAL "")
    string(REPLACE ";" "\n  " printed "${extra}")
    message(FATAL_ERROR "check-cpu: ${what} carries a runtime that "
                        "${LEVELS} does not name, in\n  ${printed}")
endif()
message(STATUS "${what}: ${checked} runtimes, one per level of each target and glibc mode")
