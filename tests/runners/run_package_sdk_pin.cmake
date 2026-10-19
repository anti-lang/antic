# tools/pack-anti.cmake links macOS against the Apple SDK that
# tools/macos-sdk-pin names, through pinned_macos_sdk of
# tools/macos-sdk.cmake, no script of tools/ asks xcrun for the SDK of the
# machine, and the pin names its version, its name and its digest.
#
#   cmake -DROOT=<repository> -P tests/runners/run_package_sdk_pin.cmake

# DESIGN: the packer links macOS against the Apple SDK that
# tools/macos-sdk-pin names, resolved by version, and never against the
# one a bare `xcrun --show-sdk-path` returns. That call follows whatever
# Xcode is installed. Xcode brought macOS SDK 27.0 on 2026-09-20, whose
# libSystem.tbd names the target arm64e.x1-macos, and the pinned ld64.lld
# read it as malformed and left every symbol of libSystem undefined.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/pins.cmake")

file(READ "${ROOT}/tools/pack-anti.cmake" packer_text)
if(NOT packer_text MATCHES "pinned_macos_sdk\\(\"\\$\\{tools_dir\\}/macos-sdk-pin\"")
    message(FATAL_ERROR "tools/pack-anti.cmake takes the SDK of macOS from "
                        "no call of pinned_macos_sdk with tools/macos-sdk-pin")
endif()
# A call of xcrun that asks for the path of an SDK names the version with
# --sdk. The comments are left out, since one may quote the bare call.
file(GLOB scripts "${ROOT}/tools/*.cmake")
foreach(script IN LISTS scripts)
    file(STRINGS "${script}" lines)
    set(code "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "#.*" "" line "${line}")
        string(APPEND code "${line}\n")
    endforeach()
    string(REGEX MATCHALL "xcrun[^)]*" calls "${code}")
    foreach(call IN LISTS calls)
        if(call MATCHES "--show-sdk-path" AND NOT call MATCHES "--sdk[ \t]")
            file(RELATIVE_PATH name "${ROOT}" "${script}")
            message(FATAL_ERROR "${name} asks xcrun for the SDK of the "
                                "machine rather than the pinned version")
        endif()
    endforeach()
endforeach()
foreach(name MACOS_SDK_VERSION MACOS_SDK_NAME MACOS_SDK_DIGEST)
    antic_pin_value(value "${ROOT}/tools/macos-sdk-pin" "${name}")
endforeach()
