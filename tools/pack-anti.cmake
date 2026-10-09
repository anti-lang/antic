# Build the package that a user installs, one per host.
#
#   cmake -DDEST=<dir> -DCLANG=<clang> -DLLVM_BIN=<dir> -DSYSROOT=<dir>
#         -DRUNTIME=<dir> -DHOSTS=<host>[;<host>] [-DSYMBOLS=<dir>]
#         [-DTOOLS=<dir>] -P tools/pack-anti.cmake
#
# CLANG is the pinned clang of build/clang, LLVM_BIN its tools, SYSROOT
# the directory of tools/get-sysroot.cmake and RUNTIME the runtime that the
# CMake build wrote. SYMBOLS is where the PDB of a Windows program goes,
# which is outside the package and is needed when a Windows host is
# compiled here. TOOLS holds the LLVM tools of every host of HOSTS that is
# not this machine, as <dir>/<host>/bin, which tools/get-llvm.cmake
# -DHOST=<host> lays out. For each host it compiles antic and anti for
# that host and lays the package out around them. ANTIC and ANTI name the
# programs already built for the one host of HOSTS, which the package
# takes instead, and then CLANG and LLVM_BIN are not needed:
#
#   bin/        antic, anti and the LLVM tools of the host with llvm-version,
#               copied as bin/ of the runtime archive holds them
#   lib/<t>/<l>/ the runtime library of all six targets, per processor level
#   include/    the headers of the native libraries, one directory each
#   std/        the standard library
#   lib/<t>-glibc/ the runtime of both Linux targets against glibc
#   sysroot/    the two Linux sysroots of musl, their two -glibc twins with
#               the X11 and OpenGL packages, the two macOS sysroots of
#               Zig's stubs and the two Windows sysroots of mingw-w64, all
#               ours to redistribute
#   tools/      the scripts that install the sysroot of the host
#   licenses/   one file per component, and sources.txt, the record of
#               the upstream source of each
#   VERSION     the version of tools/version
#
# The result is anti-<version>-<host>.tar.xz in DEST, with its digest in
# SHA256SUMS. The stubs of Apple's SDK in sdk/ of a macOS sysroot stay
# out, because their licence allows no redistribution, and a user brings
# them from a Mac with anti sdk import. The package carries no key: the
# installer holds the key that checks the manifest of the release.
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
# DESIGN: the LLVM tools of a package are those of its host, in bin/
# beside antic and anti, which Eddie decided on 2026-10-08 in
# docs/work-order-distribution.md. bin/ of the runtime archive holds the
# tools of this machine, and the tools of every other host stand in
# TOOLS/<host>/bin, which tools/get-llvm.cmake -DHOST=<host> lays out the
# same way. A host without its directory is refused here, before anything
# is compiled, since a package with the tools of another host fails on
# every machine it installs on. llvm-version of each directory has to
# name the pin, so a directory of an earlier release is refused as well.
include("${tools_dir}/fetch-release.cmake")
antic_machine_host(machine)
file(READ "${tools_dir}/llvm-version" llvm_version)
string(STRIP "${llvm_version}" llvm_version)
foreach(host IN LISTS HOSTS)
    if(host STREQUAL machine)
        set(dir "${RUNTIME}/bin")
    elseif(DEFINED TOOLS AND IS_DIRECTORY "${TOOLS}/${host}/bin")
        set(dir "${TOOLS}/${host}/bin")
    else()
        message(FATAL_ERROR "${host}: the LLVM tools of ${host} are not in "
                            "TOOLS/${host}/bin. cmake -DHOST=${host} "
                            "-DDEST=<dir>/${host} -P tools/get-llvm.cmake "
                            "lays them out, and -DTOOLS=<dir> names the "
                            "directory.")
    endif()
    if(NOT EXISTS "${dir}/llvm-version")
        message(FATAL_ERROR "${host}: ${dir} holds no llvm-version, so it is "
                            "no bin/ of the LLVM tools")
    endif()
    file(READ "${dir}/llvm-version" found)
    string(STRIP "${found}" found)
    if(NOT found STREQUAL llvm_version)
        message(FATAL_ERROR "${host}: ${dir} holds LLVM ${found}, and "
                            "tools/llvm-version names ${llvm_version}")
    endif()
    set("tools_of_${host}" "${dir}")
