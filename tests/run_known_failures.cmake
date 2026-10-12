# The list of the known failures agrees with the table of the findings and
# with the build tree. Run with cmake -P and these values:
#   LIST      tests/known-failures.txt
#   FINDINGS  docs/audit/tests-findings.md
#   TESTS     a file with the name of every test of tests/CMakeLists.txt,
#             one per line
#   MARKED    a file with the names of the tests the build tree expects to
#             fail, one per line
#
# The configure step writes TESTS and MARKED. The test fails when
#   - a line is no `<test> <id> <sentence>`, or names a test a second time
#   - a line names no test
#   - the id of a line has no row in the table, or a row marked fixed
#   - a row is open and no line names its id
#   - a row does not end with a grade and a state, or its id has a second
#     row
#   - the build tree was configured with another list
#
# DESIGN: a row of the table is a line that starts with `| T` and a digit.
# Its first cell is the id, and its last two are the grade, `severe`,
# `major` or `minor`, and the state, `open` or a text that starts with
# `fixed`. The cells between them may hold a `|` of Anti, so the runner
# reads the row from both ends and never counts its cells.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/known_failures.cmake")

get_filename_component(list_name "${LIST}" NAME)
get_filename_component(findings_name "${FINDINGS}" NAME)
set(faults "")

# The rows of the table: the ids, and of each id whether it is fixed.
known_failures_lines(rows "${FINDINGS}")
set(row_ids "")
set(number 0)
foreach(row IN LISTS rows)
    math(EXPR number "${number} + 1")
    if(NOT row MATCHES "^\\| *(T[0-9]+)")
        continue()
    endif()
    set(id "${CMAKE_MATCH_1}")
    if(id IN_LIST row_ids)
        string(APPEND faults "${findings_name}:${number}: ${id} has a second row\n")
        continue()
    endif()
    list(APPEND row_ids "${id}")
    set(row_line_${id} ${number})
    if(row MATCHES "\\| *(severe|major|minor) *\\| *(open|fixed)[^|]*\\|[ \t]*$")
        set(row_state_${id} "${CMAKE_MATCH_2}")
    else()
        set(row_state_${id} open)
        string(APPEND faults "${findings_name}:${number}: the row of ${id} does "
                             "not end with a grade and a state\n")
    endif()
endforeach()

# The lines of the list against the tests and the rows.
file(STRINGS "${TESTS}" tests)
known_failures_read("${LIST}" names ids wrong)
foreach(number IN LISTS wrong)
    string(APPEND faults "${list_name}:${number}: no line of the form "
                         "`<test> <id> <sentence>`\n")
endforeach()
set(seen "")
set(listed_ids "")
foreach(name id IN ZIP_LISTS names ids)
    if(name IN_LIST seen)
        string(APPEND faults "${list_name}: ${name} is listed twice\n")
        continue()
    endif()
    list(APPEND seen "${name}")
    list(APPEND listed_ids "${id}")
    if(NOT name IN_LIST tests)
        string(APPEND faults "${list_name}: ${name} names no test\n")
    endif()
    if(NOT id IN_LIST row_ids)
        string(APPEND faults "${list_name}: ${id} has no row in "
                             "${findings_name}, and ${name} names it\n")
    elseif(row_state_${id} STREQUAL "fixed")
        string(APPEND faults "${list_name}: ${id} is marked fixed in "
                             "${findings_name}, and ${name} names it\n")
    endif()
endforeach()
foreach(id IN LISTS row_ids)
    if(row_state_${id} STREQUAL "open" AND NOT id IN_LIST listed_ids)
        string(APPEND faults "${findings_name}:${row_line_${id}}: ${id} is open "
                             "and no line of ${list_name} names it\n")
    endif()
endforeach()

# The list against the one the tree was configured with.
file(STRINGS "${MARKED}" marked)
foreach(name IN LISTS seen)
    if(name IN_LIST tests AND NOT name IN_LIST marked)
        string(APPEND faults "the build tree does not expect ${name} to fail, "
                             "and ${list_name} lists it: configure again\n")
    endif()
endforeach()
foreach(name IN LISTS marked)
    if(NOT name IN_LIST seen)
        string(APPEND faults "the build tree expects ${name} to fail, and "
                             "${list_name} does not list it: configure again\n")
    endif()
endforeach()

if(NOT faults STREQUAL "")
    message(FATAL_ERROR "${faults}")
endif()
list(LENGTH seen count)
message(STATUS "${count} tests are expected to fail")
