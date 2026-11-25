# The areas of the tests and the map of tests/test-areas.txt, read by
# tests/CMakeLists.txt and src/native/CMakeLists.txt, which give every
# test its areas, and by tests/runners/run_test_areas.cmake, which checks
# the labels of the tree and the lines of the map.
#
# DESIGN: answer 2 of "Answers of 2026-11-25" in docs/decisions.md. Every
# test carries one area or more as a label of ctest, and `./c test
# changed` runs the tests of the areas a change touches. The line of the
# map that starts with `areas:` names the set, so the set has one
# definition. "Choosing the tests of a change" in
# docs/notes/hosts-and-harness.md gives the rule of each area.

set(ANTIC_TEST_AREAS_MAP "${CMAKE_CURRENT_LIST_DIR}/test-areas.txt")

# The words a line of the map may give in place of areas: every test, no
# test, and the tests whose command names the path.
set(ANTIC_TEST_AREAS_WORDS all none named)

# Set <out> to the areas of the map <file>, the words of its one line
# that starts with `areas:`.
function(test_areas_set out file)
    file(STRINGS "${file}" lines REGEX "^areas:")
    list(LENGTH lines count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${file} holds ${count} lines that start with areas:, not one")
    endif()
    string(REGEX REPLACE "^areas: *" "" areas "${lines}")
    string(STRIP "${areas}" areas)
    string(REGEX REPLACE " +" ";" areas "${areas}")
    set(${out} "${areas}" PARENT_SCOPE)
endfunction()

# Set ANTIC_TEST_AREAS of the calling directory to the areas of the map,
# which then is an input of the configure step.
macro(antic_test_areas_read)
    test_areas_set(ANTIC_TEST_AREAS "${ANTIC_TEST_AREAS_MAP}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${ANTIC_TEST_AREAS_MAP}")
endmacro()

# antic_test_area(<area>...) gives every test of the directory without a
# label the areas of the call before, and names the areas of the tests
# registered after it. A test that sets LABELS itself keeps them. The last
# call of a directory names no area.
#
# DESIGN: the property TESTS of a directory lists the tests in the order
# of their names, not of their registration, so a call finds the new tests
# by the label they lack. 100 calls over 8500 tests took 0.7 s of a
# configure step on the development Mac.
function(antic_test_area)
    foreach(area IN LISTS ARGN)
        if(NOT area IN_LIST ANTIC_TEST_AREAS)
            message(FATAL_ERROR "antic_test_area: ${area} is no area of "
                                "${ANTIC_TEST_AREAS_MAP}")
        endif()
    endforeach()
    get_property(current DIRECTORY PROPERTY ANTIC_TEST_AREA)
    get_property(tests DIRECTORY PROPERTY TESTS)
    set(new "")
    foreach(test IN LISTS tests)
        get_property(labelled TEST ${test} PROPERTY LABELS SET)
        if(NOT labelled)
            list(APPEND new ${test})
        endif()
    endforeach()
    if(NOT new STREQUAL "")
        if(current STREQUAL "")
            list(GET new 0 first)
            message(FATAL_ERROR "antic_test_area: ${first} and the tests "
                                "beside it stand before the first call")
        endif()
        set_property(TEST ${new} PROPERTY LABELS ${current})
    endif()
    set_property(DIRECTORY PROPERTY ANTIC_TEST_AREA "${ARGN}")
endfunction()

# Set <lines> to the lines of the map <file> that name a path, each as the
# path, one space and its words, and <wrong> to the numbers of the lines
# that are neither such a line, nor `areas:`, nor a comment or empty.
function(test_areas_lines file lines wrong)
    file(READ "${file}" text)
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "[" "(" text "${text}")
    string(REPLACE "]" ")" text "${text}")
    string(REGEX REPLACE "\r?\n" ";" all "${text}")
    set(found "")
    set(bad "")
    set(number 0)
    foreach(line IN LISTS all)
        math(EXPR number "${number} + 1")
        if(line STREQUAL "" OR line MATCHES "^#" OR line MATCHES "^areas:")
            continue()
        endif()
        if(line MATCHES "^([^ ]+) +([^ ].*)$")
            set(path "${CMAKE_MATCH_1}")
            string(STRIP "${CMAKE_MATCH_2}" words)
            string(REGEX REPLACE " +" " " words "${words}")
            list(APPEND found "${path} ${words}")
        else()
            list(APPEND bad ${number})
        endif()
    endforeach()
    set(${lines} "${found}" PARENT_SCOPE)
    set(${wrong} "${bad}" PARENT_SCOPE)
endfunction()
