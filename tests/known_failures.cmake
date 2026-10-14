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
#
# A line may end with one more word, `host=` and the systems of the hosts
# the defect shows on, `macos`, `linux` or `windows`, with a `,` between
# two: `host=linux` or `host=macos,linux`. A suite configured on one of
# those hosts expects the test to fail, and one configured on any other
# host expects it to pass. A line without the word names every host.

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

# The systems a line may name after `host=`, as ANTIC_HOST_OS names them.
set(KNOWN_FAILURES_SYSTEMS macos linux windows)

# Read the list <file>. Set <names> to the tests of its lines, <ids> to the
# id of each and <hosts> to the systems of each, `all` for a line without
# them and the systems with a `,` between two otherwise, three lists of one
# length. Set <wrong> to the numbers of the lines that are no
# `<test> <id> <sentence>` with `host=<systems>` or without it.
function(known_failures_read file names ids hosts wrong)
    known_failures_lines(lines "${file}")
    set(found_names "")
    set(found_ids "")
    set(found_hosts "")
    set(found_wrong "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        set(systems all)
        if(line MATCHES " host=([^ ]*)$")
            string(REPLACE "," ";" named "${CMAKE_MATCH_1}")
            set(known TRUE)
            set(seen "")
            foreach(system IN LISTS named)
                if(NOT system IN_LIST KNOWN_FAILURES_SYSTEMS OR
                   system IN_LIST seen)
                    set(known FALSE)
                endif()
                list(APPEND seen "${system}")
            endforeach()
            if(NOT known OR seen STREQUAL "")
                list(APPEND found_wrong ${number})
                continue()
            endif()
            set(systems "${CMAKE_MATCH_1}")
            string(REGEX REPLACE " host=[^ ]*$" "" line "${line}")
        endif()
        if(line MATCHES "^([A-Za-z0-9_.+-]+) (T[0-9][0-9][0-9]+) [^ ]")
            list(APPEND found_names "${CMAKE_MATCH_1}")
            list(APPEND found_ids "${CMAKE_MATCH_2}")
            list(APPEND found_hosts "${systems}")
        else()
            list(APPEND found_wrong ${number})
        endif()
    endforeach()
    set(${names} "${found_names}" PARENT_SCOPE)
    set(${ids} "${found_ids}" PARENT_SCOPE)
    set(${hosts} "${found_hosts}" PARENT_SCOPE)
    set(${wrong} "${found_wrong}" PARENT_SCOPE)
endfunction()

# Set <out> to TRUE when a line of the systems <systems>, as
# known_failures_read gives them, expects its test to fail on a host of
# the system <system>, and to FALSE otherwise.
function(known_failures_expected out systems system)
    string(REPLACE "," ";" systems "${systems}")
    if(systems STREQUAL "all" OR system IN_LIST systems)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()
