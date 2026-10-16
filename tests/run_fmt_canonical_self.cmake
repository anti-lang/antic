# tests/run_fmt_canonical.cmake fails when `anti fmt --check` ends by a
# signal and prints nothing. Run with cmake -P and these values:
#   RUNNER  tests/run_fmt_canonical.cmake
#   STD     the standard library
#   TESTS   the test directory
#   WORK    a directory for the stand-in of anti

cmake_minimum_required(VERSION 3.21)

if(CMAKE_HOST_WIN32)
    message("SKIP: the stand-in of anti is a shell script")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(WRITE "${WORK}/anti" "#!/bin/sh\nkill -KILL $$\n")
file(CHMOD "${WORK}/anti" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
execute_process(COMMAND "${CMAKE_COMMAND}" "-DANTI=${WORK}/anti"
                        "-DSTD=${STD}" "-DTESTS=${TESTS}" -P "${RUNNER}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "run_fmt_canonical.cmake passed an anti that a "
                        "signal ended")
endif()
# CMake wraps a long message, so the text is read with its spaces and its
# line ends made one space.
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
if(NOT flat MATCHES "anti fmt --check ended with")
    message(FATAL_ERROR "run_fmt_canonical.cmake failed for another reason\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
