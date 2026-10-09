# Pack the package of this host from the antic and the anti of the build,
# and check that the archive carries no key and nothing of tools/keys/.
# The LLVM tools of the host travel in bin/ beside antic and anti, with
# llvm-version, and tools/ holds no pin of them. The package alone then
# prints its version and builds a program, with nothing else on the PATH.
# With an empty PATH and no LIB it links a program for every target that
# names no framework. Both macOS sysroots of Zig's stubs go in, and the
# stubs of Apple's SDK in sdk/ stay out. A package of a build with the
# compiler of the machine is refused. Every licence and every header of
# the runtime tree goes in, and anti bind --clang of the package binds
# include/raylib/raylib.h as the anti of the tree binds the pinned source.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DANTI=<anti> -DHOST=<host>
#         -DSYSROOT=<dir> -DRUNTIME=<dir> -DCLANG=<clang> -DLLVM_BIN=<dir>
#         -DRAYLIB=<dir> -DWORK=<dir> -P tests/run_package.cmake

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

file(REMOVE_RECURSE "${WORK}")
# The sysroots that a package carries, and an SDK of Apple beside the
# stubs of a macOS sysroot, as APPLE_SDK or anti sdk import leave it.
foreach(name linux-x86_64 linux-arm64 macos-arm64 macos-x86_64
             windows-x86_64 windows-arm64 licenses)
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
# DESIGN: the LLVM tools travel in bin/ of the package beside antic and
# anti, copied from bin/ of the runtime archive with llvm-version, which
# Eddie decided on 2026-10-08 in docs/work-order-distribution.md. The
# installer downloads none, so tools/ carries no pin of them. VERSION in
# the root names the version of the package.
set(tools llvm-mc llvm-ar llvm-objdump llvm-readobj lld ld.lld ld64.lld
          lld-link opt llc llvm-profdata)
set(expected anti/tools/zig-stubs-pin anti/bin/antic${suffix}
             anti/bin/anti${suffix} anti/bin/llvm-version anti/VERSION
             anti/sysroot/macos-arm64/usr/lib/libSystem.tbd
             anti/sysroot/macos-x86_64/usr/lib/libSystem.tbd
             anti/sysroot/macos-arm64/sdk-version
             anti/licenses/zig.txt anti/licenses/apsl.txt
             anti/licenses/sources.txt anti/licenses/mingw-w64.txt)
# DESIGN: every package carries the two Windows sysroots of mingw-w64 as
# the runtime archive holds them: the headers, the import libraries that
# llvm-dlltool wrote from the .def files and the builtins of the pinned
# clang, which Eddie decided on 2026-09-27 under "Binary distribution" in
# docs/decisions.md and the step mingw of docs/work-order-distribution.md
# built.
foreach(cpu x86_64 arm64)
    set(windows "anti/sysroot/windows-${cpu}")
    list(APPEND expected "${windows}/include/_mingw.h"
         "${windows}/include/windows.h" "${windows}/lib/ucrtbase.lib"
         "${windows}/lib/kernel32.lib" "${windows}/lib/ntdll.lib"
         "${windows}/lib/clang_rt.builtins.lib")
endforeach()
foreach(tool IN LISTS tools)
    list(APPEND expected "anti/bin/${tool}${suffix}")
endforeach()
# DESIGN: every package carries the two glibc sysroots of the runtime
# archive as they are, with their X11 and OpenGL development files, and
# the glibc runtime of both Linux targets, which Eddie decided on
# 2026-09-27 under "Binary distribution" in docs/decisions.md.
foreach(cpu x86_64 arm64)
    set(multiarch x86_64-linux-gnu)
    set(level v1)
    if(cpu STREQUAL "arm64")
        set(multiarch aarch64-linux-gnu)
        set(level armv8.0)
    endif()
    set(glibc "anti/sysroot/linux-${cpu}-glibc")
    list(APPEND expected "${glibc}/lib/${multiarch}/libc.so.6"
         "${glibc}/usr/lib/${multiarch}/Scrt1.o"
         "${glibc}/usr/lib/${multiarch}/libX11.so"
         "${glibc}/usr/lib/${multiarch}/libGL.so"
         "${glibc}/usr/include/X11/Xlib.h" "${glibc}/usr/include/GL/gl.h"
         "anti/lib/linux-${cpu}-glibc/${level}/libanti_rt.a"
         "anti/lib/linux-${cpu}-glibc/libunwind.a")
