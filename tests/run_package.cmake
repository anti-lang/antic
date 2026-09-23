# Pack the package of this host from the antic and the anti of the build,
# and check that the archive carries no key and nothing of tools/keys/.
# The installer holds the key that checks the LLVM tools, and the package
# holds the pin that names them. Both macOS sysroots of Zig's stubs go in,
# and the stubs of Apple's SDK in sdk/ stay out. A package of a build with
# the compiler of the machine is refused.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DANTI=<anti> -DHOST=<host>
#         -DSYSROOT=<dir> -DRUNTIME=<dir> -DCLANG=<clang> -DLLVM_BIN=<dir>
#         -DWORK=<dir> -P tests/run_package.cmake

# DESIGN: the packer links macOS against the Apple SDK that
# tools/macos-sdk-pin names, resolved by version, and never against the
# one a bare `xcrun --show-sdk-path` returns. That call follows whatever
# Xcode is installed. Xcode brought macOS SDK 27.0 on 2026-09-20, whose
# libSystem.tbd names the target arm64e.x1-macos, and the pinned ld64.lld
# read it as malformed and left every symbol of libSystem undefined.
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

file(REMOVE_RECURSE "${WORK}")
# The sysroots that a package carries, and an SDK of Apple beside the
# stubs of a macOS sysroot, as APPLE_SDK or anti sdk import leave it.
foreach(name linux-x86_64 linux-arm64 macos-arm64 macos-x86_64 licenses)
    file(COPY "${SYSROOT}/${name}" DESTINATION "${WORK}/sysroot")
endforeach()
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/sdk-version" "26.5\n")
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/usr/lib/libSystem.tbd" "Apple's\n")
# DESIGN: the antic of this build links the libc of the machine, which is
# right for a host build and wrong for a package. On Linux that binary
# names the glibc of the builder, and tools/pack-anti.cmake refuses it, so
# the packer compiles the two programs against the pinned sysroot instead.
# That takes a few seconds and is the recipe a release runs. Every other
# host takes the faster path, where the machine's libc is the one the
# package ships.
set(programs "-DANTIC=${ANTIC}" "-DANTI=${ANTI}")
if(HOST MATCHES "^linux-")
    set(programs "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/out" ${programs}
                        "-DHOSTS=${HOST}"
                        "-DSYSROOT=${WORK}/sysroot" "-DRUNTIME=${RUNTIME}"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE packed)
if(NOT packed EQUAL 0)
    message(FATAL_ERROR "tools/pack-anti.cmake failed for ${HOST}")
endif()
file(GLOB archive "${WORK}/out/anti-*-${HOST}.tar.xz")
if(NOT archive)
    message(FATAL_ERROR "tools/pack-anti.cmake wrote no archive for ${HOST}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                OUTPUT_VARIABLE entries RESULT_VARIABLE listed ENCODING NONE)
if(NOT listed EQUAL 0)
    message(FATAL_ERROR "${archive} does not list")
endif()
string(REGEX MATCHALL "[^\n]+\\.(pem|gpg|asc)\n" keys "${entries}")
if(keys)
    message(FATAL_ERROR "${archive} carries ${keys}")
endif()
# Nothing of tools/keys/ goes in, whatever its name or form.
if(entries MATCHES "(^|\n)anti/tools/keys/")
    message(FATAL_ERROR "${archive} carries tools/keys/")
endif()
file(GLOB_RECURSE key_files LIST_DIRECTORIES false "${ROOT}/tools/keys/*")
foreach(key_file IN LISTS key_files)
    get_filename_component(key_name "${key_file}" NAME)
    string(REPLACE "." "\\." key_pattern "${key_name}")
    if(entries MATCHES "(^|[\n/])${key_pattern}\n")
        message(FATAL_ERROR "${archive} carries ${key_name} of tools/keys/")
    endif()
endforeach()
set(suffix "")
if(HOST MATCHES "^windows-")
    set(suffix ".exe")
endif()
foreach(entry anti/tools/llvm-pin anti/tools/llvm-version
              anti/tools/zig-stubs-pin anti/bin/antic${suffix}
              anti/bin/anti${suffix}
              anti/sysroot/macos-arm64/usr/lib/libSystem.tbd
              anti/sysroot/macos-x86_64/usr/lib/libSystem.tbd
              anti/sysroot/macos-arm64/sdk-version
              anti/licenses/zig.txt anti/licenses/apsl.txt)
    if(NOT entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${archive} lacks ${entry}")
    endif()
endforeach()
if(entries MATCHES "(^|\n)anti/sysroot/macos-[^/\n]+/sdk/")
    message(FATAL_ERROR "${archive} carries stubs of Apple's SDK")
endif()

# A release takes the pinned compiler. The cache beside the runtime names
# the compiler of the build, and a build with the one of the machine stops.
set(system "${WORK}/system-build")
file(MAKE_DIRECTORY "${system}/runtime")
file(WRITE "${system}/CMakeCache.txt" "ANTIC_SYSTEM_COMPILER:BOOL=ON\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/refused"
                        "-DANTIC=${ANTIC}" "-DHOSTS=${HOST}"
                        "-DSYSROOT=${SYSROOT}" "-DRUNTIME=${system}/runtime"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE refused ERROR_VARIABLE err ENCODING NONE)
if(refused EQUAL 0 OR NOT err MATCHES "ANTIC_SYSTEM_COMPILER")
    message(FATAL_ERROR "tools/pack-anti.cmake packed a build with the compiler "
                        "of the machine: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
