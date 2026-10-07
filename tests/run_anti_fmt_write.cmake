# Drive `anti fmt` over a source whose write fails partway, under a limit
# on the size of the files the process writes. The formatter wrote the
# canonical form over the source itself, which emptied the file before
# the write, so the failed write left the source cut short. The source
# now stays as it was until the whole new form is written. Run with
# cmake -P and these values:
#   ANTI  the anti executable
#   WORK  a directory this run writes into
#
# The limit is `ulimit -f` of a POSIX shell, so Windows has no run of
# this test.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(source "${WORK}/wide.anti")

# Many functions indented with spaces, which the canonical form indents
# with tabs: far more bytes than the limit lets through.
set(text "//! Functions indented with spaces, which `anti fmt` indents with tabs.\n")
foreach(n RANGE 1 2000)
    string(APPEND text "\nfn f${n}() -> int\n{\n    return ${n};\n}\n")
endforeach()
file(WRITE "${source}" "${text}")
file(SHA256 "${source}" before)

execute_process(
    COMMAND sh -c "ulimit -f 1 && exec \"$0\" fmt \"$1\"" "${ANTI}"
            "${source}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "anti fmt wrote past the limit:\n${out}${err}")
endif()
file(SHA256 "${source}" after)
if(NOT after STREQUAL before)
    file(SIZE "${source}" size)
    message(FATAL_ERROR "a failed write left the source changed, "
                        "${size} bytes:\n${out}${err}")
endif()

# Without the limit the canonical form replaces the source, and nothing
# else stays beside it.
execute_process(
    COMMAND "${ANTI}" fmt "${source}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti fmt failed with ${status}:\n${out}${err}")
endif()
file(SHA256 "${source}" formed)
if(formed STREQUAL before)
    message(FATAL_ERROR "anti fmt left the source as it was")
endif()
file(GLOB left "${WORK}/*")
if(NOT left STREQUAL "${source}")
    message(FATAL_ERROR "anti fmt left files beside the source: ${left}")
endif()
