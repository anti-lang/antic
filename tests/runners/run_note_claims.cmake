# Every claim of docs/notes/ and of docs/decisions.md that names a C
# file as the place of a function holds. A claim is a name in backquotes,
# or names joined by "and", followed by "of" or "in" and a .c or .h file
# in backquotes: "`check_hook` of `sema_class.c`". The file is a path from
# the root or the name of one file of the tree, and it holds every name
# as a word. The check refuses a claim whose file is gone, whose name
# stands for more than one file, or whose file lacks a name.
#
#   cmake -DROOT=<repository> -P tests/runners/run_note_claims.cmake
#
# DESIGN: a word of the file and not a definition. A definition of C has
# too many shapes, an entry of a table of macros among them, and a name
# that left its file is gone from it in every case the audit found. The
# files come from the disk, so the check reads an export of a commit,
# which has no .git, as it reads a work tree.

cmake_minimum_required(VERSION 3.21)

# The files the documents name that the tree does not hold. Each is
# outside the repository or gone with the native back end, which the
# history of docs/decisions.md describes.
set(outside
    # mimalloc, under build/deps/.
    src/libc.c
    # raylib, under build/deps/.
    raudio.c
    # The C library.
    math.h
    # The native back end.
    src/antic/arm64.c
    src/antic/x86_64.c)

set(documents "")
file(GLOB notes RELATIVE "${ROOT}" "${ROOT}/docs/notes/*.md")
list(APPEND documents ${notes} docs/decisions.md)
list(SORT documents)

set(sources "")
foreach(part src tests tools)
    file(GLOB_RECURSE part_sources RELATIVE "${ROOT}"
         "${ROOT}/${part}/*.c" "${ROOT}/${part}/*.h")
    list(APPEND sources ${part_sources})
endforeach()

set(name "[A-Za-z_][A-Za-z0-9_]*")
set(claim "(`${name}` and )*`${name}` (of|in) `[A-Za-z0-9_/.-]+\\.[ch]`")

set(failures "")
set(count 0)
foreach(document IN LISTS documents)
    if(NOT EXISTS "${ROOT}/${document}")
        string(APPEND failures "\n${document}: missing")
        continue()
    endif()
    # A claim may stand across the end of a line.
    file(READ "${ROOT}/${document}" text)
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    string(REGEX MATCHALL "${claim}" found "${text}")
    foreach(one IN LISTS found)
        string(REGEX MATCH "`([A-Za-z0-9_/.-]+\\.[ch])`$" path "${one}")
        set(path "${CMAKE_MATCH_1}")
        if(path IN_LIST outside)
            continue()
        endif()
        set(holders "")
        if(EXISTS "${ROOT}/${path}" AND NOT IS_DIRECTORY "${ROOT}/${path}")
            set(holders "${path}")
        else()
            string(REPLACE "." "\\." path_pattern "${path}")
            foreach(source IN LISTS sources)
                if(source MATCHES "(^|/)${path_pattern}$")
                    list(APPEND holders "${source}")
                endif()
            endforeach()
        endif()
        list(LENGTH holders holder_count)
        if(holder_count EQUAL 0)
            string(APPEND failures "\n${document}: ${one}: no file ${path}")
            continue()
        elseif(holder_count GREATER 1)
            list(JOIN holders ", " holder_list)
            string(APPEND failures
                   "\n${document}: ${one}: ${path} names ${holder_list}")
            continue()
        endif()
        file(READ "${ROOT}/${holders}" body)
        string(REGEX REPLACE " (of|in) `[^`]*`$" "" names "${one}")
        string(REGEX MATCHALL "`${name}`" names "${names}")
        foreach(quoted IN LISTS names)
            string(REPLACE "`" "" word "${quoted}")
            math(EXPR count "${count} + 1")
            if(NOT body MATCHES "(^|[^A-Za-z0-9_])${word}([^A-Za-z0-9_]|$)")
                string(APPEND failures
                       "\n${document}: ${one}: ${holders} holds no ${word}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(count EQUAL 0)
    message(FATAL_ERROR "run_note_claims.cmake: no claim under ${ROOT}")
endif()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_note_claims.cmake:${failures}")
endif()
message(STATUS "${count} claims hold")
