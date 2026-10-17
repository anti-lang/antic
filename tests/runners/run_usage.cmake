# The usage of a tool: the option that asks for it, and a command line the
# tool refuses with it. Run with cmake -P and these values:
#   COMMAND  the tool and its arguments, separated by commas
#   NAME     the name the usage gives the tool, antic or anti
#   REFUSED  optional. ON for a command line the tool refuses
#   FIRST    optional, with REFUSED: the line the tool writes before the
#            usage, without its line break
#   LISTS    optional, without REFUSED: options separated by |, each of
#            which the usage lists at the start of a line of its own
#
# Without REFUSED the command is the tool and the option that asks for the
# usage, alone or among other arguments. The option prints it on the
# standard output and ends with 0. The tool without an argument prints the
# same bytes on the standard error and ends with 2, since it was given
# nothing to do.
#
# With REFUSED the command ends with 2 and prints nothing on the standard
# output. Its standard error is the line FIRST, when one is given, and
# then the bytes that `--help` prints.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

foreach(value COMMAND NAME)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()
string(REPLACE "," ";" given "${COMMAND}")
list(GET given 0 tool)

if(REFUSED)
    set(command "${tool}" --help)
    execute_process(COMMAND ${command}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE usage
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status STREQUAL "0" OR NOT usage MATCHES "^usage: ${NAME} ")
        message(FATAL_ERROR "${NAME} --help ended with ${status} and "
                            "printed:\n${usage}${err}")
    endif()
    set(expected "${usage}")
    if(DEFINED FIRST)
        set(expected "${FIRST}\n${usage}")
    endif()
    set(command ${given})
    execute_process(COMMAND ${command}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status STREQUAL "2")
        message(FATAL_ERROR "the command ended with ${status}, and the test "
                            "expects 2\n${out}${err}")
    endif()
    if(NOT out STREQUAL "")
        message(FATAL_ERROR "the command wrote on the standard output:\n"
                            "${out}")
    endif()
    expect_same_text("the standard error of the command, against the usage,"
                     "${expected}" "${err}")
    return()
endif()

list(LENGTH given count)
if(count LESS 2)
    message(FATAL_ERROR "COMMAND holds ${count} words, and it takes the "
                        "tool and the option at least")
endif()

set(command ${given})
execute_process(COMMAND ${command}
    RESULT_VARIABLE asked_status
    OUTPUT_VARIABLE asked_out
    ERROR_VARIABLE asked_err
    ENCODING NONE)
if(NOT asked_status STREQUAL "0")
    message(FATAL_ERROR "the option ended with ${asked_status}, and the test "
                        "expects 0\n${asked_err}")
endif()
if(NOT asked_err STREQUAL "")
    message(FATAL_ERROR "the option wrote on the standard error:\n"
                        "${asked_err}")
endif()
if(NOT asked_out MATCHES "^usage: ${NAME} ")
    message(FATAL_ERROR "the option printed no usage of ${NAME}:\n"
                        "${asked_out}")
endif()
if(DEFINED LISTS)
    string(REPLACE "|" ";" listed "${LISTS}")
    foreach(option IN LISTS listed)
        string(FIND "${asked_out}" "\n  ${option} " at)
        if(at EQUAL -1)
            string(FIND "${asked_out}" "\n  ${option}\n" at)
        endif()
        if(at EQUAL -1)
            message(FATAL_ERROR "the usage of ${NAME} lists no `${option}`:\n"
                                "${asked_out}")
        endif()
    endforeach()
endif()

set(command "${tool}")
execute_process(COMMAND ${command}
    RESULT_VARIABLE bare_status
    OUTPUT_VARIABLE bare_out
    ERROR_VARIABLE bare_err
    ENCODING NONE)
if(NOT bare_status STREQUAL "2")
    message(FATAL_ERROR "${NAME} without an argument ended with "
                        "${bare_status}, and the test expects 2\n${bare_err}")
endif()
if(NOT bare_out STREQUAL "")
    message(FATAL_ERROR "${NAME} without an argument wrote on the standard "
                        "output:\n${bare_out}")
endif()
if(NOT bare_err STREQUAL asked_out)
    message(FATAL_ERROR "the usage of the option differs from the usage of "
                        "${NAME} without an argument:\n${asked_out}\n"
                        "and:\n${bare_err}")
endif()
