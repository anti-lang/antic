# Unit tests link the compiler's code directly. Program tests in
# tests/programs compile .anti files with antic and run the result.

add_executable(antic_unit_tests
    unit/test_main.c
    unit/test_target.c
    unit/test_text.c
    unit/test_lexer.c
    unit/test_parser.c
    unit/test_types.c
    unit/test_sema.c
    unit/test_nullable.c
    unit/test_ir.c
    unit/test_layout.c
    unit/test_lower.c
    unit/test_modules.c
    unit/test_header.c
    unit/test_optimize.c
    unit/test_select.c
    unit/test_regalloc.c
    unit/test_x86_64.c
    unit/test_arm64.c
    unit/test_emit.c
    unit/test_link.c
    unit/test_float.c
    unit/test_struct.c
    unit/test_utf.c
    unit/test_whole.c
    ../rt/utf.c)
target_link_libraries(antic_unit_tests PRIVATE antic_core)
target_include_directories(antic_unit_tests PRIVATE ../rt)
target_compile_definitions(antic_unit_tests PRIVATE
    ANTIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME unit COMMAND antic_unit_tests)

# The runtime archive holds the licence text of every component.
add_test(NAME runtime_licenses
    COMMAND "${CMAKE_COMMAND}" -E cat "${ANTIC_RUNTIME_DIR}/licenses/anti_rt.txt")
set_tests_properties(runtime_licenses PROPERTIES
    PASS_REGULAR_EXPRESSION "^BSD Zero Clause License")

