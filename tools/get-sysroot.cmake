# Install the sysroot of each target that lld links against into
# <dir>/<target>/, and the licence of each component into <dir>/licenses/.
#
#   cmake -DDEST=<dir> -DLLVM_BIN=<dir> -DTARGETS=<target>[;<target>]
#         [-DCLANG_DIR=<dir>] [-DAPPLE_SDK=<dir>] -P tools/get-sysroot.cmake
#
# linux-x86_64, linux-arm64: musl from the Alpine package of
#   tools/sysroot-pins, checked against its digest, and the compiler-rt
#   builtins of the pinned clang in CLANG_DIR, which defaults to
#   build/deps/clang of the repository.
# linux-x86_64-glibc, linux-arm64-glibc: glibc 2.35, the kernel headers and
#   the X11 and OpenGL development files of Ubuntu 22.04 from the packages
#   of GLIBC_PACKAGES in tools/sysroot-pins, and the compiler-rt builtins of
#   the pinned clang, for the Linux link mode against glibc and for the
#   native libraries raylib and miniaudio.
# macos-arm64, macos-x86_64: the stubs of libSystem that Zig generates and
#   the headers of the macOS C library, from the release of Zig that
#   tools/zig-stubs-pin names, on every host. They link every program that
#   names no framework. APPLE_SDK=<MacOSX.sdk> also copies the .tbd stubs
#   of that SDK into sdk/ of the sysroot, for a program that names one.
# windows-x86_64, windows-arm64: the headers of mingw-w64 and the import
#   libraries that llvm-dlltool writes from its .def files, from the
#   source release of tools/sysroot-pins, for ucrtbase.dll, kernel32.dll,
#   ntdll.dll and the DLLs the runtime, raylib, Mbed TLS and the tests
#   name, and the compiler-rt builtins of the pinned clang, which also
#   preprocesses the .def files for the processor of each target.
#
# Every absolute symbolic link of a sysroot becomes the relative link to the
# same path inside it.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST OR NOT DEFINED LLVM_BIN OR NOT DEFINED TARGETS)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -DLLVM_BIN=<dir> "
                        "-DTARGETS=<target>[;<target>] "
                        "-P tools/get-sysroot.cmake")
endif()
# A relative DEST or LLVM_BIN names a directory under the one this script
# runs in.
get_filename_component(DEST "${DEST}" ABSOLUTE)
get_filename_component(LLVM_BIN "${LLVM_BIN}" ABSOLUTE)
set(HOST_EXE "")
if(CMAKE_HOST_WIN32)
    set(HOST_EXE ".exe")
endif()

set(tools_dir "${CMAKE_CURRENT_LIST_DIR}")
if(NOT DEFINED CLANG_DIR)
    include("${tools_dir}/deps-dir.cmake")
    set(CLANG_DIR "${ANTIC_DEPS_DIR}/clang")
endif()
file(STRINGS "${tools_dir}/sysroot-pins" pins REGEX "^[A-Z]")
file(STRINGS "${tools_dir}/zig-stubs-pin" zig_pins REGEX "^[A-Z]")
list(APPEND pins ${zig_pins})
foreach(line IN LISTS pins)
    string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
    list(GET pair 0 key)
    list(GET pair 1 value)
    set("${key}" "${value}")
endforeach()
file(MAKE_DIRECTORY "${DEST}/licenses")

# Download url to file and stop unless its digest is the expected one.
function(fetch url file digest)
    if(NOT EXISTS "${file}")
        file(DOWNLOAD "${url}" "${file}" STATUS status SHOW_PROGRESS)
        list(GET status 0 code)
        list(GET status 1 text)
        if(NOT code EQUAL 0)
            file(REMOVE "${file}")
            message(FATAL_ERROR "${url}: ${text}")
        endif()
    endif()
    file(SHA256 "${file}" actual)
    if(NOT actual STREQUAL digest)
        message(FATAL_ERROR "${file}: SHA-256 ${actual}, expected ${digest}")
    endif()
endfunction()