endforeach()
# DESIGN: the headers of the native libraries go into include/<library>/
# of the runtime archive and the package, which Eddie decided on
# 2026-10-08 in docs/work-order-distribution.md. Each directory is the one
# a C compile names with -I, so Mbed TLS keeps mbedtls/ and psa/ below it.
foreach(header pcre2/pcre2.h sqlite3/sqlite3.h sqlite3/sqlite3ext.h
        mbedtls/mbedtls/ssl.h mbedtls/mbedtls/mbedtls_config.h
        mbedtls/psa/crypto.h miniaudio/miniaudio.h raylib/raylib.h
        raylib/raymath.h raylib/rlgl.h raylib/rcamera.h)
    list(APPEND expected "anti/include/${header}")
endforeach()
foreach(entry IN LISTS expected)
    if(NOT entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${archive} lacks ${entry}")
    endif()
endforeach()
foreach(entry anti/tools/llvm-pin anti/tools/llvm-version)
    if(entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${archive} carries ${entry}, and no installer "
                            "reads it since the tools travel in bin/")
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
# Every header of the runtime tree goes in.
file(GLOB_RECURSE headers RELATIVE "${RUNTIME}/include" "${RUNTIME}/include/*")
if(headers STREQUAL "")
    message(FATAL_ERROR "${RUNTIME}/include holds no header")
endif()
foreach(header IN LISTS headers)
    string(REPLACE "." "\\." pattern "${header}")
    if(NOT entries MATCHES "(^|\n)anti/include/${pattern}\n")
        message(FATAL_ERROR "${archive} lacks anti/include/${header}")
    endif()
endforeach()
if(entries MATCHES "(^|\n)anti/sysroot/macos-[^/\n]+/sdk/")
    message(FATAL_ERROR "${archive} carries stubs of Apple's SDK")
endif()
# DESIGN: a release build links the runtime as bitcode through full LTO
# by default, which Eddie decided on 2026-10-07, so every runtime of the
# package has its bitcode of full LTO beside it. The bitcode of ThinLTO
# stays out, and `--lto thin` then names the archive it lacks.
string(REGEX MATCHALL "anti/lib/[^/\n]+/[^/\n]+/(lib)?anti_rt\\.(a|lib)\n"
       runtimes "${entries}")
if(runtimes STREQUAL "")
    message(FATAL_ERROR "${archive} carries no runtime")
endif()
foreach(runtime IN LISTS runtimes)
    string(STRIP "${runtime}" runtime)
    get_filename_component(level "${runtime}" DIRECTORY)
    get_filename_component(library "${runtime}" NAME)
    string(REPLACE "." "\\." pattern "${level}/bitcode/full/${library}")
    if(NOT entries MATCHES "(^|\n)${pattern}\n")
        message(FATAL_ERROR "${archive} lacks ${level}/bitcode/full/${library}")
    endif()
endforeach()
if(entries MATCHES "(^|\n)anti/lib/[^\n]*/bitcode/thin/")
    message(FATAL_ERROR "${archive} carries the bitcode of ThinLTO")
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
# The record of the upstream sources travels beside the licence texts,
# with a line for glibc and one for the kernel headers whose URL names the
# version of tools/sysroot-pins.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${ROOT}"
                        "-DSOURCES=${WORK}/unpacked/anti/licenses/sources.txt"
                        -P "${ROOT}/tests/run_sources.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "licenses/sources.txt of the package: ${out}${err}")
endif()
run_packed("anti bind" bind "${ROOT}/tests/bind/raylib_api.json"
           -o "${WORK}/run/bound")
if(NOT EXISTS "${WORK}/run/bound/raylib.anti")
    message(FATAL_ERROR "the packed anti bind wrote no raylib.anti")
endif()
# DESIGN: the package alone builds a program. The PATH holds bin/ of the
# package and nothing else, so no tool of LLVM or of Anti comes from
# anywhere else, and no --runtime names the archive: antic and anti find
# it above their own bin/, and opt, llc and lld in that bin/, by the one
# rule of runtime_archive in src/antic/userdirs.c.
set(bin "${WORK}/unpacked/anti/bin")
function(run_alone what dir)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${bin}"
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                ${ARGN}
        WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the package alone: ${what} failed with "
                            "${status}\n${out}${err}")
    endif()
    set(alone_out "${out}" PARENT_SCOPE)
