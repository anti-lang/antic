# A module that writes no function, as the dev object of
# anti.collection.tree, whose functions are all generic, writes no
# CodeView subsection of symbols without a record: lld-link warns of an
# empty one, and the dev programs that link the object then print a
# warning. The test compiles the module as `anti build` compiles a dev
# object, with -g, and reads the object with llvm-readobj --codeview.
# The object must hold a subsection of symbols, so a build that writes no
# debug information fails as well. Run with cmake -P and these values:
#   ANTIC   the antic executable
#   RUNTIME the runtime archive, whose bin/ holds llc
#   READOBJ llvm-readobj of the runtime archive
#   STD     the directory of the library files of the standard library
#   SOURCE  the library file of the module
#   TARGET  a Windows target
#   WORK    a directory for the object
# With CLANG set instead of ANTIC, RUNTIME, STD and SOURCE, the test
# assembles an object whose one subsection of symbols is empty and passes
# when the check refuses it. That shows the check can fail.

cmake_minimum_required(VERSION 3.21)

# Sets ${out} to the number of subsections of symbols in the dump and
# ${out}_EMPTY to the number of them that hold no record. A record is a
# line `Kind: S_...`. A type record is `TypeLeafKind:`, which the match
# leaves out.
function(count_symbol_subsections dump out)
    set(rest "${dump}")
    set(count 0)
    set(empty 0)
    while(TRUE)
        string(FIND "${rest}" "SubSectionType: Symbols" at)
        if(at EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        math(EXPR from "${at} + 23")
        string(SUBSTRING "${rest}" ${from} -1 rest)
        string(FIND "${rest}" "SubSectionType:" next)
        if(next EQUAL -1)
            set(body "${rest}")
        else()
            string(SUBSTRING "${rest}" 0 ${next} body)
        endif()
        if(NOT body MATCHES "\n[ ]*Kind: S_")
            math(EXPR empty "${empty} + 1")
        endif()
    endwhile()
    set(${out} ${count} PARENT_SCOPE)
    set(${out}_EMPTY ${empty} PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${WORK}")
if(DEFINED CLANG)
    set(object "${WORK}/broken.${TARGET}.obj")
    if("${TARGET}" STREQUAL "windows-arm64")
        set(triple aarch64-pc-windows-gnu)
    else()
        set(triple x86_64-pc-windows-gnu)
    endif()
    file(WRITE "${WORK}/broken.${TARGET}.s"
        "\t.section\t.debug$S,\"dr\"\n"
        "\t.p2align\t2\n"
        "\t.long\t4\n"
        "\t.long\t241\n"
        "\t.long\t0\n")
    execute_process(COMMAND "${CLANG}" --target=${triple} -c
                            -o "${object}" "${WORK}/broken.${TARGET}.s"
                    RESULT_VARIABLE status ERROR_VARIABLE err
                    ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "clang failed with ${status}\n${err}")
    endif()
else()
    # antic adds .obj to the name -o gives.
    set(object "${WORK}/empty.${TARGET}.obj")
    file(REMOVE "${object}")
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" --dev -g
                            --target "${TARGET}" -I "${STD}"
                            -o "${WORK}/empty.${TARGET}" "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed with ${status}\n${err}")
    endif()
endif()

execute_process(COMMAND "${READOBJ}" --codeview "${object}"
                RESULT_VARIABLE status OUTPUT_VARIABLE dump
                ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-readobj failed with ${status}\n${err}")
endif()
count_symbol_subsections("${dump}" symbols)

if(DEFINED CLANG)
    if(NOT symbols EQUAL 1 OR NOT symbols_EMPTY EQUAL 1)
        message(FATAL_ERROR "the check found ${symbols_EMPTY} empty of "
                            "${symbols} subsections of symbols in an object "
                            "that holds one, empty\n${dump}")
    endif()
    return()
endif()
if(symbols EQUAL 0)
    message(FATAL_ERROR "the object for ${TARGET} holds no subsection of "
                        "symbols, so -g wrote no CodeView\n${dump}")
endif()
if(NOT symbols_EMPTY EQUAL 0)
    message(FATAL_ERROR "the object for ${TARGET} holds ${symbols_EMPTY} "
                        "subsections of symbols without a record\n${dump}")
endif()
