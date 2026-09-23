# Every Anti source of this checkout stands in the canonical form that
# `anti fmt` writes. Run with cmake -P and these values:
#   ANTI    the anti executable
#   STD     the standard library
#   TESTS   the test directory
#
# The fixtures whose layout or lexical error is the point of the test are
# left out, because the canonical form of a source the lexer refuses is
# unknown and the layout of the others is what they check.

set(SKIP
    "${TESTS}/check/format/src/com/example/loose.anti"
    "${TESTS}/errors/format_literal.anti"
    "${TESTS}/errors/hex_bytes.anti"
    "${TESTS}/errors/lexical.anti"
    "${TESTS}/errors/syntax.anti"
    "${TESTS}/fmt/loose.anti"
    "${TESTS}/modules/lines/com/example/geo.anti")

file(GLOB_RECURSE sources "${STD}/*.anti" "${TESTS}/*.anti")
list(REMOVE_ITEM sources ${SKIP})
list(LENGTH sources count)
if(count LESS 300)
    message(FATAL_ERROR "only ${count} sources were found under ${STD} and ${TESTS}")
endif()

execute_process(
    COMMAND "${ANTI}" fmt --check ${sources}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR
        "these sources are not in the canonical form:\n${out}${err}")
endif()
