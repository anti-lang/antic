# The native back end is gone since the step `switch`, and its register
# allocator and instruction selection with it. No comment of src/antic/
# gives a reason of theirs, and the IR keeps no field that only they
# read. Run with cmake -P and ROOT, the root of the repository.
#
# Each phrase below names a pass of the native back end. coff.c names the
# COMDAT selections of COFF, which no phrase here matches. The loop depth
# of a block weighed a spill in the register allocator, and no pass of
# the LLVM back end reads it.

cmake_minimum_required(VERSION 3.21)

set(phrases "register allocator" "spill" "before selection"
    "selection puts")

file(GLOB files "${ROOT}/src/antic/*.[ch]")
list(SORT files)
if(files STREQUAL "")
    message(FATAL_ERROR "no file of antic under ${ROOT}/src/antic")
endif()

set(failures "")
foreach(path IN LISTS files)
    file(READ "${path}" text)
    # A comment may break a phrase across two lines.
    string(REGEX REPLACE "[ \t]*\n[ \t]*(\\*[ \t]*)?" " " flat "${text}")
    file(RELATIVE_PATH name "${ROOT}" "${path}")
    foreach(phrase IN LISTS phrases)
        string(FIND "${flat}" "${phrase}" at)
        if(NOT at EQUAL -1)
            string(APPEND failures "\n${name}: names `${phrase}`")
        endif()
    endforeach()
endforeach()

file(READ "${ROOT}/src/antic/ir.h" ir)
if(NOT ir MATCHES "\nstruct ir_block {([^}]*)}")
    message(FATAL_ERROR "no struct ir_block in src/antic/ir.h")
endif()
if(CMAKE_MATCH_1 MATCHES "loop_depth")
    string(APPEND failures
        "\nsrc/antic/ir.h: struct ir_block keeps loop_depth, which no pass reads")
endif()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "what the native back end left:${failures}")
endif()
