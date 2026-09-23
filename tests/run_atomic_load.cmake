# Check that anti_rt_atomic_load of the runtime archive's library for one
# ARM64 target is a sequentially consistent load. That is ldar, ldarb or
# ldarh on every level, and never a plain ldr, which a later load may pass.
# Run with cmake -P and these values:
#   OBJDUMP   the llvm-objdump executable
#   LIBRARY   the static library of the target
#   TARGET    the antic target name, for the message

if(NOT EXISTS "${LIBRARY}" OR NOT EXISTS "${OBJDUMP}")
    message("SKIP: no runtime library or llvm-objdump for ${TARGET}")
    return()
endif()
# Mach-O writes the symbol with a leading underscore, so both names are
# asked for. objdump warns on stderr of each member that holds neither.
execute_process(COMMAND "${OBJDUMP}" -d
        "--disassemble-symbols=anti_rt_atomic_load,_anti_rt_atomic_load"
        "${LIBRARY}"
    RESULT_VARIABLE status OUTPUT_VARIABLE body ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-objdump failed with ${status}\n${err}")
endif()
if(NOT body MATCHES "<_?anti_rt_atomic_load>:")
    message(FATAL_ERROR "the ${TARGET} runtime holds no anti_rt_atomic_load")
endif()
if(NOT body MATCHES "\tldar[bh]?\t")
    message(FATAL_ERROR "anti_rt_atomic_load of the ${TARGET} runtime holds "
                        "no ldar\n${body}")
endif()
if(body MATCHES "\tldrs?[bhw]?\t")
    message(FATAL_ERROR "anti_rt_atomic_load of the ${TARGET} runtime holds "
                        "a plain load\n${body}")
endif()