endfunction()
file(STRINGS "${ROOT}/tools/version" version LIMIT_COUNT 1)
string(STRIP "${version}" version)
string(REPLACE "." "\\." version_pattern "${version}")
foreach(program antic anti)
    run_alone("${program} --version" "${WORK}/run" "${bin}/${program}${suffix}"
              --version)
    if(NOT alone_out MATCHES "^${program} ${version_pattern}\r?\n$")
        message(FATAL_ERROR "the packed ${program} prints '${alone_out}', not "
                            "${program} ${version}")
    endif()
endforeach()
file(READ "${WORK}/unpacked/anti/VERSION" packed_version)
string(STRIP "${packed_version}" packed_version)
if(NOT packed_version STREQUAL version)
    message(FATAL_ERROR "VERSION of the package holds '${packed_version}', and "
                        "tools/version ${version}")
endif()
file(READ "${WORK}/unpacked/anti/bin/llvm-version" packed_llvm)
file(READ "${ROOT}/tools/llvm-version" llvm_version)
if(NOT packed_llvm STREQUAL llvm_version)
    message(FATAL_ERROR "bin/llvm-version of the package holds "
                        "'${packed_llvm}', and tools/llvm-version "
                        "'${llvm_version}'")
endif()
# DESIGN: the package alone links a program for every target that names
# no framework, with an empty PATH and no LIB, which Eddie decided on
# 2026-09-27 under "Binary distribution" in docs/decisions.md and the
# step own-tools of docs/work-order-distribution.md built. The Windows
# sysroots of mingw-w64 travel in the package since the step mingw.
set(targets linux-x86_64 linux-arm64 macos-arm64 macos-x86_64
    windows-x86_64 windows-arm64)
# anti bind --clang of the package, with bin/ of the package and the
# pinned clang alone on the PATH, binds include/raylib/raylib.h of the
# package. It writes anti.raylib as the anti of the tree writes it from
# raylib.h of the pinned raylib source, which the test anti_bind_raylib
# compiles and probes. The PATH is set as run_anti_bind.cmake sets it, for
# the two runs, and put back after them. Each directory is made native
# alone, since file(TO_NATIVE_PATH) writes `;` for the `:` of a joined
# PATH.
get_filename_component(clang_bin "${CLANG}" DIRECTORY)
file(TO_NATIVE_PATH "${WORK}/unpacked/anti/bin" bind_path)
file(TO_NATIVE_PATH "${clang_bin}" clang_bin)
if(HOST MATCHES "^windows-")
    string(APPEND bind_path ";${clang_bin}")
else()
    string(APPEND bind_path ":${clang_bin}")
endif()
set(saved_path "$ENV{PATH}")
set(ENV{PATH} "${bind_path}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
            "${WORK}/unpacked/anti/bin/anti${suffix}" bind --clang
            "${WORK}/unpacked/anti/include/raylib/raylib.h"
            -o "${WORK}/run/bound-clang"
    WORKING_DIRECTORY "${WORK}/run" RESULT_VARIABLE packed_status
    OUTPUT_VARIABLE packed_out ERROR_VARIABLE packed_err ENCODING NONE)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
            "${ANTI}" bind --clang "${RAYLIB}/src/raylib.h"
            -o "${WORK}/run/bound-tree" --runtime "${RUNTIME}"
    WORKING_DIRECTORY "${WORK}/run" RESULT_VARIABLE tree_status
    OUTPUT_VARIABLE tree_out ERROR_VARIABLE tree_err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(NOT packed_status EQUAL 0)
    message(FATAL_ERROR "the packed anti bind --clang of include/raylib/raylib.h "
                        "failed with ${packed_status}\n${packed_out}${packed_err}")
endif()
if(NOT tree_status EQUAL 0)
    message(FATAL_ERROR "anti bind --clang of the pinned raylib.h failed with "
                        "${tree_status}\n${tree_out}${tree_err}")
endif()
file(GLOB written RELATIVE "${WORK}/run/bound-clang" "${WORK}/run/bound-clang/*")
file(GLOB wanted RELATIVE "${WORK}/run/bound-tree" "${WORK}/run/bound-tree/*")
if(NOT "raylib.anti" IN_LIST written OR NOT written STREQUAL wanted)
    message(FATAL_ERROR "the packed anti bind --clang wrote '${written}', and "
                        "the anti of the tree '${wanted}'")
endif()
foreach(file IN LISTS written)
    file(READ "${WORK}/run/bound-clang/${file}" got)
    file(READ "${WORK}/run/bound-tree/${file}" want)
    if(NOT got STREQUAL want)
        message(FATAL_ERROR "${file} that the packed anti bind --clang wrote "
                            "from include/raylib/raylib.h differs from anti.raylib "
                            "of the tree")
    endif()
