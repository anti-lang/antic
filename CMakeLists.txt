cmake_minimum_required(VERSION 3.21)

# The pinned clang compiles antic, the runtime and the libraries, unless
# ANTIC_SYSTEM_COMPILER is on. The file downloads it and the LLVM tools.
include(tools/pinned-compiler.cmake)

# The version of Anti is written in tools/version and nowhere else. The
# packer and the release script read the same file.
file(STRINGS tools/version ANTIC_VERSION_LINE LIMIT_COUNT 1)
string(STRIP "${ANTIC_VERSION_LINE}" ANTIC_VERSION_LINE)

project(antic
    VERSION ${ANTIC_VERSION_LINE}
    DESCRIPTION "Compiler for the Anti language"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# DESIGN: the sysroots and the raylib source are downloads, and every
# build directory of a checkout reads the same copy. A host may keep them
# outside the tree, which the default build records in its cache. A second
# build directory takes the path from there when its own default is not on
# disk, as the one a sanitizer preset makes does. A preset then names no
# path of one machine. Naming one is what broke them on the Linux VM
set(ANTIC_SHARED_LINES "")
if(NOT CMAKE_BINARY_DIR STREQUAL "${CMAKE_SOURCE_DIR}/build"
   AND EXISTS "${CMAKE_SOURCE_DIR}/build/CMakeCache.txt")
    file(STRINGS "${CMAKE_SOURCE_DIR}/build/CMakeCache.txt" ANTIC_SHARED_LINES
         REGEX "^ANTIC_[A-Z_]+:[A-Z]+=")
endif()

# Declare a download path as a cache entry. It keeps a value the command
# line gave. Otherwise it takes the default, or the path the default build
# recorded when that default is not on disk
macro(antic_shared_path name default doc)
    if(NOT DEFINED ${name})
        set(antic_shared_value "${default}")
        if(NOT EXISTS "${antic_shared_value}")
            foreach(antic_shared_line IN LISTS ANTIC_SHARED_LINES)
                # CMake expands the condition before it evaluates it, so
                # CMAKE_MATCH_1 of this line is read inside the branch and
                # never beside the MATCHES that sets it.
                if(antic_shared_line MATCHES "^${name}:[A-Z]+=(.+)$")
                    set(antic_shared_found "${CMAKE_MATCH_1}")
                    if(EXISTS "${antic_shared_found}")
                        set(antic_shared_value "${antic_shared_found}")
                    endif()
                endif()
            endforeach()
        endif()
        set(${name} "${antic_shared_value}" CACHE PATH "${doc}")
    endif()
endmacro()

# One warning set for clang, gcc and MSVC. Warnings are errors so that
# the code in the book compiles without noise on every platform.
# DESIGN: the C runtime of Microsoft deprecates fopen, getenv and the other
# functions that C11 defines, for MSVC and for clang with its headers alike,
# and the warnings are errors here.
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wconversion -Wstrict-prototypes)
    # DESIGN: debug information and __FILE__ carry the absolute path of
    # this checkout into every file a user receives. The map replaces it
    # with a dot, so a program that links anti_rt names no directory of
    # the machine that built it. The test no_paths checks the result.
    add_compile_options("-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
                        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
    # CodeView keeps two paths that the map does not reach. S_OBJNAME names
    # the object, which clang makes absolute against the directory of the
    # compile, and LF_BUILDINFO holds the command line. A compile for
    # Windows keeps the object relative and records no command line, so a
    # PDB that links anti_rt names no directory of this checkout either.
    if(WIN32)
        add_compile_options(-fdebug-compilation-dir=. -gno-codeview-command-line)
    endif()
endif()

# The host target name, in the form of the runtime archive directories
# lib/<os>-<cpu>. The file src/target.c detects the same name from the
# compiler's predefined macros. The antic_host_target test compares both.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ANTIC_HOST_OS macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ANTIC_HOST_OS linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ANTIC_HOST_OS windows)
else()
    message(FATAL_ERROR "antic does not support the host system ${CMAKE_SYSTEM_NAME}")
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(ANTIC_HOST_CPU arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(ANTIC_HOST_CPU x86_64)
else()
    message(FATAL_ERROR "antic does not support the host processor ${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(ANTIC_HOST_TARGET "${ANTIC_HOST_OS}-${ANTIC_HOST_CPU}")

# The compiler's code is a library so that the unit tests link the same
# objects as the antic executable.
add_library(antic_core STATIC
    src/antl.c
    src/applesdk.c
    src/arith.c
    src/arm64.c
    src/coff.c
    src/arena.c
    src/ast_dump.c
    src/cpu.c
    src/debug.c
    src/diagnostic.c
    src/driver.c
    src/emit.c
    src/expand.c
    src/header.c
    src/ir.c
    src/ir_print.c
    src/ir_verify.c
    src/layout.c
    src/lexer.c
    src/linker.c
    src/lower.c
    src/mach.c
    src/modpath.c
    src/notice.c
    src/optimize.c
    src/parser.c
    src/process.c
    src/regalloc.c
    src/select.c
    src/selfpath.c
    src/sha256.c
    src/userdirs.c
    src/sema.c
    src/target.c
    src/text.c
    src/types.c
    src/whole.c
    src/x86_64.c)
target_include_directories(antic_core PUBLIC src)

add_executable(antic
    src/main.c)
target_link_libraries(antic PRIVATE antic_core)

# DESIGN: anti is the user-facing command. docs/tooling.md places it in
# tools/anti/, linked against antic_core. Its commands are sdk export and
# sdk import, which move the stubs of Apple's SDK from a Mac to a host
# that links a program naming a framework, and test, which runs the
# `tests` and `fixtures` blocks of a module. It reads the `[inject]`
# table of anti.toml with rt/toml.c, the one parser that anti.toml, the
# logger and the runtime configuration share.
add_executable(anti
    tools/anti/main.c
    tools/anti/build.c
    tools/anti/check.c
    tools/anti/deps.c
    tools/anti/doc.c
    tools/anti/files.c
    tools/anti/fmt.c
    tools/anti/manifest.c
    tools/anti/repo.c
    tools/anti/sdk.c
    tools/anti/symmap.c
    tools/anti/test.c
    tools/anti/units.c
    tools/anti/zip.c
    rt/symbols.c
    rt/toml.c)
target_link_libraries(anti PRIVATE antic_core)
target_include_directories(anti PRIVATE rt)

target_compile_definitions(antic PRIVATE
    ANTIC_VERSION="${PROJECT_VERSION}")
target_compile_definitions(antic_core PRIVATE
    ANTIC_VERSION="${PROJECT_VERSION}")
# anti prints the same version as antic, which step 10 of a release
# reads from a fresh install. tools/pack-anti.cmake defines it for both.
target_compile_definitions(anti PRIVATE
    ANTIC_VERSION="${PROJECT_VERSION}")

# DESIGN: the levels of tools/cpu-levels are the table src/cpu.c holds, and
# the test cpu_levels_pin compares the two. The build reads the file rather
# than repeating the names, so the runtime archive's native libraries are
# built for the level each target defaults to.
set(ANTIC_CPU_LEVELS "${CMAKE_SOURCE_DIR}/tools/cpu-levels")
file(STRINGS "${ANTIC_CPU_LEVELS}" antic_cpu_lines REGEX "^(level|default) ")
foreach(line IN LISTS antic_cpu_lines)
    string(REPLACE " " ";" fields "${line}")
    list(GET fields 0 kind)
    list(GET fields 1 key)
    if(kind STREQUAL "level")
        list(GET fields 2 arch)
        list(GET fields 3 id)
        list(GET fields 4 march)
        set(ANTIC_CPU_MARCH_${key} "${march}")
        set(ANTIC_CPU_ID_${key} "${id}")
        list(APPEND ANTIC_CPU_LEVELS_${arch} "${key}")
    else()
        list(GET fields 2 ANTIC_CPU_LEVEL_${key})
    endif()
endforeach()

# The default level of a target, and the -march= value of a level.
function(antic_cpu_level out target)
    set(level "${ANTIC_CPU_LEVEL_${target}}")
    if(level STREQUAL "")
        message(FATAL_ERROR "no default processor level for ${target} in ${ANTIC_CPU_LEVELS}")
    endif()
    set(${out} "${level}" PARENT_SCOPE)
endfunction()

function(antic_cpu_march out level)
    if(ANTIC_CPU_MARCH_${level} STREQUAL "")
        message(FATAL_ERROR "no -march= value for level ${level} in ${ANTIC_CPU_LEVELS}")
    endif()
    set(${out} "-march=${ANTIC_CPU_MARCH_${level}}" PARENT_SCOPE)
endfunction()

# DESIGN: the runtime is what every program links, so the archive holds one
# of it per level the target's architecture has. A program built with --cpu
# below the target's default then links a runtime of its own level. The
# levels of a target are those of its architecture, which its name ends in.
function(antic_cpu_levels out target)
    if(target MATCHES "arm64$")
        set(${out} "${ANTIC_CPU_LEVELS_arm64}" PARENT_SCOPE)
    else()
        set(${out} "${ANTIC_CPU_LEVELS_x86_64}" PARENT_SCOPE)
    endif()
endfunction()

# anti_rt for the host, laid out as the runtime archive: lib/<target>/ with
# libanti_rt.a, or anti_rt.lib on Windows, and licenses/ with the licence
# of each component.
set(ANTIC_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime")
# DESIGN: one list of runtime sources. The host library and the cross
# builds both read it, so a new source reaches every target at once.
# license_stub.c is not here, because it sits beside the library.
set(ANTIC_RUNTIME_SOURCES assert atomic backtrace call cast check conf cpu digest
    errno f16 fs hooks init io json license loaded mem object platform plugin reflect
    registry signal start symbols sync text threads time toml trace utf)
set(anti_rt_files "")
foreach(source IN LISTS ANTIC_RUNTIME_SOURCES)
    list(APPEND anti_rt_files "rt/${source}.c")
endforeach()
# DESIGN: a macOS runtime library comes from the cross build below on every
# host, the Mac included, so that one object links to the same bytes on
# every host. A Mac builds no host library of its own.
if(NOT ANTIC_HOST_OS STREQUAL "macos")
    antic_cpu_levels(host_levels "${ANTIC_HOST_TARGET}")
    foreach(level IN LISTS host_levels)
        set(out "${ANTIC_RUNTIME_DIR}/lib/${ANTIC_HOST_TARGET}/${level}")
        # DESIGN: the runtime carries no hidden visibility. A program
        # that hosts a plugin exports its runtime symbols, and a plugin
        # is bound against them at load. A shared library for C keeps
        # the documented surface through the export list of its link.
        add_library(anti_rt_${level} STATIC ${anti_rt_files})
        set_target_properties(anti_rt_${level} PROPERTIES
            OUTPUT_NAME anti_rt
            ARCHIVE_OUTPUT_DIRECTORY "${out}")
        target_compile_definitions(anti_rt_${level} PRIVATE
            ANTI_CPU_LEVEL_ID=${ANTIC_CPU_ID_${level}})
        # DESIGN: sanitizers check antic. Anti programs link the runtime without a
        # sanitizer runtime, so the runtime carries no sanitizer instrumentation,
        # whatever CMAKE_C_FLAGS holds in a sanitizer configuration.
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            antic_cpu_march(host_march "${level}")
            target_compile_options(anti_rt_${level} PRIVATE
                -fno-sanitize=all "${host_march}")
        elseif(MSVC AND level STREQUAL "v3")
            # MSVC names a level with /arch: and has a value for AVX2 and
            # none for the SSE4.2 of v2. A reader's build on Windows then
            # writes the baseline for v1 and v2.
            target_compile_options(anti_rt_${level} PRIVATE /arch:AVX2)
        endif()

        # The licence stub of a bundled archive lies beside the runtime library as
        # an object of its own, so that a program never links it instead of the
        # licence text of rt/license.c.
        add_library(anti_rt_license_stub_${level} OBJECT rt/license_stub.c)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(anti_rt_license_stub_${level} PRIVATE
                -fno-sanitize=all)
        endif()
        add_custom_target(anti_rt_license_stub_copy_${level} ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_OBJECTS:anti_rt_license_stub_${level}>"
                "${out}/anti_rt_license_stub${CMAKE_C_OUTPUT_EXTENSION}"
            DEPENDS anti_rt_license_stub_${level}
            COMMAND_EXPAND_LISTS
            VERBATIM)
    endforeach()
endif()

configure_file(rt/LICENSE "${ANTIC_RUNTIME_DIR}/licenses/anti_rt.txt" COPYONLY)

# The sysroot of each target that lld links against, which
# tools/get-sysroot.cmake installs, goes to sysroot/<target> of the runtime
# archive with the licences of its components.
antic_shared_path(ANTIC_SYSROOT_DIR "${CMAKE_SOURCE_DIR}/build/sysroot"
    "sysroots from tools/get-sysroot.cmake")
# The LLVM tools of this host lie in bin/ of ANTIC_LLVM_DIR, which
# tools/pinned-compiler.cmake names and fills.
find_program(ANTIC_LLVM_AR NAMES llvm-ar
    HINTS "${ANTIC_LLVM_DIR}/bin"
    DOC "llvm-ar from LLVM 23.1.1")
set(ANTIC_TARGETS linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
    windows-x86_64 windows-arm64)
foreach(target IN LISTS ANTIC_TARGETS)
    if(EXISTS "${ANTIC_SYSROOT_DIR}/${target}")
        file(COPY "${ANTIC_SYSROOT_DIR}/${target}"
            DESTINATION "${ANTIC_RUNTIME_DIR}/sysroot")
    endif()
endforeach()
if(EXISTS "${ANTIC_SYSROOT_DIR}/licenses")
    file(COPY "${ANTIC_SYSROOT_DIR}/licenses/" DESTINATION
        "${ANTIC_RUNTIME_DIR}/licenses")
endif()

# The SDK that a cross compile for macOS reads. Apple clang has a default
# and a clang of the LLVM release has none, so xcrun names it.
if(APPLE)
    set(ANTIC_MACOS_SDK "${CMAKE_OSX_SYSROOT}")
    if(ANTIC_MACOS_SDK STREQUAL "")
        execute_process(COMMAND xcrun --show-sdk-path
                        OUTPUT_VARIABLE ANTIC_MACOS_SDK
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()

# DESIGN: clang compiles the runtime for every other target that has a
# sysroot, so that antic links a program for each of them from this host.
# Linux takes the headers of musl, macOS the headers of the Zig release
# that the sysroot holds and Windows the headers that xwin installs.
# DESIGN: one runtime library per target and level, so that a second root
# can hold the same target at a lower level. The archive that ships holds
# the default level of each target, which anti_cross_runtime builds.
function(anti_cross_runtime_at target triple level root)
    set(objects "")
    set(work "${CMAKE_BINARY_DIR}/cross/${target}-${level}")
    # The level of the library, so that its atomics are one instruction
    # wherever the level has them.
    antic_cpu_march(march "${level}")
    set(out "${root}/lib/${target}/${level}")
    if(target MATCHES "^windows")
        set(library "${out}/anti_rt.lib")
    else()
        set(library "${out}/libanti_rt.a")
    endif()
    foreach(source IN LISTS ANTIC_RUNTIME_SOURCES)
        set(object "${work}/${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
                -Wall -Wextra -Wpedantic -Werror
                "${march}" "-DANTI_CPU_LEVEL_ID=${ANTIC_CPU_ID_${level}}"
                "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." ${ARGN}
                -c "${PROJECT_SOURCE_DIR}/rt/${source}.c" -o "${object}"
            DEPENDS "${PROJECT_SOURCE_DIR}/rt/${source}.c"
                "${PROJECT_SOURCE_DIR}/rt/rt.h" "${PROJECT_SOURCE_DIR}/rt/std.h"
                "${PROJECT_SOURCE_DIR}/rt/object.h"
                "${PROJECT_SOURCE_DIR}/rt/atomic.h"
                "${PROJECT_SOURCE_DIR}/rt/cpu_level.h"
                "${PROJECT_SOURCE_DIR}/rt/hooks.h"
                "${PROJECT_SOURCE_DIR}/rt/json.h"
                "${PROJECT_SOURCE_DIR}/rt/plugin.h"
                "${PROJECT_SOURCE_DIR}/rt/reflect.h"
                "${PROJECT_SOURCE_DIR}/rt/registry.h"
                "${PROJECT_SOURCE_DIR}/rt/digest.h"
                "${PROJECT_SOURCE_DIR}/rt/signal.h"
                "${PROJECT_SOURCE_DIR}/rt/symbols.h"
                "${PROJECT_SOURCE_DIR}/rt/toml.h"
                "${PROJECT_SOURCE_DIR}/rt/trace.h"
                "${PROJECT_SOURCE_DIR}/rt/utf.h"
            VERBATIM)
        list(APPEND objects "${object}")
    endforeach()
    if(target MATCHES "^windows")
        set(stub "${out}/anti_rt_license_stub.obj")
    else()
        set(stub "${out}/anti_rt_license_stub.o")
    endif()
    add_custom_command(OUTPUT "${stub}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
        COMMAND "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
            -Wall -Wextra -Wpedantic -Werror
            "${march}" "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." ${ARGN}
            -c "${PROJECT_SOURCE_DIR}/rt/license_stub.c" -o "${stub}"
        DEPENDS "${PROJECT_SOURCE_DIR}/rt/license_stub.c"
            "${PROJECT_SOURCE_DIR}/rt/std.h"
        VERBATIM)
    # The stub stays beside the library, never a member of it.
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" ${objects}
        DEPENDS ${objects}
        VERBATIM)
    add_custom_target(anti_rt_${target}_${level} ALL
        DEPENDS "${library}" "${stub}")
endfunction()

function(anti_cross_runtime target triple)
    antic_cpu_levels(levels "${target}")
    foreach(level IN LISTS levels)
        anti_cross_runtime_at("${target}" "${triple}" "${level}"
            "${ANTIC_RUNTIME_DIR}" ${ARGN})
    endforeach()
endfunction()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND ANTIC_LLVM_AR)
    execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE clang_resource OUTPUT_STRIP_TRAILING_WHITESPACE)
    foreach(target IN LISTS ANTIC_TARGETS)
        set(sysroot "${ANTIC_RUNTIME_DIR}/sysroot/${target}")
        if(NOT EXISTS "${sysroot}" OR
           (target STREQUAL ANTIC_HOST_TARGET AND NOT target MATCHES "^macos-"))
            continue()
        endif()
        if(target STREQUAL "linux-x86_64" OR target STREQUAL "linux-arm64")
            string(REPLACE "linux-arm64" "aarch64" arch "${target}")
            string(REPLACE "linux-x86_64" "x86_64" arch "${arch}")
            anti_cross_runtime(${target} ${arch}-linux-musl -fPIC -nostdinc
                -isystem "${sysroot}/usr/include"
                -isystem "${clang_resource}/include")
        elseif(target STREQUAL "macos-x86_64")
            # The minimum macOS version of the triple is 11.0, the value of
            # MACOS_MIN_MAJOR and MACOS_MIN_MINOR in src/target.h.
            anti_cross_runtime(${target} x86_64-apple-macos11
                -isysroot "${sysroot}")
        elseif(target STREQUAL "macos-arm64")
            anti_cross_runtime(${target} arm64-apple-macos11
                -isysroot "${sysroot}")
        elseif(target MATCHES "^windows")
            string(REPLACE "windows-arm64" "aarch64" arch "${target}")
            string(REPLACE "windows-x86_64" "x86_64" arch "${arch}")
            # DESIGN: the link line takes the Universal CRT of the machine
            # with ucrt.lib, which MSVC spells /MD. The objects of the
            # runtime declare the same model, so that the C library of the
            # image is one library rather than two.
            anti_cross_runtime(${target} ${arch}-pc-windows-msvc
                -fms-runtime-lib=dll
                -isystem "${sysroot}/crt/include"
                -isystem "${sysroot}/sdk/include/ucrt"
                -isystem "${sysroot}/sdk/include/um"
                -isystem "${sysroot}/sdk/include/shared")
        endif()
    endforeach()
endif()

# The standard library in std/, written as library files into std/ of the
# runtime archive, where antic finds the modules under anti. without -I.
set(ANTIC_STD_MODULES lang runtime mem debug io text license error time os fs reflect random collection toml config args json log simd trace plugin)
set(ANTIC_STD_SOURCE_DIR "${PROJECT_SOURCE_DIR}/std/anti")
set(std_files "")
foreach(module IN LISTS ANTIC_STD_MODULES)
    set(output "${ANTIC_RUNTIME_DIR}/std/anti/${module}.antl")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ANTIC_RUNTIME_DIR}/std/anti"
        COMMAND antic -c --anti-internal -I "${PROJECT_SOURCE_DIR}/std"
            -I "${ANTIC_RUNTIME_DIR}/std"
            --package-name anti --package-version "${PROJECT_VERSION}"
            --license 0BSD --license-text "${PROJECT_SOURCE_DIR}/std/LICENSE"
            -o "${output}" "${ANTIC_STD_SOURCE_DIR}/${module}.anti"
        DEPENDS antic "${ANTIC_STD_SOURCE_DIR}/${module}.anti"
            ${std_files}
        VERBATIM)
    list(APPEND std_files "${output}")
endforeach()
add_custom_target(anti_std ALL DEPENDS ${std_files})

enable_testing()
add_subdirectory(tests)
