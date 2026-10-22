# The reducer of the fuzzer. It shrinks the program of a find to a small
# program that still shows it, and writes it as reduced.anti beside the
# program, after each step that shrank it.
#
#   cmake -DFIND=<find directory> [-DWORK=<dir>]
#         [-DANTIC=<antic>] [-DRUNTIME=<runtime directory>]
#         -P tools/fuzz/reduce.cmake
#
# FIND is a directory the runner wrote under the finds, with the program,
# its kind and its detail. A candidate shows the find when the check of
# tools/fuzz/common.cmake gives the same kind. A refusal must give the
# same messages, and a crash or a failure of the back end the same first
# line, each without its position, so the reduction does not drift to
# another defect. docs/notes/fuzz.md describes the steps.
#
# DESIGN: the reducer removes whole units of lines, a statement, an item
# or a block with everything inside it, as delta debugging does: halves of
# the units of one depth first, then quarters, down to one unit, depth by
# depth, until a whole pass removes nothing. The generator writes one
# statement per line and a block from the line that opens it to the line
# that closes it, so a unit stays well formed after the others go.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")

fuzz_default_tools()
if(NOT FIND OR NOT EXISTS "${FIND}/${FUZZ_PROGRAM_NAME}")
    message(FATAL_ERROR "usage: cmake -DFIND=<find directory> -P tools/fuzz/reduce.cmake\n"
                        "FIND must hold ${FUZZ_PROGRAM_NAME}")
endif()
get_filename_component(FIND "${FIND}" ABSOLUTE)
get_filename_component(find_name "${FIND}" NAME)
if(NOT WORK)
    set(WORK "${FUZZ_HOME}/reduce/${find_name}")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
fuzz_lang_object(LANG_OBJECT "${WORK}")

file(READ "${FIND}/${FUZZ_KIND_NAME}" KIND)
string(STRIP "${KIND}" KIND)
file(READ "${FIND}/${FUZZ_DETAIL_NAME}" detail)

# The modes a candidate builds in: the one that showed a failure of
# antic, the one of a report of --memory-checks, and all three for a
# difference.
set(MODES ${FUZZ_MODES})
if(KIND MATCHES "^(refused|crash|backend|timeout)$"
   AND detail MATCHES "in mode ([a-z_]+)")
    set(MODES ${CMAKE_MATCH_1})
elseif(KIND STREQUAL "memory")
    set(MODES memory_checks)
endif()
list(GET MODES 0 MODE)

# Set <out> to what antic printed in <work> for the mode MODE, without
# positions: every message of a refusal, and the first line otherwise.
function(fuzz_signature out work)
    set(log "${work}/${MODE}.antic")
    if(NOT EXISTS "${log}")
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    file(STRINGS "${log}" lines)
    if(NOT KIND STREQUAL "refused")
        list(SUBLIST lines 0 1 lines)
    endif()
    set(found "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^.*\\.anti:[0-9]+:[0-9]+: " "" line "${line}")
        string(REPLACE "${work}" "" line "${line}")
        list(APPEND found "${line}")
    endforeach()
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

# Split the program into the variables line_<i>, 1 to LINE_COUNT, each
# alive, alive_<i> set to 1.
file(READ "${FIND}/${FUZZ_PROGRAM_NAME}" rest)
set(LINE_COUNT 0)
while(NOT rest STREQUAL "")
    string(FIND "${rest}" "\n" at)
    math(EXPR LINE_COUNT "${LINE_COUNT} + 1")
    if(at EQUAL -1)
        set(line_${LINE_COUNT} "${rest}")
        set(rest "")
    else()
        string(SUBSTRING "${rest}" 0 ${at} line_${LINE_COUNT})
        math(EXPR at "${at} + 1")
        string(SUBSTRING "${rest}" ${at} -1 rest)
    endif()
    set(alive_${LINE_COUNT} 1)
endwhile()

# The braces each line opens and closes, whether it starts a unit, a line
# that does not start with `}` and is not blank, and whether it is the
# header of an item, whose body opens on the line after it.
foreach(i RANGE 1 ${LINE_COUNT})
    string(REGEX MATCHALL "{" opens "${line_${i}}")
    string(REGEX MATCHALL "}" closes "${line_${i}}")
    list(LENGTH opens opens)
    list(LENGTH closes closes)
    math(EXPR net_${i} "${opens} - ${closes}")
    set(header_${i} 0)
    if(line_${i} MATCHES "^[ \t]*$" OR line_${i} MATCHES "^[ \t]*}")
        set(starts_${i} 0)
    else()
        set(starts_${i} 1)
        if(net_${i} EQUAL 0 AND NOT line_${i} MATCHES "[;,}]$"
           AND NOT line_${i} MATCHES "^//")
            set(header_${i} 1)
        endif()
    endif()
endforeach()

# Set <out> to the text of the alive lines, with one blank line where
# the program held several in a row.
function(fuzz_text out)
    set(text "")
    set(blank 1)
    foreach(i RANGE 1 ${LINE_COUNT})
        if(alive_${i})
            if(line_${i} STREQUAL "")
                if(blank)
                    continue()
                endif()
                set(blank 1)
            else()
                set(blank 0)
            endif()
            string(APPEND text "${line_${i}}\n")
        endif()
    endforeach()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Set <out> to the units of the alive lines at the depth <level>, each