endforeach()

file(WRITE "${WORK}/run/hello.anti"
     "import anti.io;\n\nfn main() -> int\n{\n    io.print(\"hello\");\n"
     "    return 0;\n}\n")
run_alone("antic hello.anti" "${WORK}/run" "${bin}/antic${suffix}" hello.anti
          -o "${WORK}/run/hello${suffix}")
run_alone("hello" "${WORK}/run" "${WORK}/run/hello${suffix}")
if(NOT alone_out MATCHES "hello")
    message(FATAL_ERROR "the hello program of the package prints "
                        "'${alone_out}'")
endif()

foreach(target IN LISTS targets)
    set(exe "${WORK}/run/return42-${target}")
    if(target MATCHES "^windows-")
        string(APPEND exe ".exe")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=" --unset=LIB
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                "${bin}/antic${suffix}" --target "${target}" -o "${exe}"
                "${ROOT}/tests/programs/return42.anti"
        WORKING_DIRECTORY "${WORK}/run" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT EXISTS "${exe}")
        message(FATAL_ERROR "the package alone, with an empty PATH and no LIB, "
                            "links no program for ${target}: antic ended with "
                            "${status}\n${out}${err}")
    endif()
endforeach()
# The notice of a program of musl, printed by the packed anti from the
# package alone, follows the text of musl and of mimalloc with the line of
# each in licenses/sources.txt of the package.
run_alone("anti license --from of a program of musl" "${WORK}/run"
          "${bin}/anti${suffix}" license --from "${WORK}/run/return42-linux-arm64")
foreach(name musl mimalloc)
    file(STRINGS "${WORK}/unpacked/anti/licenses/sources.txt" line
         REGEX "^${name} ")
    string(FIND "${alone_out}" "\nsource ${line}\n" at)
    if(line STREQUAL "" OR at EQUAL -1)
        message(FATAL_ERROR "the packed anti license --from prints no `source "
                            "${line}` for ${name}\n${alone_out}")
    endif()
endforeach()

# DESIGN: the package alone links a Linux program against glibc, from any
# host, which the step glibc of docs/work-order-distribution.md built. A
# program of `link linux`, tests/anti-build/window, builds with anti build.
# A raylib program imports the binding that the packed anti bind writes,
# compiled into a library file, and links libraylib.a of the package with
# the Linux libraries the binding names. Both name libX11 and libGL of the
# glibc sysroot. A Linux host runs the two programs of its own target. No
# display serves them, so the raylib program asks for a mouse button, which
# reads the state of rcore and opens no window.
file(COPY "${ROOT}/tests/anti-build/window" DESTINATION "${WORK}/run")
set(game_dir "${WORK}/run/game")
file(MAKE_DIRECTORY "${game_dir}/lib/game")
run_alone("anti bind --module game.raylib" "${WORK}/run" "${bin}/anti${suffix}"
          bind "${ROOT}/tests/bind/raylib_api.json" --module game.raylib
          -o "${game_dir}/src/game")
run_alone("antic -c of the raylib binding" "${game_dir}" "${bin}/antic${suffix}"
          -c -I "${game_dir}/src" -o "${game_dir}/lib/game/raylib.antl"
          "${game_dir}/src/game/raylib.anti")
file(WRITE "${game_dir}/game.anti"
     "import anti.io;\nimport game.raylib;\n\nfn main() -> int\n{\n"
     "    if !raylib.IsMouseButtonPressed(0) {\n"
     "        io.println(\"no click\");\n    }\n    return 0;\n}\n")
file(STRINGS "${game_dir}/src/game/raylib.anti" raylib_linux
     REGEX "^link linux \"[^\"]+\";$")
set(linux_libs)
foreach(line IN LISTS raylib_linux)
    string(REGEX REPLACE "^link linux \"([^\"]+)\";$" "\\1" name "${line}")
    list(APPEND linux_libs --linux-lib "${name}")
endforeach()
if(NOT linux_libs MATCHES "X11" OR NOT linux_libs MATCHES "GL")
    message(FATAL_ERROR "the binding of raylib names no X11 and GL: "
                        "${raylib_linux}")
