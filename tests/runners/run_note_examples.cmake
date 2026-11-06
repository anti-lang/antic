# Every `anti` block of a note stands word for word in one of the files a
# test builds, so an example of the note compiles wherever that file does.
# A block is the text between a line ```anti and the line ``` that closes
# it. The check refuses a note without a block, and names each block that
# no file holds by its first line. Run with cmake -P and these values:
#   NOTE    the note, docs/notes/graphics.md
#   FILES   the files, separated by commas

cmake_minimum_required(VERSION 3.21)

# A semicolon is the separator of a CMake list, so it stands as a byte that
# no note and no source holds while the blocks are split.
string(ASCII 1 SEMI)
file(READ "${NOTE}" note)
string(REPLACE ";" "${SEMI}" note "${note}")
string(REGEX MATCHALL "\n```anti\n[^`]*\n```\n" blocks "${note}")
if(NOT blocks)
    message(FATAL_ERROR "${NOTE} holds no `anti` block")
endif()

set(texts "")
string(REPLACE "," ";" files "${FILES}")
foreach(path IN LISTS files)
    file(READ "${path}" text)
    string(REPLACE ";" "${SEMI}" text "${text}")
    list(APPEND texts "${text}")
endforeach()

set(missing "")
set(count 0)
foreach(block IN LISTS blocks)
    math(EXPR count "${count} + 1")
    string(REGEX REPLACE "^\n```anti\n" "" code "${block}")
    string(REGEX REPLACE "```\n$" "" code "${code}")
    set(found FALSE)
    foreach(text IN LISTS texts)
        string(FIND "${text}" "${code}" at)
        if(NOT at EQUAL -1)
            set(found TRUE)
            break()
        endif()
    endforeach()
    if(NOT found)
        string(REGEX MATCH "^[^\n]*" first "${code}")
        string(REPLACE "${SEMI}" ";" first "${first}")
        string(APPEND missing "\n  block ${count}: ${first}")
    endif()
endforeach()
if(missing)
    message(FATAL_ERROR "no file of FILES holds these blocks of ${NOTE}:"
                        "${missing}")
endif()
message(STATUS "${count} blocks of ${NOTE} stand in the files")
