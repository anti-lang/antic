# Every function of anti.raylib resolves at the link on every target, by
# Q4 of "Answers of 2026-11-01" in docs/decisions.md. The module the build
# wrote declares each function as `pub extern fn <name>(`, and one of the
# two libraries of the runtime archive that a program of anti.raylib links,
# raylib and miniaudio of lib/<target>/, defines a global symbol of that
# name, which a Mach-O symbol table spells with a leading `_`. A function
# that none defines would need a shim, which no library of the archive
# holds. Run with cmake -P and these values:
#   MODULE   the source of anti.raylib that anti bind wrote
#   RUNTIME  the runtime directory
#   OBJDUMP  llvm-objdump of the runtime archive
#   TARGETS  the six target names, separated by commas

cmake_minimum_required(VERSION 3.21)

file(READ "${MODULE}" module)
string(REGEX MATCHALL "pub extern fn [A-Za-z_][A-Za-z0-9_]*\\(" declared
       "${module}")
set(functions "")
foreach(item IN LISTS declared)
    string(REGEX REPLACE "^pub extern fn ([A-Za-z0-9_]+)\\($" "\\1" name
           "${item}")
    list(APPEND functions "${name}")
endforeach()
list(LENGTH functions count)
# raylib.h alone declares more than 500, and raymath, rcamera and rlgl more
# than 300 beside them.
if(count LESS 800)
    message(FATAL_ERROR "${MODULE} declares ${count} functions")
endif()

string(REPLACE "," ";" targets "${TARGETS}")
foreach(target IN LISTS targets)
    if(target MATCHES "^windows-")
        set(files "${RUNTIME}/lib/${target}/raylib.lib"
            "${RUNTIME}/lib/${target}/miniaudio.lib")
    else()
        set(files "${RUNTIME}/lib/${target}/libraylib.a"
            "${RUNTIME}/lib/${target}/libminiaudio.a")
    endif()
    execute_process(COMMAND "${OBJDUMP}" -t ${files}
        RESULT_VARIABLE status OUTPUT_VARIABLE table ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -t of ${files} failed with "
                            "${status}\n${err}")
    endif()
    # A defined global of ELF and Mach-O is `<value> g <flags> <section>
    # <name>`, where an undefined one has the section *UND*. One of COFF
    # stands in a section above 0 with the storage class 2, external.
    if(target MATCHES "^windows-")
        string(REGEX MATCHALL
               "\\(sec +[1-9][0-9]*\\)\\(fl [^)]*\\)\\(ty +[0-9a-f]+\\)\\(scl +2\\) \\(nx [0-9]+\\) 0x[0-9a-f]+ [A-Za-z0-9_]+"
               lines "${table}")
    else()
        string(REGEX MATCHALL "\n[0-9a-f]+ g [^\n]*" lines "${table}")
    endif()
    set(defined "")
    foreach(line IN LISTS lines)
        if(line MATCHES "\\*UND\\*")
            continue()
        endif()
        string(REGEX MATCH "[A-Za-z0-9_]+$" name "${line}")
        if(target MATCHES "^macos-")
            string(REGEX REPLACE "^_" "" name "${name}")
        endif()
        list(APPEND defined "${name}")
    endforeach()
    list(JOIN defined "\n" defined)
    set(defined "\n${defined}\n")
    set(missing "")
    foreach(name IN LISTS functions)
        string(FIND "${defined}" "\n${name}\n" at)
        if(at EQUAL -1)
            list(APPEND missing "${name}")
        endif()
    endforeach()
    if(NOT missing STREQUAL "")
        list(JOIN missing " " missing)
        message(FATAL_ERROR "no library of ${target} defines ${missing}")
    endif()
endforeach()