# `first:last` in line numbers, in order, and <deepest> to the greatest
# depth of a unit.
function(fuzz_units out deepest level)
    set(units "")
    set(depth 0)
    set(most 0)
    set(open "")
    foreach(i RANGE 1 ${LINE_COUNT})
        if(NOT alive_${i})
            continue()
        endif()
        if(starts_${i} AND depth GREATER most)
            set(most ${depth})
        endif()
        if(open STREQUAL "" AND starts_${i})
            if(depth EQUAL level)
                if(net_${i} GREATER 0 OR header_${i})
                    set(open ${i})
                    set(open_depth ${depth})
                    set(risen 0)
                else()
                    list(APPEND units "${i}:${i}")
                endif()
            endif()
        endif()
        math(EXPR depth "${depth} + ${net_${i}}")
        if(NOT open STREQUAL "" AND depth GREATER open_depth)
            set(risen 1)
        endif()
        if(NOT open STREQUAL "" AND risen AND depth LESS_EQUAL open_depth)
            list(APPEND units "${open}:${i}")
            set(open "")
        endif()
    endforeach()
    set(${out} "${units}" PARENT_SCOPE)
    set(${deepest} ${most} PARENT_SCOPE)
endfunction()

# Set <out> to TRUE when the alive lines show the find.
set(TRIES 0)
function(fuzz_shows out)
    math(EXPR tries "${TRIES} + 1")
    set(TRIES ${tries} PARENT_SCOPE)
    fuzz_text(text)
    set(source "${WORK}/candidate.anti")
    file(WRITE "${source}" "${text}")
    set(work "${WORK}/check")
    file(REMOVE_RECURSE "${work}")
    fuzz_check(kind found "${source}" "${work}" LANG_OBJECT "${LANG_OBJECT}"
               ONLY ${MODES})
    set(${out} FALSE PARENT_SCOPE)
    if(NOT kind STREQUAL KIND)
        return()
    endif()
    if(KIND MATCHES "^(refused|crash|backend)$")
        fuzz_signature(signature "${work}")
        if(NOT signature STREQUAL SIGNATURE)
            return()
        endif()
    endif()
    set(${out} TRUE PARENT_SCOPE)
endfunction()

# The original shows the find, and gives the signature the candidates
# keep.
set(SIGNATURE "")
fuzz_text(text)
file(WRITE "${WORK}/candidate.anti" "${text}")
fuzz_check(kind found "${WORK}/candidate.anti" "${WORK}/check"
           LANG_OBJECT "${LANG_OBJECT}" ONLY ${MODES})
if(NOT kind STREQUAL KIND)
    message(FATAL_ERROR "the program of ${FIND} shows `${kind}` and not `${KIND}` "
                        "in mode ${MODES}, so there is nothing to keep")
endif()
fuzz_signature(SIGNATURE "${WORK}/check")

# Remove the units of one depth in chunks, halving the chunk down to one
# unit, and go through the depths until a whole pass removes nothing.
set(changed TRUE)
while(changed)
    set(changed FALSE)
    set(level 0)
    set(deepest 0)
    while(level LESS_EQUAL deepest)
        fuzz_units(units deepest ${level})
        list(LENGTH units count)
        math(EXPR chunk "(${count} + 1) / 2")
        while(chunk GREATER 0 AND count GREATER 0)
            set(at 0)
            while(at LESS count)
                math(EXPR end "${at} + ${chunk} - 1")
                if(end GREATER_EQUAL count)
                    math(EXPR end "${count} - 1")
                endif()
                set(removed "")
                foreach(u RANGE ${at} ${end})
                    list(GET units ${u} unit)
                    string(REPLACE ":" ";" unit "${unit}")
                    list(GET unit 0 first)
                    list(GET unit 1 last)
                    foreach(i RANGE ${first} ${last})
                        if(alive_${i})
                            set(alive_${i} 0)
                            list(APPEND removed ${i})
                        endif()
                    endforeach()
                endforeach()
                fuzz_shows(shows)
                if(shows)
                    set(changed TRUE)
                    fuzz_text(text)
                    file(WRITE "${FIND}/${FUZZ_REDUCED_NAME}" "${text}")
                    foreach(u RANGE ${at} ${end})
                        list(REMOVE_AT units ${at})
                    endforeach()
                    math(EXPR count "${count} - (${end} - ${at} + 1)")
                else()
                    foreach(i IN LISTS removed)
                        set(alive_${i} 1)
                    endforeach()
                    math(EXPR at "${end} + 1")
                endif()
            endwhile()
            math(EXPR chunk "${chunk} / 2")
        endwhile()
        math(EXPR level "${level} + 1")
    endwhile()
endwhile()

fuzz_text(text)
file(WRITE "${FIND}/${FUZZ_REDUCED_NAME}" "${text}")
string(REGEX MATCHALL "\n" kept "${text}")
list(LENGTH kept kept)
message("${FIND}/${FUZZ_REDUCED_NAME}: ${kept} of ${LINE_COUNT} lines after ${TRIES} checks")