# The digest of the regular files under dir: their SHA-256 lines in the
# order of their paths, hashed once more. Links stay out.
function(tree_digest dir out)
    file(GLOB_RECURSE found LIST_DIRECTORIES false RELATIVE "${dir}"
         "${dir}/*")
    list(SORT found)
    set(lines "")
    foreach(name IN LISTS found)
        if(NOT IS_SYMLINK "${dir}/${name}")
            file(SHA256 "${dir}/${name}" one)
            string(APPEND lines "${one}  ./${name}\n")
        endif()
    endforeach()
    string(SHA256 digest "${lines}")
    set("${out}" "${digest}" PARENT_SCOPE)
endfunction()

# DESIGN: a package names some links by an absolute path, as libc6-dev
# links libm.so to /lib/x86_64-linux-gnu/libm.so.6. On the host that path
# lies outside the sysroot, so lld finds nothing behind the link. Every
# absolute link under <root> becomes the relative link to the same path
# inside <root>. A host that cannot write a link, as Windows may not, gets
# a copy of the file instead.
# Only a path of the package's own system, which starts with `/`, is such
# a link. Windows reads it back as `\usr\...`, so the destination takes
# the separator of CMake first. Windows follows no link written with `/`,
# so a Windows host writes the relative link with its own separator.
function(relative_links root)
    file(GLOB_RECURSE entries LIST_DIRECTORIES true "${root}/*")
    foreach(entry IN LISTS entries)
        if(NOT IS_SYMLINK "${entry}")
            continue()
        endif()
        file(READ_SYMLINK "${entry}" destination)
        file(TO_CMAKE_PATH "${destination}" destination)
        if(NOT destination MATCHES "^/" OR destination MATCHES "^//")
            continue()
        endif()
        get_filename_component(directory "${entry}" DIRECTORY)
        file(RELATIVE_PATH relative "${directory}" "${root}${destination}")
        if(CMAKE_HOST_WIN32)
            file(TO_NATIVE_PATH "${relative}" relative)
        endif()
        file(REMOVE "${entry}")
        file(CREATE_LINK "${relative}" "${entry}" RESULT failed SYMBOLIC)
        if(failed AND EXISTS "${root}${destination}" AND
           NOT IS_DIRECTORY "${root}${destination}")
            file(COPY_FILE "${root}${destination}" "${entry}")
        endif()
    endforeach()
endfunction()

