# Install the sysroot of each target that lld links against into
# <dir>/<target>/, and the licence of each component into <dir>/licenses/.
#
#   cmake -DDEST=<dir> -DLLVM_BIN=<dir> -DTARGETS=<target>[;<target>]
#         [-DCLANG_DIR=<dir>] [-DACCEPT_LICENSE=yes] [-DAPPLE_SDK=<dir>]
#         -P tools/get-sysroot.cmake
#
# linux-x86_64, linux-arm64: musl from the Alpine package of
#   tools/sysroot-pins, checked against its digest, and the compiler-rt
#   builtins of the pinned clang in CLANG_DIR, which defaults to
#   build/clang of the repository.
# linux-x86_64-glibc, linux-arm64-glibc: glibc 2.35 and the kernel headers
#   of Ubuntu 22.04 from the packages of tools/sysroot-pins, for the Linux
#   link mode against glibc.
# macos-arm64, macos-x86_64: the stubs of libSystem that Zig generates and
#   the headers of the macOS C library, from the release of Zig that
#   tools/zig-stubs-pin names, on every host. They link every program that
#   names no framework. APPLE_SDK=<MacOSX.sdk> also copies the .tbd stubs
#   of that SDK into sdk/ of the sysroot, for a program that names one.
# windows-x86_64, windows-arm64: the MSVC CRT and the Windows SDK import
#   libraries, which xwin downloads at the versions of tools/sysroot-pins.
#   Microsoft licenses them to the user, so the script runs xwin only with
#   ACCEPT_LICENSE=yes. It installs the pinned xwin when the path has none.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST OR NOT DEFINED LLVM_BIN OR NOT DEFINED TARGETS)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -DLLVM_BIN=<dir> "
                        "-DTARGETS=<target>[;<target>] "
                        "[-DACCEPT_LICENSE=yes] -P tools/get-sysroot.cmake")
endif()
# A relative DEST or LLVM_BIN names a directory under the one this script
# runs in. The digest of a Windows tree lists its files relative to an
# absolute path, and finds none under a relative one.
get_filename_component(DEST "${DEST}" ABSOLUTE)
get_filename_component(LLVM_BIN "${LLVM_BIN}" ABSOLUTE)

set(tools_dir "${CMAKE_CURRENT_LIST_DIR}")
if(NOT DEFINED CLANG_DIR)
    get_filename_component(CLANG_DIR "${tools_dir}/../build/clang" ABSOLUTE)
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

# SPLAT is empty for a run that does everything, script for one that
# writes the xwin command and stops, and done for one that checks the
# tree after the caller ran it.
if(NOT DEFINED SPLAT)
    set(SPLAT "")
endif()
set(SPLAT_SCRIPT "${DEST}/.download/splat.sh")
if(CMAKE_HOST_WIN32)
    set(SPLAT_SCRIPT "${DEST}/.download/splat.cmd")
endif()
if(SPLAT STREQUAL "script")
    file(MAKE_DIRECTORY "${DEST}/.download")
    file(WRITE "${SPLAT_SCRIPT}" "")
endif()

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
# order of their paths, hashed once more. xwin adds symbolic links for
# other spellings on a case-sensitive file system, so links stay out.
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

# Unpack the three glibc packages of <arch> into <target>. A package is an
# ar archive whose data.tar.zst holds the files. The copyright files of
# glibc and of the kernel headers go to licenses/.
function(glibc_sysroot target arch)
    set(root "${DEST}/${target}")
    set(work "${DEST}/.download/${target}")
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${work}" "${root}")
    foreach(package LIBC_DEV LIBC HEADERS)
        set(name "${GLIBC_${arch}_${package}}")
        get_filename_component(asset "${name}" NAME)
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
    file(COPY_FILE "${root}/usr/share/doc/libc6/copyright"
         "${DEST}/licenses/glibc.txt")
    file(COPY_FILE "${root}/usr/share/doc/linux-libc-dev/copyright"
         "${DEST}/licenses/linux-headers.txt")
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

