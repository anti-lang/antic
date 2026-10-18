# tests/runners/run_source_tree.cmake refuses a tree whose document of the
# source tree lacks a row, holds a row twice or holds a row of nothing,
# and accepts a tree whose every path has one row. Run with cmake -P and
# these values:
#   RUNNER  tests/runners/run_source_tree.cmake
#   WORK    a directory for the trees, inside no git work tree of its own
#
# Each case writes a tree and its document, runs the check on it and
# states whether it must accept the tree. A case that must be refused
# names a text of the message, so a refusal for another reason fails the
# case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

# The files of every tree, each a path the document covers, and one file
# of a test directory and one note, which it does not.
set(files CMakeLists.txt src/antic/lexer.c tools/version tests/unit/test_a.c
    tests/runners/run_a.cmake tests/CMakeLists.txt tests/check/messages.cmake
    tests/check/one/main.anti docs/notes/other.md)
set(rows CMakeLists.txt src/antic/lexer.c tools/version tests/unit/test_a.c
    tests/runners/run_a.cmake tests/CMakeLists.txt tests/check/messages.cmake
    tests/check/ tests/runners/ tests/unit/)

set(failures "")
# tree_case(<name> accept|<text of the refusal> FILES <path>... ROWS <path>...)
function(tree_case name verdict)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "FILES;ROWS")
    set(tree "${WORK}/${name}")
    foreach(path IN LISTS arg_FILES)
        file(WRITE "${tree}/${path}" "a file\n")
    endforeach()
    set(text "# Source tree\n\n| Path | Purpose |\n|---|---|\n")
    foreach(row IN LISTS arg_ROWS)
        string(APPEND text "| `${row}` | A purpose. |\n")
    endforeach()
    file(WRITE "${tree}/docs/notes/source-tree.md" "${text}")
    # A file under build/ is never covered.
    file(WRITE "${tree}/build/scratch.c" "a file\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${tree}"
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
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
        return()
    endif()
    # CMake wraps a long message, so the text is read with its spaces and
    # its line ends made one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    string(FIND "${flat}" "${verdict}" at)
    if(at EQUAL -1)
        set(failures
            "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

tree_case(complete accept FILES ${files} ROWS ${rows})

set(without_file ${rows})
list(REMOVE_ITEM without_file src/antic/lexer.c)
tree_case(file_without_row "src/antic/lexer.c: no row"
          FILES ${files} ROWS ${without_file})

set(without_directory ${rows})
list(REMOVE_ITEM without_directory tests/check/)
tree_case(directory_without_row "tests/check/: no row"
          FILES ${files} ROWS ${without_directory})

tree_case(row_twice "tools/version: two rows"
          FILES ${files} ROWS ${rows} tools/version)

tree_case(row_of_nothing "src/antic/gone.c: a row of nothing"
          FILES ${files} ROWS ${rows} src/antic/gone.c)

tree_case(row_of_a_test_file "tests/check/one/main.anti: a row of nothing"
          FILES ${files} ROWS ${rows} tests/check/one/main.anti)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_source_tree.cmake:\n${failures}")
endif()
