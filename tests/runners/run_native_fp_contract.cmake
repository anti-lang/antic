# Every compile of a native library of src/native/ carries
# -ffp-contract=off, so no multiply and add fuse into one instruction and
# a library gives the same results on every target and level, as antic
# does. Eddie decided this by Q7 of "Answers of 2026-11-01" in
# docs/decisions.md. src/native/CMakeLists.txt writes the command of each
# compile, and the test reads them:
#
#   cmake -DCOMPILES=<file> -DNAMES=<library>[,<library>]
#         -DTARGETS=<target>[,<target>]
#         -P tests/runners/run_native_fp_contract.cmake
#
# A line of the file is <library>|<target>[/<level>]|<command>. The last
# -ffp-contract= of a command is the one clang takes, so it must be off.
# Every library of NAMES and every target of TARGETS has a compile.

cmake_minimum_required(VERSION 3.21)

string(REPLACE "," ";" names "${NAMES}")
string(REPLACE "," ";" targets "${TARGETS}")
if(names STREQUAL "" OR targets STREQUAL "")
    message(FATAL_ERROR "NAMES or TARGETS lists nothing")
endif()
file(STRINGS "${COMPILES}" lines)
set(failures "")
set(seen_names "")
set(seen_targets "")
foreach(line IN LISTS lines)
    if(NOT line MATCHES "^([^|]+)\\|([^|/]+)(/[^|]+)?\\|(.*)$")
        string(APPEND failures "\n  a line of no form: ${line}")
        continue()
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(target "${CMAKE_MATCH_2}")
    set(command "${CMAKE_MATCH_4}")
    list(APPEND seen_names "${name}")
    list(APPEND seen_targets "${target}")
    string(REGEX MATCHALL "(^| )-ffp-contract=[^ ]*" contracts "${command}")
    list(LENGTH contracts count)
    set(last "")
    if(count GREATER 0)
        list(GET contracts -1 last)
        string(STRIP "${last}" last)
    endif()
    if(NOT last STREQUAL "-ffp-contract=off")
        string(APPEND failures
               "\n  ${name} on ${target}${CMAKE_MATCH_3} compiles without "
               "-ffp-contract=off: ${command}")
    endif()
endforeach()
foreach(name IN LISTS names)
    if(NOT name IN_LIST seen_names)
        string(APPEND failures "\n  ${name} names no compile")
    endif()
endforeach()
foreach(target IN LISTS targets)
    if(NOT target IN_LIST seen_targets)
        string(APPEND failures "\n  no native library compiles for ${target}")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "the compiles of the native libraries:${failures}")
endif()
