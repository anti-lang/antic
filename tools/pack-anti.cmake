# Build the package that a user installs, one per host.
#
#   cmake -DDEST=<dir> -DCLANG=<clang> -DLLVM_BIN=<dir> -DSYSROOT=<dir>
#         -DRUNTIME=<dir> -DHOSTS=<host>[;<host>] [-DSYMBOLS=<dir>]
#         -P tools/pack-anti.cmake
#
# CLANG is the pinned clang of build/clang, LLVM_BIN its tools, SYSROOT
# the directory of tools/get-sysroot.cmake and RUNTIME the runtime that the
# CMake build wrote. SYMBOLS is where the PDB of a Windows program goes,
# which is outside the package and is needed when a Windows host is
# compiled here. For each host it compiles antic and anti for that host
# and lays the package out around them. ANTIC and ANTI name the programs
# already built for the one host of HOSTS, which the package takes
# instead, and then CLANG and LLVM_BIN are not needed:
#
#   bin/        antic and anti
#   lib/<t>/<l>/ the runtime library of all six targets, per processor level
#   std/        the standard library
#   sysroot/    the two Linux sysroots and the two macOS sysroots of Zig's
#               stubs, which are ours to redistribute
#   tools/      the scripts that install the sysroot of the host
#   licenses/   one file per component
#
# The result is anti-<version>-<host>.tar.xz in DEST, with its digest in
# SHA256SUMS. The stubs of Apple's SDK in sdk/ of a macOS sysroot and the
# Microsoft CRT stay out, because neither licence allows redistribution. A
# user brings the first from a Mac with anti sdk import, and the installer
# adds the second. It adds the five LLVM tools from the release that
# tools/llvm-pin names. The package carries no key: the installer holds
# the key that checks them.
cmake_minimum_required(VERSION 3.20)

set(needed DEST SYSROOT RUNTIME HOSTS)
if(NOT DEFINED ANTIC)
    list(APPEND needed CLANG LLVM_BIN)
endif()
foreach(name IN LISTS needed)
    if(NOT DEFINED ${name})
        message(FATAL_ERROR "usage: cmake -DDEST=<dir> -DCLANG=<clang> "
                            "-DLLVM_BIN=<dir> -DSYSROOT=<dir> -DRUNTIME=<dir> "
                            "-DHOSTS=<host>[;<host>] -P tools/pack-anti.cmake")
    endif()
endforeach()

set(TARGETS macos-arm64 macos-x86_64 linux-x86_64 linux-arm64
            windows-x86_64 windows-arm64)

# DESIGN: every binary Anti ships is compiled with the pinned clang. The
# cache of the build beside RUNTIME names its compiler, and CLANG must
# report the pinned version, so a release never takes the compiler of the
# machine.
get_filename_component(build_dir "${RUNTIME}/.." ABSOLUTE)
file(STRINGS "${build_dir}/CMakeCache.txt" system
     REGEX "^ANTIC_SYSTEM_COMPILER:BOOL=(ON|TRUE|YES|1)$")
if(system)
    message(FATAL_ERROR "${build_dir} was configured with "
                        "ANTIC_SYSTEM_COMPILER=ON. A release takes the pinned clang.")
endif()
if(DEFINED CLANG)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" llvm_version)
    string(STRIP "${llvm_version}" llvm_version)
    execute_process(COMMAND "${CLANG}" --version OUTPUT_VARIABLE out)
    if(NOT out MATCHES "^clang version ${llvm_version}[ \n]")
        message(FATAL_ERROR "${CLANG} is not the pinned clang ${llvm_version}: ${out}")
    endif()
endif()

