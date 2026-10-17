# A runner runs antic through antic_run or antic_build of
# tests/program_output.cmake, and never in an execute_process of its own
# or through a helper that runs its arguments.
# A build that read the status of antic alone passed a warning of antic
# or of the linker. Run with cmake -P and these values:
#   ROOT  the repository
#
# The text of a bracket argument, such as a script a self-test writes, is
# no call of the runner.

cmake_minimum_required(VERSION 3.21)

file(GLOB scripts "${ROOT}/tests/runners/run_*.cmake")
set(calls "")
foreach(script IN LISTS scripts)
    file(READ "${script}" text)
    get_filename_component(name "${script}" NAME)
    # Bracket arguments of one to three `=` become spaces of the same
    # lines. A list of the brackets would not hold them, since CMake reads
    # a `[` in a list as the start of a group.
    foreach(equals IN ITEMS "=" "==" "===")
        set(open "[${equals}[")
        set(close "]${equals}]")
        while(TRUE)
            string(FIND "${text}" "${open}" from)
            if(from EQUAL -1)
                break()
            endif()
            string(SUBSTRING "${text}" ${from} -1 rest)
            string(FIND "${rest}" "${close}" to)
            if(to EQUAL -1)
                break()
            endif()
            string(LENGTH "${close}" length)
            math(EXPR to "${to} + ${length}")
            string(SUBSTRING "${rest}" 0 ${to} inside)
            string(REGEX REPLACE "[^\n]" " " blank "${inside}")
            string(SUBSTRING "${text}" 0 ${from} head)
            string(SUBSTRING "${rest}" ${to} -1 tail)
            set(text "${head}${blank}${tail}")
        endwhile()
    endforeach()
    # A helper of the runner that runs its arguments as a command, handed
    # antic as the command, with or without a label before it.
    string(REGEX MATCHALL
           "\n[ \t]*[A-Za-z_0-9]+\\([ \t\n]*(\"[^\"\n]*\"[ \t\n]+)?\"\\$\\{ANTIC\\}\""
           handed "\n${text}")
    foreach(call IN LISTS handed)
        string(REGEX MATCH "[A-Za-z_0-9]+\\(" command "${call}")
        if(NOT command MATCHES "^(set|list|string|file|get_filename_component)\\($")
            string(STRIP "${call}" call)
            list(APPEND calls "${name}: ${call}")
        endif()
    endforeach()
    string(LENGTH "${text}" size)
    set(from 0)
    while(TRUE)
        string(SUBSTRING "${text}" ${from} -1 rest)
        string(FIND "${rest}" "execute_process(" at)
        if(at EQUAL -1)
            break()
        endif()
        math(EXPR start "${from} + ${at}")
        # The call ends at the parenthesis that closes it.
        set(depth 0)
        set(i ${start})
        while(i LESS size)
            string(SUBSTRING "${text}" ${i} 1 c)
            if(c STREQUAL "(")
                math(EXPR depth "${depth} + 1")
            elseif(c STREQUAL ")")
                math(EXPR depth "${depth} - 1")
                if(depth EQUAL 0)
                    break()
                endif()
            endif()
            math(EXPR i "${i} + 1")
        endwhile()
        math(EXPR length "${i} - ${start} + 1")
        string(SUBSTRING "${text}" ${start} ${length} call)
        if(call MATCHES "(^|[^=])\\$\\{ANTIC\\}")
            string(SUBSTRING "${text}" 0 ${start} before)
            string(REGEX MATCHALL "\n" lines "${before}")
            list(LENGTH lines line)
            math(EXPR line "${line} + 1")
            list(APPEND calls "${name}:${line}")
        endif()
        math(EXPR from "${i} + 1")
    endwhile()
endforeach()
if(calls)
    list(LENGTH calls count)
    list(JOIN calls ", " calls)
    message(FATAL_ERROR "${count} calls run antic outside antic_run and "
                        "antic_build of tests/program_output.cmake: ${calls}")
endif()
