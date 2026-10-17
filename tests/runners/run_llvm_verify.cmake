# The LLVM text of every program is valid LLVM IR. The test translates each
# .anti file of the source directories with --dump-llvm, for every target
# and in release and dev mode, and runs the verifier of opt over each text
# the translator accepts. A text the verifier refuses fails the test with
# the verifier's message. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   OPT      opt of the pinned release
#   RUNTIME  the runtime directory, which holds the standard library
#   SOURCES  the directories of the programs, separated by commas
#   TARGETS  the targets, separated by |
#   WORK     a scratch directory
#
# The translator takes every operation of the IR from the step emit-wide
# on, so it must accept every program in every mode and for every target.
# WORK/accepted.txt records each accepted text, one line per program, mode
# and target. A file that the front end refuses as well, under --dump-opt,
# is no program, such as an input of the parser dumps, and
# WORK/skipped.txt records it. Both runs must end with status 1, the
# status of a refusal, so a crash fails the test. The files that are no
# program stand in NO_PROGRAM, as the name of their directory and their
# name, and the test fails for a skip that the list does not name and for
# a name of the list that is not skipped.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(NO_PROGRAM dump/format dump/generics_syntax dump/kinds dump/nodes)

string(REPLACE "," ";" directories "${SOURCES}")
string(REPLACE "|" ";" targets "${TARGETS}")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(accepted "")
set(skipped "")
set(failures "")
set(accepted_count 0)

set(sources "")
foreach(directory IN LISTS directories)
    file(GLOB found "${directory}/*.anti")
    list(SORT found)
    list(APPEND sources ${found})
endforeach()

set(skipped_files "")
foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    get_filename_component(directory "${source}" DIRECTORY)
    get_filename_component(directory "${directory}" NAME)
    foreach(target IN LISTS targets)
        foreach(mode release dev)
            set(options --runtime "${RUNTIME}" --target "${target}")
            if(mode STREQUAL "dev")
                list(APPEND options --dev)
            endif()
            set(case "${name} ${mode} ${target}")
            antic_run(status text err COMMAND ${options} --dump-llvm
                      "${source}")
            if(status EQUAL 0 AND err STREQUAL "" AND NOT text STREQUAL "")
                set(file "${WORK}/${name}.${mode}.${target}.ll")
                file(WRITE "${file}" "${text}")
                execute_process(
                    COMMAND "${OPT}" -passes=verify -disable-output "${file}"
                    RESULT_VARIABLE verified
                    OUTPUT_FILE "${file}.out"
                    ERROR_FILE "${file}.out")
                file(READ "${file}.out" out)
                file(REMOVE "${file}.out")
                if(NOT verified EQUAL 0 OR NOT out STREQUAL "")
                    string(APPEND failures
                           "${case}: the verifier refuses ${file}\n${out}\n")
                else()
                    string(APPEND accepted "${case}\n")
                    math(EXPR accepted_count "${accepted_count} + 1")
                    file(REMOVE "${file}")
                endif()
                continue()
            endif()
            antic_run(front ignored_out ignored_err
                      COMMAND ${options} --dump-opt "${source}")
            if(status EQUAL 1 AND front EQUAL 1)
                string(APPEND skipped "${case}\n")
                list(APPEND skipped_files "${directory}/${name}")
            else()
                string(APPEND failures "${case}: antic failed with ${status} "
                       "and the front end with ${front}\n${err}\n")
            endif()
        endforeach()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES skipped_files)
foreach(file IN LISTS skipped_files)
    if(NOT file IN_LIST NO_PROGRAM)
        string(APPEND failures "${file}: the front end refuses it, and "
               "NO_PROGRAM does not name it\n")
    endif()
endforeach()
foreach(file IN LISTS NO_PROGRAM)
    if(NOT file IN_LIST skipped_files)
        string(APPEND failures "${file}: NO_PROGRAM names it, and it is "
               "not skipped\n")
    endif()
endforeach()

file(WRITE "${WORK}/accepted.txt" "${accepted}")
file(WRITE "${WORK}/skipped.txt" "${skipped}")
message(STATUS "${accepted_count} texts verified; the lists stand in "
               "${WORK}")
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
if(accepted_count EQUAL 0)
    message(FATAL_ERROR "the translator accepts no program")
endif()
