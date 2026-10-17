# tests/runners/run_known_failures.cmake accepts a list that agrees with the table
# of the findings and with the build tree, and refuses every other one with
# a message that names the fault. Run with cmake -P and these values:
#   RUNNER  tests/runners/run_known_failures.cmake
#   WORK    a directory for the files
#
# Each case writes the four files the runner reads, runs it as on a host
# of Linux and states whether it must accept them. A case that must be refused names a text of
# the message, so a refusal for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(MAKE_DIRECTORY "${WORK}")
set(header "# The findings\n\n| Id | Test | Construct and context | What happens | Rule | Grade | State |\n|---|---|---|---|---|---|---|\n")
set(row_1 "| T001 | matrix_a_dev | a field of a case | reads 0 | \"Sum types\" | severe | open |\n")
set(row_2 "| T002 | matrix_b_refused | `a || b` on a `?T` | antic stops | \"Optional values\" | major | open |\n")
set(tests "matrix_a_dev\nmatrix_a_release\nmatrix_b_refused\n")

set(failures "")
# known_case(<name> accept|<text of the refusal> [LIST <text>] [ROWS <text>]
#            [MARKED <text> | NONE_MARKED])
# LIST and ROWS are empty without their keyword. MARKED holds the tests
# the build tree expects to fail, one per line, and NONE_MARKED says it
# expects none. Without either the tree expects every name of LIST that
# starts with `matrix_`.
function(known_case name verdict)
    cmake_parse_arguments(PARSE_ARGV 2 arg "NONE_MARKED" "LIST;ROWS;MARKED" "")
    foreach(value LIST ROWS)
        if(NOT DEFINED arg_${value})
            set(arg_${value} "")
        endif()
    endforeach()
    if(arg_NONE_MARKED)
        set(arg_MARKED "")
    elseif(NOT DEFINED arg_MARKED)
        string(REGEX MATCHALL "matrix_[a-z_]+" names "${arg_LIST}")
        list(REMOVE_DUPLICATES names)
        list(JOIN names "\n" arg_MARKED)
    endif()
    set(dir "${WORK}/${name}")
    file(MAKE_DIRECTORY "${dir}")
    file(WRITE "${dir}/known-failures.txt" "${arg_LIST}")
    file(WRITE "${dir}/tests-findings.md" "${header}${arg_ROWS}")
    file(WRITE "${dir}/tests.txt" "${tests}")
    file(WRITE "${dir}/marked.txt" "${arg_MARKED}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            "-DLIST=${dir}/known-failures.txt"
                            "-DFINDINGS=${dir}/tests-findings.md"
                            "-DTESTS=${dir}/tests.txt"
                            "-DMARKED=${dir}/marked.txt" -DSYSTEM=linux
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept")
        if(NOT status EQUAL 0)
            set(failures "${failures}${name}: refused\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${out}${err}" "${verdict}" at)
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
    elseif(at EQUAL -1)
        set(failures "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

# No finding and no line, the state after the harness is built.
known_case(empty accept)
# One open finding with its line. A comment and an empty line are no lines.
known_case(one accept
    LIST "# name id sentence\n\nmatrix_a_dev T001 The field reads 0.\n"
    ROWS "${row_1}")
# Two tests show one defect and name one id.
known_case(two_tests_one_id accept
    LIST "matrix_a_dev T001 The field reads 0.\nmatrix_a_release T001 The field reads 0.\n"
    ROWS "${row_1}")
# A sentence and a cell hold the characters a CMake list reads as its own.
known_case(list_characters accept
    LIST "matrix_b_refused T002 `a[i]; b` stops antic with a \\ in the path.\n"
    ROWS "${row_2}")
# A fixed finding has lost its line.
known_case(fixed_without_line accept
    ROWS "| T001 | matrix_a_dev | a field | reads 0 | \"Sum types\" | severe | fixed in 0123abc |\n")

known_case(no_test "matrix_c_dev names no test"
    LIST "matrix_c_dev T001 The field reads 0.\n" ROWS "${row_1}")
known_case(no_row "T002 has no row"
    LIST "matrix_a_dev T001 The field reads 0.\nmatrix_b_refused T002 antic stops.\n"
    ROWS "${row_1}")
known_case(open_without_line "T002 is open and no line"
    LIST "matrix_a_dev T001 The field reads 0.\n" ROWS "${row_1}${row_2}")
known_case(fixed_with_line "T001 is marked fixed"
    LIST "matrix_a_dev T001 The field reads 0.\n"
    ROWS "| T001 | matrix_a_dev | a field | reads 0 | \"Sum types\" | severe | fixed |\n")
known_case(no_sentence "known-failures.txt:1: no line of the form"
    LIST "matrix_a_dev T001\n" ROWS "${row_1}")
known_case(no_id "known-failures.txt:2: no line of the form"
    LIST "matrix_a_dev T001 The field reads 0.\nmatrix_a_release the field reads 0\n"
    ROWS "${row_1}")
known_case(listed_twice "matrix_a_dev is listed twice"
    LIST "matrix_a_dev T001 The field reads 0.\nmatrix_a_dev T001 Again.\n"
    ROWS "${row_1}")
known_case(second_row "T001 has a second row"
    LIST "matrix_a_dev T001 The field reads 0.\n" ROWS "${row_1}${row_1}")
known_case(no_grade "the row of T001 does not end with a grade and a state"
    LIST "matrix_a_dev T001 The field reads 0.\n"
    ROWS "| T001 | matrix_a_dev | a field | reads 0 | \"Sum types\" | bad | open |\n")
known_case(no_state "the row of T001 does not end with a grade and a state"
    LIST "matrix_a_dev T001 The field reads 0.\n"
    ROWS "| T001 | matrix_a_dev | a field | reads 0 | \"Sum types\" | severe |\n")
# The tree was configured before the line was written, so the suite runs
# the test without expecting its failure.
known_case(not_marked "does not expect matrix_a_dev to fail"
    LIST "matrix_a_dev T001 The field reads 0.\n" ROWS "${row_1}" NONE_MARKED)
# The tree was configured before the line was removed.
known_case(marked_alone "expects matrix_a_release to fail"
    LIST "matrix_a_dev T001 The field reads 0.\n" ROWS "${row_1}"
    MARKED "matrix_a_dev\nmatrix_a_release\n")

# A line for this host alone, one for two hosts, and one for another host,
# whose test the tree expects to pass.
known_case(host_this accept
    LIST "matrix_a_dev T001 The field reads 0. host=linux\n" ROWS "${row_1}")
known_case(host_two accept
    LIST "matrix_a_dev T001 The field reads 0. host=macos,linux\n"
    ROWS "${row_1}")
known_case(host_other accept
    LIST "matrix_a_dev T001 The field reads 0. host=windows\n"
    ROWS "${row_1}" NONE_MARKED)
known_case(host_this_not_marked "does not expect matrix_a_dev to fail"
    LIST "matrix_a_dev T001 The field reads 0. host=linux\n"
    ROWS "${row_1}" NONE_MARKED)
known_case(host_other_marked "for other hosts than linux"
    LIST "matrix_a_dev T001 The field reads 0. host=macos,windows\n"
    ROWS "${row_1}" MARKED "matrix_a_dev\n")
known_case(host_unknown "known-failures.txt:1: no line of the form"
    LIST "matrix_a_dev T001 The field reads 0. host=plan9\n"
    ROWS "${row_1}" NONE_MARKED)
known_case(host_twice "known-failures.txt:1: no line of the form"
    LIST "matrix_a_dev T001 The field reads 0. host=linux,linux\n"
    ROWS "${row_1}" NONE_MARKED)
known_case(host_empty "known-failures.txt:1: no line of the form"
    LIST "matrix_a_dev T001 The field reads 0. host=\n"
    ROWS "${row_1}" NONE_MARKED)
known_case(host_without_sentence "known-failures.txt:1: no line of the form"
    LIST "matrix_a_dev T001 host=linux\n" ROWS "${row_1}" NONE_MARKED)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
