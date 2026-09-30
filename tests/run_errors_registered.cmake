# Every program of tests/errors is read by a test. A program with its
# expected messages and no registration checks nothing, and the suite
# passes without it. Run with cmake -P and these values:
#   TESTS  the tests directory
#
# A registration names a program as `<name>.anti`, as `<name>.err` or as
# the case `"<name>|` of a list, in tests/CMakeLists.txt or in a runner.

file(GLOB programs RELATIVE "${TESTS}/errors" "${TESTS}/errors/*.anti")
file(GLOB expected RELATIVE "${TESTS}/errors" "${TESTS}/errors/*.err")
file(GLOB runners "${TESTS}/run_*.cmake")
set(text "")
foreach(file "${TESTS}/CMakeLists.txt" ${runners})
    file(READ "${file}" part)
    string(APPEND text "${part}\n")
endforeach()

set(missing "")
foreach(program IN LISTS programs)
    string(REGEX REPLACE "\\.anti$" "" name "${program}")
    string(REGEX MATCH "(^|[^A-Za-z0-9_])${name}(\\.anti|\\.err|\\|)" found
           "${text}")
    if(found STREQUAL "")
        string(APPEND missing "tests/errors/${program} is read by no test\n")
    endif()
endforeach()
foreach(file IN LISTS expected)
    string(REGEX REPLACE "\\.err$" ".anti" program "${file}")
    if(NOT EXISTS "${TESTS}/errors/${program}")
        string(APPEND missing "tests/errors/${file} has no program\n")
    endif()
endforeach()
if(NOT missing STREQUAL "")
    message(FATAL_ERROR "${missing}")
endif()
