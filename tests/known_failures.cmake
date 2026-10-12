# The list of the known failures, tests/known-failures.txt, read by
# tests/CMakeLists.txt, which expects each listed test to fail, and by
# tests/run_known_failures.cmake, which checks the list.
#
# DESIGN: decision 30 of "Decisions of 2026-10-11" in docs/decisions.md. A
# test that shows a defect stays in the suite, written as the rule says
# antic must behave, and its line here makes the suite expect the failure.
# A line is the name of the test, the id of its finding in
# docs/audit/tests-findings.md and one sentence, separated by one space
# each. A line that starts with `#` is a comment.

# Set <out> to the lines of <file> as a list. A `;`, a `[`, a `]` and a
# `\` mean something to a CMake list, so each is replaced first: a sentence
# and a cell of a table may hold them, and no name or id does.
function(known_failures_lines out file)
    file(READ "${file}" text)
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "[" "(" text "${text}")
    string(REPLACE "]" ")" text "${text}")
    string(REPLACE "\\" "/" text "${text}")
    string(REGEX REPLACE "\r?\n" ";" lines "${text}")
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# Read the list <file>. Set <names> to the tests of its lines and <ids> to
# the id of each, two lists of one length, and <wrong> to the numbers of
# the lines that are no `<test> <id> <sentence>`.
function(known_failures_read file names ids wrong)
    known_failures_lines(lines "${file}")
    set(found_names "")
    set(found_ids "")
    set(found_wrong "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        if(line MATCHES "^([A-Za-z0-9_.+-]+) (T[0-9][0-9][0-9]+) [^ ]")
            list(APPEND found_names "${CMAKE_MATCH_1}")
            list(APPEND found_ids "${CMAKE_MATCH_2}")
        else()
            list(APPEND found_wrong ${number})
        endif()
    endforeach()
    set(${names} "${found_names}" PARENT_SCOPE)
    set(${ids} "${found_ids}" PARENT_SCOPE)
    set(${wrong} "${found_wrong}" PARENT_SCOPE)
endfunction()