# The xwin program of the pin, from the path or from its own release.
function(xwin_program out)
    find_program(found xwin)
    if(found)
        execute_process(COMMAND "${found}" --version
                        OUTPUT_VARIABLE text OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" version "${text}")
        if(version STREQUAL XWIN_VERSION)
            set("${out}" "${found}" PARENT_SCOPE)
            return()
        endif()
    endif()
    cmake_host_system_information(RESULT os QUERY OS_NAME)
    cmake_host_system_information(RESULT cpu QUERY OS_PLATFORM)
    string(TOLOWER "${os}" os)
    string(TOLOWER "${cpu}" cpu)
    if(cpu MATCHES "^(arm64|aarch64)$")
        set(cpu arm64)
    elseif(cpu MATCHES "^(x86_64|amd64|x64)$")
        set(cpu x86_64)
    endif()
    set(asset "${XWIN_BIN_${os}-${cpu}}")
    set(digest "${XWIN_BIN_${os}-${cpu}_DIGEST}")
    if(asset STREQUAL "" AND os STREQUAL "windows")
        # xwin publishes no build for Windows on ARM64, and Windows runs an
        # x64 program there under emulation.
        set(asset "${XWIN_BIN_windows-x86_64}")
        set(digest "${XWIN_BIN_windows-x86_64_DIGEST}")
    endif()
    if(asset STREQUAL "")
        message(FATAL_ERROR
                "xwin ${XWIN_VERSION} publishes no build for ${os}-${cpu}. "
                "Run cargo install xwin --locked --version ${XWIN_VERSION}")
    endif()
    set(work "${DEST}/.download/xwin-bin")
    file(MAKE_DIRECTORY "${work}")
    fetch("${XWIN_BIN_URL}/${asset}" "${work}/${asset}" "${digest}")
    file(ARCHIVE_EXTRACT INPUT "${work}/${asset}" DESTINATION "${work}")
    file(GLOB_RECURSE program "${work}/*/xwin" "${work}/*/xwin.exe")
    if(program STREQUAL "")
        message(FATAL_ERROR "${asset} holds no xwin program")
    endif()
    list(GET program 0 program)
    set("${out}" "${program}" PARENT_SCOPE)
endfunction()

# The CRT and the SDK that the Build Tools of Visual Studio installed on
# this machine. A junction needs no privilege where a symbolic link does,
# so the sysroot points at them and downloads nothing. Returns the path of
# the installation, or an empty string when the machine has none.
function(build_tools out)
    set("${out}" "" PARENT_SCOPE)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    set(where "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
    if(DEFINED VSWHERE)
        set(where "${VSWHERE}")
    endif()
    if(NOT EXISTS "${where}")
        return()
    endif()
    execute_process(COMMAND "${where}" -nologo -latest -products *
                            -property installationPath
                    OUTPUT_VARIABLE found OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    string(REGEX REPLACE "\r?\n.*$" "" found "${found}")
    if(found STREQUAL "" OR NOT IS_DIRECTORY "${found}")
        return()
    endif()
    set("${out}" "${found}" PARENT_SCOPE)
endfunction()

# The newest directory under root, by version order.
function(newest root out)
    file(GLOB found "${root}/*")
    set(directories "")
    foreach(path IN LISTS found)
        if(IS_DIRECTORY "${path}")
            list(APPEND directories "${path}")
        endif()
    endforeach()
    list(SORT directories COMPARE NATURAL ORDER DESCENDING)
    list(GET directories 0 first)
    set("${out}" "${first}" PARENT_SCOPE)
endfunction()

# Point link at target with a junction, which any account may create.
function(junction link target)
    if(NOT IS_DIRECTORY "${target}")
        message(FATAL_ERROR "${target} is missing, so the sysroot has no "
                            "${link}")
    endif()
    get_filename_component(parent "${link}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    file(TO_NATIVE_PATH "${link}" from)
    file(TO_NATIVE_PATH "${target}" to)
    execute_process(COMMAND cmd /c mklink /J "${from}" "${to}"
                    RESULT_VARIABLE made OUTPUT_QUIET
                    ERROR_VARIABLE complaint)
    if(NOT made EQUAL 0)
        message(FATAL_ERROR "cannot join ${from} to ${to}: ${complaint}")
    endif()
endfunction()

# Lay the sysroot of a Windows target over the local Build Tools.
function(windows_local target arch install)
    set(root "${DEST}/${target}")
    set(ms x64)
    if(arch STREQUAL "aarch64")
        set(ms arm64)
    endif()
    newest("${install}/VC/Tools/MSVC" msvc)
    set(kits "$ENV{ProgramFiles\(x86\)}/Windows Kits/10")
    if(DEFINED WINDOWS_KITS)
        set(kits "${WINDOWS_KITS}")
    endif()
    newest("${kits}/Include" headers)
    get_filename_component(sdk_version "${headers}" NAME)

    file(REMOVE_RECURSE "${root}")
    junction("${root}/crt/include" "${msvc}/include")
    junction("${root}/crt/lib/${arch}" "${msvc}/lib/${ms}")
    foreach(part ucrt um shared)
        junction("${root}/sdk/include/${part}" "${headers}/${part}")
    endforeach()
    foreach(part ucrt um)
        junction("${root}/sdk/lib/${part}/${arch}"
                 "${kits}/Lib/${sdk_version}/${part}/${ms}")
    endforeach()
    file(WRITE "${DEST}/licenses/windows-sdk.txt"
"The sysroot of ${target} points at the Microsoft C runtime and Windows SDK
${sdk_version} that the Build Tools installed on this machine, under
${install}. Nothing of Microsoft is copied or redistributed.
")
    message(STATUS "${target}: over the Build Tools in ${install}")
endfunction()

# Append the splat of one target to the script that the caller runs. A
# shell gives xwin the terminal that its progress bar looks for.
function(write_splat program target arch options)
    set(quoted "")
    foreach(argument "${program}" --accept-license --cache-dir
            "${DEST}/.download/xwin" --arch "${arch}" --crt-version
            "${XWIN_CRT_VERSION}" --sdk-version "${XWIN_SDK_VERSION}" splat
            --output "${DEST}/${target}" ${options})
        if(CMAKE_HOST_WIN32)
            file(TO_NATIVE_PATH "${argument}" argument)
        endif()
        string(APPEND quoted " \"${argument}\"")
    endforeach()
    file(APPEND "${SPLAT_SCRIPT}" "echo ${target}\n${quoted}\n")
endfunction()

function(windows_sysroot target arch digest)
    build_tools(install)
    if(NOT install STREQUAL "")
        windows_local("${target}" "${arch}" "${install}")
        return()
    endif()
    if(NOT ACCEPT_LICENSE STREQUAL "yes")
        message(FATAL_ERROR
                "${target}: xwin downloads the Microsoft CRT ${XWIN_CRT_VERSION} "
                "and the Windows SDK ${XWIN_SDK_VERSION}, which Microsoft "
                "licenses to you. Pass -DACCEPT_LICENSE=yes to accept their "
                "terms.")
    endif()
    if(CMAKE_HOST_WIN32)
        # xwin links sdk/lib/<version> to its own directory whatever the
        # flags say, and Windows grants a symbolic link only to Developer
        # Mode or to an administrator. One link costs less to try than a
        # gigabyte to download.
        set(probe "${DEST}/.probe-directory")
        file(REMOVE_RECURSE "${probe}" "${probe}-link")
        file(MAKE_DIRECTORY "${probe}")
        file(CREATE_LINK "${probe}" "${probe}-link" SYMBOLIC RESULT linked)
        file(REMOVE_RECURSE "${probe}" "${probe}-link")
        if(NOT linked STREQUAL "0")
            message(FATAL_ERROR
                    "${target}: Windows refuses a symbolic link in ${DEST}, "
                    "and xwin lays out the SDK with one. Turn on Developer "
                    "Mode under Settings, System, For developers, or run "
                    "this command as an administrator. A Developer Command "
                    "Prompt of Visual Studio needs neither, because "
                    "lld-link reads the LIB variable it sets.")
        endif()
    endif()
    # The symlinks of xwin only fix the casing of the SDK for a
    # case-sensitive file system. Windows has none, and it refuses a
    # symlink to a program without the privilege, so they go.
    set(splat_options "")
    if(CMAKE_HOST_WIN32)
        set(splat_options --disable-symlinks)
    endif()
    if(NOT SPLAT STREQUAL "done")
        xwin_program(program)
        file(REMOVE_RECURSE "${DEST}/${target}")
        # DESIGN: the progress bar of xwin asks whether its own standard
        # output is a console and draws nothing when it is not. Under
        # cmake it never is, so SPLAT=script writes the command instead
        # and the caller runs it with a terminal of its own.
        if(SPLAT STREQUAL "script")
            write_splat("${program}" "${target}" "${arch}" "${splat_options}")
            return()
        endif()
        message(STATUS "${target}: xwin downloads about 1 GB and unpacks "
                       "it, which takes minutes without a word")
        execute_process(
            COMMAND "${program}" --accept-license
                    --cache-dir "${DEST}/.download/xwin" --arch "${arch}"
                    --crt-version "${XWIN_CRT_VERSION}"
                    --sdk-version "${XWIN_SDK_VERSION}"
                    splat --output "${DEST}/${target}" ${splat_options}
            RESULT_VARIABLE ran)
        if(NOT ran EQUAL 0)
            message(FATAL_ERROR "${program} failed for ${target}")
        endif()
    endif()
    tree_digest("${DEST}/${target}" actual)
    if(NOT actual STREQUAL digest)
        message(FATAL_ERROR "${DEST}/${target}: SHA-256 of the files "
                            "${actual}, expected ${digest}")
    endif()
    file(WRITE "${DEST}/licenses/windows-sdk.txt"
"The import libraries in sysroot/windows-x86_64 and sysroot/windows-arm64
come from the Microsoft C runtime ${XWIN_CRT_VERSION} and the Windows SDK
${XWIN_SDK_VERSION}, fetched with xwin ${XWIN_VERSION}. Microsoft distributes
them under the licence terms that xwin shows and that the caller accepted
with ACCEPT_LICENSE.
")
endfunction()

set(wrote_script FALSE)
foreach(target IN LISTS TARGETS)
    if(target MATCHES "^windows-" AND SPLAT STREQUAL "script")
        set(wrote_script TRUE)
    endif()
    if(target STREQUAL "linux-x86_64")
        linux_sysroot("${target}" x86_64 "${MUSL_DEV_X86_64}")
    elseif(target STREQUAL "linux-arm64")
        linux_sysroot("${target}" aarch64 "${MUSL_DEV_AARCH64}")
    elseif(target STREQUAL "linux-x86_64-glibc")
        glibc_sysroot("${target}" X86_64)
    elseif(target STREQUAL "linux-arm64-glibc")
        glibc_sysroot("${target}" AARCH64)
    elseif(target STREQUAL "macos-arm64")
        macos_sysroot("${target}" arm64)
    elseif(target STREQUAL "macos-x86_64")
        macos_sysroot("${target}" x86_64)
    elseif(target STREQUAL "windows-x86_64")
        windows_sysroot("${target}" x86_64 "${XWIN_TREE_X86_64}")
    elseif(target STREQUAL "windows-arm64")
        windows_sysroot("${target}" aarch64 "${XWIN_TREE_AARCH64}")
    else()
        message(FATAL_ERROR "unknown target ${target}")
    endif()
    if(NOT (target MATCHES "^windows-" AND SPLAT STREQUAL "script"))
        message(STATUS "${DEST}/${target}")
    endif()
endforeach()
if(wrote_script)
    message(STATUS "splat script: ${SPLAT_SCRIPT}")
endif()
