# tests/run_expect.cmake refuses what a pattern of ctest lets pass, and no
# test uses such a pattern. Run with cmake -P and these values:
#   EXPECT  tests/run_expect.cmake
#   WORK    a directory for the files
#   ROOT    the repository
#
# Each case runs the runner on a command of CMake itself and states
# whether the runner must accept it. `cmake -E echo` prints its text and
# exits 0. `cmake -E cat` of a missing file prints a message and exits 1.

file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/refused.txt" "error: refused\n")
set(missing "${WORK}/missing.anti")

set(failures "")
function(expect_case name verdict)
    execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN} -P "${EXPECT}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept" AND NOT status EQUAL 0)
        set(failures "${failures}${name}: refused\n${out}${err}\n"
            PARENT_SCOPE)
    elseif(verdict STREQUAL "refuse" AND status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
    endif()
endfunction()

# The message of a refusal from a program that then exits 0.
expect_case(message_status_0 refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,error: refused" -DSTATUS=1
    "-DPATTERN=error: refused")
expect_case(message_status_1 accept
    "-DCOMMAND=${CMAKE_COMMAND},-E,cat,${missing}" -DSTATUS=1
    "-DPATTERN=missing\\.anti")
# A refusal with another status than the one named.
expect_case(other_status refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,cat,${missing}" -DSTATUS=2
    "-DPATTERN=missing\\.anti")
# A warning, which must leave the status 0.
expect_case(warning_status_0 accept
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,warning: kept" -DSTATUS=0
    "-DPATTERN=warning: kept")
expect_case(warning_status_1 refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,cat,${missing}" -DSTATUS=0
    "-DPATTERN=missing")
expect_case(pattern_absent refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,warning: kept" -DSTATUS=0
    "-DPATTERN=error")
# The bytes of the output against a file.
expect_case(expected_same accept
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,error: refused" -DSTATUS=0
    "-DEXPECTED=${WORK}/refused.txt")
expect_case(expected_longer refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,error: refused twice" -DSTATUS=0
    "-DEXPECTED=${WORK}/refused.txt")
# A status that is no number, and a test with no check of the output.
expect_case(status_word refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,x" -DSTATUS=refused "-DPATTERN=x")
expect_case(no_check refuse
    "-DCOMMAND=${CMAKE_COMMAND},-E,echo,x" -DSTATUS=0)

# No test sets PASS_REGULAR_EXPRESSION, whose pass ignores the status.
# Every such test goes through add_expect_test instead.
file(GLOB_RECURSE scripts "${ROOT}/src/*.cmake" "${ROOT}/src/*CMakeLists.txt"
     "${ROOT}/tests/*.cmake" "${ROOT}/tests/*CMakeLists.txt"
     "${ROOT}/tools/*.cmake")
list(APPEND scripts "${ROOT}/CMakeLists.txt")
foreach(script IN LISTS scripts)
    file(STRINGS "${script}" lines
         REGEX "(^|[ \t(])PASS_REGULAR_EXPRESSION([ \t]|$)")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^[ \t]*#")
            file(RELATIVE_PATH name "${ROOT}" "${script}")
            string(APPEND failures "${name} sets PASS_REGULAR_EXPRESSION: "
                                   "${line}\n")
        endif()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
