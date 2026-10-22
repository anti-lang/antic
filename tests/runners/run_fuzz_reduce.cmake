# The reducer of tools/fuzz shrinks a find to a small program that still
# shows it. The test writes the program of one seed with the generator,
# adds a line that the checker refuses, saves the two as a find of the
# kind `refused` and runs the reducer on it. The reduced program must be
# at most MOST lines, hold the line and give the one message of the
# original. Run with cmake -P and these values:
#   ROOT     the repository
#   ANTIC    the antic executable
#   RUNTIME  the runtime directory
#   WORK     a scratch directory
#
# DESIGN: a refusal stands in for a defect, so the test needs no defect of
# antic and takes one front end run per check, a few seconds in all.

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tests/program_output.cmake")
include("${ROOT}/tools/fuzz/generate.cmake")

set(MOST 10)
set(LINE "\tlet zz: int = no_such_name;")
set(MESSAGE "error: unknown name `no_such_name`")

set(find "${WORK}/find")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${find}")
fuzz_generate(program 7 2)
string(FIND "${program}" "\tlet acc: int = " at)
if(at EQUAL -1)
    message(FATAL_ERROR "the program of seed 7 declares no accumulator")
endif()
string(SUBSTRING "${program}" ${at} -1 rest)
string(FIND "${rest}" "\n" end)
math(EXPR end "${at} + ${end} + 1")
string(SUBSTRING "${program}" 0 ${end} head)
string(SUBSTRING "${program}" ${end} -1 tail)
file(WRITE "${find}/${FUZZ_PROGRAM_NAME}" "${head}${LINE}\n${tail}")
file(WRITE "${find}/${FUZZ_KIND_NAME}" "refused\n")
file(WRITE "${find}/${FUZZ_DETAIL_NAME}" "antic exited with 1 in mode dev\n")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DFIND=${find}"
                        "-DWORK=${WORK}/reduce" "-DANTIC=${ANTIC}"
                        "-DRUNTIME=${RUNTIME}"
                        -P "${ROOT}/tools/fuzz/reduce.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the reducer failed with ${status}:\n${out}${err}")
endif()

set(reduced "${find}/${FUZZ_REDUCED_NAME}")
file(STRINGS "${reduced}" lines)
list(LENGTH lines count)
if(count GREATER MOST)
    message(FATAL_ERROR "the reduced program holds ${count} lines, more than ${MOST}:\n${err}")
endif()
file(READ "${reduced}" text)
string(FIND "${text}" "${LINE}" kept)
if(kept EQUAL -1)
    message(FATAL_ERROR "the reduced program lost the line it shows:\n${text}")
endif()
antic_run(status out err COMMAND --runtime "${RUNTIME}" --front-end "${reduced}")
string(REGEX REPLACE "^[^\n]*\\.anti:[0-9]+:[0-9]+: " "" err "${err}")
if(NOT status EQUAL 1 OR NOT err STREQUAL "${MESSAGE}\n")
    message(FATAL_ERROR "antic gives ${status} and `${err}` for the reduced program, "
                        "where the find gives 1 and `${MESSAGE}`")
endif()
