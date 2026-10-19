# S312 and M301. The text builder, the copy of an object and the copy of
# an owned buffer stop the program through anti_rt_out_of_memory when the
# C library refuses their memory, and never come back with a literal or
# NULL. Each case aborts with the message of the failure routine, which
# names the bytes asked for. The case start ends with status 70 and the
# line of the memory of the start of a program. Run with cmake -P and these values:
#   PROGRAM  rt_oom_tests, built from tests/unit/test_rt_oom.c
#   CASES    the cases, separated by commas
#   WORK     a directory this run writes into
# A case without a count or a line below gives any count of bytes.
#
# DESIGN: abort() of the C runtime that the programs of the tests link on
# Windows ends the process with the status 3, where the runtime of an Anti
# program fails fast. A case there expects that status and the message.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(bytes_take 1)
set(bytes_append 4)
set(bytes_fill 4)
set(bytes_append_count 9223372036854775803)
set(bytes_fill_count 9223372036854775807)
set(bytes_copy_buffer 3)
set(bytes_dup 24)
# A case whose line names what the memory was for.
set(line_snapshot "anti: out of memory for a snapshot of 16 bytes\n")
string(REPLACE "," ";" cases "${CASES}")
foreach(case IN LISTS cases)
    if(DEFINED line_${case})
        set(message ERR "${line_${case}}")
    elseif(DEFINED bytes_${case})
        set(message ERR "anti: out of memory for ${bytes_${case}} bytes\n")
    else()
        set(message ERR_MATCH "^anti: out of memory for [0-9]+ bytes\n$")
    endif()
    set(ends ABORTS)
    if(CMAKE_HOST_WIN32)
        set(ends STATUS 3)
    endif()
    # The start of a program exits as a refusal at start does.
    if(case STREQUAL "start")
        set(message ERR "anti: out of memory at program start\n")
        set(ends STATUS 70)
    endif()
    program_expect("rt_oom_${case}" COMMAND "${PROGRAM}" ${case} ${ends}
                   ${message})
endforeach()
