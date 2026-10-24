# Every Anti source of this checkout stands in the canonical form that
# `anti fmt` writes. Run with cmake -P and these values:
#   ANTI    the anti executable
#   STD     the standard library
#   TESTS   the test directory
#
# The fixtures whose layout or lexical error is the point of the test are
# left out, because the canonical form of a source the lexer refuses is
# unknown and the layout of the others is what they check. A source in the
# canonical form that `anti fmt` does not write yet is left out while the
# finding of its own test is open.

cmake_minimum_required(VERSION 3.21)

set(SKIP
    "${TESTS}/check/format/src/com/example/loose.anti"
    "${TESTS}/errors/format_literal.anti"
    "${TESTS}/errors/hex_bytes.anti"
    "${TESTS}/errors/hex_escape_short.anti"
    "${TESTS}/errors/lexical.anti"
    "${TESTS}/errors/syntax.anti"
    "${TESTS}/fmt/comment_before_operator.anti"
    "${TESTS}/fmt/default_struct_literal.anti"
    "${TESTS}/fmt/handler_in_parentheses.anti"
    "${TESTS}/fmt/loose.anti"
    "${TESTS}/matrix/classes_call_in_an_inherited_field_of_a_default_parameter.anti"
    "${TESTS}/matrix/flow_exit_from_a_handler_in_a_loop_condition.anti"
    "${TESTS}/matrix/types_as_with_a_question_mark_before_a_number_type.anti"
    "${TESTS}/matrix/types_as_with_a_question_mark_before_its_type.anti"
    "${TESTS}/modules/lines/com/example/geo.anti")

file(GLOB_RECURSE sources "${STD}/*.anti" "${TESTS}/*.anti")
list(REMOVE_ITEM sources ${SKIP})
list(LENGTH sources count)
if(count LESS 300)
    message(FATAL_ERROR "only ${count} sources were found under ${STD} and ${TESTS}")
endif()

# Run `anti fmt --check` over one batch of sources and keep what it
# reports, with its status, so a run that ends by a signal and prints
# nothing fails as well.
set(report "")
function(check_batch)
    execute_process(
        COMMAND "${ANTI}" fmt --check ${batch}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        set(report
            "${report}anti fmt --check ended with ${status}\n${out}${err}"
            PARENT_SCOPE)
    endif()
endfunction()

# The sources go in batches of at most 8000 characters of paths. Windows
# takes a command line of 32767 characters, and all of them together are
# longer.
set(batch "")
set(length 0)
foreach(source IN LISTS sources)
    string(LENGTH "${source}" n)
    math(EXPR length "${length} + ${n} + 1")
    if(length GREATER 8000 AND batch)
        check_batch()
        set(batch "")
        math(EXPR length "${n} + 1")
    endif()
    list(APPEND batch "${source}")
endforeach()
check_batch()
if(NOT report STREQUAL "")
    message(FATAL_ERROR
        "these sources are not in the canonical form:\n${report}")
endif()
