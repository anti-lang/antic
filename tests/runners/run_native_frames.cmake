# Every function of a native library of Linux that calls another keeps its
# frame record, as every function of the runtime does, by Q6 of "Answers
# of 2026-10-24" in docs/decisions.md. anti_rt_trace_walk follows the
# records, so a trace walks through a frame of a library that calls back
# into Anti. The pinned clang omits the frame pointer at -O2 on x86_64
# Linux, and the libraries take -fno-omit-frame-pointer from the function
# antic_linux_code_options of CMakeLists.txt, as the runtime does. The
# glue of anti.regex is read with them. The libraries of clang that the
# archive copies stand outside it. Run with cmake -P and these values:
#   RUNTIME   the runtime archive
#   OBJDUMP   the llvm-objdump executable
#   WORK      a file for the listing

cmake_minimum_required(VERSION 3.21)

file(GLOB libraries "${RUNTIME}/lib/linux-*/*.a"
     "${RUNTIME}/lib/linux-*/*/libmimalloc.a")
list(FILTER libraries EXCLUDE REGEX "/lib(clang_rt\\.|unwind\\.)[^/]*$")
if(libraries STREQUAL "")
    message("SKIP: the runtime archive holds no native library of Linux")
    return()
endif()
# A call is `callq` on x86_64 and `bl` on ARM64, and a frame record is the
# frame pointer set from the stack pointer. A line of a symbol opens a
# function.
set(open "^[0-9a-f]+ <([^>]+)>:$")
set(call "\t(callq?|bl)\t")
set(record "\t(movq\t%rsp, %rbp|mov\tx29, sp|add\tx29, sp)")
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" -d --no-show-raw-insn "${library}"
        RESULT_VARIABLE status OUTPUT_FILE "${WORK}"
        ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}\n${err}")
    endif()
    file(STRINGS "${WORK}" lines REGEX "(${open}|${call}|${record})")
    # The sentinel line closes the last function.
    list(APPEND lines "0 <end>:")
    set(name "")
    set(calls FALSE)
    set(kept FALSE)
    set(count 0)
    foreach(line IN LISTS lines)
        if(line MATCHES "${open}")
            if(calls AND NOT kept)
                message(FATAL_ERROR
                    "${name} of ${library} calls another function and keeps no frame record")
            endif()
            set(name "${CMAKE_MATCH_1}")
            set(calls FALSE)
            set(kept FALSE)
            math(EXPR count "${count} + 1")
        elseif(line MATCHES "${record}")
            set(kept TRUE)
        elseif(line MATCHES "${call}")
            # A call to a label of the function itself is a jump of
            # assembly, as the AES-NI code of Mbed TLS writes, and no
            # frame of its own.
            string(FIND "${line}" "<${name}+" inside)
            if(inside EQUAL -1)
                set(calls TRUE)
            endif()
        endif()
    endforeach()
    if(count LESS 2)
        message(FATAL_ERROR "${library} holds no function")
    endif()
endforeach()
file(REMOVE "${WORK}")
