cmake_minimum_required(VERSION 3.21)

# Every function of the runtime that calls another keeps its frame record
# on Linux and macOS, as every Anti function does, since anti_rt_trace_walk
# follows the records through the frames of the runtime. The pinned clang
# omits the frame pointer at -O2 on x86_64 Linux, and the trace of a
# program on real x86_64 hardware stopped at the C main of the runtime.
# The test reads `main` of src/rt/start.c in the runtime of every level of
# the Linux and macOS targets, which must set the frame pointer. Run with
# cmake -P and these values:
#   RUNTIME   the runtime archive
#   OBJDUMP   the llvm-objdump executable
#   WORK      a file for the listing

file(GLOB libraries "${RUNTIME}/lib/linux-*/*/libanti_rt.a"
     "${RUNTIME}/lib/macos-*/*/libanti_rt.a")
if(libraries STREQUAL "")
    message("SKIP: the runtime archive holds no runtime of Linux or macOS")
    return()
endif()
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" -d --no-show-raw-insn
                            --disassemble-symbols=main,_main "${library}"
                    RESULT_VARIABLE status OUTPUT_FILE "${WORK}"
                    ERROR_FILE "${WORK}.err")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}")
    endif()
    file(STRINGS "${WORK}" lines)
    set(record FALSE)
    set(seen FALSE)
    foreach(line IN LISTS lines)
        if(line MATCHES "<_?main>:")
            set(seen TRUE)
        elseif(seen AND line MATCHES "\t(movq\t%rsp, %rbp|mov\tx29, sp|add\tx29, sp)")
            set(record TRUE)
        endif()
    endforeach()
    if(NOT seen)
        message(FATAL_ERROR "${library} holds no main")
    endif()
    if(NOT record)
        message(FATAL_ERROR "main of ${library} keeps no frame record")
    endif()
endforeach()
