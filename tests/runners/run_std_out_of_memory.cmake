# M330. Memory that runs out in the standard library stops the program
# with a message, by decision 4 of "Decisions of 2026-10-11". `alloc(T, n)`
# gives `none` when the memory runs out, so every one of a module is bound
# with `else` and a block that calls anti_rt_out_of_memory, which ends the
# program through the failure routine of the runtime. No module fails
# with "out of memory" either. Run with cmake -P and ROOT, the directory
# whose `.anti` files are read at every depth.
#
# `alloc` after a `.` is a function of an allocator, which gives `none` to
# its caller as the language does, and `fn alloc(` declares one. The
# comments are left out first, since they name both forms.

cmake_minimum_required(VERSION 3.21)

file(GLOB_RECURSE sources "${ROOT}/*.anti")
if(NOT sources)
    message(FATAL_ERROR "no module in ${ROOT}")
endif()
string(ASCII 1 SEMI)
set(offenders "")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    string(REPLACE ";" "${SEMI}" content "${content}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" content "${content}")
    string(REGEX REPLACE "\n[ \t]*//[^\n]*" "\n" content "\n${content}")
    string(REPLACE "fn alloc(" "fn declared(" content "${content}")
    string(REGEX MATCHALL "[^.A-Za-z_0-9]alloc\\([^)]*\\)" calls "${content}")
    string(REGEX MATCHALL
        "[^.A-Za-z_0-9]alloc\\([^)]*\\) else {[^}]*anti_rt_out_of_memory\\([^}]*}"
        stopped "${content}")
    string(REGEX MATCHALL "fail [^\n]*\"out of memory\"" failures "${content}")
    list(LENGTH calls call_count)
    list(LENGTH stopped stopped_count)
    list(LENGTH failures failure_count)
    file(RELATIVE_PATH file "${ROOT}" "${source}")
    if(NOT call_count EQUAL stopped_count)
        math(EXPR left "${call_count} - ${stopped_count}")
        string(APPEND offenders
            "\n  ${file}: ${left} of ${call_count} alloc(T, n) without the stop")
    endif()
    if(failure_count GREATER 0)
        string(APPEND offenders
            "\n  ${file}: ${failure_count} failure of \"out of memory\"")
    endif()
endforeach()
if(offenders)
    message(FATAL_ERROR
        "memory that runs out answered other than by the stop:${offenders}")
endif()
