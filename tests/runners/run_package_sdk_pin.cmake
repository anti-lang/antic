# tools/pack-anti.cmake links macOS against the Apple SDK that
# tools/macos-sdk-pin names, and the pin names its version, its name and
# its digest.
#
#   cmake -DROOT=<repository> -P tests/runners/run_package_sdk_pin.cmake

# DESIGN: the packer links macOS against the Apple SDK that
# tools/macos-sdk-pin names, resolved by version, and never against the
# one a bare `xcrun --show-sdk-path` returns. That call follows whatever
# Xcode is installed. Xcode brought macOS SDK 27.0 on 2026-09-20, whose
# libSystem.tbd names the target arm64e.x1-macos, and the pinned ld64.lld
# read it as malformed and left every symbol of libSystem undefined.

cmake_minimum_required(VERSION 3.21)

file(READ "${ROOT}/tools/pack-anti.cmake" packer_text)
if(packer_text MATCHES "xcrun[^\n]*--show-sdk-path" AND
   NOT packer_text MATCHES "xcrun --sdk")
    message(FATAL_ERROR "tools/pack-anti.cmake asks xcrun for the SDK of the "
                        "machine rather than the pinned version")
endif()
foreach(name MACOS_SDK_VERSION MACOS_SDK_NAME MACOS_SDK_DIGEST)
    file(STRINGS "${ROOT}/tools/macos-sdk-pin" row REGEX "^${name}=.")
    if(row STREQUAL "")
        message(FATAL_ERROR "tools/macos-sdk-pin names no ${name}")
    endif()
endforeach()
