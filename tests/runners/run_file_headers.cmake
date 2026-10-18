# Every file of the parts of the tree named below opens with a comment
# that says its purpose, and every function a public header of them
# declares has a comment of its own. Run with cmake -P and ROOT, the root
# of the repository.
#
# The header of a file is its first line after the include guard. It is a
# comment, it is no DESIGN, PERF DECISION or [AI AGENT] comment, and a
# blank line follows it, so it describes the file and not the item below
# it. A declaration of a public header has a comment that ends on the
# line above its first line, so two declarations never share one. The
# shared headers of a family of files are no public headers, and
# header_sections checks them.
#
# Step 29 of "Fix steps" in docs/audit/summary.md reaches one part of the
# tree per session, and each part joins the files read here.

cmake_minimum_required(VERSION 3.21)

# Every file of antic. The shared headers are those header_sections checks.
set(shared antl_io.h driver_parts.h lower_lowerer.h parser_parser.h
    sema_checker.h whole_parts.h)

file(GLOB files RELATIVE "${ROOT}" "${ROOT}/src/antic/*.[ch]")
list(SORT files)
if(files STREQUAL "")
    message(FATAL_ERROR "no file of antic under ${ROOT}/src/antic")
endif()

set(failures "")

foreach(path IN LISTS files)
    file(STRINGS "${ROOT}/${path}" lines)
    list(LENGTH lines count)

    # The header: the first line after the guard and the blank lines.
    set(at 0)
    while(at LESS count)
        list(GET lines ${at} line)
        if(line STREQUAL "" OR line MATCHES "^#(ifndef|define) ANTIC_[A-Z0-9_]+_H$")
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

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "files and functions without their comment:${failures}")
endif()