# Unpack the glibc packages of <arch> into <target>. A package is an ar
# archive whose data.tar.zst holds the files. The copyright files of glibc
# and of the kernel headers go to licenses/ as glibc.txt and
# linux-headers.txt, and the one of each X11 and OpenGL package under the
# name of its package.
function(glibc_sysroot target arch triple)
    set(root "${DEST}/${target}")
    set(work "${DEST}/.download/${target}")
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${work}" "${root}")
    separate_arguments(packages UNIX_COMMAND "${GLIBC_PACKAGES}")
    if(NOT packages)
        message(FATAL_ERROR "tools/sysroot-pins has no GLIBC_PACKAGES")
    endif()
    set(copyrights "")
    foreach(package IN LISTS packages)
        set(name "${GLIBC_${arch}_${package}}")
        if(name STREQUAL "" OR "${GLIBC_${arch}_${package}_DIGEST}" STREQUAL "")
            message(FATAL_ERROR "tools/sysroot-pins has no GLIBC_${arch}_${package}")
        endif()
        get_filename_component(asset "${name}" NAME)
        # The name of the package is the part of the file before the first
        # underscore, and its copyright lies under that name.
        if(NOT package MATCHES "^(LIBC_DEV|LIBC|HEADERS)$")
            string(REGEX REPLACE "_.*$" "" debian "${asset}")
            list(APPEND copyrights "${debian}")
        endif()
        fetch("${GLIBC_${arch}_URL}/${name}" "${work}/${asset}"
              "${GLIBC_${arch}_${package}_DIGEST}")
        file(REMOVE_RECURSE "${work}/deb")
        file(ARCHIVE_EXTRACT INPUT "${work}/${asset}" DESTINATION "${work}/deb")
        file(GLOB data "${work}/deb/data.tar.*")
        list(LENGTH data count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "${asset} holds ${count} data archives, not one")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${data}" DESTINATION "${root}")
    endforeach()
    file(REMOVE_RECURSE "${work}/deb")
    relative_links("${root}")
    # DESIGN: a package links a development name such as libm.so to the
    # library. Every link becomes a copy of the file it reaches in the
    # sysroot, once relative_links has turned the absolute ones relative.
    # lld then reads libm.so as the libm of the sysroot, and a Windows host,
    # where lld reads no link the script can write, holds the same tree. A
    # link that reaches no file goes, as the changelog of a -dev package
    # does, which names the one of a package not installed. A library link
    # among them would fail the link that names it.
    file(GLOB_RECURSE links LIST_DIRECTORIES false "${root}/*")
    foreach(link IN LISTS links)
        set(at "${link}")
        set(steps 0)
        while(IS_SYMLINK "${at}" AND steps LESS 8)
            file(READ_SYMLINK "${at}" points)
            get_filename_component(dir "${at}" DIRECTORY)
            set(at "${dir}/${points}")
            math(EXPR steps "${steps} + 1")
        endwhile()
        if(NOT at STREQUAL link)
            file(REMOVE "${link}")
            if(EXISTS "${at}" AND NOT IS_SYMLINK "${at}")
                file(COPY_FILE "${at}" "${link}")
            endif()
        endif()
    endforeach()
    file(GLOB builtins "${CLANG_DIR}/lib/clang/*/lib/${triple}-unknown-linux-musl/libclang_rt.builtins.a")
    if(NOT builtins)
        message(FATAL_ERROR "${CLANG_DIR} holds no builtins of ${triple}. "
                            "Run tools/get-clang.cmake first.")
    endif()
    file(COPY_FILE "${builtins}" "${root}/usr/lib/libclang_rt.builtins.a")
    file(COPY_FILE "${root}/usr/share/doc/libc6/copyright"
         "${DEST}/licenses/glibc.txt")
    file(COPY_FILE "${root}/usr/share/doc/linux-libc-dev/copyright"
         "${DEST}/licenses/linux-headers.txt")
    foreach(debian IN LISTS copyrights)
        if(EXISTS "${root}/usr/share/doc/${debian}/copyright")
            file(COPY_FILE "${root}/usr/share/doc/${debian}/copyright"
                 "${DEST}/licenses/${debian}.txt")
        endif()
    endforeach()
    file(COPY_FILE "${CLANG_DIR}/licenses/llvm.txt"
         "${DEST}/licenses/compiler-rt.txt")
endfunction()

function(linux_sysroot target arch musl_digest)
    set(root "${DEST}/${target}")
    set(work "${DEST}/.download/${target}")
    file(MAKE_DIRECTORY "${work}" "${root}/usr/lib")
    fetch("${ALPINE_URL}/${arch}/musl-dev-${MUSL_APK}.apk"
          "${work}/musl-dev.apk" "${musl_digest}")
    file(REMOVE_RECURSE "${work}/usr" "${root}/usr/include")
    file(ARCHIVE_EXTRACT INPUT "${work}/musl-dev.apk" DESTINATION "${work}"
         PATTERNS "usr/include/*" "usr/lib/*")
    file(COPY "${work}/usr/include" DESTINATION "${root}/usr")
    foreach(name crt1.o crti.o crtn.o rcrt1.o Scrt1.o libc.a)
        file(COPY "${work}/usr/lib/${name}" DESTINATION "${root}/usr/lib")
    endforeach()
    # DESIGN: the builtins come from the pinned clang, which builds them
    # from the LLVM source of the pin. A cross build of rt/ for musl then
    # takes nothing from a distribution but musl itself.
    file(GLOB builtins "${CLANG_DIR}/lib/clang/*/lib/${arch}-unknown-linux-musl/libclang_rt.builtins.a")
    if(NOT builtins)
        message(FATAL_ERROR "${CLANG_DIR} holds no builtins of ${arch} musl. "
                            "Run tools/get-clang.cmake first.")
    endif()
    file(COPY_FILE "${builtins}" "${root}/usr/lib/libclang_rt.builtins.a")
    fetch("${MUSL_SOURCE_URL}" "${DEST}/.download/musl.tar.gz"
          "${MUSL_SOURCE_DIGEST}")
    file(ARCHIVE_EXTRACT INPUT "${DEST}/.download/musl.tar.gz"
         DESTINATION "${DEST}/.download"
         PATTERNS "musl-${MUSL_VERSION}/COPYRIGHT")
    file(COPY_FILE "${DEST}/.download/musl-${MUSL_VERSION}/COPYRIGHT"
         "${DEST}/licenses/musl.txt")
    file(COPY_FILE "${CLANG_DIR}/licenses/llvm.txt"
         "${DEST}/licenses/compiler-rt.txt")
