# Merge the raw profiles of a coverage tree and write its report.
#
#   cmake -DLLVM_BIN=<dir> -DROOT=<repository> -DPROFILES=<dir> -DOUT=<dir>
#         -DOBJECTS=<program>,<program>... -P tools/coverage-report.cmake
#
# LLVM_BIN holds llvm-profdata and llvm-cov of the pinned release. PROFILES
# is the directory the instrumented programs wrote their raw profiles
# into, and OBJECTS are those programs, compiled with the flags that
# antic_coverage_compile_flags of tools/coverage.cmake gives for ROOT. The
# script writes into OUT:
#
#   coverage.profdata  every raw profile of PROFILES, merged
#   summary.txt        one row per file of src/antic/, src/anti/ and
#                      src/rt/ with its lines, functions, regions and
#                      branches, and a row of totals. llvm-cov names a
#                      file from src/, the directory all of them share
#   html/              the annotated source of each of those files
#
# The target coverage_report of tools/coverage.cmake runs it, and so does
# the test coverage_report.
#
# A suite that ran in parts left the profiles of every part in PROFILES,
# and the merge takes them all. The script runs no test and removes no
# profile.

cmake_minimum_required(VERSION 3.21)

foreach(name LLVM_BIN ROOT PROFILES OUT OBJECTS)
    if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
        message(FATAL_ERROR "tools/coverage-report.cmake takes -D${name}")
    endif()
endforeach()

# The directories of the report, from ROOT. The tests and the third-party
# sources a program carries stay out of it.
set(measured src/antic src/anti src/rt)

include("${CMAKE_CURRENT_LIST_DIR}/host.cmake")
antic_machine_host(machine)
antic_executable_suffix(exe "${machine}")
set(profdata "${LLVM_BIN}/llvm-profdata${exe}")
set(cov "${LLVM_BIN}/llvm-cov${exe}")

file(GLOB_RECURSE raw "${PROFILES}/*.profraw")
if(NOT raw)
    message(FATAL_ERROR "${PROFILES} holds no raw profile. The suite of the "
                        "coverage tree writes them, so run it first.")
endif()
list(LENGTH raw raw_count)

# Run a tool, stop on a failure and show what it wrote to standard error:
# llvm-cov reports a function whose profile does not fit its code there.
function(tool what)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${ROOT}"
                    RESULT_VARIABLE status ERROR_VARIABLE errors
                    ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} gave ${status}\n${errors}")
    endif()
    if(NOT errors STREQUAL "")
        message("${errors}")
    endif()
endfunction()

file(MAKE_DIRECTORY "${OUT}")
set(merged "${OUT}/coverage.profdata")
list(JOIN raw "\n" raw_lines)
file(WRITE "${OUT}/coverage-profiles.txt" "${raw_lines}\n")
tool("llvm-profdata merge"
     "${profdata}" merge -sparse "--input-files=${OUT}/coverage-profiles.txt"
     -o "${merged}")

# The programs: the first by position and the others after -object.
string(REPLACE "," ";" objects "${OBJECTS}")
set(programs "")
foreach(object IN LISTS objects)
    if(NOT EXISTS "${object}")
        message(FATAL_ERROR "${object} is no program of the coverage tree")
    endif()
    if(programs STREQUAL "")
        list(APPEND programs "${object}")
    else()
        list(APPEND programs -object "${object}")
    endif()
endforeach()

# DESIGN: the coverage mapping names a file from ROOT, as
# src/antic/lexer.c, so llvm-cov runs in ROOT, where it finds each source
# by that name. The three directories are a pattern over the names. Given
# as paths, llvm-cov compares their absolute form with the relative names,
# finds no file and reports every file instead.
list(JOIN measured "|" pattern)
set(common "-instr-profile=${merged}"
    "--include-filename-regex=^(${pattern})/")

execute_process(COMMAND "${cov}" report ${common} ${programs}
                WORKING_DIRECTORY "${ROOT}"
                RESULT_VARIABLE status OUTPUT_VARIABLE summary
                ERROR_VARIABLE errors ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-cov report gave ${status}\n${errors}")
endif()
if(NOT errors STREQUAL "")
    message("${errors}")
endif()
file(WRITE "${OUT}/summary.txt" "${summary}")

# The pages of an earlier report go first, so a file that left the
# measurement leaves no page behind.
file(REMOVE_RECURSE "${OUT}/html")
tool("llvm-cov show"
     "${cov}" show ${common} -format=html "-output-dir=${OUT}/html"
     -show-branches=count ${programs})

string(REGEX MATCH "TOTAL[^\n]*" total "${summary}")
message("coverage: ${raw_count} raw profiles merged into ${merged}")
message("coverage: ${OUT}/summary.txt and ${OUT}/html/index.html")
message("coverage: ${total}")
