# Run a program and check its exit status and its output. Run with
# cmake -P and these values:
#   COMMAND    the program and its arguments, separated by commas
#   STATUS     the exit status the program must give
#   DIRECTORY  optional, the directory to run in, so that messages name
#              the file alone
#   EXPECTED   a file that the standard output followed by the standard
#              error must equal byte for byte
#   PATTERN    instead of EXPECTED, a regular expression that the standard
#              output followed by the standard error must match
#
# DESIGN: every test of a refusal or a warning runs here, and the status
# is compared as a number. ctest ignores the exit status of a test with
# PASS_REGULAR_EXPRESSION, so a program that printed the message and then
# exited 0, or crashed, passed. A refusal names the status antic gives
# it: 1 for a program it refuses and 2 for a command line it refuses. A
# crash gives execute_process a text such as "Segmentation fault", which
# equals no number.

if(NOT STATUS MATCHES "^[0-9]+$")
    message(FATAL_ERROR "STATUS is `${STATUS}`, and it must be a number")
endif()
set(checks 0)
foreach(value EXPECTED PATTERN)
    if(DEFINED ${value})
        math(EXPR checks "${checks} + 1")
    endif()
endforeach()
if(NOT checks EQUAL 1)
    message(FATAL_ERROR "give EXPECTED or PATTERN, and not both")
endif()

string(REPLACE "," ";" command "${COMMAND}")
set(where "")
if(DEFINED DIRECTORY)
    set(where WORKING_DIRECTORY "${DIRECTORY}")
endif()
execute_process(COMMAND ${command} ${where}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
set(output "${out}${err}")
if(NOT status STREQUAL STATUS)
    message(FATAL_ERROR "the program exited with ${status}, and the test "
                        "expects ${STATUS}\n${output}")
endif()
if(DEFINED EXPECTED)
    file(READ "${EXPECTED}" expected)
    if(NOT output STREQUAL expected)
        message(FATAL_ERROR "the output differs from ${EXPECTED}\n"
                            "got:\n${output}")
    endif()
elseif(NOT output MATCHES "${PATTERN}")
    message(FATAL_ERROR "the output does not match `${PATTERN}`\n"
                        "got:\n${output}")
endif()
