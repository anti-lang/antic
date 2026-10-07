# No file that a user receives holds a path of the machine that built it.
# Debug information and __FILE__ carry one unless the build maps it away.
#
#   cmake -DROOT=<repository> -DRUNTIME=<runtime archive> -DANTIC=<antic>
#         -DREADOBJ=<llvm-readobj> -P tests/run_no_paths.cmake
#
# file(STRINGS) reads the text of every byte of each file, so the test
# needs no tool of the host and runs wherever the suite does.
#
# DESIGN: the debug map of a Mach-O program is left out, the symbols of
# type N_OSO that name each object the linker read, for the debugger. A
# development build of antic has one, and the antic of a package has
# none, since the packer compiles without -g. The strings of macOS never
# read the symbol table, so the test read no debug map before either.
# llvm-readobj, which the runtime archive carries, lists those names, and
# the test removes each from the text before it looks for the path.

cmake_minimum_required(VERSION 3.21)

file(GLOB_RECURSE shipped "${RUNTIME}/lib/*.a" "${RUNTIME}/lib/*.lib"
     "${RUNTIME}/lib/*.o" "${RUNTIME}/lib/*.obj")
if(shipped STREQUAL "")
    message(FATAL_ERROR "no library under ${RUNTIME}/lib")
endif()
list(APPEND shipped "${ANTIC}")

get_filename_component(root "${ROOT}" ABSOLUTE)
set(found "")
foreach(file IN LISTS shipped)
    file(STRINGS "${file}" text)
    file(READ "${file}" magic LIMIT 4 HEX)
    if(magic STREQUAL "cffaedfe")
        execute_process(COMMAND "${READOBJ}" --symbols "${file}"
                        RESULT_VARIABLE status OUTPUT_VARIABLE symbols
                        ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "llvm-readobj failed on ${file}\n${err}")
        endif()
        string(REGEX MATCHALL
               "Name: [^\n]* \\([0-9]+\\)\n *Type: SymDebugTable \\(0x66\\)"
               objects "${symbols}")
        foreach(object IN LISTS objects)
            string(REGEX REPLACE "^Name: (.*) \\([0-9]+\\)\n.*$" "\\1" name
                   "${object}")
            string(REPLACE "${name}" "" text "${text}")
        endforeach()
    endif()
    string(FIND "${text}" "${root}" at)
    if(NOT at EQUAL -1)
        get_filename_component(name "${file}" NAME)
        string(APPEND found "${name} holds ${root}\n")
    endif()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "${found}")
endif()
