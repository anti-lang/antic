# Pack the package of this host from the antic and the anti of the build,
# and check that the archive carries no key and nothing of tools/keys/.
# The installer holds the key that checks the LLVM tools, and the package
# holds the pin that names them. Both macOS sysroots of Zig's stubs go in,
# and the stubs of Apple's SDK in sdk/ stay out. A package of a build with
# the compiler of the machine is refused. Every licence of the runtime tree
# goes in.
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
# Every licence of the runtime tree goes in, those of the native libraries
# among them.
file(GLOB licences RELATIVE "${RUNTIME}/licenses" "${RUNTIME}/licenses/*.txt")
foreach(licence IN LISTS licences)
    string(REPLACE "." "\\." pattern "${licence}")
    if(NOT entries MATCHES "(^|\n)anti/licenses/${pattern}\n")
        message(FATAL_ERROR "${archive} lacks anti/licenses/${licence}")
    endif()
endforeach()
if(entries MATCHES "(^|\n)anti/sysroot/macos-[^/\n]+/sdk/")
    message(FATAL_ERROR "${archive} carries stubs of Apple's SDK")
endif()

# The anti of the package runs the commands that read JSON, TOML and the
# symbols of a binary, whose readers stand in src/rt. On a Linux host the
# packer compiled that anti itself, which once left the three readers out.
function(run_packed what)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                "${WORK}/unpacked/anti/bin/anti${suffix}" ${ARGN}
        WORKING_DIRECTORY "${WORK}/run" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the packed anti: ${what} failed with ${status}\n"
                            "${out}${err}")
    endif()
    set(packed_out "${out}" PARENT_SCOPE)
endfunction()
file(MAKE_DIRECTORY "${WORK}/unpacked" "${WORK}/run" "${WORK}/deploy")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
                WORKING_DIRECTORY "${WORK}/unpacked" RESULT_VARIABLE unpacked)
if(NOT unpacked EQUAL 0)
    message(FATAL_ERROR "${archive} does not unpack")
endif()
run_packed("anti bind" bind "${ROOT}/tests/bind/raylib_api.json"
           -o "${WORK}/run/bound")
if(NOT EXISTS "${WORK}/run/bound/raylib.anti")
    message(FATAL_ERROR "the packed anti bind wrote no raylib.anti")
endif()
file(COPY "${ROOT}/tests/anti-build/app" DESTINATION "${WORK}/run")
set(project "${WORK}/run/app")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
            "${WORK}/unpacked/anti/bin/anti${suffix}" build --release
            --runtime "${RUNTIME}" --llvm-mc "${LLVM_BIN}/llvm-mc${suffix}"
    WORKING_DIRECTORY "${project}" RESULT_VARIABLE status
    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
set(release "${project}/dist/${HOST}/release")
if(NOT status EQUAL 0 OR NOT EXISTS "${release}/app${suffix}")
    message(FATAL_ERROR "the packed anti did not build the project of "
                        "anti.toml: ${status}\n${out}${err}")
endif()
# A Windows program keeps its functions in the PDB, which only DbgHelp
# reads. The inventory runs on the other hosts, as anti_symbols does.
if(NOT HOST MATCHES "^windows-")
    file(COPY "${release}/app" "${release}/app-symbols.zip"
         DESTINATION "${WORK}/deploy")
    file(WRITE "${WORK}/deploy/app.toml" "")
    file(STRINGS "${WORK}/deploy/app" id_line REGEX "^build [0-9a-f]+$")
    list(GET id_line 0 id_line)
    string(SUBSTRING "${id_line}" 6 -1 program_id)
    run_packed("anti symbols inventory" symbols inventory
               --conf "${WORK}/deploy/app.toml" --out "${WORK}/run/all.zip")
    if(NOT packed_out MATCHES "present [^\n]*app ${program_id}\n")
        message(FATAL_ERROR "the packed anti found no symbols of the "
                            "program\n${packed_out}")
    endif()
endif()

# A host that is not Linux packs linux-arm64 as well. The packer then
# compiles a Linux anti from the source list of the CMake build, and the
# link fails when a source is missing.
if(NOT HOST MATCHES "^linux-")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/linux"
                            "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}"
                            "-DHOSTS=linux-arm64" "-DSYSROOT=${WORK}/sysroot"
                            "-DRUNTIME=${RUNTIME}"
                            -P "${ROOT}/tools/pack-anti.cmake"
                    RESULT_VARIABLE packed)
    file(GLOB archive "${WORK}/linux/anti-*-linux-arm64.tar.xz")
    if(NOT packed EQUAL 0 OR NOT archive)
        message(FATAL_ERROR "tools/pack-anti.cmake packed no linux-arm64")
    endif()
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
