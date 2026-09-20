# Check the instructions of the runtime archive's library for one target.
# The library is built for the default level of that target, so its
# atomics are one instruction at armv8.2 and above and a load-store
# exclusive loop below. Run with cmake -P and these values:
#   OBJDUMP   the llvm-objdump executable
#   LIBRARY   the static library of the target
#   TARGET    the antic target name
#   LEVEL     the level it is built for, for the message
#   HOST_BUILD yes when the library is the one this build compiled
#   USES      mnemonics the library must hold, separated by |
#   AVOIDS    mnemonics it must not hold, separated by |

if(HOST_BUILD STREQUAL "yes")
    message("SKIP: the ${TARGET} library is the one of this build, at the "
            "optimisation of this build, and not the cross build the "
            "archive ships")
    return()
endif()
if(NOT EXISTS "${LIBRARY}" OR NOT EXISTS "${OBJDUMP}")
    message("SKIP: no runtime library or llvm-objdump for ${TARGET}")
    return()
endif()
execute_process(COMMAND "${OBJDUMP}" -d "${LIBRARY}"
    RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-objdump failed with ${status}\n${err}")
endif()
string(REPLACE "|" ";" uses "${USES}")
string(REPLACE "|" ";" avoids "${AVOIDS}")
foreach(mnemonic IN LISTS uses)
    if(NOT text MATCHES "\t${mnemonic}\t")
        message(FATAL_ERROR
            "the ${TARGET} runtime at ${LEVEL} holds no ${mnemonic}")
    endif()
endforeach()
foreach(mnemonic IN LISTS avoids)
    if(text MATCHES "\t${mnemonic}\t")
        message(FATAL_ERROR
            "the ${TARGET} runtime at ${LEVEL} holds ${mnemonic}")
    endif()
endforeach()
