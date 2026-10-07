# M03. Each allocation helper of antic, the memory pool and the text
# buffer exit through alloc_out_of_memory when the C library refuses the
# memory or a size overflows, with status 70 and the message, and never
# come back with NULL. Run with cmake -P and these values:
#   PROGRAM  alloc_oom_tests, built from tests/unit/test_alloc_oom.c
#   WORK     a directory this run writes into
# The C library of macOS writes a line of its own about the refused
# memory first, so the message is matched where it stands.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(helper zeroed resize grow product sum arena text)
    program_expect("alloc_${helper}" COMMAND "${PROGRAM}" ${helper}
                   STATUS 70 ERR_MATCH "antic: out of memory\n$")
endforeach()
