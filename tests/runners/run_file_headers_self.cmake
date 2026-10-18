# tests/runners/run_file_headers.cmake refuses a file that opens without
# the comment of its purpose and a function of a public header without a
# comment of its own, and accepts a tree whose files have both. Run with
# cmake -P and these values:
#   RUNNER  tests/runners/run_file_headers.cmake
#   WORK    a directory for the trees
#
# Each case writes a tree of two files of antic, one of the runtime and
# one of anti, runs the check on it and states whether it must accept the tree. A
# case that must be refused names a text of the message, so a refusal
# for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

set(good_c "/* Reads a thing. */\n\n#include \"lexer.h\"\n")
set(good_h "#ifndef ANTIC_LEXER_H\n#define ANTIC_LEXER_H\n\n/* The thing. */\n\n#include <stddef.h>\n\n/* Gives one. */\nint lexer_one(void);\n\n/* Gives two from a\n   and b. */\nint lexer_two(int a,\n              int b);\n\n#endif\n")
# A header of the runtime, whose comment stands before its guard.
set(good_rt "/* The calls of hash.c. */\n\n#ifndef ANTI_HASH_H\n#define ANTI_HASH_H\n\n/* Gives a hash. */\nint anti_rt_hash(void);\n\n#endif\n")

# A header of anti.
set(good_anti "#ifndef ANTI_ZIP_H\n#define ANTI_ZIP_H\n\n/* The zip archive. */\n\n/* Frees an archive. */\nvoid zip_free(void *z);\n\n#endif\n")

set(failures "")
# tree_case(<name> accept|<text of the refusal> <text of lexer.c> <text of lexer.h>
#           [<text of src/rt/hash.h> [<text of src/anti/zip.h>]])
function(tree_case name verdict c_text h_text)
    set(tree "${WORK}/${name}")
    set(rt_text "${good_rt}")
    set(anti_text "${good_anti}")
    if(ARGC GREATER 4)
        set(rt_text "${ARGV4}")
    endif()
    if(ARGC GREATER 5)
        set(anti_text "${ARGV5}")
    endif()
    file(WRITE "${tree}/src/antic/lexer.c" "${c_text}")
    file(WRITE "${tree}/src/antic/lexer.h" "${h_text}")
    file(WRITE "${tree}/src/rt/hash.h" "${rt_text}")
    file(WRITE "${tree}/src/anti/zip.h" "${anti_text}")
    # A shared header needs no comment per function.
    file(WRITE "${tree}/src/antic/parser_parser.h"
        "/* The shared state. */\n\nint parser_one(void);\nint parser_two(void);\n")
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

tree_case(complete accept "${good_c}" "${good_h}")

tree_case(no_header "src/antic/lexer.c: opens with no comment"
          "#include \"lexer.h\"\n\n/* Reads a thing. */\n" "${good_h}")

tree_case(header_after_includes "src/antic/lexer.h: opens with no comment"
          "${good_c}"
          "#ifndef ANTIC_LEXER_H\n#define ANTIC_LEXER_H\n\n#include <stddef.h>\n\n/* The thing. */\n\n#endif\n")

tree_case(design_header "src/antic/lexer.c: opens with a DESIGN comment"
          "/* DESIGN: one pass. */\n\n#include \"lexer.h\"\n" "${good_h}")

tree_case(header_of_an_item
          "src/antic/lexer.c: the first comment describes the item below it"
          "/* Gives one. */\nint lexer_one(void)\n{\n    return 1;\n}\n"
          "${good_h}")

tree_case(function_without_comment
          "src/antic/lexer.h:5: lexer_one has no comment of its own"
          "${good_c}"
          "/* The thing. */\n\n#include <stddef.h>\n\nint lexer_one(void);\n")

tree_case(shared_comment
          "src/antic/lexer.h:5: lexer_two has no comment of its own"
          "${good_c}"
          "/* The thing. */\n\n/* Give one and two. */\nint lexer_one(void);\nint lexer_two(\n    int a);\n")

tree_case(comment_apart
          "src/antic/lexer.h:5: lexer_one has no comment of its own"
          "${good_c}"
          "/* The thing. */\n\n/* Gives one. */\n\nint lexer_one(void);\n")

tree_case(runtime_header_fastened
          "src/rt/hash.h: the first comment describes the item below it"
          "${good_c}" "${good_h}"
          "/* The calls of hash.c. */\n#ifndef ANTI_HASH_H\n#define ANTI_HASH_H\n\n/* Gives a hash. */\nint anti_rt_hash(void);\n\n#endif\n")

tree_case(runtime_function_without_comment
          "src/rt/hash.h:6: anti_rt_hash has no comment of its own"
          "${good_c}" "${good_h}"
          "/* The calls of hash.c. */\n\n#ifndef ANTI_HASH_H\n#define ANTI_HASH_H\n\nint anti_rt_hash(void);\n\n#endif\n")

tree_case(anti_header_after_includes "src/anti/zip.h: opens with no comment"
          "${good_c}" "${good_h}" "${good_rt}"
          "#ifndef ANTI_ZIP_H\n#define ANTI_ZIP_H\n\n#include <stddef.h>\n\n/* The zip archive. */\n\n#endif\n")

tree_case(anti_function_without_comment
          "src/anti/zip.h:8: zip_close has no comment of its own"
          "${good_c}" "${good_h}" "${good_rt}"
          "#ifndef ANTI_ZIP_H\n#define ANTI_ZIP_H\n\n/* The zip archive. */\n\n/* Frees an archive. */\nvoid zip_free(void *z);\nvoid zip_close(void *z);\n\n#endif\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_file_headers.cmake:\n${failures}")
endif()
