# Link tests/framework/core_foundation.anti for TARGET, a macOS target, with
# the framework CoreFoundation, which only Apple's SDK carries. The stubs
# come from sdk/ of the sysroot, or from the Command Line Tools on a Mac.
# Without either the link fails with a message that names the framework
# and where a Mac keeps the SDK. Run with cmake -P and these values:
#   ANTIC         the antic executable
#   LLVM_MC       the llvm-mc executable
#   LLVM_OBJDUMP  the llvm-objdump executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file
#   WORK          a directory for the executable
#   TARGET        macos-arm64 or macos-x86_64

file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/libanti_rt.a")
if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}" OR runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no sysroot or runtime for ${TARGET}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/core_foundation-${TARGET}")
file(REMOVE "${exe}")
execute_process(
    COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" --framework CoreFoundation -o "${exe}"
            "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err)
if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}/sdk/sdk-version" AND
   NOT CMAKE_HOST_APPLE)
    string(REGEX REPLACE "[ \n]+" " " said "${err}")
    if(status EQUAL 0 OR NOT said MATCHES "CoreFoundation" OR
       NOT said MATCHES "/Library/Developer/CommandLineTools/SDKs")
        message(FATAL_ERROR "a link that names a framework without an SDK "
                            "passed or named neither the framework nor the "
                            "SDK of a Mac:\n${err}")
    endif()
    return()
endif()
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for ${TARGET}\n${err}")
endif()
execute_process(COMMAND "${LLVM_OBJDUMP}" --macho --dylibs-used "${exe}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR
   NOT out MATCHES "CoreFoundation\\.framework/Versions/A/CoreFoundation")
    message(FATAL_ERROR "${exe} names no CoreFoundation\n${out}${err}")
endif()
# A Mac runs both macOS targets, the x86_64 one under Rosetta.
if(CMAKE_HOST_APPLE)
    execute_process(COMMAND "${exe}" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${exe} exited with ${status}")
    endif()
endif()
