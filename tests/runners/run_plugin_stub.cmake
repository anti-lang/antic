# The stub of the loader of plugins, by Q2 of "Answers of 2026-11-09,
# third set" in docs/decisions.md. Each case of test_plugin_stub.c calls
# the slot of the interface that the library lacks, and the stub stops
# the program with its message. Called through the sub-object the loader
# built, the message names the class, its version and the interface.
# Called with no object, an object without a table or a table the loader
# did not write, it names no library. Run with cmake -P and these values:
#   PROGRAM  rt_plugin_stub_tests, built from tests/unit/test_plugin_stub.c
#   WORK     a directory this run writes into
#
# DESIGN: abort() of the C runtime that the programs of the tests link on
# Windows ends the process with the status 3, as run_rt_oom.cmake says.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ends ABORTS)
if(CMAKE_HOST_WIN32)
    set(ends STATUS 3)
endif()
program_expect("plugin_stub_named" COMMAND "${PROGRAM}" named ${ends}
               ERR "anti: `Impl` 1.0.0 carries no such function of `host.Service`\n")
foreach(case no_object no_table other_table)
    program_expect("plugin_stub_${case}" COMMAND "${PROGRAM}" ${case} ${ends}
                   ERR "anti: a library carries no such function of its interface\n")
endforeach()
