# Run antic on sources that hold a NUL byte. The lexer holds the rule on
# the bytes of a source, as anti reaches it too. A NUL outside a literal
# is refused, and the NUL stands at the start, inside the first block the
# driver reads, at its last byte, at the first byte of the next and at
# the end of the file. A NUL inside a bytes literal is a byte of it, and
# antic takes the source. Run with cmake -P and these values:
#   ANTIC  the antic executable
#   NUL    a file of one NUL byte, tests/errors/nul_byte.bin
#   WORK   a directory for the sources
#
# CMake cannot hold a NUL in a string, so each source is the NUL file
# between two files of spaces, joined by `cmake -E cat`. Spaces alone make
# a module with no items, which the front end takes.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Write a file of n spaces.
function(spaces path n)
    set(text "")
    if(n GREATER 0)
        string(REPEAT " " ${n} text)
    endif()
    file(WRITE "${path}" "${text}")
endfunction()

spaces("${WORK}/plain.anti" 8191)
execute_process(
    COMMAND "${ANTIC}" --front-end "${WORK}/plain.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic refused a source of spaces\n${err}")
endif()

foreach(place "0|0" "0|10" "1|0" "100|100" "4094|0" "4094|5" "4095|0"
        "4095|3" "4096|0" "8189|1" "8190|0")
    string(REPLACE "|" ";" parts "${place}")
    list(GET parts 0 before)
    list(GET parts 1 after)
    spaces("${WORK}/before.txt" ${before})
    spaces("${WORK}/after.txt" ${after})
    set(source "${WORK}/nul-${before}-${after}.anti")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E cat "${WORK}/before.txt" "${NUL}"
                "${WORK}/after.txt"
        OUTPUT_FILE "${source}" RESULT_VARIABLE status)
    file(SIZE "${source}" size)
    math(EXPR want "${before} + 1 + ${after}")
    if(NOT status EQUAL 0 OR NOT size EQUAL want)
        message(FATAL_ERROR "cannot write ${source}: ${size} bytes, not ${want}")
    endif()
    execute_process(
        COMMAND "${ANTIC}" --front-end "${source}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(status EQUAL 0)
        message(FATAL_ERROR "antic took a source with a NUL byte after "
                            "${before} bytes")
    endif()
    math(EXPR column "${before} + 1")
    if(NOT err MATCHES "^${source}:1:${column}: error: NUL is not allowed here"
       OR NOT out STREQUAL "")
        message(FATAL_ERROR "antic did not name the NUL byte after ${before} "
                            "bytes\n${out}${err}")
    endif()
endforeach()

# A NUL byte inside a bytes literal.
file(WRITE "${WORK}/before.txt" "fn main() -> int\n{\n    let b = b\"a")
file(WRITE "${WORK}/after.txt" "b\";\n    return b.len as int;\n}\n")
set(source "${WORK}/bytes.anti")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E cat "${WORK}/before.txt" "${NUL}"
            "${WORK}/after.txt"
    OUTPUT_FILE "${source}" RESULT_VARIABLE status)
execute_process(
    COMMAND "${ANTIC}" --front-end "${source}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic refused a NUL byte inside a bytes literal\n"
                        "${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
