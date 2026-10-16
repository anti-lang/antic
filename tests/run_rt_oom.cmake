# S312 and M301. The text builder, the copy of an object and the copy of
# an owned buffer stop the program through anti_rt_out_of_memory when the
# C library refuses their memory, and never come back with a literal or
# NULL. Each case aborts with the message of the failure routine, which
# names the bytes asked for. Run with cmake -P and these values:
#   PROGRAM  rt_oom_tests, built from tests/unit/test_rt_oom.c
#   WORK     a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(bytes_take 1)
set(bytes_append 4)
set(bytes_fill 4)
set(bytes_append_count 9223372036854775803)
set(bytes_fill_count 9223372036854775807)
set(bytes_copy_buffer 3)
set(bytes_dup 24)
foreach(case take append fill append_count fill_count copy_buffer dup)
    program_expect("rt_oom_${case}" COMMAND "${PROGRAM}" ${case} ABORTS
                   ERR "anti: out of memory for ${bytes_${case}} bytes\n")
endforeach()