# CI runs only by hand: every workflow triggers on workflow_dispatch alone.
add_test(NAME workflows_dispatch_only
    COMMAND "${CMAKE_COMMAND}"
        "-DWORKFLOWS=${PROJECT_SOURCE_DIR}/.github/workflows"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_workflows.cmake")

add_test(NAME antic_version COMMAND antic --version)
set_tests_properties(antic_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^antic [0-9]+\\.[0-9]+\\.[0-9]+")

# The host target name has two spellings: ANTIC_HOST_TARGET here and the
# detection in src/target.c. This test fails when they disagree.
add_test(NAME antic_host_target COMMAND antic --print-host-target)
set_tests_properties(antic_host_target PROPERTIES
    PASS_REGULAR_EXPRESSION "^${ANTIC_HOST_TARGET}\n$")

# DESIGN: a test that compiles C or C++ beside Anti takes the compiler of
# the build, so a build with the pinned clang runs no compiler of the
# machine. The pinned clang reads the SDK of macOS from -isysroot and
# compiles C++ in the driver mode of g++. A reader's build takes c++.
#
# DESIGN: antic never fuses a multiply and an add, so the C that a test
# compiles as the reference of an Anti result fuses none either. Otherwise
# the test compares two arithmetics. clang fuses by default on arm64 Linux.
# MSVC fuses nothing by default and knows no such option.
set(ANTIC_REFERENCE_C_FLAGS "")
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(ANTIC_REFERENCE_C_FLAGS -ffp-contract=off)
endif()
set(ANTIC_TEST_CC "${CMAKE_C_COMPILER}" ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    list(APPEND ANTIC_TEST_CC -isysroot "${ANTIC_MACOS_SDK}")
endif()
if(ANTIC_SYSTEM_COMPILER)
    set(ANTIC_TEST_CXX c++ ${ANTIC_REFERENCE_C_FLAGS})
else()
    set(ANTIC_TEST_CXX ${ANTIC_TEST_CC} --driver-mode=g++)
endif()

# One test per program in tests/programs
find_program(ANTIC_LLVM_MC NAMES llvm-mc
    HINTS "${ANTIC_LLVM_DIR}/bin"
    DOC "llvm-mc from LLVM 23.1.1")

# The runtime archive carries llvm-mc, lld and llvm-ar of LLVM
# tools/llvm-version, which the build copies to runtime/bin from the
# directory that tools/get-llvm.cmake installs.
if(ANTIC_LLVM_MC)
    get_filename_component(ANTIC_LLVM_BIN "${ANTIC_LLVM_MC}" DIRECTORY)
    add_custom_target(llvm_tools ALL
        COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${ANTIC_LLVM_BIN}"
            "-DDEST=${ANTIC_RUNTIME_DIR}/bin"
            -P "${PROJECT_SOURCE_DIR}/tools/check-llvm.cmake")
    add_test(NAME toolchain_pin
        COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${ANTIC_RUNTIME_DIR}/bin"
            -P "${PROJECT_SOURCE_DIR}/tools/check-llvm.cmake")
endif()

file(GLOB program_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/*.anti")
foreach(source IN LISTS program_sources)
    get_filename_component(name "${source}" NAME_WE)
    add_test(NAME program_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()
# The program whose calls release mode makes direct, in dev mode, which
# keeps every call through a table.
add_test(NAME program_devirtualise_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/devirtualise.anti"
        "-DOPTIONS=--dev"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
# The registry of the classes in dev mode, where the module that links
# writes it.
add_test(NAME program_deserialize_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/deserialize.anti"
        "-DOPTIONS=--dev"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
# Ownership without a field list: delete, destroy and dup call the
# teardown and the copy that the compiler writes for each class.
add_test(NAME program_delete_owned_no_reflect
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/delete_owned.anti"
        "-DOPTIONS=--no-reflect"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-no-reflect"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")

# The pinned raylib source, which tools/get-raylib.cmake downloads. The
# directory is named here, before the first test that reads it.
file(STRINGS "${CMAKE_SOURCE_DIR}/tools/raylib-pin" raylib_version
     REGEX "^RAYLIB_VERSION=")
string(REGEX REPLACE "^RAYLIB_VERSION=" "" raylib_version "${raylib_version}")
antic_shared_path(ANTIC_RAYLIB_DIR
    "${CMAKE_BINARY_DIR}/raylib/raylib-${raylib_version}"
    "raylib release source from tools/get-raylib.cmake")

# DESIGN: the output of program_abi_wchar follows the width and signedness
# of wchar_t, never the host. The C compiler that builds wchar.c names the
# type as i32, u32 or u16. The file abi_wchar.<type>.expected holds its
# values, and decisions.md gives the type of each target.
include(CheckCSourceCompiles)
function(abi_wchar_expected result suffix)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    list(JOIN ARGN " " CMAKE_REQUIRED_FLAGS)
    set(CMAKE_REQUIRED_QUIET ON)
    foreach(type i32 u32 u16)
        string(SUBSTRING "${type}" 1 -1 bits)
        set(signed 0)
        if(type MATCHES "^i")
            set(signed 1)
        endif()
        check_c_source_compiles("#include <stddef.h>
_Static_assert(sizeof(wchar_t) * 8 == ${bits} && ((wchar_t)-1 < 0) == ${signed}, \"\");
int abi_wchar_type;" ANTIC_WCHAR_${type}${suffix})
        if(ANTIC_WCHAR_${type}${suffix})
            set(${result} "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_wchar.${type}.expected"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "wchar_t of ${CMAKE_C_COMPILER} is none of i32, u32 and u16")
endfunction()

# The same C files compiled for macos-x86_64, which the ABI programs of
# that target link. The host runs them, so antic meets clang on the System
# V rules of x86_64 as well.
function(abi_x86_64_object name source)
    add_library(${name} OBJECT "${source}")
    target_compile_options(${name} PRIVATE -arch x86_64
        -mmacosx-version-min=11.0 -fno-sanitize=all ${ANTIC_REFERENCE_C_FLAGS})
endfunction()

# A macos-arm64 host runs a macos-x86_64 program as well, so every program
# runs for that target too. The other four targets need their own machine.
if(ANTIC_HOST_TARGET STREQUAL "macos-arm64")
    abi_x86_64_object(abi_structs_x86_64 abi/structs.c)
    abi_x86_64_object(abi_wchar_x86_64 abi/wchar.c)
    abi_wchar_expected(abi_wchar_x86_64_expected _X86_64 -arch x86_64)
    set(abi_structs_x86_64_options "")
    set(abi_wchar_x86_64_options "-DEXPECTED=${abi_wchar_x86_64_expected}")
    foreach(case structs wchar)
        add_test(NAME program_abi_${case}_macos-x86_64
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_${case}.anti"
                "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_${case}_x86_64>,,>"
                "-DOPTIONS=--target,macos-x86_64"
                ${abi_${case}_x86_64_options}
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    endforeach()
    if(EXISTS "${ANTIC_RAYLIB_DIR}/src/raymath.h")
        abi_x86_64_object(abi_raymath_x86_64 abi/raymath.c)
        target_include_directories(abi_raymath_x86_64 PRIVATE
            "${ANTIC_RAYLIB_DIR}/src")
        add_test(NAME program_abi_raymath_macos-x86_64
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.anti"
                "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_raymath_x86_64>,,>"
                "-DOPTIONS=--target,macos-x86_64"
                "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    endif()
    foreach(source IN LISTS program_sources)
        get_filename_component(name "${source}" NAME_WE)
        add_test(NAME program_${name}_macos-x86_64
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOPTIONS=--target,macos-x86_64"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    endforeach()
    set_tests_properties(program_args_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_GREETING=héllo")
    set_tests_properties(program_nullable_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")
endif()

# The environment variable that tests/programs/args.anti looks for.
set_tests_properties(program_args PROPERTIES
    ENVIRONMENT "ANTI_TEST_GREETING=héllo")

# The `?*byte` that tests/programs/nullable.anti reads from getenv.
set_tests_properties(program_nullable PROPERTIES
    ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")

# The modules of the standard library in std/, which the build writes as
# library files into std/ of the runtime archive. Each test program imports
# one of them.
file(GLOB std_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/std/*.anti")
# A program whose output follows the Win32 errors of Windows brings
# NAME.win32.expected for the Windows targets.
foreach(source IN LISTS std_sources)
    get_filename_component(name "${source}" NAME_WE)
    set(expected "")
    set(win32_expected "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.win32.expected")
    if(ANTIC_HOST_TARGET MATCHES "^windows-" AND EXISTS "${win32_expected}")
        set(expected "-DEXPECTED=${win32_expected}")
    endif()
    add_test(NAME std_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            ${expected}
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()

# The file of the logger that tests/std/log.anti writes. The environment
# of the test names it, since the C runtime of Windows has no setenv.
set_tests_properties(std_log PROPERTIES ENVIRONMENT "ANTI_LOGGER=log-test.toml")

# Struct passing checked against C: the functions of tests/abi/structs.c,
# compiled by the C compiler of the build, take and return structs by value.
add_library(abi_structs OBJECT abi/structs.c)
target_compile_options(abi_structs PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_structs PRIVATE -mmacosx-version-min=11.0)
endif()
# Anti programs link these objects without a sanitizer runtime, like the
# runtime library itself.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_structs PRIVATE -fno-sanitize=all)
endif()
add_test(NAME program_abi_structs
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_structs.anti"
        "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_structs>,,>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")

add_library(abi_wchar OBJECT abi/wchar.c)
target_compile_options(abi_wchar PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_wchar PRIVATE -mmacosx-version-min=11.0)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_wchar PRIVATE -fno-sanitize=all)
endif()
abi_wchar_expected(abi_wchar_expected "")
add_test(NAME program_abi_wchar
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_wchar.anti"
        "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_wchar>,,>"
        "-DEXPECTED=${abi_wchar_expected}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")

# A small raylib binding: the raymath functions of the pinned raylib
# release, which tools/get-raylib.cmake downloads. tools/raylib-pin holds
# the version, so the default directory below cannot name another one.
# A user receives these files. None of them may name a directory of the
# machine that built it.
# An ordinary build maps its paths away with -ffile-prefix-map. A
# sanitizer writes the path of the module into its own metadata, which no
# flag maps, so the test runs on a build without one.
find_program(ANTIC_STRINGS strings)
string(FIND "${CMAKE_C_FLAGS}" "-fsanitize" antic_sanitized)
if(ANTIC_STRINGS AND antic_sanitized EQUAL -1)
    add_test(NAME no_paths
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}" "-DSTRINGS=${ANTIC_STRINGS}"
                "-DANTIC=$<TARGET_FILE:antic>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_no_paths.cmake")
endif()

add_test(NAME cmake_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" -DNAME=cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pin.cmake")
add_test(NAME release_pins
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_pins.cmake")
add_test(NAME pinned_compiler
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DCACHE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pinned_compiler.cmake")
add_test(NAME release_key
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_key.cmake")
add_test(NAME sysroot_digest
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sysroot_digest"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_sysroot_digest.cmake")
set_tests_properties(sysroot_digest PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# A Linux program of a release names no glibc symbol above the pinned
# 2.35, which tools/pack-anti.cmake reads before it writes a package
add_test(NAME linux_libc
    COMMAND "${CMAKE_COMMAND}"
        "-DCLANG=${CMAKE_C_COMPILER}"
        "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
        "-DSYSROOT=${ANTIC_SYSROOT_DIR}"
        "-DCHECK=${CMAKE_SOURCE_DIR}/tools/check-libc.cmake"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/linux-libc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_linux_libc.cmake")
set_tests_properties(linux_libc PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")
add_executable(raw_bytes_c raw/raw_bytes.c)
add_test(NAME raw_output
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DC_PROGRAM=$<TARGET_FILE:raw_bytes_c>"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/raw"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_raw_output.cmake")
# The `tests` and `fixtures` blocks, the runner of `anti test` and the
# proof that every other build drops both blocks.
add_test(NAME anti_test
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test.cmake")

add_test(NAME anti_sdk
    COMMAND "${CMAKE_COMMAND}" "-DANTI=$<TARGET_FILE:anti>"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti_sdk"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_sdk.cmake")
add_test(NAME macos_sysroot
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/macos_sysroot"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_macos_sysroot.cmake")
add_test(NAME glibc_sysroot
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/glibc_sysroot"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_glibc_sysroot.cmake")
# The package of this host, packed from the antic of this build. It needs
# the Linux and macOS sysroots that every package carries.
if(EXISTS "${ANTIC_SYSROOT_DIR}/linux-x86_64" AND
   EXISTS "${ANTIC_SYSROOT_DIR}/linux-arm64" AND
   EXISTS "${ANTIC_SYSROOT_DIR}/macos-arm64" AND
   EXISTS "${ANTIC_SYSROOT_DIR}/macos-x86_64" AND
   EXISTS "${ANTIC_SYSROOT_DIR}/licenses")
    add_test(NAME package_keys
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DANTIC=$<TARGET_FILE:antic>" "-DANTI=$<TARGET_FILE:anti>"
                "-DHOST=${ANTIC_HOST_TARGET}"
                "-DSYSROOT=${ANTIC_SYSROOT_DIR}" "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DCLANG=${CMAKE_C_COMPILER}"
                "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/package"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package.cmake")
endif()
add_test(NAME raylib_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_raylib_pin.cmake")
add_test(NAME package_api
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package_api.cmake")
add_test(NAME line_endings
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_line_endings.cmake")
add_test(NAME installer_options
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installer_options.cmake")

if(EXISTS "${ANTIC_RAYLIB_DIR}/src/raymath.h")
    add_library(abi_raymath OBJECT abi/raymath.c)
    target_compile_options(abi_raymath PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
    target_include_directories(abi_raymath PRIVATE "${ANTIC_RAYLIB_DIR}/src")
    if(APPLE)
        target_compile_options(abi_raymath PRIVATE -mmacosx-version-min=11.0)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(abi_raymath PRIVATE -fno-sanitize=all)
    endif()
    # DESIGN: one expected file serves every target. The values that once
    # differed between hosts came from a multiply and an add that clang
    # fused on arm64, and the reference C fuses none now.
    set(raymath_expected
        "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected")
    add_test(NAME program_abi_raymath
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.anti"
            "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_raymath>,,>"
            "-DEXPECTED=${raymath_expected}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
else()
    # Fail rather than skip, so a missing raylib cannot pass unnoticed.
    add_test(NAME program_abi_raymath
        COMMAND "${CMAKE_COMMAND}" -E echo
            "raylib not found in ${ANTIC_RAYLIB_DIR}: run tools/get-raylib.cmake")
    set_tests_properties(program_abi_raymath PROPERTIES
        FAIL_REGULAR_EXPRESSION "raylib not found")
endif()

# Diagnostics. Each test passes when antic's output matches the pattern.
add_test(NAME error_missing_semicolon
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/missing_semicolon.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/missing_semicolon.anti")
set_tests_properties(error_missing_semicolon PROPERTIES
    PASS_REGULAR_EXPRESSION "missing_semicolon.anti:4:1: error: expected `[;]`")
# A ';' in a property value separates list elements, so a pattern writes
# the semicolon as [;].

# The runtime's module path belongs to no compilation, the anti root needs
# --anti-internal for a library, and a library path of one segment warns.
add_test(NAME error_runtime_module
    COMMAND antic -S -I "${CMAKE_CURRENT_SOURCE_DIR}/errors"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/rt.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/anti/rt.anti")
set_tests_properties(error_runtime_module PROPERTIES
    PASS_REGULAR_EXPRESSION "the module path `anti.rt` is the runtime's")

add_test(NAME error_reserved_library
    COMMAND antic -c -I "${CMAKE_CURRENT_SOURCE_DIR}/modules"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_text.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/anti/text.anti")
set_tests_properties(error_reserved_library PROPERTIES
    PASS_REGULAR_EXPRESSION "the module path `anti.text` is reserved for the language's own libraries")

add_test(NAME antl_internal
    COMMAND antic -c --anti-internal -I "${CMAKE_CURRENT_SOURCE_DIR}/modules"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_text.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/anti/text.anti")

# Doc warnings come only with --doc-warnings, which anti check passes.
add_test(NAME warning_note
    COMMAND antic --doc-warnings -S -o "${CMAKE_CURRENT_BINARY_DIR}/note.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
set_tests_properties(warning_note PROPERTIES
    PASS_REGULAR_EXPRESSION "note.anti:2:8: warning: the pub item `warm` has a `//#` note and no `///` comment")
add_test(NAME silent_note
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/note_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
set_tests_properties(silent_note PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

add_test(NAME warning_dropped
    COMMAND antic --doc-warnings -S -o "${CMAKE_CURRENT_BINARY_DIR}/dropped.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
set_tests_properties(warning_dropped PROPERTIES
    PASS_REGULAR_EXPRESSION "dropped.anti:3:2: warning: the `/\\*\\*` comment is dropped, because no item or field follows it")
add_test(NAME silent_dropped
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/dropped_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
set_tests_properties(silent_dropped PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

add_test(NAME warning_one_segment
    COMMAND antic -c -o "${CMAKE_CURRENT_BINARY_DIR}/geometry.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/geometry.anti")
set_tests_properties(warning_one_segment PROPERTIES
    PASS_REGULAR_EXPRESSION "geometry.anti: warning: the module path `geometry` has one segment, which is for a program's own files")

add_test(NAME error_no_main
    COMMAND antic --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/no_main"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/no_main.anti")
set_tests_properties(error_no_main PROPERTIES
    PASS_REGULAR_EXPRESSION "no_main.anti: the program has no function `main`")

# lld links a program for every target on this host. The test of a
# target without a sysroot in the runtime archive is skipped. The second
# program holds an aggregate constant with an address in it, which lands
# in the section each format keeps for data the loader writes.
foreach(case "linux-x86_64|elf64-x86-64" "linux-arm64|elf64-littleaarch64"
        "macos-x86_64|mach-o 64-bit x86-64" "macos-arm64|mach-o arm64"
        "windows-x86_64|coff-x86-64" "windows-arm64|coff-arm64")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 format)
    foreach(program return42 const_data)
        set(suffix "")
        if(NOT program STREQUAL "return42")
            set(suffix "_${program}")
        endif()
        add_test(NAME cross_link_${target}${suffix}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${program}.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cross"
                "-DTARGET=${target}"
                "-DFORMAT=${format}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cross_link.cmake")
        set_tests_properties(cross_link_${target}${suffix} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endforeach()

# antic writes the same assembly for every test program on every host.
add_test(NAME emit_identity
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DPROGRAMS=${CMAKE_CURRENT_SOURCE_DIR}/programs"
        "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/emit-identity/programs.sha256"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/emit-identity"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_emit_identity.cmake")

# One object links to the same bytes for macos-arm64 on every host.
add_test(NAME link_identity_macos-arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/link-identity/return42.macos-arm64.sha256"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/link-identity"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_link_identity.cmake")
set_tests_properties(link_identity_macos-arm64 PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# A program that names a framework links against Apple's SDK.
foreach(target macos-arm64 macos-x86_64)
    add_test(NAME framework_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/framework/core_foundation.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/framework"
            "-DTARGET=${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_framework.cmake")
    set_tests_properties(framework_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# The platform linker of the host links the first executable of chapter 3.
if(NOT WIN32)
    add_test(NAME program_platform_linker
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/platform"
            "-DOPTIONS=--linker,platform"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endif()

# The platform linker links only for the operating system it runs on.
if(ANTIC_HOST_OS STREQUAL "linux")
    set(foreign_target macos-arm64)
else()
    set(foreign_target linux-x86_64)
endif()
add_test(NAME error_foreign_link
    COMMAND antic --target ${foreign_target} --linker platform
            --runtime "${ANTIC_RUNTIME_DIR}" --llvm-mc "${ANTIC_LLVM_MC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/return42"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
set_tests_properties(error_foreign_link PROPERTIES
    PASS_REGULAR_EXPRESSION "linking for ${foreign_target} with the platform linker needs a host with the same operating system")

# The back end folds an array length from size_of and names the target.
add_test(NAME error_array_length
    COMMAND antic -S --target linux-x86_64
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/zero_length.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/zero_length.anti")
set_tests_properties(error_array_length PROPERTIES
    PASS_REGULAR_EXPRESSION "antic: the array length `size_of\\(Pair\\) - 8` is 0 on linux-x86_64, and an array length is at least 1")

add_test(NAME error_missing_runtime
    COMMAND antic -o "${CMAKE_CURRENT_BINARY_DIR}/errors/return42"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
set_tests_properties(error_missing_runtime PROPERTIES
    PASS_REGULAR_EXPRESSION "antic: linking needs --runtime")

# Dumps of compiler stages, compared with expected files in tests/dump.
# scale.tokens is the token listing that chapter 1 prints.
add_test(NAME dump_tokens_scale
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-tokens"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.tokens"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# A doc comment is one token, printed with its first source line.
add_test(NAME dump_tokens_docs
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-tokens"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/docs.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/docs.tokens"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

add_test(NAME error_lexical
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/lexical.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/lexical.anti")
set_tests_properties(error_lexical PROPERTIES
    PASS_REGULAR_EXPRESSION "lexical.anti:3:14: error: unknown escape `\\\\q`\n[^\n]*lexical.anti:4:10: error: a character literal holds one character\n")

# scale.ast is the syntax tree listing that chapter 1 prints.
add_test(NAME dump_ast_scale
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ast"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.ast"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# Two mistakes produce exactly two messages.
add_test(NAME error_syntax
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/syntax.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/syntax.anti")
set_tests_properties(error_syntax PROPERTIES
    PASS_REGULAR_EXPRESSION "^[^\n]*syntax.anti:3:10: error: expected an expression\n[^\n]*syntax.anti:5:2: error: expected `[;]`\n$")

# scale.types is the typed tree listing that chapter 1 prints.
add_test(NAME dump_types_scale
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-types"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.types"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# main.ir is the IR listing that chapter 1 prints for main.anti.
add_test(NAME dump_ir_main
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ir"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.ir"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# Library files. scale.antl.hex lists the bytes that every host writes for
# scale.anti. main.ir is the whole program after loading two libraries,
# which the search root supplies, beside the build directory's copies.
set(MODULES "${CMAKE_CURRENT_SOURCE_DIR}/modules")
set(LIBS "${CMAKE_CURRENT_BINARY_DIR}/modules")
file(MAKE_DIRECTORY "${LIBS}/com/example")

add_test(NAME antl_scale
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DSOURCE=${MODULES}/com/example/scale.anti"
        "-DROOT=${MODULES}"
        "-DOPTIONS=--package-name|com.example|--package-version|1.2.4|--license|MIT|--license-text|${MODULES}/LICENSE.txt|--attribution|Copyright 2026 Example|--dependency|com.example.vec,^1.2,https://anti.example.com/repo"
        "-DOUTPUT=${LIBS}/com/example/scale.antl"
        "-DEXPECTED=${MODULES}/scale.antl.hex"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antl.cmake")
set_tests_properties(antl_scale PROPERTIES FIXTURES_SETUP scale_antl)

# The line form and the block form of the doc comments write one file. The
# two sources put every item on the same line, with blank lines where the
# other form needs more, because a library file records the line of every
# statement.
add_test(NAME antl_docs_lines
    COMMAND antic -c -I "${MODULES}/lines" -o "${LIBS}/lines.antl"
            "${MODULES}/lines/com/example/geo.anti")
set_tests_properties(antl_docs_lines PROPERTIES FIXTURES_SETUP docs_lines)
add_test(NAME antl_docs_blocks
    COMMAND antic -c -I "${MODULES}/blocks" -o "${LIBS}/blocks.antl"
            "${MODULES}/blocks/com/example/geo.anti")
set_tests_properties(antl_docs_blocks PROPERTIES FIXTURES_SETUP docs_blocks)
add_test(NAME antl_docs_equivalent
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${LIBS}/lines.antl"
            "${LIBS}/blocks.antl")
set_tests_properties(antl_docs_equivalent PROPERTIES
    FIXTURES_REQUIRED "docs_lines;docs_blocks")

add_test(NAME antl_twice
    COMMAND antic -c -I "${MODULES}" -I "${LIBS}"
            -o "${LIBS}/com/example/twice.antl"
            "${MODULES}/com/example/twice.anti")
set_tests_properties(antl_twice PROPERTIES
    FIXTURES_REQUIRED scale_antl FIXTURES_SETUP twice_antl)

# A library whose interface holds a class and an enum: the file carries
# the layout, the chain, the table and the descriptor of the class, and
# the public functions of its body.
add_test(NAME classlib
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/classlib"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/classlib"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_classlib.cmake")

# A program of two modules holds one descriptor and one table per class,
# so `is`, `==`, `equals` and `reflect.new` agree whichever module made
# the object, in release mode and in dev mode
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME shared_class_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/shared"
            "-DLIBRARIES=points"
            "-DSTD=text,io,error,reflect"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/shared-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
    # A chain of classes over three modules, each naming its base with
    # the module it comes from
    add_test(NAME inherits_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/chain"
            "-DLIBRARIES=animal,dog"
            "-DSTD=text,io,error"
            "-DPROGRAM=chain"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/chain-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
    # Ownership over two modules: the teardown and the copy of a class
    # call those of the base, the inline field and the elements that
    # another module declares
    add_test(NAME owned_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/owned"
            "-DLIBRARIES=parts"
            "-DSTD="
            "-DPROGRAM=crate"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/owned-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# The internal level: a module of the same package sees an internal item
# of a library, and a module of another package does not.
add_test(NAME internal_package
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/internal"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/internal"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_internal.cmake")

add_test(NAME dump_ir_modules
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-ir,-I,${LIBS},${MODULES}/main.anti"
        "-DEXPECTED=${MODULES}/main.ir"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
set_tests_properties(dump_ir_modules PROPERTIES FIXTURES_REQUIRED twice_antl)

# Dev mode: one object per module, linked into the same program.
if(NOT WIN32)
    add_test(NAME dev_modules
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DMODULES=${MODULES}"
            "-DLIBS=${LIBS}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dev.cmake")
    set_tests_properties(dev_modules PROPERTIES FIXTURES_REQUIRED twice_antl)
endif()

# The singleton check follows a worker into another module, in release
# mode and in the dev compilation of the module that links.
add_test(NAME antl_board
    COMMAND antic -c -I "${MODULES}" -o "${LIBS}/com/example/board.antl"
            "${MODULES}/com/example/board.anti")
set_tests_properties(antl_board PROPERTIES FIXTURES_SETUP board_antl)
foreach(mode release dev)
    set(flag "")
    if(mode STREQUAL "dev")
        set(flag "--dev,")
    endif()
    add_test(NAME error_worker_reach_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
            "-DCOMMAND=${flag}-S,-I,${LIBS},-o,${CMAKE_CURRENT_BINARY_DIR}/worker_reach.s,worker_reach.anti"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/worker_reach.err"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
    set_tests_properties(error_worker_reach_${mode} PROPERTIES
        FIXTURES_REQUIRED board_antl)
endforeach()

# A qualified base that the module does not declare as a public class
add_test(NAME error_inherits_module
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=-S,-I,${LIBS},-o,${CMAKE_CURRENT_BINARY_DIR}/inherits_module.s,inherits_module.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/inherits_module.err"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
set_tests_properties(error_inherits_module PROPERTIES
    FIXTURES_REQUIRED board_antl)

add_test(NAME error_missing_library
    COMMAND antic --dump-ir "${MODULES}/main.anti"
            "${LIBS}/com/example/twice.antl")
set_tests_properties(error_missing_library PROPERTIES
    FIXTURES_REQUIRED twice_antl
    PASS_REGULAR_EXPRESSION "twice.antl needs module `com.example.scale`")

add_test(NAME error_library_cycle
    COMMAND antic -c -I "${MODULES}" -o "${LIBS}/cycle.antl"
            "${MODULES}/com/example/scale.anti"
            "${LIBS}/com/example/twice.antl")
set_tests_properties(error_library_cycle PROPERTIES
    FIXTURES_REQUIRED twice_antl
    PASS_REGULAR_EXPRESSION "twice.antl depends on `com.example.scale`, so the import forms a cycle")

add_test(NAME antl_one
    COMMAND antic -c -I "${MODULES}" -o "${LIBS}/com/example/one.antl"
            "${MODULES}/com/example/one.anti")
set_tests_properties(antl_one PROPERTIES FIXTURES_SETUP one_antl)

# The first library loads, and the second one is stuck on the cycle.
add_test(NAME error_library_cycle_later
    COMMAND antic -c -I "${MODULES}" -o "${LIBS}/cycle.antl"
            "${MODULES}/com/example/scale.anti"
            "${LIBS}/com/example/one.antl" "${LIBS}/com/example/twice.antl")
set_tests_properties(error_library_cycle_later PROPERTIES
    FIXTURES_REQUIRED "one_antl;twice_antl"
    PASS_REGULAR_EXPRESSION "twice.antl depends on `com.example.scale`, so the import forms a cycle")

add_test(NAME error_duplicate_library
    COMMAND antic --dump-ir "${MODULES}/main.anti"
            "${LIBS}/com/example/scale.antl" "${LIBS}/com/example/scale.antl")
set_tests_properties(error_duplicate_library PROPERTIES
    FIXTURES_REQUIRED scale_antl
    PASS_REGULAR_EXPRESSION "scale.antl and [^ ]*scale.antl both hold module `com.example.scale`")

# The five behaviours of `assert`: a release build without it, a forced
# build with it, dev mode keeping it and --no-asserts dropping it.
if(ANTIC_LLVM_MC AND ANTIC_STRINGS)
    add_test(NAME asserts
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/asserts.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asserts"
            "-DSTRINGS=${ANTIC_STRINGS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asserts.cmake")
endif()

# Every dev-mode check: bounds, overflow, a narrowing `as`, division and
# shifts, in a build that keeps them and a build that drops them.
if(ANTIC_LLVM_MC AND ANTIC_STRINGS)
    add_test(NAME checks
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/checks"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/checks"
            "-DSTRINGS=${ANTIC_STRINGS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_checks.cmake")
endif()

# The debug information of -g, read by the debugger of the host: lldb on
# macOS and gdb on Linux. A breakpoint by file and line in an imported
# module and a backtrace of Anti function names.
find_program(ANTIC_LLDB NAMES lldb DOC "the debugger of macOS")
find_program(ANTIC_GDB NAMES gdb DOC "the debugger of Linux")
if(ANTIC_LLVM_MC AND (ANTIC_LLDB OR ANTIC_GDB))
    if(APPLE AND ANTIC_LLDB)
        set(debugger "${ANTIC_LLDB}")
        set(debugger_kind lldb)
    elseif(ANTIC_GDB)
        set(debugger "${ANTIC_GDB}")
        set(debugger_kind gdb)
    else()
        set(debugger "${ANTIC_LLDB}")
        set(debugger_kind lldb)
    endif()
    add_test(NAME debug_info
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/debug"
            "-DDEBUGGER=${debugger}"
            "-DKIND=${debugger_kind}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_debug.cmake")
endif()

# A zero table traps with the name of the class.
add_test(NAME table_unset
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/table.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_table.cmake")

# The ABI probe compares the layouts of C and antic on the host.
if(NOT WIN32)
    add_test(NAME abi_probe
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/abi"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/probe"
            "-DCC=${ANTIC_TEST_CC}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_probe.cmake")
endif()

# A linked program holds the licence notice of its packages.
if(NOT WIN32)
    add_test(NAME program_licenses
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/licenses"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/licenses.notice"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_licenses.cmake")
endif()

# A program that imports a standard library module carries the package of
# the library, which is the reserved root anti.
if(NOT WIN32)
    add_test(NAME program_licenses_std
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/std/io.anti"
            "-DWANTED=ANTI_LICENSES_BEGIN|package anti.rt [0-9.]+ 0BSD|package anti [0-9.]+ 0BSD|package com.example.hello 2.0.0 MIT"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/licenses-std"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_licenses.cmake")
endif()

# Libraries for C, built with antic --lib and used from C programs on the
# host. Windows needs the MSVC toolchain and is left to the CI runners.
find_program(ANTIC_LLVM_AR NAMES llvm-ar
    HINTS "${ANTIC_LLVM_DIR}/bin"
    DOC "llvm-ar from LLVM 23.1.1")
if(NOT WIN32 AND ANTIC_LLVM_AR)
    foreach(case static shared exports two loader header bundle classes)
        add_test(NAME clib_${case}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DLLVM_AR=${ANTIC_LLVM_AR}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/clib"
                "-DDUMP=${CMAKE_CURRENT_SOURCE_DIR}/dump"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/clib"
                "-DCASE=${case}"
                "-DCC=${ANTIC_TEST_CC}"
                "-DCXX=${ANTIC_TEST_CXX}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_clib.cmake")
    endforeach()
endif()

# The Windows unwind data of chapter 16, decoded by llvm-readobj from the
# object that llvm-mc writes.
if(ANTIC_LLVM_MC)
    foreach(target_triple "windows-x86_64,x86_64-pc-windows-msvc"
            "windows-arm64,aarch64-pc-windows-msvc")
        string(REPLACE "," ";" parts "${target_triple}")
        list(GET parts 0 target)
        list(GET parts 1 triple)
        add_test(NAME unwind_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DLLVM_READOBJ=${ANTIC_RUNTIME_DIR}/bin/llvm-readobj"
                "-DTARGET=${target}"
                "-DTRIPLE=${triple}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/unwind.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/unwind"
                "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/unwind.${target}.unwind"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_unwind.cmake")
    endforeach()
endif()

# targets.txt is the target matrix that chapter 11 prints.
add_test(NAME print_targets
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--print-targets"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/targets.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# main.opt is the optimized IR listing that chapter 1 prints.
add_test(NAME dump_opt_main
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-opt"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.opt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The selected machine code of main.anti that chapter 12 prints.
add_test(NAME dump_select_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-select,--target,macos-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.arm64.sel"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
add_test(NAME dump_select_x86_64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-select,--target,linux-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.x86_64.sel"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The allocated machine code of main.anti that chapter 13 prints.
add_test(NAME dump_alloc_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,macos-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.arm64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
add_test(NAME dump_alloc_x86_64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,linux-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/main.x86_64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

add_test(NAME dump_alloc_twice_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,macos-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/twice.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/twice.arm64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
add_test(NAME dump_alloc_cell_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,macos-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/cell.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/cell.arm64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# Spilling: more values live across a call than callee-saved registers.
add_test(NAME dump_alloc_spill_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,macos-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/spill12.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/spill12.arm64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
add_test(NAME dump_alloc_spill_x86_64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-alloc,--target,linux-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/dump/spill7.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/spill7.x86_64.alloc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The back ends of chapters 14 and 15: addressing modes and calls.
foreach(case "sum,linux-x86_64,sum.x86_64" "args,linux-x86_64,args.linux-x86_64"
        "args,windows-x86_64,args.windows-x86_64" "x86,linux-x86_64,x86.x86_64"
        "sum,macos-arm64,sum.arm64" "args,macos-arm64,args.macos-arm64"
        "args,linux-arm64,args.linux-arm64" "x86,linux-arm64,x86.arm64"
        "arm64,macos-arm64,arm64.macos-arm64" "arm64,linux-arm64,arm64.linux-arm64"
        "floatcalls,linux-x86_64,floatcalls.linux-x86_64"
        "floatcalls,windows-x86_64,floatcalls.windows-x86_64"
        "floatcalls,macos-arm64,floatcalls.macos-arm64"
        "floatcalls,windows-arm64,floatcalls.windows-arm64"
        "floatcalls,linux-arm64,floatcalls.linux-arm64"
        "floats,linux-x86_64,floats.linux-x86_64"
        "floats,macos-arm64,floats.macos-arm64"
        "structs,linux-x86_64,structs.linux-x86_64"
        "structs,windows-x86_64,structs.windows-x86_64"
        "fnptr,linux-x86_64,fnptr.linux-x86_64"
        "fnptr,linux-arm64,fnptr.linux-arm64")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 source)
    list(GET parts 1 target)
    list(GET parts 2 expected)
    add_test(NAME dump_alloc_${expected}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=--dump-alloc,--target,${target},${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.anti"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/${expected}.alloc"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()

# The three ARM64 conventions pass these structs alike. The Windows frame
# adds its unwind directives, so it has a file of its own.
foreach(target macos-arm64 linux-arm64 windows-arm64)
    set(expected structs.arm64.alloc)
    if(target STREQUAL "windows-arm64")
        set(expected structs.windows-arm64.alloc)
    endif()
    add_test(NAME dump_alloc_structs.${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=--dump-alloc,--target,${target},${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.anti"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()

# Listings of the chapters. The error listings name the file alone, so
# each runs in the directory of its program.
set(LISTINGS "${CMAKE_CURRENT_BINARY_DIR}/listings")
file(MAKE_DIRECTORY "${LISTINGS}")
foreach(case "layout|-S,--target,linux-x86_64,-o,${LISTINGS}/layout.s,layout.anti"
        "sizeof|-S,-o,${LISTINGS}/sizeof.s,sizeof.anti"
        "modifiers|-S,-o,${LISTINGS}/modifiers.s,modifiers.anti"
        "geo|-S,-o,${LISTINGS}/geo.s,geo.anti"
        "tail|-S,--target,windows-x86_64,-o,${LISTINGS}/tail.s,tail.anti"
        "loops|-S,-o,${LISTINGS}/loops.s,loops.anti"
        "loop_name|-S,-o,${LISTINGS}/loop_name.s,loop_name.anti"
        "visibility|-S,-o,${LISTINGS}/visibility.s,visibility.anti"
        "errors|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/errors.s,errors.anti"
        "workers|-S,-o,${LISTINGS}/workers.s,workers.anti"
        "own_copy|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/own_copy.s,own_copy.anti"
        "inline_required|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/inline_required.s,inline_required.anti"
        "construct_sets|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/construct_sets.s,construct_sets.anti")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 arguments)
    add_test(NAME listing_error_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
            "-DCOMMAND=${arguments}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/${name}.err"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
endforeach()
set(DUMP "${CMAKE_CURRENT_SOURCE_DIR}/dump")
foreach(case "symbolic.types|--dump-types,${DUMP}/symbolic.anti"
        "header/main.ir|--dump-ir,${DUMP}/header/main.anti"
        "size_of/main.ir|--dump-ir,${DUMP}/size_of/main.anti"
        "size_of/main.linux-arm64.sel|--dump-select,--target,linux-arm64,${DUMP}/size_of/main.anti"
        "c_long/main.linux-x86_64.sel|--dump-select,--target,linux-x86_64,${DUMP}/c_long/main.anti"
        "c_long/main.windows-x86_64.sel|--dump-select,--target,windows-x86_64,${DUMP}/c_long/main.anti"
        "devirt.ir|--dump-ir,${DUMP}/devirt.anti"
        "devirt.dev.ir|--dev,--dump-ir,${DUMP}/devirt.anti"
        "devirt.opt|--dump-opt,${DUMP}/devirt.anti"
        "devirt.dev.opt|--dev,--dump-opt,${DUMP}/devirt.anti"
        "forward.ir|--dump-ir,${DUMP}/forward.anti"
        "forward.opt|--dump-opt,${DUMP}/forward.anti"
        "probe.ir|--dump-ir,${CMAKE_CURRENT_SOURCE_DIR}/abi/probe.anti"
        "sizes.ir|--dump-ir,${CMAKE_CURRENT_SOURCE_DIR}/programs/sizes.anti"
        "spillweight.linux-x86_64.alloc|--dump-alloc,--target,linux-x86_64,${DUMP}/spillweight.anti"
        "sizes.linux-x86_64.alloc|--dump-alloc,--target,linux-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/programs/sizes.anti"
        "bitfields.ir|--dump-ir,${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "bitfields.linux-x86_64.alloc|--dump-alloc,--target,linux-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "bitfields.windows-x86_64.alloc|--dump-alloc,--target,windows-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "measure.opt|--dump-opt,${DUMP}/measure.anti"
        "measure.linux-arm64.alloc|--dump-alloc,--target,linux-arm64,${DUMP}/measure.anti")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 expected)
    list(GET parts 1 arguments)
    add_test(NAME listing_${expected}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=${arguments}"
            "-DEXPECTED=${DUMP}/${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()
# Assembly files of dev mode and of a shared library for C.
foreach(case "scale.dev.macos-arm64.s|--dev,-S,--target,macos-arm64,-I,${MODULES},MODULE_SCALE"
        "scale.dev.windows-x86_64.s|--dev,-S,--target,windows-x86_64,-I,${MODULES},MODULE_SCALE"
        "main.dev.linux-x86_64.s|--dev,-S,--target,linux-x86_64,-I,${LIBS},${MODULES}/main.anti"
        "doubling.linux-x86_64.s|--lib,shared,-S,--target,linux-x86_64,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,CLIB_DOUBLING"
        "doubling.macos-arm64.s|--lib,shared,-S,--target,macos-arm64,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,CLIB_DOUBLING"
        "doubling.windows-arm64.s|--lib,shared,-S,--target,windows-arm64,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,CLIB_DOUBLING")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 expected)
    list(GET parts 1 arguments)
    string(REPLACE "MODULE_SCALE" "${MODULES}/com/example/scale.anti"
           arguments "${arguments}")
    string(REPLACE "CLIB_DOUBLING"
           "${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
           arguments "${arguments}")
    add_test(NAME listing_${expected}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=${arguments},-o,${LISTINGS}/${expected}"
            "-DOUTPUT=${LISTINGS}/${expected}"
            "-DEXPECTED=${DUMP}/${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()
set_tests_properties(listing_main.dev.linux-x86_64.s PROPERTIES
    FIXTURES_REQUIRED twice_antl)

# Calls through function pointers in the IR of chapter 20.
add_test(NAME dump_opt_fnptr
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-opt,${CMAKE_CURRENT_SOURCE_DIR}/dump/fnptr.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/fnptr.opt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# String literals as globals in the IR of chapter 19.
add_test(NAME dump_opt_strings
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-opt,${CMAKE_CURRENT_SOURCE_DIR}/dump/strings.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/strings.opt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# Aggregates in the IR of chapter 18.
add_test(NAME dump_opt_structs
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-opt,${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.opt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The parameter extensions of chapter 15 in the IR.
add_test(NAME dump_opt_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-opt,${CMAKE_CURRENT_SOURCE_DIR}/dump/arm64.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/arm64.opt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The assembly files of chapters 16, 19 and 20 for each object format.
foreach(case "main,macos-arm64" "main,macos-x86_64" "main,linux-x86_64"
        "main,linux-arm64" "main,windows-x86_64" "main,windows-arm64"
        "letters,macos-arm64" "letters,linux-x86_64"
        "strings,linux-x86_64" "strings,macos-arm64" "strings,windows-x86_64"
        "fnptr,macos-arm64")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 source)
    list(GET parts 1 target)
    if(source STREQUAL "letters")
        set(path "${CMAKE_CURRENT_SOURCE_DIR}/programs/${source}.anti")
    else()
        set(path "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.anti")
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/emit/${source}.${target}.s")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/emit")
    add_test(NAME emit_${source}.${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=-S,--target,${target},-o,${output},${path}"
            "-DOUTPUT=${output}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.${target}.s"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()

# Every instruction that antic writes assembles with llvm-mc.
foreach(target_triple "linux-x86_64,x86_64-unknown-linux-gnu"
        "windows-x86_64,x86_64-pc-windows-msvc"
        "macos-x86_64,x86_64-apple-macos" "linux-arm64,aarch64-unknown-linux-gnu"
        "macos-arm64,arm64-apple-macos"
        "windows-arm64,aarch64-pc-windows-msvc")
    string(REPLACE "," ";" parts "${target_triple}")
    list(GET parts 0 target)
    list(GET parts 1 triple)
    foreach(source main loop twice cell spill7 sum args x86 arm64 floatcalls
            floats structs strings fnptr)
        add_test(NAME asm_${source}_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DTARGET=${target}"
                "-DTRIPLE=${triple}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    endforeach()
    # Sizes, C types, unions, bitfields, packed and aligned structs and
    # export, from the program and ABI tests.
    foreach(source programs/sizes programs/c_types programs/bitfields
            abi/abi_structs abi/probe)
        get_filename_component(name "${source}" NAME)
        add_test(NAME asm_${name}_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DTARGET=${target}"
                "-DTRIPLE=${triple}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    endforeach()
    # The directives of the debug information, for the DWARF of ELF and
    # Mach-O and the CodeView of COFF.
    add_test(NAME asm_debug_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DTARGET=${target}"
            "-DTRIPLE=${triple}"
            "-DOPTIONS=-g"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-debug"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    # A shared library for C, with the constructor section of the target.
    add_test(NAME asm_clib_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DTARGET=${target}"
            "-DTRIPLE=${triple}"
            "-DOPTIONS=--lib|shared|-I|${CMAKE_CURRENT_SOURCE_DIR}/clib"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/geo.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
endforeach()

add_test(NAME dump_select_loop_arm64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-select,--target,linux-arm64,${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.arm64.sel"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
add_test(NAME dump_select_loop_x86_64
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--dump-select,--target,macos-x86_64,${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.x86_64.sel"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# The optimizer examples of chapter 10. Each NAME.anti in tests/opt has its
# IR before optimization in NAME.ir and after optimization in NAME.opt.
file(GLOB opt_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/opt/*.anti")
foreach(source ${opt_sources})
    get_filename_component(name "${source}" NAME_WE)
    get_filename_component(dir "${source}" DIRECTORY)
    add_test(NAME ir_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DOPTION=--dump-ir"
            "-DSOURCE=${source}"
            "-DEXPECTED=${dir}/${name}.ir"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
    add_test(NAME opt_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DOPTION=--dump-opt"
            "-DSOURCE=${source}"
            "-DEXPECTED=${dir}/${name}.opt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endforeach()

# Semantic analysis reports every error it finds.
add_test(NAME error_types
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/types.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/types.anti")
set_tests_properties(error_types PROPERTIES
    PASS_REGULAR_EXPRESSION "^[^\n]*types.anti:3:19: error: expected `bool`, found an integer literal\n[^\n]*types.anti:4:9: error: expected `int`, found `bool`\n$")

