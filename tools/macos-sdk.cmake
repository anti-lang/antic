# The Apple SDK that a release links its macOS binaries against.
#
#   cmake -DPIN=<tools/macos-sdk-pin> [-DSDKS=<dir>] -P tools/macos-sdk.cmake
#
# Run with -P it prints the path of the pinned SDK, or fails naming the
# version. tools/pack-anti.cmake includes it and calls pinned_macos_sdk.
# SDKS names the directory the SDKs stand in, and the Command Line Tools
# are the default.
#
# DESIGN: a release links its macOS binaries against a pinned Apple SDK
# and never against whatever `xcrun --show-sdk-path` returns, because that
# follows the Xcode of the machine. Xcode brought macOS SDK 27.0 on
# 2026-09-20, whose libSystem.tbd names the target arm64e.x1-macos. The
# pinned ld64.lld read that file as malformed and left every symbol of
# libSystem undefined, and the packer stopped at "macos-arm64: antic did
# not link". The digest of libSystem.tbd catches an SDK that changed
# under the same name. The pin moves with the pinned LLVM and not before,
# which docs/toolchain-later.md holds.
cmake_minimum_required(VERSION 3.20)

# The default directory of the SDKs. The Command Line Tools keep every
# SDK they have installed, while the SDKs directory of Xcode holds the
# newest alone.
set(MACOS_SDK_ROOT "/Library/Developer/CommandLineTools/SDKs")

function(pinned_macos_sdk pin sdks out)
    file(STRINGS "${pin}" rows REGEX "^MACOS_SDK_")
    foreach(row IN LISTS rows)
        if(row MATCHES "^MACOS_SDK_VERSION=(.*)$")
            set(version "${CMAKE_MATCH_1}")
        elseif(row MATCHES "^MACOS_SDK_NAME=(.*)$")
            set(name "${CMAKE_MATCH_1}")
        elseif(row MATCHES "^MACOS_SDK_DIGEST=(.*)$")
            set(want "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED version OR NOT DEFINED name OR NOT DEFINED want)
        message(FATAL_ERROR "${pin} names no version, name and digest of the "
                            "Apple SDK")
    endif()
    if(sdks STREQUAL "")
        set(sdks "${MACOS_SDK_ROOT}")
    endif()

    # xcrun answers by version when it is pointed at the Command Line
    # Tools, and the path of the pin stands for the case where it does
    # not answer at all.
    set(sdk "")
    if(sdks STREQUAL "${MACOS_SDK_ROOT}")
        get_filename_component(developer "${sdks}/.." ABSOLUTE)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "DEVELOPER_DIR=${developer}"
                    xcrun --sdk "macosx${version}" --show-sdk-path
            OUTPUT_VARIABLE sdk OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_VARIABLE ignored RESULT_VARIABLE failed)
        if(failed)
            set(sdk "")
        endif()
    endif()
    if(NOT IS_DIRECTORY "${sdk}")
        set(sdk "${sdks}/${name}")
    endif()
    if(NOT IS_DIRECTORY "${sdk}")
        message(FATAL_ERROR "macOS SDK ${version} is on no path of this "
                            "machine. ${pin} names it, and a release links "
                            "every macOS binary against it. Install the "
                            "Command Line Tools that carry ${name}.")
    endif()

    set(tbd "${sdk}/usr/lib/libSystem.tbd")
    if(NOT EXISTS "${tbd}")
        message(FATAL_ERROR "${sdk} carries no usr/lib/libSystem.tbd")
    endif()
    file(SHA256 "${tbd}" got)
    if(NOT got STREQUAL "${want}")
        message(FATAL_ERROR "libSystem.tbd of macOS SDK ${version} has the "
                            "digest ${got}, and ${pin} names ${want}. The SDK "
                            "changed under its own name. Read the new one "
                            "before the pin moves.")
    endif()
    set("${out}" "${sdk}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if(NOT DEFINED PIN)
        message(FATAL_ERROR "usage: cmake -DPIN=<tools/macos-sdk-pin> "
                            "[-DSDKS=<dir>] -P tools/macos-sdk.cmake")
    endif()
    if(NOT DEFINED SDKS)
        set(SDKS "")
    endif()
    pinned_macos_sdk("${PIN}" "${SDKS}" sdk)
    # message() writes to standard error in script mode, and the path is
    # the output of this script.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${sdk}")
endif()
