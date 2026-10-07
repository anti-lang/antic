# S30. Each allocation helper of anti exits through files_out_of_memory
# when the C library refuses the memory, with status 70 and the message,
# and never comes back with NULL. Run with cmake -P and these values:
#   PROGRAM  files_oom_tests, built from tests/unit/test_files_oom.c
#   WORK     a directory this run writes into
# The C library of macOS writes a line of its own about the refused
# memory first, so the message is matched where it stands.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(helper array resize grow)
    program_expect("files_${helper}" COMMAND "${PROGRAM}" ${helper}
                   STATUS 70 ERR_MATCH "anti: out of memory\n$")
endforeach()
