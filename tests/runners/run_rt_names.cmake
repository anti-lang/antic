# Check that every symbol each runtime library of the archive defines for
# other objects starts with anti_rt_, as rule 25 of docs/c-guidelines.md
# asks. The runtime links into every program, so its names share the
# namespace of the user's code. Run with cmake -P and these values:
#   OBJDUMP   the llvm-objdump executable
#   RUNTIME   the runtime archive, with lib/<target>/<level>/
#
# Three kinds of name are not the runtime's own and pass:
# - main, which src/rt/start.c defines for the program.
# - anti_lang_*, the root class under the mangling of Anti, which
#   CLAUDE.md records.
# - On COFF, the static part of the C runtime that src/rt/platform_windows.c
#   defines under the names the compiler, the linker and the loader of
#   Windows look for, as the entry on the runtime of a Windows target
#   under "Binary distribution" in docs/decisions.md records: the entry
#   points, the tables and the marks, the stack probe, the cookie and the
#   printf family, which the headers of the UCRT declare and
#   ucrtbase.dll does not export, and the fourteen functions of the
#   library of mingw-w64 the native libraries and the C of the tests
#   reach. The list here is the seam the test pins.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${OBJDUMP}")
    message("SKIP: no llvm-objdump in the runtime archive")
    return()
endif()
file(GLOB libraries "${RUNTIME}/lib/*/*/libanti_rt.a"
                    "${RUNTIME}/lib/*/*/anti_rt.lib")
if(libraries STREQUAL "")
    message("SKIP: the runtime archive holds no runtime library")
    return()
endif()

set(windows_crt mainCRTStartup DllMainCRTStartup __main _tls_index _tls_used
    _fltused __security_cookie __security_check_cookie __chkstk ___chkstk_ms
    atexit __local_stdio_printf_options __local_stdio_scanf_options
    printf vprintf fprintf vfprintf sprintf vsprintf snprintf vsnprintf
    _snprintf _vsnprintf scanf vscanf fscanf vfscanf sscanf vsscanf _snscanf
    wprintf vwprintf fwprintf vfwprintf swprintf vswprintf wscanf vwscanf
    fwscanf vfwscanf swscanf vswscanf _assert hypotf opendir readdir closedir
    __fpclassify __fpclassifyf __fpclassifyl __isnan __isnanf __isnanl
    __signbit __signbitf __signbitl)
set(bad "")
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" --syms "${library}"
        RESULT_VARIABLE status OUTPUT_VARIABLE table ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}\n${err}")
    endif()
    string(REPLACE ";" "\\;" table "${table}")
    string(REPLACE "\n" ";" lines "${table}")
    set(names "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^\\[ *[0-9]+\\]\\(sec +([0-9-]+)\\).*\\(scl +2\\) \\(nx [0-9]+\\) 0x[0-9a-f]+ (.+)$")
            # COFF: an external symbol with a section is a definition. A
            # weak one of the C runtime part stands as the default of its
            # name, .weak.<name>.default with the first symbol of the
            # object after it where the object has one, which clang writes
            # for a weak definition.
            set(section "${CMAKE_MATCH_1}")
            set(name "${CMAKE_MATCH_2}")
            if(name MATCHES "^\\.weak\\.([^.]+)\\.default(\\.|$)")
                set(name "${CMAKE_MATCH_1}")
            endif()
            if(NOT section STREQUAL "0" AND NOT name IN_LIST windows_crt)
                list(APPEND names "${name}")
            endif()
        elseif(line MATCHES "^[0-9a-f]+ g " AND NOT line MATCHES "\\*UND\\*")
            # ELF and Mach-O: a global symbol that is not undefined.
            string(REGEX REPLACE "^.*[ \t]" "" name "${line}")
            if(library MATCHES "/lib/macos-")
                string(REGEX REPLACE "^_" "" name "${name}")
            endif()
            list(APPEND names "${name}")
        endif()
    endforeach()
    foreach(name IN LISTS names)
        if(NOT name MATCHES "^(anti_rt_|anti_lang_)" AND NOT name STREQUAL "main")
            file(RELATIVE_PATH where "${RUNTIME}" "${library}")
            list(APPEND bad "${where}: ${name}")
        endif()
    endforeach()
endforeach()
if(NOT bad STREQUAL "")
    list(REMOVE_DUPLICATES bad)
    string(REPLACE ";" "\n  " bad "${bad}")
    message(FATAL_ERROR "runtime symbols outside anti_rt_:\n  ${bad}")
endif()
