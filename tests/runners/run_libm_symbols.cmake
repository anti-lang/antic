# Every symbol of the libm of musl in the runtime carries the prefix
# anti_rt_libm_, which src/native/libm-names.h gives it, at every level of
# every target. So no function of a libm of the system binds to a call of
# anti.math, and no function of anti.math binds to a call of C. The
# members libm-<file>.o of the library of the runtime are the files of
# musl, and every global symbol they define or name has the prefix. The
# member libm.o holds the exact functions of src/rt/libm.c, and every
# symbol it names has the prefix as well, so the level without an
# instruction of rounding calls musl and no floorf of the system. Run with
# cmake -P and these values:
#   OBJDUMP    llvm-objdump of the runtime archive
#   LIBRARIES  the libraries of the runtime, separated by commas

cmake_minimum_required(VERSION 3.21)

string(REPLACE "," ";" libraries "${LIBRARIES}")
if(libraries STREQUAL "")
    message(FATAL_ERROR "LIBRARIES names no library")
endif()
# A Mach-O object of an Apple target names the guard of the stack that
# clang writes there by default, which is no function of a libm.
set(allowed __stack_chk_fail __stack_chk_guard)
set(failures "")
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" --syms "${library}"
        RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${OBJDUMP} --syms ${library} failed:\n${err}")
    endif()
    string(REPLACE "\n" ";" lines "${text}")
    set(macho OFF)
    if(text MATCHES "file format mach-o")
        set(macho ON)
    endif()
    set(members 0)
    set(member "")
    set(seen_glue OFF)
    foreach(line IN LISTS lines)
        if(line MATCHES "\\(([^()]+\\.o)\\):[ \t]+file format")
            set(member "${CMAKE_MATCH_1}")
            if(member MATCHES "^libm-")
                math(EXPR members "${members} + 1")
            elseif(member STREQUAL "libm.o")
                set(seen_glue ON)
            endif()
            continue()
        endif()
        if(NOT member MATCHES "^libm(-.*)?\\.o$")
            continue()
        endif()
        set(symbol "")
        set(undefined OFF)
        if(line MATCHES "\\(scl +2\\) .* ([A-Za-z0-9_.$]+)$")
            # COFF: an external symbol, of section 0 when undefined.
            set(symbol "${CMAKE_MATCH_1}")
            if(line MATCHES "\\(sec +0\\)")
                set(undefined ON)
            endif()
        elseif(line MATCHES "\\*UND\\*.* ([A-Za-z0-9_.$]+)$")
            set(symbol "${CMAKE_MATCH_1}")
            set(undefined ON)
        elseif(line MATCHES "^[0-9a-f]+ g.* ([A-Za-z0-9_.$]+)$")
            set(symbol "${CMAKE_MATCH_1}")
        endif()
        if(symbol STREQUAL "")
            continue()
        endif()
        # A Mach-O name carries one _ before the name of C. REGEX REPLACE
        # would take every _ at the start, so MATCHES takes the one.
        set(bare "${symbol}")
        if(macho AND symbol MATCHES "^_(.*)$")
            set(bare "${CMAKE_MATCH_1}")
        endif()
        if(bare IN_LIST allowed)
            continue()
        endif()
        if(member STREQUAL "libm.o" AND NOT undefined)
            if(NOT bare MATCHES "^anti_rt_")
                string(APPEND failures
                    "\n  ${library}(${member}) defines ${symbol}")
            endif()
        elseif(NOT bare MATCHES "^anti_rt_libm_")
            if(undefined)
                string(APPEND failures
                    "\n  ${library}(${member}) names ${symbol}")
            else()
                string(APPEND failures
                    "\n  ${library}(${member}) defines ${symbol}")
            endif()
        endif()
    endforeach()
    if(NOT seen_glue)
        string(APPEND failures "\n  ${library} holds no member libm.o")
    endif()
    if(members EQUAL 0)
        string(APPEND failures "\n  ${library} holds no member libm-<file>.o")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "the symbols of the libm of the runtime:${failures}")
endif()
