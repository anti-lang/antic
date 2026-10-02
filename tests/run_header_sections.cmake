# The shared headers of the parser, the checker and lowering group their
# declarations under a comment that names a file, as /* sema_call.c */.
# Every function declared under such a comment is defined in that file,
# and the comment at the top of the header names every file of its
# family. Run with cmake -P and ROOT, the root of the repository.

set(src "${ROOT}/src/antic")
set(failures "")

# header: the shared header. family: the prefix of its files, as sema.
function(check_header header family)
    file(GLOB members RELATIVE "${src}" "${src}/${family}.c"
        "${src}/${family}_*.c")
    file(READ "${src}/${header}" text)
    # The comment at the top, up to the first */.
    string(FIND "${text}" "*/" end)
    string(SUBSTRING "${text}" 0 ${end} purpose)
    foreach(member IN LISTS members)
        string(REPLACE "." "\\." quoted "${member}")
        if(NOT purpose MATCHES "(^|[ \n])${quoted}([ ,.\n]|$)")
            string(APPEND failures
                "\n${header}: the comment at the top does not name ${member}")
        endif()
    endforeach()

    file(STRINGS "${src}/${header}" lines)
    set(section "")
    set(number 0)
    set(count 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^/\\* ([a-z_]+\\.c) \\*/$")
            set(section "${CMAKE_MATCH_1}")
            if(NOT section IN_LIST members)
                string(APPEND failures
                    "\n${header}:${number}: a section of ${section}, which is no file of ${family}")
            endif()
            continue()
        endif()
        if(section STREQUAL "" OR line MATCHES "^(typedef|extern|#|[ /])"
           OR NOT line MATCHES "^[a-z].*[ *]([a-z_][a-z0-9_]*)\\(")
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        math(EXPR count "${count} + 1")
        # A definition starts at the first column and ends its first line
        # without the ; of a declaration.
        file(STRINGS "${src}/${section}" defined
            REGEX "^[a-z].*[ *]${name}\\(.*[^;]$")
        set(found FALSE)
        foreach(definition IN LISTS defined)
            if(NOT definition MATCHES "^static ")
                set(found TRUE)
            endif()
        endforeach()
        if(NOT found)
            string(APPEND failures
                "\n${header}:${number}: ${name} stands under ${section}, which does not define it")
        endif()
    endforeach()
    if(count EQUAL 0)
        string(APPEND failures "\n${header}: no declaration under a section")
    endif()
    set(failures "${failures}" PARENT_SCOPE)
endfunction()

check_header("parser_parser.h" "parser")
check_header("sema_checker.h" "sema")
check_header("lower_lowerer.h" "lower")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "declarations under the wrong file:${failures}")
endif()
