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
# WORK/skipped.txt records it.

cmake_minimum_required(VERSION 3.21)

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

foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    foreach(target IN LISTS targets)
        foreach(mode release dev)
            set(options --runtime "${RUNTIME}" --target "${target}")
            if(mode STREQUAL "dev")
                list(APPEND options --dev)
            endif()
            set(case "${name} ${mode} ${target}")
            execute_process(
                COMMAND "${ANTIC}" ${options} --dump-llvm "${source}"
                RESULT_VARIABLE status
                OUTPUT_VARIABLE text
                ERROR_VARIABLE err
                ENCODING NONE)
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
            execute_process(
                COMMAND "${ANTIC}" ${options} --dump-opt "${source}"
                RESULT_VARIABLE front
                OUTPUT_QUIET
                ERROR_QUIET)
            if(front EQUAL 0)
                string(APPEND failures
                       "${case}: antic failed with ${status}\n${err}\n")
            else()
                string(APPEND skipped "${case}\n")
            endif()
        endforeach()
    endforeach()
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