endforeach()

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

include("${tools_dir}/host-compile.cmake")

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
# sysroot, or the mingw-w64 sysroot. The sources and the
# include directories of each program are the lists of tools/sources.cmake,
# which the CMake build reads as well.
# DESIGN: the sources of the compiler take its include directories alone.
# Those of anti take anti's as well. The CMake build compiles antic_core
# and anti the same way.
include("${tools_dir}/sources.cmake")
include("${tools_dir}/warnings.cmake")

# DESIGN: antic checks every pattern literal with PCRE2, so antic and anti,
# which links the same compiler, compile the PCRE2 sources of the build
# beside RUNTIME with the flags of each host. The list and the flags are
# those of src/native/pcre2-files.cmake, and the headers are the ones the
# build wrote. The library of the runtime tree is not taken: it is built
# for the default level of its target, and antic runs on every level.
include("${root}/src/native/pcre2-files.cmake")
set(pcre2_work "${build_dir}/native/pcre2")
set(pcre2_files "")
if(NOT DEFINED ANTIC)
    file(STRINGS "${build_dir}/CMakeCache.txt" pcre2_dir
         REGEX "^ANTIC_PCRE2_DIR:[A-Z]+=")
    string(REGEX REPLACE "^ANTIC_PCRE2_DIR:[A-Z]+=" "" pcre2_dir "${pcre2_dir}")
    file(STRINGS "${tools_dir}/pcre2-pin" pcre2_version REGEX "^PCRE2_VERSION=")
    string(REGEX REPLACE "^PCRE2_VERSION=" "" pcre2_version "${pcre2_version}")
    set(pcre2_source "${pcre2_dir}/pcre2-${pcre2_version}")
    if(pcre2_dir STREQUAL "" OR NOT EXISTS "${pcre2_source}/src")
        message(FATAL_ERROR "${build_dir} names no PCRE2 source, which antic "
                            "compiles")
    endif()
    foreach(source IN LISTS ANTIC_PCRE2_SOURCES)
        list(APPEND pcre2_files "${pcre2_source}/src/pcre2_${source}.c")
    endforeach()
    list(APPEND pcre2_files "${pcre2_work}/pcre2_chartables.c")
endif()

