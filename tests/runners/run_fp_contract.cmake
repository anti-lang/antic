# Every compile of a native library of src/native/ and of the runtime of
# src/rt/ carries -ffp-contract=off, so no multiply and add fuse into one
# instruction and each gives the same results on every target and level,
# as antic does. Eddie decided this by Q7 of "Answers of 2026-11-01" for
# the native libraries and by Q1 of "Answers of 2026-11-02" for the
# runtime, in docs/decisions.md. The configure step writes the command of
# each compile, and the test reads them:
#
#   cmake -DCOMPILES=<file> -DNAMES=<library>[,<library>]
#         -DTARGETS=<target>[,<target>] [-DLEVELS=<target>/<level>[,...]]
#         -P tests/runners/run_fp_contract.cmake
#
# A line of the file is <library>|<target>[/<level>]|<command>. The last
# -ffp-contract= of a command is the one clang takes, so it must be off.
# Every library of NAMES, every target of TARGETS and every level of
# LEVELS has a compile.

cmake_minimum_required(VERSION 3.21)

string(REPLACE "," ";" names "${NAMES}")
string(REPLACE "," ";" targets "${TARGETS}")
string(REPLACE "," ";" levels "${LEVELS}")
if(names STREQUAL "" OR targets STREQUAL "")
    message(FATAL_ERROR "NAMES or TARGETS lists nothing")
endif()
file(STRINGS "${COMPILES}" lines)
set(failures "")
set(seen_names "")
set(seen_targets "")
set(seen_levels "")
foreach(line IN LISTS lines)
    if(NOT line MATCHES "^([^|]+)\\|([^|/]+)(/[^|]+)?\\|(.*)$")
        string(APPEND failures "\n  a line of no form: ${line}")
        continue()
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(target "${CMAKE_MATCH_2}")
    set(level "${CMAKE_MATCH_3}")
    set(command "${CMAKE_MATCH_4}")
    list(APPEND seen_names "${name}")
    list(APPEND seen_targets "${target}")
    list(APPEND seen_levels "${target}${level}")
    string(REGEX MATCHALL "(^| )-ffp-contract=[^ ]*" contracts "${command}")
    list(LENGTH contracts count)
    set(last "")
    if(count GREATER 0)
        list(GET contracts -1 last)
        string(STRIP "${last}" last)
    endif()
    if(NOT last STREQUAL "-ffp-contract=off")
        string(APPEND failures
               "\n  ${name} on ${target}${level} compiles without "
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
        string(APPEND failures "\n  nothing compiles for ${target}")
    endif()
endforeach()
foreach(level IN LISTS levels)
    if(NOT level IN_LIST seen_levels)
        string(APPEND failures "\n  nothing compiles for ${level}")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "the compiles:${failures}")
endif()
