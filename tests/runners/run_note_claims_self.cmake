# tests/runners/run_note_claims.cmake refuses a claim of a note or of the
# decisions whose file lacks the name it gives, whose file is gone or
# whose name stands for two files, and accepts a tree whose claims hold.
# Run with cmake -P and these values:
#   RUNNER  tests/runners/run_note_claims.cmake
#   WORK    a directory for the trees
#
# Each case writes a tree of one note, docs/decisions.md and three files
# of C, runs the check on it and states whether it must accept it. A
# case that must be refused names a text of the message, so a refusal
# for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

set(good_note "# A note\n\n`lexer_one` of `src/antic/lexer.c` reads. `lexer_two`\nof `lexer.c` and `anti_rt_hash` in `src/rt/hash.c` follow.\n")
set(good_decisions "# Decisions\n\n- `lexer_one` and `lexer_two` of `lexer.c` hold it. `_mi_getenv` of `src/libc.c` reads nothing.\n")

set(failures "")
# tree_case(<name> accept|<text of the refusal> <text of the note>
#           [<text of docs/decisions.md>])
function(tree_case name verdict note_text)
    set(tree "${WORK}/${name}")
    set(decisions_text "${good_decisions}")
    if(ARGC GREATER 3)
        set(decisions_text "${ARGV3}")
    endif()
    file(WRITE "${tree}/docs/notes/a.md" "${note_text}")
    file(WRITE "${tree}/docs/decisions.md" "${decisions_text}")
    file(WRITE "${tree}/src/antic/lexer.c"
        "/* Reads. */\n\nint lexer_one(void)\n{\n    return 1;\n}\n\nint lexer_two(void)\n{\n    return 2;\n}\n")
    file(WRITE "${tree}/src/rt/hash.c"
        "/* Hashes. */\n\nint anti_rt_hash(void)\n{\n    return 0;\n}\n")
    file(WRITE "${tree}/src/anti/hash.c"
        "/* Hashes again. */\n\nint hash_again(void)\n{\n    return 0;\n}\n")
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

tree_case(complete accept "${good_note}")

tree_case(moved
          "docs/notes/a.md: `lexer_three` of `lexer.c`: src/antic/lexer.c holds no lexer_three"
          "# A note\n\n`lexer_three` of `lexer.c` reads.\n")

tree_case(moved_across_a_line
          "src/antic/lexer.c holds no lexer_three"
          "# A note\n\nThe function `lexer_three`\nof `src/antic/lexer.c` reads.\n")

tree_case(second_of_two
          "`lexer_one` and `hash_again` of `lexer.c`: src/antic/lexer.c holds no hash_again"
          "# A note\n\n`lexer_one` and `hash_again` of `lexer.c` read.\n")

tree_case(moved_in
          "src/rt/hash.c holds no hash_again"
          "# A note\n\n`hash_again` in `src/rt/hash.c` reads.\n")

tree_case(name_as_part_of_a_word
          "src/antic/lexer.c holds no lexer"
          "# A note\n\n`lexer` of `lexer.c` reads.\n")

tree_case(file_gone "no file src/antic/parser.c"
          "# A note\n\n`parser_one` of `src/antic/parser.c` reads.\n")

tree_case(two_files "hash.c names src/anti/hash.c, src/rt/hash.c"
          "# A note\n\n`anti_rt_hash` of `hash.c` reads.\n")

tree_case(decisions_moved
          "docs/decisions.md: `lexer_three` of `lexer.c`"
          "${good_note}"
          "# Decisions\n\n- `lexer_three` of `lexer.c` holds it.\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_note_claims.cmake:\n${failures}")
endif()
