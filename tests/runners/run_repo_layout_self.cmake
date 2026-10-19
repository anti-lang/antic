# tests/runners/run_repo_layout.cmake refuses a tree with a file directly
# in src/ or with the output of a tool, and accepts a tree that follows
# the layout. Run with cmake -P and these values:
#   RUNNER  tests/runners/run_repo_layout.cmake
#   WORK    a directory for the trees, inside no git work tree of its own
#           whose top is the tree, so the check reads the disk
#
# Each case writes a tree, runs the check on it and states whether it
# must accept the tree. A case that must be refused names a text of the
# message, so a refusal for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

set(files CMakeLists.txt src/antic/lexer.c tools/version
    tools/docs-style/check_docs.py tests/runners/run_a.cmake
    docs/notes/a.md LICENSES/a.txt)

set(failures "")
# layout_case(<name> accept|<text of the refusal> <path>...)
function(layout_case name verdict)
    set(tree "${WORK}/${name}")
    foreach(path IN LISTS ARGN)
        file(WRITE "${tree}/${path}" "a file\n")
    endforeach()
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

layout_case(complete accept ${files})
layout_case(file_in_src "src/stray.c: src/ holds directories alone"
            ${files} src/stray.c)
layout_case(compiled_python
            "tools/docs-style/__pycache__/check_docs.pyc: the output of a tool"
            ${files} tools/docs-style/__pycache__/check_docs.pyc)
layout_case(finder_file "LICENSES/.DS_Store: the output of a tool"
            ${files} LICENSES/.DS_Store)
layout_case(directory_in_tests "tests/stray/a.c: tests/stray/ is no directory"
            ${files} tests/stray/a.c)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_repo_layout.cmake:\n${failures}")
endif()