set(tools_dir "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(root "${tools_dir}/.." ABSOLUTE)
file(STRINGS "${tools_dir}/version" version LIMIT_COUNT 1)
string(STRIP "${version}" version)
if(DEFINED ANTIC)
    list(LENGTH HOSTS count)
    if(NOT count EQUAL 1 OR NOT DEFINED ANTI)
        message(FATAL_ERROR "ANTIC and ANTI are the programs of one host, and "
                            "HOSTS names ${HOSTS}")
    endif()
else()
    execute_process(COMMAND "${CLANG}" -print-resource-dir
                    OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# The pinned Apple SDK, whose reason tools/macos-sdk.cmake holds. A test
# of its own drives that file, so the rule is read where it is written.
include("${tools_dir}/macos-sdk.cmake")

function(triple_of host out)
    set(triples
        "macos-arm64=arm64-apple-macos11"
        "macos-x86_64=x86_64-apple-macos11"
        "linux-x86_64=x86_64-unknown-linux-musl"
        "linux-arm64=aarch64-unknown-linux-musl"
        "windows-x86_64=x86_64-pc-windows-msvc"
        "windows-arm64=aarch64-pc-windows-msvc")
    foreach(row IN LISTS triples)
        if(row MATCHES "^${host}=(.*)$")
            set("${out}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "unknown host ${host}")
endfunction()

# DESIGN: a Linux program of a release links the pinned sysroot and never
# the libc of the machine that packed it, so it starts on any Linux from
# Ubuntu 22.04 on. Only the binary says which libc it reached, so it is
# read here, before the package is written. A program ANTIC or ANTI named
# was built by something else, which is the case this guards hardest: a
# build of the machine carries the versions of the machine.
function(check_libc host binary program)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DBINARY=${binary}"
                "-DREADOBJ=${LLVM_BIN}/llvm-readobj"
                -P "${root}/tools/check-libc.cmake"
        RESULT_VARIABLE refused)
    if(refused)
        message(FATAL_ERROR "${host}: ${program} links the wrong libc")
    endif()
endfunction()

# Compile and link antic or anti for one host. Each family of targets reads
# its headers from a different place: the macOS stubs of SYSROOT, the musl
# sysroot, or the Microsoft headers that xwin wrote. anti takes the sources
# of antic but its main.c, and those of tools/anti.
function(build_program host output program)
    triple_of("${host}" triple)
    file(GLOB sources "${root}/src/*.c")
    if(program STREQUAL "anti")
        list(REMOVE_ITEM sources "${root}/src/main.c")
        file(GLOB anti_sources "${root}/tools/anti/*.c")
        list(APPEND sources ${anti_sources})
    endif()
    set(common --target=${triple} -std=c11 -O2 -Wall -Wextra -Wpedantic
               -Werror "-ffile-prefix-map=${root}=."
               "-DANTIC_VERSION=\"${version}\"" -I "${root}/src"
               -I "${root}/tools/anti")
    set(link "")
    if(host MATCHES "^macos-")
        list(APPEND common -isysroot "${macos_sdk}")
        set(link --ld-path=${LLVM_BIN}/ld64.lld)
    elseif(host MATCHES "^linux-")
        list(APPEND common --sysroot "${SYSROOT}/${host}")
    else()
        set(win "${SYSROOT}/${host}")
        # antic is a Windows program too, and it takes the C runtime of
        # the machine as the programs it compiles do.
        list(APPEND common -D_CRT_SECURE_NO_WARNINGS -fms-runtime-lib=dll
             -isystem "${resource}/include" -isystem "${win}/crt/include"
             -isystem "${win}/sdk/include/ucrt"
             -isystem "${win}/sdk/include/um"
             -isystem "${win}/sdk/include/shared")
        set(arch x86_64)
        if(host STREQUAL "windows-arm64")
            set(arch aarch64)
        endif()
        set(link -fuse-ld=lld -B "${LLVM_BIN}"
                 -L "${win}/crt/lib/${arch}" -L "${win}/sdk/lib/ucrt/${arch}"
                 -L "${win}/sdk/lib/um/${arch}")
        # DESIGN: a Windows program carries no symbol table, so what names
        # a frame of a report from a user is the PDB that lld-link writes
        # with /DEBUG. It stands in SYMBOLS, outside the package, and step
        # 4 of the release folds it into the symbols archive of the host.
        # /PDBALTPATH:%_PDB% keeps the CodeView record of the executable
        # to the file name of the PDB, so the path of this machine does
        # not ship. /ignore:4099 drops the warning that the objects of the
        # Microsoft C runtime name PDBs no machine here holds.
        if(NOT DEFINED SYMBOLS)
            message(FATAL_ERROR "${host}: a Windows program is linked with "
                                "/DEBUG, and SYMBOLS names the directory its "
                                "PDB goes in")
        endif()
        file(MAKE_DIRECTORY "${SYMBOLS}/${host}")
        list(APPEND link -Wl,/DEBUG "-Wl,/PDBALTPATH:%_PDB%" -Wl,/ignore:4099
             "-Wl,/PDB:${SYMBOLS}/${host}/${program}.pdb")
    endif()
    if(host MATCHES "^linux-")
        # The clang driver looks for the start files of gcc on Linux, so
        # the objects go to ld.lld with the musl ones instead.
        set(objects "")
        file(MAKE_DIRECTORY "${DEST}/work/${host}/${program}")
        foreach(source IN LISTS sources)
            get_filename_component(name "${source}" NAME_WE)
            set(object "${DEST}/work/${host}/${program}/${name}.o")
            execute_process(COMMAND "${CLANG}" ${common} -c -o "${object}"
                                    "${source}" RESULT_VARIABLE failed)
            if(failed)
                message(FATAL_ERROR "${host}: ${name}.c did not compile")
            endif()
            list(APPEND objects "${object}")
        endforeach()
        set(lib "${SYSROOT}/${host}/usr/lib")
        execute_process(
            COMMAND "${LLVM_BIN}/ld.lld" -static -pie --no-dynamic-linker
                    -o "${output}" "${lib}/rcrt1.o" "${lib}/crti.o" ${objects}
                    "${lib}/libc.a" "${lib}/libclang_rt.builtins.a"
                    "${lib}/crtn.o"
            RESULT_VARIABLE failed)
    else()
        execute_process(COMMAND "${CLANG}" ${common} ${link} -o "${output}"
                                ${sources} RESULT_VARIABLE failed)
    endif()
    if(failed)
        message(FATAL_ERROR "${host}: ${program} did not link")
    endif()
    # DESIGN: a Linux program of a release links the pinned sysroot and
    # never the libc of the machine that packed it, so it starts on any
    # Linux from Ubuntu 22.04 on. Only the binary says which libc it
    # reached, so it is read here, before the package is written.
    if(host MATCHES "^linux-")
        check_libc("${host}" "${output}" "${program}")
    endif()
endfunction()


# The pinned Apple SDK, read once and only when a macOS program is
# compiled here. A run that packs programs ANTIC and ANTI already named
# links nothing and needs no SDK.
set(macos_sdk "")
if(NOT DEFINED ANTIC)
    foreach(host IN LISTS HOSTS)
        if(host MATCHES "^macos-" AND macos_sdk STREQUAL "")
            pinned_macos_sdk("${tools_dir}/macos-sdk-pin" "" macos_sdk)
            message(STATUS "macOS SDK of ${tools_dir}/macos-sdk-pin in ${macos_sdk}")
        endif()
    endforeach()
endif()

set(sums "")
foreach(host IN LISTS HOSTS)
    set(work "${DEST}/work/${host}")
    set(tree "${work}/anti")
    file(REMOVE_RECURSE "${work}")
    file(MAKE_DIRECTORY "${tree}/bin" "${tree}/tools" "${tree}/licenses")

    set(suffix "")
    if(host MATCHES "^windows-")
        set(suffix ".exe")
    endif()
    if(DEFINED ANTIC)
        file(COPY_FILE "${ANTIC}" "${tree}/bin/antic${suffix}")
        file(COPY_FILE "${ANTI}" "${tree}/bin/anti${suffix}")
        if(host MATCHES "^linux-")
            check_libc("${host}" "${tree}/bin/antic${suffix}" antic)
            check_libc("${host}" "${tree}/bin/anti${suffix}" anti)
        endif()
    else()
        build_program("${host}" "${tree}/bin/antic${suffix}" antic)
        build_program("${host}" "${tree}/bin/anti${suffix}" anti)
    endif()

    foreach(target IN LISTS TARGETS)
        file(COPY "${RUNTIME}/lib/${target}" DESTINATION "${tree}/lib")
    endforeach()
    file(COPY "${RUNTIME}/std" DESTINATION "${tree}")
    file(COPY "${RUNTIME}/licenses/" DESTINATION "${tree}/licenses")
    foreach(target linux-x86_64 linux-arm64)
        file(COPY "${SYSROOT}/${target}" DESTINATION "${tree}/sysroot")
    endforeach()
    # The stubs and headers of Zig go in, and never the SDK of Apple in sdk/.
    foreach(target macos-arm64 macos-x86_64)
        file(COPY "${SYSROOT}/${target}/usr" "${SYSROOT}/${target}/sdk-version"
             DESTINATION "${tree}/sysroot/${target}")
    endforeach()
    file(COPY "${SYSROOT}/licenses/" DESTINATION "${tree}/licenses")
    # The installer reads llvm-pin, and checks the LLVM release with the key
    # it holds itself.
    foreach(name get-sysroot.cmake sysroot-pins zig-stubs-pin cmake-pin
            cmake-version llvm-version llvm-pin package-api)
        file(COPY "${root}/tools/${name}" DESTINATION "${tree}/tools")
    endforeach()
    file(COPY "${root}/LICENSE" DESTINATION "${tree}")

    set(packed "${DEST}/anti-${version}-${host}.tar.xz")
    file(REMOVE "${packed}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E chdir "${work}"
                            "${CMAKE_COMMAND}" -E tar cJf "${packed}" anti
                    RESULT_VARIABLE failed)
    if(failed)
        message(FATAL_ERROR "${packed}: cmake -E tar failed")
    endif()
    file(SHA256 "${packed}" digest)
    file(SIZE "${packed}" size)
    get_filename_component(name "${packed}" NAME)
    string(APPEND sums "${digest}  ${name}\n")
    message(STATUS "${packed} ${size} bytes")
    file(REMOVE_RECURSE "${work}")
endforeach()
# DESIGN: DEST is the directory of the release, which carries the files
# the manifest names and nothing else. The objects of a Linux program are
# written under it and go with the last package.
file(REMOVE_RECURSE "${DEST}/work")
# DESIGN: the manifest covers the directory on the server. A run of one
# host keeps the lines of the packages that DEST already holds, so that
# publishing one host does not drop the other five from SHA256SUMS.
if(EXISTS "${DEST}/SHA256SUMS")
    file(STRINGS "${DEST}/SHA256SUMS" old_lines)
    set(kept "")
    foreach(line IN LISTS old_lines)
        string(REGEX REPLACE "^[0-9a-f]+  " "" name "${line}")
        if(NOT sums MATCHES "  ${name}\n")
            string(APPEND kept "${line}\n")
        endif()
    endforeach()
    set(sums "${kept}${sums}")
endif()
file(WRITE "${DEST}/SHA256SUMS" "${sums}")
