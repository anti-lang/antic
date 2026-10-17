# The headers of the runtime that hold one interface each, finding M321
# of docs/audit/summary.md. Each file below has one header, which opens
# with the comment of its purpose and declares every function and every
# object with an anti_rt_ name that the file defines outside a `static`.
# No other header of src/rt declares one of them. `struct anti_text`, the
# layout of a str, stands in str.h alone. Run with cmake -P and ROOT, the
# root of the repository.

cmake_minimum_required(VERSION 3.21)

set(src "${ROOT}/src/rt")
# Each pair is a file and its header.
set(pairs
    "platform_posix.c=platform.h" "platform_windows.c=platform.h"
    "assert.c=fail.h" "check.c=fail.h" "cast.c=fail.h"
    "hash.c=hash.h" "init.c=rt.h")
set(failures "")

# The names a header declares: a name before `(` or `;` on a line that
# starts no comment and continues none.
function(declared header out)
    set(names "")
    if(EXISTS "${src}/${header}")
        file(STRINGS "${src}/${header}" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^[A-Za-z_]" AND
               line MATCHES "[ *](anti_rt_[a-z0-9_]+)[(;]")
                list(APPEND names "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endif()
    set("${out}" "${names}" PARENT_SCOPE)
endfunction()

file(GLOB headers RELATIVE "${src}" "${src}/*.h")
foreach(header IN LISTS headers)
    declared("${header}" "names_${header}")
endforeach()
set(owners "")
foreach(pair IN LISTS pairs)
    string(REPLACE "=" ";" parts "${pair}")
    list(GET parts 0 source)
    list(GET parts 1 header)
    list(APPEND owners "${header}")
    if(NOT EXISTS "${src}/${header}")
        string(APPEND failures "\nsrc/rt/${header}: missing")
        continue()
    endif()
    set(own "${names_${header}}")
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "^(static|extern|typedef|#)" OR NOT line MATCHES "^[A-Za-z_]")
            continue()
        endif()
        if(NOT line MATCHES "[ *](anti_rt_[a-z0-9_]+)( =|[(;])")
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        if(NOT name IN_LIST own)
            string(APPEND failures
                "\nsrc/rt/${source}: ${name} is not declared in ${header}")
        endif()
        foreach(other IN LISTS headers)
            if(other STREQUAL header)
                continue()
            endif()
            if(name IN_LIST "names_${other}")
                string(APPEND failures
                    "\nsrc/rt/${other}: declares ${name} of ${source}")
            endif()
        endforeach()
    endforeach()
endforeach()

list(APPEND owners "str.h")
list(REMOVE_DUPLICATES owners)
foreach(header IN LISTS owners)
    if(NOT EXISTS "${src}/${header}")
        continue()
    endif()
    file(STRINGS "${src}/${header}" first LIMIT_COUNT 1)
    if(NOT first MATCHES "^/[*]")
        string(APPEND failures
            "\nsrc/rt/${header}: no comment of its purpose before its guard")
    endif()
endforeach()

foreach(header IN LISTS headers)
    file(STRINGS "${src}/${header}" lines REGEX "^struct anti_text [{]")
    if(lines AND NOT header STREQUAL "str.h")
        string(APPEND failures "\nsrc/rt/${header}: defines struct anti_text")
    endif()
endforeach()
if(NOT EXISTS "${src}/str.h")
    string(APPEND failures "\nsrc/rt/str.h: missing")
endif()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "a header of the runtime out of its place:${failures}")
endif()
