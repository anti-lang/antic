# Every file of the parts of the tree named below opens with a comment
# that says its purpose, and every function a public header of them
# declares has a comment of its own. Run with cmake -P and ROOT, the root
# of the repository.
#
# The header of a file is its first line, or its first after the include
# guard where the guard comes first. It is a comment, it is no DESIGN,
# PERF DECISION or [AI AGENT] comment, and a blank line follows it, so it
# describes the file and not the item below it. A declaration of a
# public header has a comment that ends on the line above its first
# line, so two declarations never share one. The shared headers of a
# family of files are no public headers, and header_sections checks
# them.
#
# A module of the standard library opens with its `//!` comment. Every
# `pub` item and field and every `abstract fn` has a `///` comment that
# ends on the line above it, and the comment of such a function that may
# fail says how it fails.
#
# Step 29 of "Fix steps" in docs/audit/summary.md reaches one part of the
# tree per session, and each part joins the files read here.

cmake_minimum_required(VERSION 3.21)

# Every file of antic, of the runtime and of anti. The shared headers of
# antic are those header_sections checks. Every header of the runtime is
# public: generated code, antic, anti or another file of the runtime
# calls what it declares. Every header of anti is public as well, since
# main.c or another file of anti calls what each declares.
set(parts antic rt anti)
set(shared antl_io.h driver_parts.h lower_lowerer.h parser_parser.h
    sema_checker.h whole_parts.h)

set(files "")
foreach(part IN LISTS parts)
    file(GLOB part_files RELATIVE "${ROOT}" "${ROOT}/src/${part}/*.[ch]")
    if(part_files STREQUAL "")
        message(FATAL_ERROR "no file of ${part} under ${ROOT}/src/${part}")
    endif()
    list(APPEND files ${part_files})
endforeach()
list(SORT files)

set(failures "")

foreach(path IN LISTS files)
    file(STRINGS "${ROOT}/${path}" lines)
    list(LENGTH lines count)

    # The header: the first line after the guard and the blank lines.
    set(at 0)
    while(at LESS count)
        list(GET lines ${at} line)
        if(line STREQUAL "" OR line MATCHES "^#(ifndef|define) ANTIC?_[A-Z0-9_]+_H$")
            math(EXPR at "${at} + 1")
        else()
            break()
        endif()
    endwhile()
    set(line "")
    if(at LESS count)
        list(GET lines ${at} line)
    endif()
    if(NOT line MATCHES "^/\\*")
        string(APPEND failures "\n${path}: opens with no comment")
    elseif(line MATCHES "^/\\* *(DESIGN|PERF DECISION|\\[AI AGENT\\])")
        string(APPEND failures "\n${path}: opens with a ${CMAKE_MATCH_1} comment")
    else()
        # The line after the end of the comment is blank.
        while(at LESS count)
            list(GET lines ${at} line)
            math(EXPR at "${at} + 1")
            if(line MATCHES "\\*/$")
                break()
            endif()
        endwhile()
        set(next "")
        if(at LESS count)
            list(GET lines ${at} next)
        endif()
        if(NOT next STREQUAL "")
            string(APPEND failures
                "\n${path}: the first comment describes the item below it")
        endif()
    endif()

    get_filename_component(name "${path}" NAME)
    if(NOT path MATCHES "\\.h$" OR name IN_LIST shared)
        continue()
    endif()

    # Each declaration at the first column, with the line above it.
    set(previous "")
    set(number 0)
    set(inside FALSE)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(inside)
            # The rest of a declaration that spans lines.
            if(line MATCHES "[;{]$")
                set(inside FALSE)
            endif()
        elseif(NOT line MATCHES "^(typedef|#)"
               AND line MATCHES "^[a-z].*[ *]([a-z_][a-z0-9_]*)\\(")
            set(function "${CMAKE_MATCH_1}")
            if(NOT previous MATCHES "\\*/$")
                string(APPEND failures
                    "\n${path}:${number}: ${function} has no comment of its own")
            endif()
            if(NOT line MATCHES "[;{]$")
                set(inside TRUE)
            endif()
        endif()
        set(previous "${line}")
    endforeach()
endforeach()

# Every module of the standard library.
file(GLOB_RECURSE modules RELATIVE "${ROOT}" "${ROOT}/src/std/*.anti")
if(modules STREQUAL "")
    message(FATAL_ERROR "no module under ${ROOT}/src/std")
endif()
list(SORT modules)

foreach(path IN LISTS modules)
    file(STRINGS "${ROOT}/${path}" lines)
    set(first "")
    list(LENGTH lines count)
    if(count GREATER 0)
        list(GET lines 0 first)
    endif()
    if(NOT first MATCHES "^//! ")
        string(APPEND failures "\n${path}: opens with no `//!` comment")
    endif()

    # The `///` comment above each line, and the signature of a function
    # up to its body or its `;`.
    set(doc "")
    set(previous "")
    set(number 0)
    set(signature "")
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        string(STRIP "${line}" text)
        if(NOT signature STREQUAL "")
            string(APPEND signature " ${text}")
        elseif(text MATCHES "^(pub|abstract fn)[ \t]")
            if(NOT previous MATCHES "^///")
                string(APPEND failures
                    "\n${path}:${number}: `${text}` has no `///` comment")
            elseif(text MATCHES "^(pub[ \t]+)?([a-z]+[ \t]+)*fn[ \t]")
                set(signature "${text}")
                set(signature_doc "${doc}")
                set(signature_line ${number})
            endif()
        endif()
        if(NOT signature STREQUAL "" AND text MATCHES "[{;]$")
            # A `may fail` after the parameters, not inside a parameter type.
            string(REGEX REPLACE "[ \t]*[{;]$" "" signature "${signature}")
            if(signature MATCHES "may fail$" AND NOT signature_doc MATCHES "fail")
                string(APPEND failures
                    "\n${path}:${signature_line}: the comment of a function that may fail does not say how it fails")
            endif()
            set(signature "")
        endif()
        if(text MATCHES "^///")
            string(APPEND doc " ${text}")
        else()
            set(doc "")
        endif()
        set(previous "${text}")
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "files and functions without their comment:${failures}")
endif()
