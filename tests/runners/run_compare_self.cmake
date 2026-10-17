# expect_same_bytes of tests/program_output.cmake names the first line
# where two texts differ, and the runners that compare with an expected
# file fail through it. Run with cmake -P and these values:
#   ROOT  the repository
#   WORK  a directory for the files
#
# Each case runs a script of its own, whose message must hold every
# pattern of the case. A case without patterns must pass.

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tests/program_output.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(failures "")

# Run <script> with cmake -P. Without patterns it must end with 0, and
# with them it must fail with a message that matches each.
function(compare_case name script)
    file(WRITE "${WORK}/${name}.cmake"
         "set(ROOT \"${ROOT}\")\n"
         "set(WORK \"${WORK}/${name}\")\n"
         "file(MAKE_DIRECTORY \"${WORK}/${name}\")\n"
         "include(\"${ROOT}/tests/program_output.cmake\")\n"
         "${script}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK}/${name}.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(ARGN STREQUAL "")
        if(NOT status EQUAL 0)
            set(failures "${failures}${name}: refused\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
        return()
    endif()
    foreach(pattern IN LISTS ARGN)
        if(NOT err MATCHES "${pattern}")
            set(failures "${failures}${name}: no `${pattern}` in\n${err}\n"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

compare_case(same [=[
expect_same_text("same" "a\nb\n" "a\nb\n")
]=])
compare_case(line_three [=[
expect_same_text("the dump" "one\ntwo\nthree\nfour\nfive\n"
                            "one\ntwo\nTHREE\nfour\nfive\n")
]=] "the dump differs at line 3"
    "\n +line 2: +two\n"
    "\n +expected: +three\n"
    "\n +got: +THREE\n"
    "\n +line 4: +four\n")
compare_case(shorter [=[
expect_same_text("the text" "one\ntwo\n" "one\n")
]=] "the text differs at line 2"
    "\n +expected: +two\n"
    "\n +got: +the end of the text\n")
compare_case(carriage_return [=[
string(HEX "a\r\nb\n" got)
string(HEX "a\nb\n" wanted)
expect_same_bytes("the output" "${wanted}" "${got}")
]=] "the output differs at line 1"
    "\n +expected: +a\n"
    "\n +got: +a\\\\x0d\n")
compare_case(no_newline [=[
expect_same_text("the text" "one\n" "one")
]=] "the text differs at line 1"
    "\n +got: +one \\(no line end\\)\n")

# The runners print the first difference in place of the whole text.
write_bytes("${WORK}/dump.expected" "first\nsecond\nthird\n")
compare_case(run_dump [=[
execute_process(COMMAND "${CMAKE_COMMAND}" "-DANTIC=${CMAKE_COMMAND}"
                        "-DCOMMAND=-E,echo,first\nsecnd\nthird"
                        "-DEXPECTED=${WORK}/../dump.expected"
                        -P "${ROOT}/tests/runners/run_dump.cmake"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${err}")
endif()
]=] "differs at line 2" "\n +expected: +second\n" "\n +got: +secnd\n")
compare_case(run_expect [=[
execute_process(COMMAND "${CMAKE_COMMAND}"
                        "-DCOMMAND=${CMAKE_COMMAND},-E,echo,first\nsecnd\nthird"
                        -DSTATUS=0 "-DEXPECTED=${WORK}/../dump.expected"
                        -P "${ROOT}/tests/runners/run_expect.cmake"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${err}")
endif()
]=] "differs at line 2" "\n +expected: +second\n" "\n +got: +secnd\n")
compare_case(program_expect [=[
program_expect("echo" COMMAND "${CMAKE_COMMAND}" -E echo "first\nsecnd\nthird"
               OUT_FILE "${WORK}/../dump.expected")
]=] "differs at line 2" "\n +expected: +second\n" "\n +got: +secnd\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
