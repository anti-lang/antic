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
# - On COFF, the constants clang writes into COMDAT sections, ??_C@ for a
#   text and _real@, _xmm@ and _ymm@ for a number, and the inline
#   functions of the Windows C headers, which every object that calls one
#   defines once more. ARM64 writes the numbers and one of the functions
#   with a second underscore in front.

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

set(coff_inline fprintf printf snprintf vfprintf _local_stdio_printf_options
    __local_stdio_printf_options)
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
            # COFF: an external symbol with a section is a definition.
            if(NOT CMAKE_MATCH_1 STREQUAL "0")
                set(name "${CMAKE_MATCH_2}")
                if(NOT name MATCHES "^(\\?\\?_C@|__?real@|__?xmm@|__?ymm@)")
                    list(FIND coff_inline "${name}" inline)
                    if(inline EQUAL -1)
                        list(APPEND names "${name}")
                    endif()
                endif()
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