endfunction()

# Copy the .tbd stubs of the SDK that APPLE_SDK names into <dest>: those of
# usr/lib and of System/Library/Frameworks, as regular files. A linked stub
# becomes a copy, as the top-level stub of a framework is. A linked
# directory stays out, since lld reads no path through Versions/Current.
# Write the version of the SDK and the digest of what was copied.
function(apple_sdk dest)
    get_filename_component(sdk "${APPLE_SDK}" ABSOLUTE)
    if(NOT EXISTS "${sdk}/SDKSettings.json")
        message(FATAL_ERROR "APPLE_SDK names ${sdk}, which holds no "
                            "SDKSettings.json of a MacOSX.sdk")
    endif()
    file(READ "${sdk}/SDKSettings.json" settings)
    string(JSON version GET "${settings}" Version)
    file(REMOVE_RECURSE "${dest}")
    foreach(dir usr/lib System/Library/Frameworks)
        file(GLOB_RECURSE stubs RELATIVE "${sdk}" "${sdk}/${dir}/*.tbd")
        foreach(stub IN LISTS stubs)
            get_filename_component(parent "${dest}/${stub}" DIRECTORY)
            file(MAKE_DIRECTORY "${parent}")
            file(READ "${sdk}/${stub}" text)
            file(WRITE "${dest}/${stub}" "${text}")
        endforeach()
    endforeach()
    tree_digest("${dest}" digest)
    file(WRITE "${dest}/sdk-version" "${version}\n")
    file(WRITE "${dest}/digest" "${digest}\n")
    message(STATUS "${dest}: the stubs of the SDK ${version}")
endfunction()

# DESIGN: a macOS program that names no framework links against the stubs
# of libSystem that Zig generates, on every host and the Mac as well, so
# one object links to the same bytes anywhere. The runtime library
# compiles against the headers beside them for the same reason. Zig ships
# no framework, so a program that names one takes Apple's SDK. The SDK
# keeps to sdk/ of the sysroot, which this function leaves alone.
function(macos_sysroot target arch)
    set(root "${DEST}/${target}")
    set(work "${DEST}/.download/zig")
    set(zig "zig-${ZIG_TAG}")
    string(REPLACE "@TAG@" "${ZIG_TAG}" url "${ZIG_URL}")
    fetch("${url}" "${work}/${zig}.tar.xz" "${ZIG_DIGEST}")
    if(NOT EXISTS "${work}/${zig}/LICENSE")
        file(ARCHIVE_EXTRACT INPUT "${work}/${zig}.tar.xz" DESTINATION "${work}"
             PATTERNS "${zig}/LICENSE" "${zig}/lib/libc/darwin/*"
                      "${zig}/lib/libc/include/any-darwin-any/*")
    endif()
    file(REMOVE_RECURSE "${root}/usr" "${root}/sdk-version")
    file(MAKE_DIRECTORY "${root}/usr/lib")
    file(COPY_FILE "${work}/${zig}/lib/libc/darwin/libSystem.tbd"
         "${root}/usr/lib/libSystem.tbd")
    file(COPY "${work}/${zig}/lib/libc/include/any-darwin-any/"
         DESTINATION "${root}/usr/include")
    file(READ "${work}/${zig}/lib/libc/darwin/SDKSettings.json" settings)
    string(JSON version GET "${settings}" MinimalDisplayName)
    file(WRITE "${root}/sdk-version" "${version}\n")
    file(COPY_FILE "${work}/${zig}/LICENSE" "${DEST}/licenses/zig.txt")
    fetch("${APSL_URL}" "${DEST}/.download/apsl.txt" "${APSL_DIGEST}")
    file(COPY_FILE "${DEST}/.download/apsl.txt" "${DEST}/licenses/apsl.txt")
    if(DEFINED APPLE_SDK)
        apple_sdk("${root}/sdk")
    endif()
endfunction()