endif()
foreach(target linux-x86_64 linux-arm64)
    run_alone("anti build --target ${target} of a program of link linux"
              "${WORK}/run/window" "${bin}/anti${suffix}" build
              --target "${target}")
    set(window "${WORK}/run/window/dist/${target}/dev/window")
    set(game "${WORK}/run/game-${target}")
    run_alone("a raylib program for ${target}" "${game_dir}"
              "${bin}/antic${suffix}" --target "${target}"
              -I "${game_dir}/lib" ${linux_libs} -o "${game}"
              "${game_dir}/game.anti"
              "${WORK}/unpacked/anti/lib/${target}/libraylib.a")
    foreach(exe "${window}" "${game}")
        run_alone("llvm-readobj of ${exe}" "${WORK}/run"
                  "${bin}/llvm-readobj${suffix}" --needed-libs
                  --program-headers "${exe}")
        foreach(needed PT_INTERP libX11\\.so\\.6 libGL\\.so\\.1 libc\\.so\\.6)
            if(NOT alone_out MATCHES "${needed}")
                message(FATAL_ERROR "${exe} of the package lacks ${needed}\n"
                                    "${alone_out}")
            endif()
        endforeach()
    endforeach()
    if(target STREQUAL HOST)
        run_alone("the program of link linux" "${WORK}/run" "${window}")
        if(NOT alone_out STREQUAL "no display\nno context\n")
            message(FATAL_ERROR "the program of link linux of the package "
                                "prints '${alone_out}'")
        endif()
        run_alone("the raylib program" "${WORK}/run" "${game}")
        if(NOT alone_out MATCHES "no click\n$")
            message(FATAL_ERROR "the raylib program of the package prints "
                                "'${alone_out}'")
        endif()
    endif()
endforeach()
file(COPY "${ROOT}/tests/anti-build/app" DESTINATION "${WORK}/run")
set(project "${WORK}/run/app")
run_alone("anti build --release" "${project}" "${bin}/anti${suffix}" build
          --release)
set(release "${project}/dist/${HOST}/release")
if(NOT EXISTS "${release}/app${suffix}")
    message(FATAL_ERROR "the packed anti built no program of anti.toml")
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
# DESIGN: the runtime archive holds the tools of this machine alone. The
# tools of another host come from TOOLS/<host>/bin, which
# tools/get-llvm.cmake -DHOST=<host> lays out as bin/ of the runtime
# archive. A stand-in of that layout keeps the test off the network, and
# a host without its directory is refused before anything is compiled.
if(NOT HOST MATCHES "^linux-")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/linux"
                            "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}"
                            "-DHOSTS=linux-arm64" "-DSYSROOT=${WORK}/sysroot"
                            "-DRUNTIME=${RUNTIME}"
                            -P "${ROOT}/tools/pack-anti.cmake"
                    RESULT_VARIABLE refused ERROR_VARIABLE err ENCODING NONE)
    if(refused EQUAL 0 OR NOT err MATCHES "get-llvm.cmake")
        message(FATAL_ERROR "tools/pack-anti.cmake packed linux-arm64 without "
                            "the tools of linux-arm64: ${err}")
    endif()
    set(stand_in "${WORK}/tools/linux-arm64/bin")
    file(MAKE_DIRECTORY "${stand_in}")
    foreach(tool IN LISTS tools)
        file(WRITE "${stand_in}/${tool}" "stand-in ${tool} of linux-arm64\n")
    endforeach()
    file(WRITE "${stand_in}/llvm-version" "${llvm_version}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/linux"
                            "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}"
                            "-DHOSTS=linux-arm64" "-DSYSROOT=${WORK}/sysroot"
                            "-DRUNTIME=${RUNTIME}" "-DTOOLS=${WORK}/tools"
                            -P "${ROOT}/tools/pack-anti.cmake"
                    RESULT_VARIABLE packed)
    file(GLOB archive "${WORK}/linux/anti-*-linux-arm64.tar.xz")
    if(NOT packed EQUAL 0 OR NOT archive)
        message(FATAL_ERROR "tools/pack-anti.cmake packed no linux-arm64")
    endif()
    file(MAKE_DIRECTORY "${WORK}/linux/unpacked")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
                            anti/bin/ld.lld anti/bin/llvm-version
                    WORKING_DIRECTORY "${WORK}/linux/unpacked"
                    RESULT_VARIABLE unpacked)
    if(NOT unpacked EQUAL 0)
        message(FATAL_ERROR "${archive} holds no bin/ld.lld of linux-arm64")
    endif()
    file(READ "${WORK}/linux/unpacked/anti/bin/ld.lld" packed_lld)
    if(NOT packed_lld STREQUAL "stand-in ld.lld of linux-arm64\n")
        message(FATAL_ERROR "${archive} carries another ld.lld than the one of "
                            "linux-arm64")
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
