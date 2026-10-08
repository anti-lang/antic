# Under --lto none every function of an ARM64 program and of the runtime
# it links starts on a 16-byte boundary. The test reads the object antic
# writes beside the program and the object runtime of every level of the
# target, which the link of --lto none and dev mode take. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   READOBJ   the llvm-readobj executable
#   OBJDUMP   the llvm-objdump executable
#   SOURCE    a program of several functions
#   TARGET    macos-arm64, linux-arm64 or windows-arm64
#   WORK      a directory for the files
#
# An ELF or a COFF object puts each function into a section of its own, so
# the alignment of every section of code is the alignment of its function.
# A Mach-O object holds them all in __text, so the section and the offset
# of each function within it are read.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(program "${WORK}/program")
execute_process(COMMAND "${ANTIC}" --target "${TARGET}" --runtime "${RUNTIME}"
                        --lto none -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --lto none failed\n${err}")
endif()
if("${TARGET}" MATCHES "^windows-")
    set(objects "${program}.obj")
    set(library anti_rt.lib)
else()
    set(objects "${program}.o")
    set(library libanti_rt.a)
endif()
# The object runtime of each level, and on Linux that of the glibc mode.
file(GLOB runtimes "${RUNTIME}/lib/${TARGET}/*/${library}"
     "${RUNTIME}/lib/${TARGET}-glibc/*/${library}")
if(runtimes STREQUAL "")
    message(FATAL_ERROR "the runtime archive has no ${library} for ${TARGET}")
endif()
list(APPEND objects ${runtimes})

# Check every function of the object or archive <file>.
function(check_aligned file)
    execute_process(COMMAND "${READOBJ}" --sections "${file}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE text
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-readobj --sections ${file} failed\n${err}")
    endif()
    # Square brackets would hold a list element together, and a semicolon
    # of the text would split one.
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "[" "<" text "${text}")
    string(REPLACE "]" ">" text "${text}")
    string(REPLACE "Section {" ";" sections "${text}")
    set(code 0)
    foreach(section IN LISTS sections)
        string(REGEX MATCH "Name: ([^ \n]+)" name "${section}")
        set(name "${CMAKE_MATCH_1}")
        if(section MATCHES "SHF_EXECINSTR")
            if(section MATCHES "\n +Size: 0\n")
                continue()
            endif()
            string(REGEX MATCH "AddressAlignment: ([0-9]+)" found "${section}")
            set(bytes "${CMAKE_MATCH_1}")
        elseif(section MATCHES "IMAGE_SCN_CNT_CODE")
            if(section MATCHES "\n +RawDataSize: 0\n")
                continue()
            endif()
            string(REGEX MATCH "IMAGE_SCN_ALIGN_([0-9]+)BYTES" found
                   "${section}")
            set(bytes "${CMAKE_MATCH_1}")
        elseif(name STREQUAL "__text")
            if(section MATCHES "\n +Size: 0x0\n")
                continue()
            endif()
            string(REGEX MATCH "Alignment: ([0-9]+)" found "${section}")
            math(EXPR bytes "1 << ${CMAKE_MATCH_1}")
        else()
            continue()
        endif()
        if(bytes STREQUAL "" OR bytes LESS 16)
            message(FATAL_ERROR "${file}: the code section ${name} is "
                                "aligned at `${bytes}` bytes, not 16")
        endif()
        math(EXPR code "${code} + 1")
    endforeach()
    if(code EQUAL 0)
        message(FATAL_ERROR "${file} holds no section of code")
    endif()
    if(NOT "${TARGET}" MATCHES "^macos-")
        return()
    endif()
    execute_process(COMMAND "${OBJDUMP}" -t "${file}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE table
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -t ${file} failed\n${err}")
    endif()
    string(REGEX MATCHALL "[0-9a-f]+ [^\n]* F __TEXT,__text [^\n]+" lines
           "${table}")
    foreach(line IN LISTS lines)
        string(REGEX MATCH "^([0-9a-f]+) .* ([^ ]+)$" found "${line}")
        math(EXPR offset "0x${CMAKE_MATCH_1} % 16")
        if(NOT offset EQUAL 0)
            message(FATAL_ERROR "${file}: ${CMAKE_MATCH_2} starts at "
                                "0x${CMAKE_MATCH_1}, off a 16-byte boundary")
        endif()
    endforeach()
endfunction()

foreach(file IN LISTS objects)
    check_aligned("${file}")
endforeach()