# The source release of mingw-w64 that tools/sysroot-pins names, fetched
# and unpacked once under .download/. <out> is its directory.
function(mingw_source out)
    set(work "${DEST}/.download/mingw")
    set(name "mingw-w64-v${MINGW_VERSION}")
    string(REPLACE "@VERSION@" "${MINGW_VERSION}" url "${MINGW_URL}")
    file(MAKE_DIRECTORY "${work}")
    fetch("${url}" "${work}/${name}.tar.bz2" "${MINGW_DIGEST}")
    if(NOT EXISTS "${work}/${name}/mingw-w64-headers")
        file(ARCHIVE_EXTRACT INPUT "${work}/${name}.tar.bz2"
             DESTINATION "${work}"
             PATTERNS "${name}/mingw-w64-headers/*"
                      "${name}/mingw-w64-crt/def-include/*"
                      "${name}/mingw-w64-crt/lib-common/*"
                      "${name}/mingw-w64-crt/lib64/*"
                      "${name}/mingw-w64-crt/libarm64/*"
                      "${name}/COPYING.MinGW-w64-runtime/*")
    endif()
    set("${out}" "${work}/${name}" PARENT_SCOPE)
endfunction()

# DESIGN: the headers of a Windows sysroot are the ones mingw-w64-headers
# installs, from crt/, include/ and ddk/include/ of the release, all into
# include/ of the sysroot: the headers and the few other files a header
# includes, with the .idl files, the change logs and the makefiles left
# behind. _mingw.h is written from _mingw.h.in with the two values its
# configure fills in: the UCRT as the C runtime, which ucrtbase.dll is,
# and Windows 10 as the version of the API.
function(windows_headers source root)
    set(headers "${source}/mingw-w64-headers")
    foreach(dir crt include ddk/include)
        file(COPY "${headers}/${dir}/" DESTINATION "${root}/include"
             FILES_MATCHING PATTERN "*.h" PATTERN "*.c" PATTERN "*.inl"
             PATTERN "*.dlg" PATTERN "*.h16" PATTERN "*.hxx" PATTERN "*.rh"
             PATTERN "*.ver")
    endforeach()
    file(READ "${headers}/crt/_mingw.h.in" text)
    string(REPLACE "@DEFAULT_MSVCRT_VERSION@" "0xE00" text "${text}")
    string(REPLACE "@DEFAULT_WIN32_WINNT@" "0xa00" text "${text}")
    file(WRITE "${root}/include/_mingw.h" "${text}")
endfunction()

# DESIGN: the import libraries are those of the DLLs a program of ours
# links: ucrtbase.dll, the C library, kernel32.dll and ntdll.dll, which
# every program links, and the DLLs the runtime, raylib, Mbed TLS and the
# tests name: dbghelp for the stack traces of the runtime, user32, gdi32,
# shell32 and winmm for raylib, bcrypt and ws2_32 for Mbed TLS. Eddie
# decided the list on 2026-10-08 in decision 4 of
# docs/work-order-distribution.md.
set(WINDOWS_DLLS ucrtbase ntdll kernel32 user32 gdi32 shell32 winmm dbghelp
    bcrypt ws2_32)

# The .def file of <dll> for the processor of <triple>, written to <def>:
# the .def.in of lib-common/ preprocessed by the pinned clang for the
# triple, as the makefile of mingw-w64-crt does, with the stdcall
# decoration of a name stripped, or the plain .def of <lib_dir>, the
# directory of the processor, or of lib-common/.
function(windows_def crt dll triple lib_dir def)
    if(EXISTS "${crt}/lib-common/${dll}.def.in")
        execute_process(
            COMMAND "${CLANG_DIR}/bin/clang${HOST_EXE}" "--target=${triple}"
                    -x c -E -P -nostdinc -I "${crt}/def-include"
                    "${crt}/lib-common/${dll}.def.in"
            RESULT_VARIABLE failed OUTPUT_VARIABLE text ERROR_VARIABLE err)
        if(failed)
            message(FATAL_ERROR "clang did not preprocess ${dll}.def.in:\n${err}")
        endif()
        # A name of the form name@N carries the stdcall decoration of
        # 32-bit code, which no 64-bit code has.
        string(REPLACE ";" "\;" text "${text}")
        string(REPLACE "\n" ";" lines "${text}")
        set(text "")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "^([^ ]+)@[0-9]+( |$)" "\\1\\2" line "${line}")
            string(APPEND text "${line}\n")
        endforeach()
        file(WRITE "${def}" "${text}")
    elseif(EXISTS "${crt}/${lib_dir}/${dll}.def")
        file(COPY_FILE "${crt}/${lib_dir}/${dll}.def" "${def}")
    elseif(EXISTS "${crt}/lib-common/${dll}.def")
        file(COPY_FILE "${crt}/lib-common/${dll}.def" "${def}")
    else()
        message(FATAL_ERROR "${crt} holds no .def file of ${dll}")
    endif()
