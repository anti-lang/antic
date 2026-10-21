# Link tests/framework/core_foundation.anti for TARGET, a macOS target, with
# the framework CoreFoundation, which only Apple's SDK carries. The stubs
# come from sdk/ of the sysroot, or from the Command Line Tools on a Mac.
# Without either the link fails with a message that names the framework
# and where a Mac keeps the SDK. A Mac links once more against a stand-in
# runtime whose sysroot holds sdk/. Run with cmake -P and these values:
#   ANTIC         the antic executable
#   LLVM_OBJDUMP  the llvm-objdump executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file
#   WORK          a directory for the executable
#   TARGET        macos-arm64 or macos-x86_64
#   OPTIONS       optional options of antic, separated by commas

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/libanti_rt.a")
if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}" OR runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no sysroot or runtime for ${TARGET}")
    return()
endif()
string(REPLACE "," ";" options "${OPTIONS}")
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/core_foundation-${TARGET}")
file(REMOVE "${exe}")
antic_run(status ignored_out err COMMAND --target "${TARGET}"
          --runtime "${RUNTIME}" ${options} --framework
          CoreFoundation -o "${exe}" "${SOURCE}")
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
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
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

# The link above takes the SDK of the Command Line Tools on a Mac. A
# stand-in runtime whose sysroot holds sdk/, as anti sdk import lays it
# out, links against that SDK instead, and the program records the
# version that sdk/sdk-version gives. The stubs of sdk/ are those of an
# SDK of the Command Line Tools, tried from the newest, since ld64.lld
# reads only some of them. Every other part of the stand-in is a link to
# the runtime.
if(CMAKE_HOST_APPLE)
    set(stand_in "${WORK}/sdk-runtime-${TARGET}")
    set(sysroot "${stand_in}/sysroot/${TARGET}")
    file(REMOVE_RECURSE "${stand_in}")
    file(MAKE_DIRECTORY "${sysroot}/sdk")
    foreach(pair "${RUNTIME}|${stand_in}|sysroot"
                 "${RUNTIME}/sysroot|${stand_in}/sysroot|${TARGET}"
                 "${RUNTIME}/sysroot/${TARGET}|${sysroot}|")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 from)
        list(GET pair 1 to)
        list(GET pair 2 kept)
        file(GLOB parts RELATIVE "${from}" "${from}/*")
        foreach(part IN LISTS parts)
            if(NOT part STREQUAL kept)
                file(CREATE_LINK "${from}/${part}" "${to}/${part}" SYMBOLIC)
            endif()
        endforeach()
    endforeach()
    file(WRITE "${sysroot}/sdk/sdk-version" "12.0")
    file(GLOB sdks "/Library/Developer/CommandLineTools/SDKs/MacOSX*.*.sdk")
    list(SORT sdks COMPARE NATURAL ORDER DESCENDING)
    set(linked "")
    foreach(sdk IN LISTS sdks)
        foreach(part usr System)
            file(REMOVE "${sysroot}/sdk/${part}")
            file(CREATE_LINK "${sdk}/${part}" "${sysroot}/sdk/${part}" SYMBOLIC)
        endforeach()
        antic_run(status ignored_out err COMMAND --target "${TARGET}"
                  --runtime "${stand_in}" ${options} --framework
                  CoreFoundation -o "${exe}-sdk" "${SOURCE}")
        if(status EQUAL 0)
            set(linked "${sdk}")
            break()
        endif()
    endforeach()
    if(linked STREQUAL "")
        message(FATAL_ERROR "no SDK of the Command Line Tools linked "
                            "through sdk/ of the sysroot\n${err}")
    endif()
    execute_process(COMMAND "${LLVM_OBJDUMP}" --macho --private-headers
                            "${exe}-sdk"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out MATCHES "\n +sdk 12\\.0\n")
        message(FATAL_ERROR "${exe}-sdk, linked through ${linked}, records "
                            "no SDK 12.0\n${out}${err}")
    endif()
    execute_process(COMMAND "${exe}-sdk" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${exe}-sdk exited with ${status}")
    endif()
endif()
