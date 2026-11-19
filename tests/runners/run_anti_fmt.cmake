# Drive `anti fmt` over the fixture of tests/fmt and read what it writes.
# Run with cmake -P and these values:
#   ANTI     the anti executable
#   FIXTURE  the directory that holds <NAME>.anti and <NAME>.expected
#   NAME     the name of the fixture, loose without it
#   WORK     a directory for the copies this run formats
#
# The run covers the rules of the canonical form on one file, the format
# stability of docs/tooling.md, which asks that `anti fmt` on its own
# output changes nothing, and `--check`, which writes nothing and lists
# the files that differ.
#
# Each file is read as hexadecimal, since a read of text drops the CR of
# a CRLF, which the fixture of a source with CRLF holds.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT DEFINED NAME)
    set(NAME loose)
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${FIXTURE}/${NAME}.anti" "${WORK}/${NAME}.anti" COPYONLY)

function(run_fmt arguments out_status out_text)
    execute_process(
        COMMAND "${ANTI}" fmt ${arguments}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

# The file is not in the canonical form, so --check reports it and
# changes nothing.
run_fmt("--check;${WORK}/${NAME}.anti" status text)
if(status EQUAL 0)
    message(FATAL_ERROR "--check passed on an unformatted file\n${text}")
endif()
string(FIND "${text}" "${NAME}.anti" at)
if(at LESS 0)
    message(FATAL_ERROR "--check named no file\n${text}")
endif()
file(READ "${WORK}/${NAME}.anti" after_check HEX)
file(READ "${FIXTURE}/${NAME}.anti" before HEX)
if(NOT after_check STREQUAL before)
    message(FATAL_ERROR "--check wrote to the file")
endif()

# The canonical form of the fixture, byte for byte.
run_fmt("${WORK}/${NAME}.anti" status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "fmt failed with ${status}\n${text}")
endif()
file(READ "${WORK}/${NAME}.anti" formed HEX)
file(READ "${FIXTURE}/${NAME}.expected" expected HEX)
expect_same_bytes("the canonical form of ${FIXTURE}/${NAME}.anti"
                 "${expected}" "${formed}")

# Format stability: `anti fmt` on its own output changes nothing, and
# --check says so.
run_fmt("${WORK}/${NAME}.anti" status text)
if(NOT status EQUAL 0 OR NOT text STREQUAL "")
    message(FATAL_ERROR "the second run changed the file\n${text}")
endif()
file(READ "${WORK}/${NAME}.anti" again HEX)
expect_same_bytes("the form of the second run" "${expected}" "${again}")
run_fmt("--check;${WORK}/${NAME}.anti" status text)
if(NOT status EQUAL 0 OR NOT text STREQUAL "")
    message(FATAL_ERROR "--check reported a formatted file\n${text}")
endif()