# The lowest processor level of host, the first that tools/cpu-levels
# lists for its architecture.
function(lowest_level out host)
    string(REGEX REPLACE "^[a-z]+-" "" arch "${host}")
    file(STRINGS "${tools_dir}/cpu-levels" rows REGEX "^level ")
    foreach(row IN LISTS rows)
        if(row MATCHES "^level ([^ ]+) ${arch} ")
            set("${out}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "tools/cpu-levels lists no level of ${arch}")
endfunction()

function(build_program host output program)
    antic_host_triple("${host}" triple)
    set(program_sources ${ANTIC_MAIN_SOURCES})
    set(program_dirs "")
    if(program STREQUAL "anti")
        set(program_sources ${ANTI_SOURCES})
        set(program_dirs ${ANTI_INCLUDE_DIRS})
    endif()
    set(core_includes -I "${pcre2_work}/include")
    foreach(dir IN LISTS ANTIC_CORE_INCLUDE_DIRS)
        list(APPEND core_includes -I "${root}/${dir}")
    endforeach()
    set(program_includes ${core_includes})
    foreach(dir IN LISTS program_dirs)
        list(APPEND program_includes -I "${root}/${dir}")
    endforeach()
    antic_host_compile_options(common "${host}" "${root}" "${SYSROOT}"
                               "${resource}" "${macos_sdk}" "${version}")
    set(link "")
    if(host MATCHES "^macos-")
        set(link --ld-path=${LLVM_BIN}/ld64.lld)
    elseif(host MATCHES "^windows-")
        # DESIGN: a Windows program carries no symbol table, so what names
        # a frame of a report from a user is the PDB that lld-link writes
        # with /DEBUG. It stands in SYMBOLS, outside the package, and step
        # 4 of the release folds it into the symbols archive of the host.
        # /PDBALTPATH:%_PDB% keeps the CodeView record of the executable
        # to the file name of the PDB, so the path of this machine does
        # not ship. /pdbsourcepath:. keeps the directory of the link out of
        # the relative file names of the PDB. /OPT:REF drops what the
        # program never reaches, as /DEBUG turns it off.
        if(NOT DEFINED SYMBOLS)
            message(FATAL_ERROR "${host}: a Windows program is linked with "
                                "/DEBUG, and SYMBOLS names the directory its "
                                "PDB goes in")
        endif()
        file(MAKE_DIRECTORY "${SYMBOLS}/${host}")
        antic_windows_link_options(link "${host}" "${SYSROOT}/${host}")
        list(APPEND link /DEBUG "/PDBALTPATH:%_PDB%" /pdbsourcepath:.
             /OPT:REF)
    endif()
    # Every program links from objects, which stand outside the tree of the
    # package, work/<host>/anti. The clang driver looks for the start files
    # of gcc on Linux, so the objects go to ld.lld with the musl ones
    # instead. A Windows link names them by relative paths.
    set(objects "")
    file(MAKE_DIRECTORY "${DEST}/work/${host}/objects/${program}")
    # An object is named after the path of its source, since two
    # directories hold a main.c.
    foreach(source IN LISTS ANTIC_CORE_SOURCES program_sources)
        set(includes ${core_includes})
        if(source IN_LIST program_sources)
            set(includes ${program_includes})
        endif()
        string(REGEX REPLACE "\\.c$" "" name "${source}")
        string(REPLACE "/" "_" name "${name}")
        set(object "${DEST}/work/${host}/objects/${program}/${name}.o")
        execute_process(COMMAND "${CLANG}" ${common} ${ANTIC_C_WARNINGS}
                                ${includes} -c
                                -o "${object}" "${root}/${source}"
                        RESULT_VARIABLE failed)
        if(failed)
            message(FATAL_ERROR "${host}: ${source} did not compile")
        endif()
        list(APPEND objects "${object}")
    endforeach()
    foreach(source IN LISTS pcre2_files)
        get_filename_component(name "${source}" NAME_WE)
        set(object "${DEST}/work/${host}/objects/${program}/${name}.o")
        execute_process(COMMAND "${CLANG}" ${common} ${ANTIC_PCRE2_WARNINGS}
                                ${ANTIC_PCRE2_DEFINES}
                                "-ffile-prefix-map=${pcre2_source}=."
                                "-ffile-prefix-map=${build_dir}=."
                                -I "${pcre2_work}/include"
                                -I "${pcre2_source}/src" -c
                                -o "${object}" "${source}"
                        RESULT_VARIABLE failed)
        if(failed)
            message(FATAL_ERROR "${host}: ${source} did not compile")
        endif()
        list(APPEND objects "${object}")
    endforeach()
    if(host MATCHES "^linux-")
        set(lib "${SYSROOT}/${host}/usr/lib")
        execute_process(
            COMMAND "${LLVM_BIN}/ld.lld" -static -pie --no-dynamic-linker
                    -o "${output}" "${lib}/rcrt1.o" "${lib}/crti.o" ${objects}
                    "${lib}/libc.a" "${lib}/libclang_rt.builtins.a"
                    "${lib}/crtn.o"
            RESULT_VARIABLE failed)
    elseif(host MATCHES "^windows-")
        # DESIGN: the link runs in the directory of the output and names
        # the objects, the PDB and the output relative to it, as antic
        # links a Windows program. lld-link records each of them and the
        # whole command line in the PDB, which the symbols archive ships.
        get_filename_component(out_dir "${output}" DIRECTORY ABSOLUTE)
        get_filename_component(out_name "${output}" NAME)
        set(relative "")
        foreach(object IN LISTS objects)
            get_filename_component(object "${object}" ABSOLUTE)
            file(RELATIVE_PATH path "${out_dir}" "${object}")
            list(APPEND relative "${path}")
        endforeach()
        get_filename_component(pdb "${SYMBOLS}/${host}/${program}.pdb" ABSOLUTE)
        file(RELATIVE_PATH pdb "${out_dir}" "${pdb}")
        # DESIGN: antic and anti are Windows programs of ours, and link
        # the runtime of Anti at the lowest level of the host for the
        # static part of the C runtime it holds, the entry point among
        # it, as every program of a Windows target does. The level is the
        # lowest, since the two run on every machine of the host.
        lowest_level(level "${host}")
        file(RELATIVE_PATH runtime_library "${out_dir}"
             "${RUNTIME}/lib/${host}/${level}/anti_rt.lib")
        execute_process(COMMAND "${LLVM_BIN}/lld-link" ${link} "/PDB:${pdb}"
                                "/OUT:${out_name}" ${relative}
                                "${runtime_library}"
                                ${ANTIC_WINDOWS_LIBRARIES}
                        WORKING_DIRECTORY "${out_dir}" RESULT_VARIABLE failed)
    else()
        execute_process(COMMAND "${CLANG}" ${common} ${link} -o "${output}"
                                ${objects} RESULT_VARIABLE failed)
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
    # The LLVM tools of the host, as the runtime archive holds them. antic
    # finds them beside itself, by the rule of runtime_archive in
    # src/antic/userdirs.c: bin/ of the archive above its own bin/.
    file(COPY "${tools_of_${host}}/" DESTINATION "${tree}/bin")
    file(WRITE "${tree}/VERSION" "${version}\n")

    # DESIGN: a release build links the runtime as bitcode through full
    # LTO by default, which Eddie decided on 2026-10-07, so bitcode/full/
    # beside the runtime of each level goes in. The bitcode of ThinLTO,
    # bitcode/thin/, stays out, since no build takes it without being
    # asked. CMakeLists.txt names both directories.
    foreach(target IN LISTS TARGETS)
        file(COPY "${RUNTIME}/lib/${target}" DESTINATION "${tree}/lib"
             REGEX "/bitcode/thin$" EXCLUDE)
    endforeach()
    file(COPY "${RUNTIME}/std" DESTINATION "${tree}")
    # DESIGN: the headers of the native libraries go in as include/ of the
    # runtime archive holds them, one directory per library, which Eddie
    # decided on 2026-10-08 in docs/work-order-distribution.md. They serve
    # anti bind --clang and a C program that links an Anti library and the
    # native library it uses.
    file(COPY "${RUNTIME}/include" DESTINATION "${tree}")
    file(COPY "${RUNTIME}/licenses/" DESTINATION "${tree}/licenses")
    # DESIGN: the two Windows sysroots of mingw-w64 go in as the runtime
    # archive holds them, which Eddie decided on 2026-09-27 under "Binary
    # distribution" in docs/decisions.md, and the step mingw of
    # docs/work-order-distribution.md built.
    foreach(target linux-x86_64 linux-arm64 windows-x86_64 windows-arm64)
        file(COPY "${SYSROOT}/${target}" DESTINATION "${tree}/sysroot")
    endforeach()
    # DESIGN: every package holds the two glibc sysroots with their X11 and
    # OpenGL development files, and the glibc runtime of both Linux
    # targets, so a program of `link linux`, raylib, miniaudio or a plugin
    # host links for Linux from any host. Eddie decided this on 2026-09-27
    # under "Binary distribution" in docs/decisions.md. Both come from the
    # runtime archive, copied as it holds them, as the step glibc of
    # docs/work-order-distribution.md says.
    foreach(target linux-x86_64-glibc linux-arm64-glibc)
        file(COPY "${RUNTIME}/sysroot/${target}" DESTINATION "${tree}/sysroot")
        file(COPY "${RUNTIME}/lib/${target}" DESTINATION "${tree}/lib"
             REGEX "/bitcode/thin$" EXCLUDE)
    endforeach()
    # The stubs and headers of Zig go in, and never the SDK of Apple in sdk/.
    foreach(target macos-arm64 macos-x86_64)
        file(COPY "${SYSROOT}/${target}/usr" "${SYSROOT}/${target}/sdk-version"
             DESTINATION "${tree}/sysroot/${target}")
    endforeach()
    file(COPY "${SYSROOT}/licenses/" DESTINATION "${tree}/licenses")
    # The installer reads package-api and drives get-sysroot.cmake with
    # the pins, until the step installers of docs/work-order-distribution.md
    # takes tools/ out of the package. No pin of the LLVM tools is here,
    # since bin/ carries the tools themselves.
    foreach(name get-sysroot.cmake sysroot-pins zig-stubs-pin cmake-pin
            cmake-version package-api)
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
