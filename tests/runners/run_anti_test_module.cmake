# One module of tests/anti-test under `anti test`, in dev mode. The run
# compiles the `tests` and the `fixtures` block of the module, links the
# runner and runs it. It passes when every test passed and the output is
# the bytes of <module>.expected: one `ok` line per test and the count.
#
#   cmake -DANTI=<anti> -DRUNTIME=<runtime directory>
#         -DMODULES=<tests/anti-test> -DMODULE=<com/example/name>
#         -DWORK=<dir> -P tests/runners/run_anti_test_module.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

program_output(got status "${WORK}/anti-test.out"
               "${ANTI}" test --work "${WORK}" -I "${MODULES}"
               --runtime "${RUNTIME}"
               "${MODULES}/${MODULE}.anti")
file(READ "${WORK}/anti-test.out" text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti test failed with ${status}\n${text}")
endif()
file(READ "${MODULES}/${MODULE}.expected" wanted HEX)
expect_same_bytes("the output of anti test" "${wanted}" "${got}")