endfunction()

# The Windows sysroot of target: include/ with the headers, lib/ with the
# import libraries of WINDOWS_DLLS for <machine>, the processor as
# llvm-dlltool names it, and the builtins of the pinned clang for the
# msvc triple of <arch>, which the text of antic and the C of the gnu
# <triple> both call.
function(windows_sysroot target arch triple machine lib_dir)
    set(root "${DEST}/${target}")
    set(work "${DEST}/.download/mingw/${target}")
    mingw_source(source)
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${root}/lib" "${work}")
    windows_headers("${source}" "${root}")
    # DESIGN: llvm-dlltool is llvm-ar under that name, which reads its
    # own name, so the pinned llvm-ar stands in under a link, or a copy
    # where the host writes no link.
    set(dlltool "${DEST}/.download/mingw/llvm-dlltool${HOST_EXE}")
    file(REMOVE "${dlltool}")
    file(CREATE_LINK "${LLVM_BIN}/llvm-ar${HOST_EXE}" "${dlltool}"
         COPY_ON_ERROR SYMBOLIC)
    set(crt "${source}/mingw-w64-crt")
    foreach(dll IN LISTS WINDOWS_DLLS)
        windows_def("${crt}" "${dll}" "${triple}" "${lib_dir}"
                    "${work}/${dll}.def")
        execute_process(COMMAND "${dlltool}" -m "${machine}"
                                -d "${work}/${dll}.def"
                                -l "${root}/lib/${dll}.lib"
                        RESULT_VARIABLE failed ERROR_VARIABLE err)
        if(failed)
            message(FATAL_ERROR "llvm-dlltool wrote no ${dll}.lib:\n${err}")
        endif()
    endforeach()
    file(GLOB builtins "${CLANG_DIR}/lib/clang/*/lib/${arch}-pc-windows-msvc/clang_rt.builtins.lib")
    if(NOT builtins)
        message(FATAL_ERROR "${CLANG_DIR} holds no builtins of ${arch} Windows. "
                            "Run tools/get-clang.cmake first.")
    endif()
    file(COPY_FILE "${builtins}" "${root}/lib/clang_rt.builtins.lib")
    file(COPY_FILE "${source}/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt"
         "${DEST}/licenses/mingw-w64.txt")
    file(COPY_FILE "${CLANG_DIR}/licenses/llvm.txt"
         "${DEST}/licenses/compiler-rt.txt")
endfunction()

foreach(target IN LISTS TARGETS)
    if(target STREQUAL "linux-x86_64")
        linux_sysroot("${target}" x86_64 "${MUSL_DEV_X86_64}")
    elseif(target STREQUAL "linux-arm64")
        linux_sysroot("${target}" aarch64 "${MUSL_DEV_AARCH64}")
    elseif(target STREQUAL "linux-x86_64-glibc")
        glibc_sysroot("${target}" X86_64 x86_64)
    elseif(target STREQUAL "linux-arm64-glibc")
        glibc_sysroot("${target}" AARCH64 aarch64)
    elseif(target STREQUAL "macos-arm64")
        macos_sysroot("${target}" arm64)
    elseif(target STREQUAL "macos-x86_64")
        macos_sysroot("${target}" x86_64)
    elseif(target STREQUAL "windows-x86_64")
        windows_sysroot("${target}" x86_64 x86_64-w64-windows-gnu
                        i386:x86-64 lib64)
    elseif(target STREQUAL "windows-arm64")
        windows_sysroot("${target}" aarch64 aarch64-w64-windows-gnu arm64
                        libarm64)
    else()
        message(FATAL_ERROR "unknown target ${target}")
    endif()
    relative_links("${DEST}/${target}")
    message(STATUS "${DEST}/${target}")
endforeach()
