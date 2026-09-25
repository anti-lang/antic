# Unit tests link the compiler's code directly. Program tests in
# tests/programs compile .anti files with antic and run the result.

# llvm-mc of the pinned release, which many tests pass to antic. The
# search stands above the first of them: a fresh build directory has no
# cache entry yet, and a value read before find_program writes it is
# empty. cpu_check_refuses then ran antic with no assembler, and the
# dry run of the release script found it in an export of the tree.
find_program(ANTIC_LLVM_MC NAMES llvm-mc
    HINTS "${ANTIC_LLVM_DIR}/bin"
    DOC "llvm-mc from LLVM 23.1.1")

add_executable(antic_unit_tests
    unit/test_main.c
    unit/test_target.c
    unit/test_cpu.c
    unit/test_text.c
    unit/test_lexer.c
    unit/test_parser.c
    unit/test_types.c
    unit/test_sema.c
    unit/test_nullable.c
    unit/test_variant.c
    unit/test_sync.c
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
    unit/test_userdirs.c
    unit/test_float.c
    unit/test_struct.c
    unit/test_utf.c
    unit/test_whole.c
    unit/test_sha256.c
    unit/test_arith.c
    unit/test_f16.c
    unit/test_symbols.c
    unit/test_float_read.c
    unit/test_coff.c
    unit/test_toml.c
    unit/test_rt_bounds.c
    unit/test_fs.c
    unit/test_json.c
    unit/test_bind.c
    unit/test_deps.c
    unit/test_zip.c
    unit/test_syms.c
    unit/test_files.c
    ../src/anti/bindexpr.c
    ../src/anti/bindtype.c
    ../src/anti/deps.c
    ../src/anti/files.c
    ../src/anti/jsontree.c
    ../src/anti/manifest.c
    ../src/anti/repo.c
    ../src/anti/symmap.c
    ../src/anti/syms.c
    ../src/anti/zip.c
    ../src/rt/assert.c
    ../src/rt/atomic.c
    ../src/rt/cpu.c
    ../src/rt/f16.c
    ../src/rt/fs.c
    ../src/rt/json.c
    ../src/rt/symbols.c
    ../src/rt/text.c
    ../src/rt/toml.c)
target_link_libraries(antic_unit_tests PRIVATE antic_core)
# test_f16.c calls nextafterf, which glibc keeps in libm. libSystem and the
# CRT of Windows hold it themselves.
if(UNIX AND NOT APPLE)
    target_link_libraries(antic_unit_tests PRIVATE m)
endif()
target_include_directories(antic_unit_tests PRIVATE ../src/rt ../src/anti)
# ANTI_DEV_CPU lets test_cpu.c stand a level in for the processor, so the
# refusal of a lower machine is checked on this one. The runtime of the
# archive is compiled without it. ANTI_CPU_LEVEL_ID is the level of a
# runtime, which every build of src/rt/cpu.c names.
antic_cpu_level(unit_level "${ANTIC_HOST_TARGET}")
target_compile_definitions(antic_unit_tests PRIVATE
    ANTIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}" ANTI_DEV_CPU
    ANTI_CPU_LEVEL_ID=${ANTIC_CPU_ID_${unit_level}})
add_test(NAME unit COMMAND antic_unit_tests)

# The signal table of the runtime, in a program of its own. src/rt/signal.h
# shares its name with the header of C, so the include path of the unit
# tests would hide the one of C from src/rt/signal.c. The test starts
# threads, and so does the reader of the runtime.
find_package(Threads REQUIRED)
add_executable(rt_signal_tests unit/test_signal.c ../src/rt/signal.c
    ../src/rt/atomic.c)
target_link_libraries(rt_signal_tests PRIVATE Threads::Threads)
add_test(NAME rt_signal COMMAND rt_signal_tests)

# rt.configure while another thread reads the keys, in a program of its own
# with stand-ins for the loader. The runtime of an Anti program carries no
# sanitizer, and this program does in the sanitizer builds.
add_executable(rt_conf_tests unit/test_conf_race.c ../src/rt/conf.c
    ../src/rt/assert.c ../src/rt/toml.c ../src/rt/atomic.c ../src/rt/fs.c
    ../src/rt/hash.c ../src/rt/utf.c ../src/rt/platform_posix.c
    ../src/rt/platform_windows.c)
target_link_libraries(rt_conf_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_conf_tests PRIVATE
    ANTIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME rt_conf_race COMMAND rt_conf_tests)

# The loader of the runtime under threads that load at once, in a program
# of its own with stand-ins for the rest of the runtime. It loads two
# libraries that each carry an empty table of a plugin.
foreach(which one two)
    add_library(plugin_table_${which} SHARED unit/plugin_table.c)
endforeach()
add_executable(rt_plugin_tests unit/test_plugin_race.c ../src/rt/plugin.c
    ../src/rt/assert.c ../src/rt/loaded.c ../src/rt/atomic.c
    ../src/rt/digest.c ../src/rt/text.c ../src/rt/toml.c ../src/rt/utf.c
    ../src/rt/platform_posix.c ../src/rt/platform_windows.c)
target_link_libraries(rt_plugin_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_plugin_tests PRIVATE
    "PLUGIN_ONE=\"$<TARGET_FILE:plugin_table_one>\""
    "PLUGIN_TWO=\"$<TARGET_FILE:plugin_table_two>\"")
add_dependencies(rt_plugin_tests plugin_table_one plugin_table_two)
add_test(NAME rt_plugin_race COMMAND rt_plugin_tests)

# The platform layer of the runtime on the host. Both platform files go
# in, and the one of the other system compiles to nothing. The library of
# an empty plugin table is copied into a directory whose name is outside
# ASCII, which a loader that takes the ANSI code page cannot open.
add_executable(rt_platform_tests unit/test_platform.c
    ../src/rt/platform_posix.c ../src/rt/platform_windows.c)
target_link_libraries(rt_platform_tests PRIVATE ${CMAKE_DL_LIBS})
set(platform_plugin "${CMAKE_CURRENT_BINARY_DIR}/platform-ü")
add_custom_command(TARGET rt_platform_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${platform_plugin}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:plugin_table_one>"
        "${platform_plugin}/$<TARGET_FILE_NAME:plugin_table_one>"
    VERBATIM)
target_compile_definitions(rt_platform_tests PRIVATE
    "PLUGIN_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\""
    "PLUGIN_NAME=\"$<TARGET_FILE_NAME:plugin_table_one>\"")
add_dependencies(rt_platform_tests plugin_table_one)
add_test(NAME rt_platform COMMAND rt_platform_tests)

# The loader against a table of a plugin with one value damaged at a time.
add_library(plugin_bad SHARED unit/plugin_bad.c)
add_executable(rt_plugin_table_tests unit/test_plugin_table.c
    ../src/rt/plugin.c ../src/rt/assert.c ../src/rt/loaded.c
    ../src/rt/atomic.c ../src/rt/digest.c ../src/rt/text.c ../src/rt/toml.c
    ../src/rt/utf.c ../src/rt/platform_posix.c ../src/rt/platform_windows.c)
target_link_libraries(rt_plugin_table_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_plugin_table_tests PRIVATE
    "PLUGIN_BAD=\"$<TARGET_FILE:plugin_bad>\"")
add_dependencies(rt_plugin_table_tests plugin_bad)
add_test(NAME rt_plugin_table COMMAND rt_plugin_table_tests)

# The runtime archive holds the licence text of every component.
add_test(NAME runtime_licenses
    COMMAND "${CMAKE_COMMAND}" -E cat "${ANTIC_RUNTIME_DIR}/licenses/anti_rt.txt")
set_tests_properties(runtime_licenses PROPERTIES
    PASS_REGULAR_EXPRESSION "^BSD Zero Clause License")

# The processor levels. src/antic/cpu.c holds the table for antic and
# tools/cpu-levels holds it for the build, so the two are compared.
add_test(NAME cpu_levels_pin
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLEVELS=${PROJECT_SOURCE_DIR}/tools/cpu-levels"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_pin.cmake")

# The archive holds a runtime per level, so that a program built with --cpu
# below its target's default links a runtime of that level.
add_test(NAME cpu_archive_levels
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLEVELS=${PROJECT_SOURCE_DIR}/tools/cpu-levels"
        "-DTARGETS=linux-x86_64|linux-arm64|macos-x86_64|macos-arm64|windows-x86_64|windows-arm64"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_archive.cmake")
set_tests_properties(cpu_archive_levels PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# One test per x86_64 level: v3 writes the VEX forms of AVX and the two
# levels below it write the SSE forms. v2 adds no instruction that antic
# writes today, so it differs from v1 in the assembler and the runtime
# alone.
set(cpu_sse "    addsd|    addss|    mulss|    ucomisd|    cvtss2sd|    movsd")
set(cpu_avx "    vaddsd|    vaddss|    vmulss|    vucomisd|    vcvtss2sd|    vmovaps")
foreach(case "v1,${cpu_sse},${cpu_avx}" "v2,${cpu_sse},${cpu_avx}"
        "v3,${cpu_avx},${cpu_sse}")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 level)
    list(GET parts 1 uses)
    list(GET parts 2 avoids)
    add_test(NAME cpu_level_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DTARGET=linux-x86_64"
            "-DLEVEL=${level}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/floats.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cpu-level"
            "-DUSES=${uses}"
            "-DAVOIDS=${avoids}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_level.cmake")
endforeach()

# One test per ARM64 level, through the target that defaults to it. The
# level's instructions are the atomics: one instruction at armv8.2 and
# above, a load-store exclusive loop below.
set(cpu_lse "casal|ldaddal|swpal")
set(cpu_exclusive "ldaxr|stlxr")
foreach(case "linux-arm64,armv8.0,${cpu_exclusive},${cpu_lse}"
        "windows-arm64,armv8.2,${cpu_lse},${cpu_exclusive}"
        "macos-arm64,armv8.5,${cpu_lse},${cpu_exclusive}")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 uses)
    list(GET parts 3 avoids)
    if(target STREQUAL "windows-arm64")
        set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}/anti_rt.lib")
    else()
        set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}/libanti_rt.a")
    endif()
    # DESIGN: the library of the host target is the one this build
    # compiled, at the optimisation of this build. A Debug build writes a
    # call to the runtime's own helper where a release writes the
    # exclusive loop, so the mnemonics of a level are not in it. The
    # check is of what the archive ships, which is a cross build, and it
    # skips the host's own library. The Mac builds no host library, so
    # all three targets are cross builds there.
    set(cpu_host_build no)
    if(target STREQUAL "${ANTIC_HOST_TARGET}"
       AND NOT ANTIC_HOST_OS STREQUAL "macos")
        set(cpu_host_build yes)
    endif()
    add_test(NAME cpu_level_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DOBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
            "-DLIBRARY=${library}"
            "-DTARGET=${target}"
            "-DLEVEL=${level}"
            "-DHOST_BUILD=${cpu_host_build}"
            "-DUSES=${uses}"
            "-DAVOIDS=${avoids}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_runtime.cmake")
    set_tests_properties(cpu_level_${level} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# An atomic load of the runtime is sequentially consistent on every ARM64
# target and level, as src/rt/atomic.c promises. Windows compiled it through
# `volatile` once, which is a plain load there.
foreach(target linux-arm64 macos-arm64 windows-arm64)
    foreach(level armv8.0 armv8.2 armv8.5)
        if(target STREQUAL "windows-arm64")
            set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}/anti_rt.lib")
        else()
            set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}/libanti_rt.a")
        endif()
        add_test(NAME atomic_load_${target}_${level}
            COMMAND "${CMAKE_COMMAND}"
                "-DOBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
                "-DLIBRARY=${library}"
                "-DTARGET=${target}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_atomic_load.cmake")
        set_tests_properties(atomic_load_${target}_${level} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endforeach()

# The two conversions of f16. ARM64 and x86-64-v3 have one instruction
# for each, and v1 and v2, which lack F16C, call the runtime.
set(f16_call "anti_rt_f16_to_f32|anti_rt_f32_to_f16")
set(f16_f16c "    vcvtph2ps|    vcvtps2ph")
set(f16_fcvt "    fcvt s|    fcvt h")
foreach(case "linux-x86_64,v1,${f16_call},cvtph2ps|cvtps2ph"
        "linux-x86_64,v2,${f16_call},cvtph2ps|cvtps2ph"
        "linux-x86_64,v3,${f16_f16c},${f16_call}"
        "windows-x86_64,v1,${f16_call},cvtph2ps|cvtps2ph"
        "windows-x86_64,v3,${f16_f16c},${f16_call}"
        "linux-arm64,armv8.0,${f16_fcvt},${f16_call}"
        "macos-arm64,armv8.5,${f16_fcvt},${f16_call}")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 uses)
    list(GET parts 3 avoids)
    add_test(NAME f16_${target}_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DTARGET=${target}"
            "-DLEVEL=${level}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/half.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/f16"
            "-DUSES=${uses}"
            "-DAVOIDS=${avoids}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_level.cmake")
endforeach()

# A simd struct above the vector cap is an array and every operation on
# it a loop, which a warning at the declaration says. The program allows
# it, because a release build accepts no warning.
add_test(NAME program_simd_big
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/simd_big.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")

# The native width of a simd operation: one instruction on the registers
# of 32 bytes at x86-64-v3, and two on the 16 bytes of every other level
# and of ARM64.
foreach(case "linux-x86_64,v3,    vaddps %ymm,1"
        "linux-x86_64,v1,    addps %xmm,2"
        "linux-x86_64,v2,    addps %xmm,2"
        "macos-arm64,armv8.5,    fadd v,2"
        "linux-arm64,armv8.0,    fadd v,2")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 pattern)
    list(GET parts 3 count)
    add_test(NAME simd_width_${target}_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DTARGET=${target}"
            "-DLEVEL=${level}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/simd_wide.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/simd"
            "-DPATTERN=${pattern}"
            "-DCOUNT=${count}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_simd_width.cmake")
endforeach()

# DESIGN: the start-up check on a real machine below the level. Rosetta
# runs a macos-x86_64 program on a macos-arm64 host and has no AVX2, so a
# program of the default level x86-64-v3 refuses there with the message of
# the section. Every other host runs the check in the unit tests, where
# ANTI_DEV_CPU stands a level in for the processor.
if(ANTIC_HOST_TARGET STREQUAL "macos-arm64")
    add_test(NAME cpu_check_refuses
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DTARGET=macos-x86_64"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cpu-check"
            "-DNEEDS=AVX2 .x86-64-v3, 2013 or later."
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_check.cmake")
    set_tests_properties(cpu_check_refuses PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endif()

# CI runs only by hand: every workflow triggers on workflow_dispatch alone.
add_test(NAME workflows_dispatch_only
    COMMAND "${CMAKE_COMMAND}"
        "-DWORKFLOWS=${PROJECT_SOURCE_DIR}/.github/workflows"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_workflows.cmake")

# Rule 25: every symbol a runtime library of the archive defines starts
# with anti_rt_, on every target and level.
add_test(NAME rt_names
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_names.cmake")
set_tests_properties(rt_names PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# The layout rule of "Repository layout" in CLAUDE.md, over git ls-files.
add_test(NAME repo_layout
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_repo_layout.cmake")

# The version stands in tools/version alone, and both programs print
# that one. Step 10 of the release reads them from a fresh install.
add_test(NAME antic_version COMMAND antic --version)
set_tests_properties(antic_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^antic ${PROJECT_VERSION}\n")
add_test(NAME anti_version COMMAND anti --version)
set_tests_properties(anti_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^anti ${PROJECT_VERSION}\n")

# The host target name has two spellings: ANTIC_HOST_TARGET here and the
# detection in src/antic/target.c. This test fails when they disagree.
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
# Snapshots in dev mode, where each module is an object of its own.
add_test(NAME program_snapshots_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/snapshots.anti"
        "-DOPTIONS=--dev"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
# Qualified bodies in dev mode, where every call through an interface
# reads a table this module wrote.
foreach(name IN ITEMS qualified_bodies qualified_chain qualified_single)
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()
# The wrapping and saturating operators, mul_high and the flags form in
# dev mode, whose checks trap on the plain operators and on none of these.
foreach(name IN ITEMS wrapping saturating compound_wrapping mul_high flags
        carry_chain variants)
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()
# The two forms of a walk in dev mode, whose loops over a collection
# carry the check of its changes.
add_test(NAME program_walk_forms_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/walk_forms.anti"
        "-DOPTIONS=--dev"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
# The default hash in dev mode, which writes it in each module apart, and
# the patterns of `for`.
foreach(name IN ITEMS hash_builtin hash_default for_patterns lent_derived
        lent_tuples owning_values destroy_locals)
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()
# The copies of generics in dev mode, which compiles each module apart and
# prints what the release build of the same program prints.
foreach(name IN ITEMS generic_copies generic_constants generic_methods
        generic_nested generic_sync generic_hooks generic_may_fail
        generic_fn_args generic_variants construct_keep_own walk_generic
        hash_generic generic_static walk_owned_parts)
    set(objects "")
    if(name STREQUAL "generic_may_fail" OR name STREQUAL "generic_fn_args" OR
       name STREQUAL "walk_generic" OR name STREQUAL "hash_generic" OR
       name STREQUAL "walk_owned_parts")
        set(objects "${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o")
    endif()
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DOBJECTS=${objects}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    if(NOT objects STREQUAL "")
        set_tests_properties(program_${name}_dev PROPERTIES
            FIXTURES_REQUIRED lang_dev_object)
    endif()
endforeach()
# Locking and channels in dev mode, whose checks run inside every `sync`
# block and every arm of a `select`. sync_exits fails with the errors of
# anti.lang, so its dev link names the object of anti.lang.
add_test(NAME dev_object_lang
    COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
            -I "${ANTIC_RUNTIME_DIR}/std"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev"
            "${ANTIC_RUNTIME_DIR}/std/anti/lang.antl")
set_tests_properties(dev_object_lang PROPERTIES FIXTURES_SETUP lang_dev_object)
foreach(name IN ITEMS sync_exits sync_workers channels select_two
        channel_workers synchronized concurrent
        lock_free_queue)
    set(objects "")
    if(name STREQUAL "sync_exits")
        set(objects "${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o")
    endif()
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DOBJECTS=${objects}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    set_tests_properties(program_${name}_dev PROPERTIES TIMEOUT 120)
endforeach()
set_tests_properties(program_sync_exits_dev PROPERTIES
    FIXTURES_REQUIRED lang_dev_object)
# The pattern literals in dev mode, where the object of each module runs
# the constructor that compiles its own literals. The link names the
# objects of anti.lang and anti.regex, whose classes the registry holds.
add_test(NAME dev_object_regex
    COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
            -I "${ANTIC_RUNTIME_DIR}/std"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_regex_dev"
            "${ANTIC_RUNTIME_DIR}/std/anti/regex.antl")
set_tests_properties(dev_object_regex PROPERTIES FIXTURES_SETUP regex_dev_object)
add_test(NAME program_pattern_literals_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/pattern_literals.anti"
        "-DOPTIONS=--dev"
        "-DOBJECTS=${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_regex_dev.o"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
set_property(TEST program_pattern_literals_dev PROPERTY FIXTURES_REQUIRED
    lang_dev_object regex_dev_object)
# The methods of `str` in dev mode, where the functions of anti.regex that
# the checker calls in their place stand in the object of anti.regex.
add_test(NAME program_pattern_replace_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/pattern_replace.anti"
        "-DOPTIONS=--dev"
        "-DOBJECTS=${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_regex_dev.o"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
set_property(TEST program_pattern_replace_dev PROPERTY FIXTURES_REQUIRED
    lang_dev_object regex_dev_object)
# The registry of the classes in dev mode, where the module that links
# writes it. The program takes its allocator from anti.mem. A dev link
# names the object of anti.mem on the command line.
add_test(NAME dev_object_mem
    COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
            -I "${ANTIC_RUNTIME_DIR}/std"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_mem_dev"
            "${ANTIC_RUNTIME_DIR}/std/anti/mem.antl")
set_tests_properties(dev_object_mem PROPERTIES FIXTURES_SETUP mem_dev_object)
add_test(NAME program_deserialize_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/deserialize.anti"
        "-DOPTIONS=--dev"
        "-DOBJECTS=${CMAKE_CURRENT_BINARY_DIR}/anti_mem_dev.o"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
set_tests_properties(program_deserialize_dev PROPERTIES
    FIXTURES_REQUIRED mem_dev_object)
# The methods of `[]byte` and `patch` in dev mode, where the byte
# literals compile in the constructor of the program's module. The
# conversions call anti.text, whose object imports anti.mem.
add_test(NAME dev_object_text
    COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
            -I "${ANTIC_RUNTIME_DIR}/std"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_text_dev"
            "${ANTIC_RUNTIME_DIR}/std/anti/text.antl")
set_tests_properties(dev_object_text PROPERTIES FIXTURES_SETUP text_dev_object)
foreach(name IN ITEMS pattern_bytes pattern_patch)
    add_test(NAME program_${name}_dev
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
            "-DOPTIONS=--dev"
            "-DOBJECTS=${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_regex_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_text_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_mem_dev.o"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-dev"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    set_property(TEST program_${name}_dev PROPERTY FIXTURES_REQUIRED
        lang_dev_object regex_dev_object text_dev_object mem_dev_object)
endforeach()
# The hooks under the options that decide them. hooks.anti,
# hook_trace.anti and trace_handlers.anti run above without options,
# which is release mode. The five always-on hooks fire there and the call
# hooks do not, so each case below names an expected file of its own.
#
# The option --no-hooks drops every hook site, the always-on ones as
# well. Bare --trace turns the code marked `trace` on. The value writes
# adds the `changed` hook after a write. A pattern reaches a class that
# did not ask, and leaves the marked code alone.
foreach(case "hooks|no_hooks|--no-hooks|hooks.no-hooks.expected"
        "hook_trace|trace|--trace|hook_trace.trace.expected"
        "hook_trace|writes|--trace,--trace,writes|hook_trace.writes.expected"
        "hook_trace|pattern|--trace,Quiet|hook_trace.pattern.expected"
        "trace_handlers|traced|--trace,--trace,writes|trace_handlers.traced.expected")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 source)
    list(GET parts 1 case_name)
    list(GET parts 2 case_options)
    list(GET parts 3 case_expected)
    add_test(NAME program_${source}_${case_name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/${source}.anti"
            "-DOPTIONS=${case_options}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/programs/${case_expected}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-hooks/${case_name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
endforeach()

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
    "${ANTIC_DEPS_DIR}/raylib/raylib-${raylib_version}"
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
# Rosetta has no AVX2, so those programs take --cpu v1 and link the v1
# runtime of the archive, as any --cpu v1 build does.
set(ANTIC_X86_64_OPTIONS "--target,macos-x86_64,--cpu,v1")
if(ANTIC_HOST_TARGET STREQUAL "macos-arm64")
    abi_x86_64_object(abi_structs_x86_64 abi/structs.c)
    abi_x86_64_object(abi_wchar_x86_64 abi/wchar.c)
    abi_x86_64_object(abi_simd_x86_64 abi/simd.c)
    abi_wchar_expected(abi_wchar_x86_64_expected _X86_64 -arch x86_64)
    set(abi_structs_x86_64_options "")
    set(abi_simd_x86_64_options "")
    set(abi_wchar_x86_64_options "-DEXPECTED=${abi_wchar_x86_64_expected}")
    foreach(case structs wchar simd)
        add_test(NAME program_abi_${case}_macos-x86_64
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_${case}.anti"
                "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_${case}_x86_64>,,>"
                "-DOPTIONS=${ANTIC_X86_64_OPTIONS}"
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
                "-DOPTIONS=${ANTIC_X86_64_OPTIONS}"
                "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    endif()
    add_test(NAME program_simd_big_macos-x86_64
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/simd_big.anti"
            "-DOPTIONS=${ANTIC_X86_64_OPTIONS}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    # DESIGN: a program that holds anti.regex links PCRE2, which stands at
    # the default level x86-64-v3 alone, and a program below it is refused
    # at link. Rosetta runs no v3 program, so the programs of patterns run
    # on the host target alone. errors/pattern_level.anti holds the
    # refusal.
    set(rosetta_sources ${program_sources})
    list(FILTER rosetta_sources EXCLUDE REGEX "/(pattern_[a-z]+|regex_compile)\\.anti$")
    foreach(source IN LISTS rosetta_sources)
        get_filename_component(name "${source}" NAME_WE)
        add_test(NAME program_${name}_macos-x86_64
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOPTIONS=${ANTIC_X86_64_OPTIONS}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/programs-x86_64"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    endforeach()
    set_tests_properties(program_args_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_GREETING=héllo")
    set_tests_properties(program_nullable_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")
    foreach(name IN ITEMS sync_exits sync_workers channels select_two
            channel_workers parallel_join synchronized concurrent
        lock_free_queue)
        set_tests_properties(program_${name}_macos-x86_64 PROPERTIES
            TIMEOUT 120)
    endforeach()
    set_tests_properties(program_by_unsigned_macos-x86_64 PROPERTIES
        TIMEOUT 60)
endif()

# The environment variable that tests/programs/args.anti looks for.
set_tests_properties(program_args PROPERTIES
    ENVIRONMENT "ANTI_TEST_GREETING=héllo")

# The `?*byte` that tests/programs/nullable.anti reads from getenv.
set_tests_properties(program_nullable PROPERTIES
    ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")

# The workers of tests/programs/channel_workers.anti wait on each other, so
# each needs a thread of the pool. Its .args file passes --anti.threads=4,
# which every run of the program takes. A program that leaves a mutex
# locked, a channel waiting or a waiter of the pool asleep stops, and the
# timeout reports it. tests/programs/parallel_join.anti passes
# --anti.threads=6 for the joiner, the job it waits for and a chunk.
foreach(name IN ITEMS sync_exits sync_workers channels select_two
        channel_workers parallel_join synchronized concurrent
        lock_free_queue)
    set_tests_properties(program_${name} PROPERTIES TIMEOUT 120)
endforeach()

# A step that reads as negative walks tests/programs/by_unsigned.anti in
# a loop without end, and the timeout reports it.
set_tests_properties(program_by_unsigned PROPERTIES TIMEOUT 60)

# The modules of the standard library in src/std/, which the build writes as
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

# Every .anti file under src/std/anti/collection/ is a module the build
# compiles by itself, after the modules it imports.
add_test(NAME std_collection_modules
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std-collection-modules"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_std_collection_modules.cmake")

# The parts every collection shares in dev mode. The copies of the
# generics of anti.collection come from its library file into the module
# of the program, and the link names an object of each module it imports.
add_test(NAME dev_object_collection
    COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
            -I "${ANTIC_RUNTIME_DIR}/std"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_collection_dev"
            "${ANTIC_RUNTIME_DIR}/std/anti/collection.antl")
set_tests_properties(dev_object_collection PROPERTIES
    FIXTURES_SETUP collection_dev_object)
add_test(NAME std_collection_parts_dev
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/std/collection_parts.anti"
        "-DOPTIONS=--dev"
        "-DOBJECTS=${CMAKE_CURRENT_BINARY_DIR}/anti_lang_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_mem_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_text_dev.o,${CMAKE_CURRENT_BINARY_DIR}/anti_collection_dev.o"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std-dev"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
set_tests_properties(std_collection_parts_dev PROPERTIES
    FIXTURES_REQUIRED
        "lang_dev_object;mem_dev_object;text_dev_object;collection_dev_object")

# Every failing function of src/std/ is written with `may fail`, apart from
# a binding, which refuses it.
add_test(NAME std_may_fail_only
    COMMAND "${CMAKE_COMMAND}"
        "-DROOT=${ANTIC_STD_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_may_fail_only.cmake")

# So is every failing function of the test programs. The functions the
# listing skips write the form on purpose: construct_forms refuses it on
# a `construct`, failing and error_values check that a function written
# so is ordinary, whose error is a value, and catch_exits returns the
# error of a handler as one, and catch_moves a wrap of it.
add_test(NAME tests_may_fail_only
    COMMAND "${CMAKE_COMMAND}"
        "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DSKIP=errors/construct_forms.anti:construct,errors/failing.anti:by_hand,errors/failing.anti:fail_by_hand,programs/error_values.anti:stored,programs/error_values.anti:maybe,programs/error_values.anti:caught,programs/catch_exits.anti:passed,programs/catch_moves.anti:by_return"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_may_fail_only.cmake")

# DESIGN: the three directory functions of anti.os fail when the
# environment names no home, and the message names the variable that is
# absent. The same program runs a second time without those variables,
# and reads the failure rather than a directory.
set(no_home "${CMAKE_CURRENT_SOURCE_DIR}/std/userdirs-no-home.expected")
if(ANTIC_HOST_TARGET MATCHES "^windows-")
    set(no_home "${CMAKE_CURRENT_SOURCE_DIR}/std/userdirs-no-home.win32.expected")
endif()
add_test(NAME std_userdirs_no_home
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/std/userdirs.anti"
        "-DEXPECTED=${no_home}"
        "-DUNSET=HOME,LOCALAPPDATA,XDG_CONFIG_HOME,XDG_DATA_HOME,XDG_CACHE_HOME"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std-no-home"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")

# The roots that tests/std/userdirs.anti reads. The environment of the
# test holds them, so one expected output stands on every machine. A
# Windows host reads LOCALAPPDATA alone and no XDG variable, and the two
# expected files differ by that.
set_tests_properties(std_userdirs PROPERTIES ENVIRONMENT
    "XDG_CONFIG_HOME=/conf;XDG_DATA_HOME=/data;XDG_CACHE_HOME=/tmp/cache;LOCALAPPDATA=C:\\Users\\Test\\AppData\\Local")

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

# Simd structs against C: the functions of tests/abi/simd.c take and
# return the vector types of C by value.
add_library(abi_simd OBJECT abi/simd.c)
target_compile_options(abi_simd PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_simd PRIVATE -mmacosx-version-min=11.0)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_simd PRIVATE -fno-sanitize=all)
endif()
add_test(NAME program_abi_simd
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_simd.anti"
        "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:abi_simd>,,>"
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
# The permissions of the text step 9 publishes on anti-lang.com.
add_test(NAME release_site
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_site.cmake")
add_test(NAME sysroot_digest
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sysroot_digest"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_sysroot_digest.cmake")
set_tests_properties(sysroot_digest PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# A configure with -DANTI_DEPS_DIR reads the pinned downloads of another
# directory, and a copy of the sources builds against this tree's.
add_test(NAME deps_dir
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/deps_dir"
            "-DDEPS=${ANTIC_DEPS_DIR}" "-DGENERATOR=${CMAKE_GENERATOR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_deps_dir.cmake")

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

# `anti build`, `anti run` and `anti new` over a two-module project: the
# dev cache, release mode, the -g rule, --target, --cpu and the lock file.
if(ANTIC_HOST_CPU STREQUAL "arm64")
    set(anti_build_cpu "armv8.0")
else()
    set(anti_build_cpu "v1")
endif()
if(ANTIC_HOST_TARGET STREQUAL "linux-x86_64")
    set(anti_build_other "macos-arm64")
else()
    set(anti_build_other "linux-x86_64")
endif()
add_test(NAME anti_build
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DOTHER=${anti_build_other}"
        "-DCPU=${anti_build_cpu}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build.cmake")

# `anti symbols` over a deployment of a release program, with a plugin
# on a macOS host. It takes the inventory, the check of present, stale
# and missing symbols and a release trace resolved into names and lines. A
# Windows program keeps its functions in the PDB, which only DbgHelp
# reads, so the test runs on the other hosts.
if(NOT ANTIC_HOST_OS STREQUAL "windows")
    if(ANTIC_HOST_OS STREQUAL "macos")
        set(anti_symbols_plugin "-DSUFFIX=.dylib")
    else()
        set(anti_symbols_plugin "-DSUFFIX=")
    endif()
    add_test(NAME anti_symbols
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-symbols"
            "-DPLUGIN=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
            "${anti_symbols_plugin}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-symbols"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_symbols.cmake")
endif()

# `anti symbols resolve` refuses a line of a map or a trace whose number
# does not fit 64 bits. The archive is written here, so every host runs it.
add_test(NAME anti_symbols_input
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-symbols-input"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_symbols_input.cmake")

# A binding names its framework with `link framework`, and `anti build`
# passes it to antic. A Mac takes the SDK from the Command Line Tools.
if(APPLE)
    add_test(NAME anti_build_framework
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DOBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-framework"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_framework.cmake")
endif()

# A binding names its Linux libraries with `link linux`, and `anti build`
# passes them to antic. The program that reaches them links dynamically
# against glibc, and one that does not links statically against musl.
add_test(NAME linux_modes
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DREADOBJ=${ANTIC_RUNTIME_DIR}/bin/llvm-readobj"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/linux-modes"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_linux_modes.cmake")

# The two shapes of a dependency, the repository layout over `file://`
# and the two libraries for C.
add_test(NAME anti_build_deps
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DLLVM_AR=${ANTIC_LLVM_AR}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-deps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_deps.cmake")
# A malformed index.toml and anti.lock: names, versions, module paths and
# digests that are not the grammar of their kind are refused before they
# reach a path in the cache, a comparison or the lock file.
add_test(NAME anti_build_inputs
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-inputs"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_inputs.cmake")

# The rules of the canonical form on one file, the format stability of
# docs/tooling.md and `--check`, which writes nothing.
add_test(NAME anti_fmt
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fmt"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/fmt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_fmt.cmake")

# Every source of the standard library and of the tests stands in the
# canonical form, so that `anti fmt` over the checkout writes nothing.
add_test(NAME fmt_canonical
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DSTD=${CMAKE_SOURCE_DIR}/src/std"
        "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_fmt_canonical.cmake")

# Every class of `anti check`: the front end on every source of a project,
# the `anti` blocks of the doc comments, the doc warnings and the formatting
# rules.
add_test(NAME anti_check
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DPROJECTS=${CMAKE_CURRENT_SOURCE_DIR}/check"
        "-DSTD=${CMAKE_SOURCE_DIR}/src/std"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/check"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_check.cmake")

# The documentation generator: the pages of the fixture, the doc
# equivalence of docs/tooling.md, the two forms and the two options that
# need the source.
add_executable(antl_rename doc/antl_rename.c)
add_test(NAME anti_doc
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRENAME=$<TARGET_FILE:antl_rename>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/doc"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/doc"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_doc.cmake")

# anti bind --header writes the header that antic --lib writes for the
# same module, from the library file alone. The other cases bind
# tests/bind/layout.h and raymath through clang and a part of
# raylib_api.json, and check the refusals.
set(anti_bind_cc ${ANTIC_TEST_CC})
if(APPLE)
    list(APPEND anti_bind_cc -mmacosx-version-min=11.0)
endif()
set(anti_bind_cases header clang raymath raylib api api_malformed
    clang_malformed)
if(NOT WIN32)
    # The cases refusals and ast_malformed stand a shell script in for
    # clang.
    list(APPEND anti_bind_cases refusals ast_malformed)
endif()
foreach(case IN LISTS anti_bind_cases)
    add_test(NAME anti_bind_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/clib"
            "-DDUMP=${CMAKE_CURRENT_SOURCE_DIR}/dump"
            "-DBIND=${CMAKE_CURRENT_SOURCE_DIR}/bind"
            "-DABI=${CMAKE_CURRENT_SOURCE_DIR}/abi"
            "-DRAYLIB=${ANTIC_RAYLIB_DIR}"
            "-DCC=${anti_bind_cc}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/bind-${case}"
            -DCASE=${case}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_bind.cmake")
endforeach()

# The `tests` block of a module of the standard library. The module
# imports other modules, so the dev run links an object of each.
add_test(NAME std_tests_trace
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DSTD=${CMAKE_SOURCE_DIR}/src/std"
        "-DSOURCE=${ANTIC_STD_SOURCE_DIR}/trace.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std-tests-trace"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_std_tests.cmake")

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
add_test(NAME sysroot_links
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sysroot_links"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_sysroot_links.cmake")
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

# A variable of find_program is empty above the call in a fresh build
# directory, and right in one that a run already filled.
add_test(NAME find_order
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_find_order.cmake")
add_test(NAME installer_options
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installer_options.cmake")

# The binaries of a release are assets of its GitHub release, and the
# installers take them from there and from nowhere else.
add_test(NAME installer_github
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/installer-github"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installer_github.cmake")
set_tests_properties(installer_github PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# The installer trusts no line of SHA256SUMS before it has checked
# SHA256SUMS.sig against the key it carries.
add_test(NAME manifest_signature
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/manifest-signature"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_manifest_signature.cmake")
set_tests_properties(manifest_signature PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# The checks tools/publish.cmake makes before it uploads a version
# directory of the download area.
add_test(NAME publish_manifest
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/publish-manifest"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_publish_manifest.cmake")
set_tests_properties(publish_manifest PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# ./r on a copy of the tree, with the build, the packer and the two VMs
# stood in for on the PATH. It checks the order of the steps, the output
# of each and the refusals of the preflight.
add_test(NAME release_dry_run
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DCC=${CMAKE_C_COMPILER}"
            "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/release"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_dry_run.cmake")
set_tests_properties(release_dry_run PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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

# A `may fail` function without a `fail` and without a `try` is a warning,
# not an error, because an interface function may fail in one
# implementation and not in another. A dev build prints it and carries on.
add_test(NAME warning_never_fails
    COMMAND antic --runtime "${ANTIC_RUNTIME_DIR}" --dev -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/never_fails.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/never_fails.anti")
set_tests_properties(warning_never_fails PROPERTIES
    PASS_REGULAR_EXPRESSION "never_fails.anti:3:21: warning: `close_it` may fail and never does \\[never-fails\\]")

# A release build accepts no warning, and --warnings-as-errors gives a dev
# build the same behaviour. The warning is then an error with its name.
foreach(mode "release|-S" "strict|--dev,--warnings-as-errors,-S")
    string(REPLACE "|" ";" parts "${mode}")
    list(GET parts 0 name)
    list(GET parts 1 flags)
    add_test(NAME error_never_fails_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
            "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},${flags},-o,${CMAKE_CURRENT_BINARY_DIR}/never_fails_${name}.s,never_fails.anti"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/never_fails.err"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
endforeach()

# `allow` at every level silences its warning, so a release build and a
# build of the `tests` block both accept the program without a word.
foreach(mode "release|-S" "tests|--tests,--dev,-S")
    string(REPLACE "|" ";" parts "${mode}")
    list(GET parts 0 name)
    list(GET parts 1 flags)
    string(REPLACE "," ";" flags "${flags}")
    add_test(NAME silent_allow_levels_${name}
        COMMAND antic --runtime "${ANTIC_RUNTIME_DIR}" ${flags}
                -o "${CMAKE_CURRENT_BINARY_DIR}/allow_levels_${name}.s"
                "${CMAKE_CURRENT_SOURCE_DIR}/errors/allow_levels.anti")
    set_tests_properties(silent_allow_levels_${name} PROPERTIES
        FAIL_REGULAR_EXPRESSION "warning|error")
endforeach()

# A clause that silences nothing is a warning, which a dev build prints and
# carries on. The listing `allow_unused` holds the release build, which
# refuses each. A clause of a doc warning waits for --doc-warnings.
add_test(NAME warning_unused_clauses
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},--dev,-S,-o,${CMAKE_CURRENT_BINARY_DIR}/allow_unused.s,allow_unused.anti"
        "-DNEEDLES=allow_unused.anti:8:1: warning: `allow(assert-call)` silences nothing [unused-allow];allow_unused.anti:16:19: warning: `unchecked(unguarded-field)` overrules nothing [unused-unchecked];allow_unused.anti:34:2: warning: `allow(shadowed-catch)` silences nothing [unused-allow]"
        "-DABSENT=allow(undocumented)"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")
add_test(NAME warning_unused_doc_clause
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},--dev,--doc-warnings,--warn-undocumented,-S,-o,${CMAKE_CURRENT_BINARY_DIR}/allow_unused_doc.s,allow_unused.anti"
        "-DNEEDLES=allow_unused.anti:10:1: warning: `allow(undocumented)` silences nothing [unused-allow]"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")

# Every name of the compiler's table stands in docs/notes/warnings.md with
# its meaning and its fix, and the note names nothing else.
add_test(NAME warning_names
    COMMAND "${CMAKE_COMMAND}"
        "-DTABLE=${CMAKE_SOURCE_DIR}/src/antic/warnings.c"
        "-DNOTE=${CMAKE_SOURCE_DIR}/docs/notes/warnings.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warning_names.cmake")

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

# `catch e` shadowing a name of an outer scope is a warning. The three
# forms that bind a name each report it, and a handler whose name is free
# reports nothing.
add_test(NAME warning_catch_shadow
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},--dev,-S,-o,${CMAKE_CURRENT_BINARY_DIR}/catch_shadow.s,catch_shadow.anti"
        "-DNEEDLES=catch_shadow.anti:22:25: warning: `e` shadows the outer `e` [shadowed-catch];catch_shadow.anti:26:21: warning: `e` shadows the outer `e` [shadowed-catch];catch_shadow.anti:31:10: warning: `e` shadows the outer `e` [shadowed-catch]"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")
add_test(NAME silent_catch_no_shadow
    COMMAND antic --runtime "${ANTIC_RUNTIME_DIR}" -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/catch_exits.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/catch_exits.anti")
set_tests_properties(silent_catch_no_shadow PROPERTIES
    FAIL_REGULAR_EXPRESSION "shadows")

# Markup outside the doc subset, a backtick name that resolves nowhere
# and, with --warn-undocumented, a `pub` item without a `///` comment.
# Every one of them comes with --doc-warnings and with nothing else.
set(MARKUP "the doc comment")
add_test(NAME warning_doc_markup
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--doc-warnings,--runtime,${ANTIC_RUNTIME_DIR},-S,-o,${CMAKE_CURRENT_BINARY_DIR}/doc_markup.s,doc_markup.anti"
        "-DNEEDLES=doc_markup.anti:1:1: warning: ${MARKUP} of the module holds HTML, which the doc markup has not;doc_markup.anti:6:1: warning: ${MARKUP} of `heads` holds a heading, which the doc markup has not;${MARKUP} of `tables` holds a table, which the doc markup has not;${MARKUP} of `stars` holds emphasis, which the doc markup has not;${MARKUP} of `numbers` holds a numbered list, which the doc markup has not;${MARKUP} of `images` holds an image, which the doc markup has not;doc_markup.anti:27:1: warning: `nowhere` in ${MARKUP} of `unresolved` resolves to nothing"
        "-DABSENT=${MARKUP} of `plus` holds;in ${MARKUP} of `plus` resolves;has no `///` comment"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")
add_test(NAME warning_undocumented
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--doc-warnings,--warn-undocumented,--runtime,${ANTIC_RUNTIME_DIR},-S,-o,${CMAKE_CURRENT_BINARY_DIR}/doc_undocumented.s,doc_markup.anti"
        "-DNEEDLES=warning: the pub item `undocumented` has no `///` comment"
        "-DABSENT=the pub item `plus` has no;the pub item `heads` has no"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")
add_test(NAME silent_doc_markup
    COMMAND antic --runtime "${ANTIC_RUNTIME_DIR}" -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/doc_markup_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/doc_markup.anti")
set_tests_properties(silent_doc_markup PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

# The front end alone, which `anti check` runs on every source. It writes
# no assembly, no object and no library file.
add_test(NAME front_end_only
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DGOOD=${CMAKE_CURRENT_SOURCE_DIR}/programs/catch_exits.anti"
        "-DBAD=${CMAKE_CURRENT_SOURCE_DIR}/errors/errors.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/front-end"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_front_end.cmake")

# Every example of the syntax overview passes the front end, apart from a
# block marked as not built.
add_test(NAME overview_examples
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DOVERVIEW=${PROJECT_SOURCE_DIR}/docs/anti-syntax-overview.md"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/overview"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_overview_examples.cmake")

add_test(NAME warning_one_segment
    COMMAND antic -c -o "${CMAKE_CURRENT_BINARY_DIR}/geometry.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/geometry.anti")
set_tests_properties(warning_one_segment PROPERTIES
    PASS_REGULAR_EXPRESSION "geometry.anti:1:1: warning: the module path `geometry` has one segment, which is for a program's own files \\[single-segment-path\\]")

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

# A release links its macOS binaries against the pinned Apple SDK, and an
# SDK that changed under that name is refused.
add_test(NAME macos_sdk
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DCLANG=${ANTIC_CLANG_DIR}/bin/clang"
            "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/macossdk"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_macos_sdk.cmake")
set_tests_properties(macos_sdk PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# Without --runtime, antic takes the runtime archive of the user's data
# directory, which is where an install puts it.
add_test(NAME runtime_from_data_dir
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/datadir"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_from_data_dir.cmake")

# A Windows executable carries the CodeView record of the PDB that
# lld-link wrote beside it. It holds no path of the machine that linked it
# either, because the link passes /PDBALTPATH:%_PDB%.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME pdb_guid_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DREADOBJ=${ANTIC_RUNTIME_DIR}/bin/llvm-readobj"
            "-DCHECK=${CMAKE_SOURCE_DIR}/tools/check-pdb.cmake"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/pdb"
            "-DTARGET=${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pdb_guid.cmake")
    set_tests_properties(pdb_guid_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# The PDB of a Windows program names every function of it, in a release
# build and in one with -g, and holds no path of the build.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME pdb_names_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/trace/stack_trace.anti"
            "-DNAMES=stack_trace.inner;stack_trace.outer;stack_trace.skipped;stack_trace.put;stack_trace.main"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/pdb-names"
            "-DTARGET=${target}"
            "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pdb_names.cmake")
    set_tests_properties(pdb_names_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
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

# A program that names a framework links against Apple's SDK. The
# macos-x86_64 program runs under Rosetta, which has no AVX2, so it takes
# the v1 level and the v1 runtime of the archive.
foreach(target macos-arm64 macos-x86_64)
    set(framework_options "")
    if(target STREQUAL "macos-x86_64")
        set(framework_options "--cpu,v1")
    endif()
    add_test(NAME framework_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOPTIONS=${framework_options}"
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

# A source of 64 MiB passes the front end, and one a byte longer is
# refused when it is read, before the lexer sees it.
add_test(NAME error_source_cap
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/source-cap"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_source_cap.cmake")

# scale.ast is the syntax tree listing that chapter 1 prints.
add_test(NAME dump_ast_scale
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ast"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.ast"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# `fallthrough;` is a statement of its own in the tree of an arm.
add_test(NAME dump_ast_switch_fallthrough
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ast"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/switch_fallthrough.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/switch_fallthrough.ast"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# Type parameters and type arguments in the tree: `>>` closes two lists,
# and `<` in an expression opens a list by the rule of C#.
add_test(NAME dump_ast_generics_syntax
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ast"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/generics_syntax.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/generics_syntax.ast"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")

# An `f"..."` is one node, with the value of each `{expr}` below it.
add_test(NAME dump_ast_format
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPTION=--dump-ast"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/format.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/format.ast"
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

# A library with a generic carries the checked tree of its body, which
# every host writes byte for byte the same. pick.antl.hex lists them.
add_test(NAME antl_generic
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DSOURCE=${MODULES}/generics/com/example/pick.anti"
        "-DROOT=${MODULES}/generics"
        "-DOUTPUT=${LIBS}/pick.antl"
        "-DEXPECTED=${MODULES}/generics/pick.antl.hex"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antl.cmake")

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

# The `trace` marking of a class crosses a module boundary in the library
# file, so a write to a field of that class calls `changed` where the
# write stands. The `enter` and `leave` of a function stand in its body,
# so the build of the module that holds the body decides them. The
# library here is built with --trace and without --trace writes, and the
# program with both.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME hooks_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/hooks"
            "-DLIBRARIES=store"
            "-DLIB_OPTIONS=--trace"
            "-DOPTIONS=--trace,--trace,writes"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/hooks-modules-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

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
            "-DSTD=lang,mem,text,reflect"
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
            "-DSTD=lang"
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

# Generics over a module boundary: a program and a second library use the
# generics of a library with their own arguments. Each makes its copies
# from the checked tree the library file carries, and a copy two modules
# make is one type and one symbol. Release mode and dev mode print the
# same.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME generics_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/generics"
            "-DLIBRARIES=gen,mid"
            "-DSTD=lang,mem,text"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/generics-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# Direct imports. A program and a library name the items of another
# library without its module name, and reach the module by its name as
# well. A generic among them has its copy made in the program.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME direct_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/direct"
            "-DLIBRARIES=shapes,frame"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/direct-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# `may fail` over a module boundary: the library file records the flag, so
# the program handles the error, `try` forwards it and `undo` runs on the
# fail path alone.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME failing_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/failing"
            "-DLIBRARIES=counter"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/failing-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# Default values over a module boundary: the library file carries the
# default of each parameter, and `here` is the position of the call in the
# program.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME default_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/defaults"
            "-DLIBRARIES=greeter"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/defaults-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# A tuple over a module boundary: the library file carries the tuple of
# each signature, so the two modules name one type and the program takes
# the elements apart.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME tuple_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/tuples"
            "-DLIBRARIES=pairs"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/tuples-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# A variant of a library, which another module builds and switches on
# with every case named. It tests one with `is` and `if let` and passes
# it back inside a struct of the library.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME variant_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/variants"
            "-DLIBRARIES=geo"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/variants-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# A public class of a library whose layout holds types nested in it,
# which another module lays out, builds and calls.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME nested_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/nested"
            "-DLIBRARIES=people"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/nested-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# The hash of the types of a library and its generics that hash and order,
# which another module uses and hashes inside a struct of its own.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME hashing_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/hashing"
            "-DLIBRARIES=keys"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/hashing-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# A collection, its iterator and a struct with the language hooks in a
# library, which another module walks, indexes and collects.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME iteration_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/iteration"
            "-DLIBRARIES=bag"
            "-DSTD=lang"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/iteration-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# Closures and functions passed to the parameters of a library.
# The library file carries each parameter in its form. A `concurrent` one
# reaches the workers of `parallel`, and a `keep` one stores a C function
# pointer.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME closures_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/closures"
            "-DLIBRARIES=walk"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/closures-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
endforeach()

# A Mutex in a struct of a library and channels that pass between the
# program and a library, which drains one and selects over two.
foreach(mode release dev)
    if(mode STREQUAL "dev" AND WIN32)
        continue()
    endif()
    add_test(NAME sync_modules_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/modules/sync"
            "-DLIBRARIES=queue"
            "-DPROGRAM=client"
            "-DMODE=${mode}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sync-${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
    set_tests_properties(sync_modules_${mode} PROPERTIES TIMEOUT 120)
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

add_executable(poke_byte modules/poke_byte.c)
add_test(NAME dev_library_verify
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DPOKE=$<TARGET_FILE:poke_byte>"
        "-DLIBS=${LIBS}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/dev-verify"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antl_verify.cmake")
set_tests_properties(dev_library_verify PROPERTIES FIXTURES_REQUIRED scale_antl)

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

# The forms of the parameters of a library reach its importer.
# A closure never reaches a `keep` parameter of the library. A function
# that is not `concurrent` never reaches a `concurrent` one.
file(MAKE_DIRECTORY "${LIBS}/closures/com/example")
add_test(NAME antl_walk
    COMMAND antic -c -I "${MODULES}/closures"
            -o "${LIBS}/closures/com/example/walk.antl"
            "${MODULES}/closures/com/example/walk.anti")
set_tests_properties(antl_walk PROPERTIES FIXTURES_SETUP walk_antl)
add_test(NAME error_closures_modules
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=-S,-I,${LIBS}/closures,-o,${CMAKE_CURRENT_BINARY_DIR}/closures_modules.s,closures_modules.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/closures_modules.err"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
set_tests_properties(error_closures_modules PROPERTIES
    FIXTURES_REQUIRED walk_antl)

# The `own` and `lent` parameters of a library reach the importer through
# the library file.
file(MAKE_DIRECTORY "${LIBS}/owned/com/example")
add_test(NAME antl_parts
    COMMAND antic -c -I "${MODULES}/owned"
            -o "${LIBS}/owned/com/example/parts.antl"
            "${MODULES}/owned/com/example/parts.anti")
set_tests_properties(antl_parts PROPERTIES FIXTURES_SETUP parts_antl)
add_test(NAME error_own_modules
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=-S,-I,${LIBS}/owned,-o,${CMAKE_CURRENT_BINARY_DIR}/own_modules.s,own_modules.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/own_modules.err"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
set_tests_properties(error_own_modules PROPERTIES
    FIXTURES_REQUIRED parts_antl)

# The kinds of the fields of a concurrent class and the fields of a
# synchronized class reach the importer through the library file.
file(MAKE_DIRECTORY "${LIBS}/sync/com/example")
add_test(NAME antl_queue
    COMMAND antic -c -I "${MODULES}/sync"
            -o "${LIBS}/sync/com/example/queue.antl"
            "${MODULES}/sync/com/example/queue.anti")
set_tests_properties(antl_queue PROPERTIES FIXTURES_SETUP queue_antl)
add_test(NAME error_concurrent_modules
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=-S,-I,${LIBS}/sync,-o,${CMAKE_CURRENT_BINARY_DIR}/concurrent_modules.s,concurrent_modules.anti"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/errors/concurrent_modules.err"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_error.cmake")
set_tests_properties(error_concurrent_modules PROPERTIES
    FIXTURES_REQUIRED queue_antl)

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

# The results of `parallel` take one value per chunk, and a count of them
# that no size holds stops the program before a worker runs.
add_test(NAME parallel_size
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/parallel_size.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_parallel_size.cmake")

# A search with a pattern literal that reaches the match limit stops the
# program with the pattern and the line.
add_test(NAME pattern_limit
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/pattern_limit.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pattern_limit.cmake")

# The size of a Mutex per target: four bytes on Linux and macOS, eight on
# Windows.
add_test(NAME mutex_size
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/mutex_size.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mutex_size"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_mutex_size.cmake")

# Two threads that take two locks in opposite orders: a dev build reports
# the conflict with its four sites, and a release build says nothing.
add_test(NAME lock_order
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/lock_order.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_lock_order.cmake")
set_tests_properties(lock_order PROPERTIES TIMEOUT 120)

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

# Stack traces and the origin of an error. A trace holds addresses that
# differ from run to run, so run_trace.cmake matches the output against
# patterns. Each case is a name, the source in tests/trace, the options of
# antic and the arguments of the program. The pattern files of the output
# and of the error output follow, then the exit status. A dev build links
# the object of anti.lang, which it compiles first.
set(TRACE "${CMAKE_CURRENT_SOURCE_DIR}/trace")
foreach(case
        "stack|stack_trace|||stack_trace.pattern||0"
        "stack_g|stack_trace|-g||stack_trace.g.pattern||0"
        "stack_dev|stack_trace|--dev||stack_trace.pattern||0"
        "copies|copies|||copies.pattern||0"
        "copies_g|copies|-g||copies.pattern||0"
        "copies_dev|copies|--dev||copies.pattern||0"
        "origin|origin|||origin.pattern||0"
        "origin_dev|origin|--dev||origin.trace.pattern||0"
        "origin_option|origin||--anti.backtrace|origin.trace.pattern||0"
        "origin_dev_off|origin|--dev|--anti.backtrace=false|origin.pattern||0"
        "options|options||--anti.backtrace,plain,--anti.backtrace=false,last|options.pattern||0"
        "options_unknown|options||--anti.nonsense|options.empty.pattern|options.unknown.pattern|70"
        "options_value|options||--anti.backtrace=maybe|options.empty.pattern|options.value.pattern|70"
        "handlers|handler_reports|--trace,--trace,writes||handler_reports.pattern|handler_reports.errors.pattern|0")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 source)
    list(GET parts 2 options)
    list(GET parts 3 arguments)
    list(GET parts 4 expected)
    list(GET parts 5 errors)
    list(GET parts 6 status)
    set(error_patterns "")
    if(NOT errors STREQUAL "")
        set(error_patterns "-DERRORS=${TRACE}/${errors}")
    endif()
    set(std "")
    if(options MATCHES "--dev")
        set(std "lang")
    endif()
    add_test(NAME trace_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${TRACE}/${source}.anti"
            "-DOPTIONS=${options}"
            "-DARGS=${arguments}"
            "-DEXPECTED=${TRACE}/${expected}"
            ${error_patterns}
            "-DSTD=${std}"
            "-DSTATUS=${status}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/trace-${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trace.cmake")
endforeach()

# The runtime configuration: the options under --anti., the file that
# --anti.conf, ANTI_CONF or rt.configure names, and the includes of that
# file. A case is a name, the source in tests/conf, the options of antic
# and the arguments of the program, then the pattern files of the output
# and of the error output and the exit status. The runner is the one of
# the trace tests, because the layer of a key names an absolute path
# that a comparison cannot hold.
set(CONF "${CMAKE_CURRENT_SOURCE_DIR}/conf")
foreach(case
        "inspect_build|report||--anti.inspect|inspect.build.pattern||0"
        "inspect_options|report||--anti.backtrace,--anti.hash_seed=42,--anti.logger=log.toml,--anti.plugins=a:b,--anti.threads=4,--anti.trace=leaks,--anti.inspect|inspect.options.pattern||0"
        "inspect_file|report||--anti.conf=${CONF}/basic.toml,--anti.inspect|inspect.file.pattern||0"
        "precedence|report||--anti.conf=${CONF}/basic.toml,--anti.threads=9,--anti.inspect|inspect.precedence.pattern||0"
        "include|report||--anti.conf=${CONF}/outer.toml,--anti.inspect|inspect.include.pattern||0"
        "array|report||--anti.conf=${CONF}/array.toml,--anti.inspect|inspect.array.pattern||0"
        "environment|report||--anti.inspect|inspect.file.pattern||0"
        "environment_option|report||--anti.conf=${CONF}/outer.toml,--anti.inspect|inspect.include.pattern||0"
        "environment_long|report||--anti.inspect|empty.pattern|missing.pattern|70"
        "environment_utf8|report||--anti.inspect|inspect.file.pattern||0"
        "arguments|report||--anti.threads=2,one,--anti.backtrace,two|arguments.pattern||0"
        "help|report||--anti.help|help.pattern||0"
        "configure|configure||${CONF}/backtrace.toml|configure.pattern||0"
        "configure_dev|configure|--dev|${CONF}/no-backtrace.toml|configure.plain.pattern||0"
        "configure_missing|configure||${CONF}/nowhere.toml|empty.pattern|missing.pattern|70"
        "configure_nul|configure_nul||${CONF}/basic.toml|empty.pattern|configure-nul.pattern|70"
        "file_missing|report||--anti.conf=${CONF}/nowhere.toml|empty.pattern|missing.pattern|70"
        "include_missing|report||--anti.conf=${CONF}/missing.toml|empty.pattern|missing.pattern|70"
        "include_cycle|report||--anti.conf=${CONF}/cycle.toml|empty.pattern|cycle.pattern|70"
        "unknown_option|report||--anti.nonsense|empty.pattern|unknown-option.pattern|70"
        "unknown_key|report||--anti.conf=${CONF}/unknown-key.toml|empty.pattern|unknown-key.pattern|70"
        "bad_value|report||--anti.conf=${CONF}/bad-value.toml|empty.pattern|bad-value.pattern|70"
        "nul_value|report||--anti.conf=${CONF}/nul-value.toml|empty.pattern|nul-value.pattern|70"
        "threads_range|report||--anti.conf=${CONF}/threads-range.toml|empty.pattern|threads-range.pattern|70"
        "threads_option|report||--anti.threads=99999999999|empty.pattern|threads-option.pattern|70"
        "threads_zero|report||--anti.threads=0|empty.pattern|threads-zero.pattern|70"
        "injection|report||--anti.conf=${CONF}/injection.toml|empty.pattern|injection.pattern|70"
        "inject_option|report||--anti.inject=anti.log.Logger=lib/x.so|empty.pattern|inject-option.pattern|70"
        "hash_seed_option|seed||--anti.hash_seed=42|seed.fixed.pattern||0"
        "hash_seed_file|seed||--anti.conf=${CONF}/seed.toml|seed.file.pattern||0"
        "hash_seed_configure|seed||${CONF}/seed.toml|seed.configure.pattern||0"
        "hash_seed_range|report||--anti.hash_seed=18446744073709551616|empty.pattern|seed-range.pattern|70"
        "hash_seed_text|report||--anti.conf=${CONF}/seed-text.toml|empty.pattern|seed-text.pattern|70")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 source)
    list(GET parts 2 options)
    list(GET parts 3 arguments)
    list(GET parts 4 expected)
    list(GET parts 5 errors)
    list(GET parts 6 status)
    set(error_patterns "")
    if(NOT errors STREQUAL "")
        set(error_patterns "-DERRORS=${CONF}/${errors}")
    endif()
    # A dev build links the objects of the modules the program imports.
    set(std "")
    if(options MATCHES "--dev")
        set(std "lang,runtime")
    endif()
    add_test(NAME conf_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CONF}/${source}.anti"
            "-DOPTIONS=${options}"
            "-DARGS=${arguments}"
            "-DEXPECTED=${CONF}/${expected}"
            ${error_patterns}
            "-DSTD=${std}"
            "-DSTATUS=${status}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/conf-${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trace.cmake")
endforeach()

# The seed of the hashing collections without a fixed one: two runs of one
# program choose two seeds.
add_test(NAME conf_hash_seed_random
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CONF}/seed.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/conf-hash_seed_random"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_hash_seed.cmake")

# Injection. The program of tests/inject names three interfaces and takes
# a provider of each from --inject: the `get` of a singleton that
# implements the interface, a module function, and the name of a
# singleton that inherits the interface, whose `get` the provider is.
# One field is called `alloc`, which the
# example of the overview writes. `inject final` then refuses a
# replacement at start, and --anti.inspect lists what it injects. The runner
# is the one of the configuration tests, because a pattern file holds the
# output of each.
set(INJECT "${CMAKE_CURRENT_SOURCE_DIR}/inject")
set(INJECT_OPTIONS "--inject,service.Logger=service.Console.get,--inject,service.Clock=service.the_clock,--inject,service.Allocator=service.Memory")
foreach(case
        "service|${INJECT_OPTIONS}||service.pattern||0"
        "inspect|${INJECT_OPTIONS}|--anti.inspect|inspect.pattern||0"
        "final|${INJECT_OPTIONS}|--anti.inject=service.Clock=lib/x.so|empty.pattern|final.pattern|70"
        "unknown|${INJECT_OPTIONS}|--anti.inject=other.Thing=lib/x.so|empty.pattern|unknown.pattern|70"
        "plugin|${INJECT_OPTIONS}|--anti.inject=service.Logger=lib/x.so|empty.pattern|plugin.pattern|70")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 options)
    list(GET parts 2 arguments)
    list(GET parts 3 expected)
    list(GET parts 4 errors)
    list(GET parts 5 status)
    set(error_patterns "")
    if(NOT errors STREQUAL "")
        set(error_patterns "-DERRORS=${INJECT}/${errors}")
    endif()
    add_test(NAME inject_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${INJECT}/service.anti"
            "-DOPTIONS=${options}"
            "-DARGS=${arguments}"
            "-DEXPECTED=${INJECT}/${expected}"
            ${error_patterns}
            "-DSTATUS=${status}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/inject-${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trace.cmake")
endforeach()

# The `[inject]` and `[inject.test]` tables of a manifest. `anti test`
# runs in the project root, reads both and lays the test table over the
# plain one, per interface. The one test of the module adds the label of
# its logger to the reading of its clock. The test table names the
# first provider and the plain table the second, so the sum says which
# provider filled each field.
foreach(mode dev release)
    set(extra "")
    if(mode STREQUAL "release")
        set(extra --release)
    endif()
    add_test(NAME inject_manifest_${mode}
        COMMAND anti test ${extra}
                --work "${CMAKE_CURRENT_BINARY_DIR}/inject-manifest-${mode}"
                -I src --runtime "${ANTIC_RUNTIME_DIR}"
                --llvm-mc "${ANTIC_LLVM_MC}" src/com/example/app.anti)
    set_tests_properties(inject_manifest_${mode} PROPERTIES
        WORKING_DIRECTORY "${INJECT}/project"
        PASS_REGULAR_EXPRESSION "1 tests in 1 modules, 0 modules failed")
endforeach()

# The six standard interfaces under `anti test`. The project names no
# provider for the build, so the link takes the default of each, and
# `[inject.test]` lays a fake over every one of them. The one test asks
# each interface for a value no default gives.
foreach(mode dev release)
    set(extra "")
    if(mode STREQUAL "release")
        set(extra --release)
    endif()
    add_test(NAME inject_standard_${mode}
        COMMAND anti test ${extra}
                --work "${CMAKE_CURRENT_BINARY_DIR}/inject-standard-${mode}"
                -I src --runtime "${ANTIC_RUNTIME_DIR}"
                --llvm-mc "${ANTIC_LLVM_MC}" src/com/example/standard.anti)
    set_tests_properties(inject_standard_${mode} PROPERTIES
        WORKING_DIRECTORY "${INJECT}/standard"
        PASS_REGULAR_EXPRESSION "1 tests in 1 modules, 0 modules failed")
endforeach()

# Plugins. The library of tests/plugin is built with --lib shared
# --no-runtime, so it carries no runtime and is bound against the host
# that loads it. The host asks it for the interface, counts what it made
# and closes it, and two programs take the provider of an injected field
# from it, one by path and one through the index of its directory.
#
# DESIGN: a Linux host links dynamically against glibc, because it can
# host a plugin, and a Windows plugin links against the import library of
# its host. The test runs on all three systems.
set(plugin_suffix ".dylib")
set(plugin_exe "")
if(ANTIC_HOST_OS STREQUAL "linux")
    set(plugin_suffix ".so")
elseif(ANTIC_HOST_OS STREQUAL "windows")
    set(plugin_suffix ".dll")
    set(plugin_exe ".exe")
endif()
add_test(NAME plugin_host
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/host.expected"
        "-DPROVIDED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/provided.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin.cmake")
# The three checks of "Versions": the chain of the interface, the
# fields it declares and the slots the program reaches through it.
add_test(NAME plugin_versions
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-versions"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_versions.cmake")

# ANTI_CONF names the file when the command line names none, and the
# command line wins when both name one. It is the one environment
# variable the runtime reads.
set_tests_properties(conf_environment conf_environment_option PROPERTIES
    ENVIRONMENT "ANTI_CONF=${CONF}/basic.toml")

# A value past 1023 bytes, and a path outside ASCII, reach the runtime
# whole on every system. The long one names a file that is not there, and
# the other a copy of basic.toml in a directory whose name is not ASCII.
string(REPEAT "long/" 220 conf_long)
set_tests_properties(conf_environment_long PROPERTIES
    ENVIRONMENT "ANTI_CONF=${CONF}/${conf_long}nowhere.toml")
set(conf_utf8 "${CMAKE_CURRENT_BINARY_DIR}/conf-ü€")
add_test(NAME conf_environment_utf8_file
    COMMAND "${CMAKE_COMMAND}" -E copy "${CONF}/basic.toml"
        "${conf_utf8}/basic.toml")
set_tests_properties(conf_environment_utf8_file PROPERTIES
    FIXTURES_SETUP conf_utf8)
set_tests_properties(conf_environment_utf8 PROPERTIES
    ENVIRONMENT "ANTI_CONF=${conf_utf8}/basic.toml"
    FIXTURES_REQUIRED conf_utf8)

# The readers of the symbol table and the line table, on the binaries
# that this host links for the other targets.
add_executable(symbols_probe unit/symbols_probe.c ../src/rt/symbols.c)
target_include_directories(symbols_probe PRIVATE ../src/rt)
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64)
    add_test(NAME trace_symbols_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DPROBE=$<TARGET_FILE:symbols_probe>"
            "-DSOURCE=${TRACE}/symbols.anti"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/symbols-${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_symbols.cmake")
    set_tests_properties(trace_symbols_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# The build id in the notice is the digest of the code. Two builds of one
# program carry the same id, and another program carries another. An
# object that opens and fails to read is refused.
if(NOT WIN32)
    add_test(NAME program_build_id
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DOTHER=${CMAKE_CURRENT_SOURCE_DIR}/programs/args.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/build-id"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_build_id.cmake")
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
            "-DWANTED=ANTI_LICENSES_BEGIN|build [0-9a-f]+|package anti.rt [0-9.]+ 0BSD|package anti [0-9.]+ 0BSD|package com.example.hello 2.0.0 MIT"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/licenses-std"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_licenses.cmake")
endif()

# Libraries for C, built with antic --lib and used from C programs on the
# host. Windows compiles the programs with the pinned clang against the
# xwin sysroot of the runtime archive.
find_program(ANTIC_LLVM_AR NAMES llvm-ar
    HINTS "${ANTIC_LLVM_DIR}/bin"
    DOC "llvm-ar from LLVM 23.1.1")

# The text of a float takes no digit from the C library in any runtime of
# the archive. std/float_text.anti holds the text itself.
if(ANTIC_LLVM_AR)
    add_test(NAME float_digits
        COMMAND "${CMAKE_COMMAND}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/float_digits"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_float_digits.cmake")
endif()
if(ANTIC_LLVM_AR)
    foreach(case static shared exports two loader header bundle classes
            failing tuples flags variants nested simd names handlers ledger
            generics optional)
        # The C program of ledger starts its threads with pthreads.
        if(case STREQUAL "ledger" AND WIN32)
            continue()
        endif()
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
                "-DTARGET=${ANTIC_HOST_TARGET}"
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_clib.cmake")
    endforeach()
endif()
# Two bundled libraries of each Windows target on every host. The pinned
# clang compiles the C program for the target and lld-link links it, and
# nothing runs.
if(ANTIC_LLVM_AR AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(target windows-x86_64 windows-arm64)
        if(target STREQUAL ANTIC_HOST_TARGET)
            continue()
        endif()
        add_test(NAME clib_bundle_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DLLVM_AR=${ANTIC_LLVM_AR}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/clib"
                "-DDUMP=${CMAKE_CURRENT_SOURCE_DIR}/dump"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/clib-${target}"
                "-DCASE=bundle"
                "-DCC=${CMAKE_C_COMPILER}"
                "-DCXX=${CMAKE_C_COMPILER}"
                "-DTARGET=${target}"
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_DIR}/bin/llvm-objdump"
                -DCROSS=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_clib.cmake")
        set_tests_properties(clib_bundle_${target} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endif()

# A function that may fail and returns a tuple takes one out pointer, the
# last parameter, and not one per element. The header of clib_tuples
# pins the same signature on the C side.
add_test(NAME ir_tuple_out
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/tuples.anti"
        "-DLINE=export fn com.example.tuples.tuples_divide(%0: i64, %1: i64, %2: ptr) -> ptr {"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ir_line.cmake")

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
        "layout_unit|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_unit.s,layout_unit.anti"
        "layout_size|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_size.s,layout_size.anti"
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
        "construct_sets|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/construct_sets.s,construct_sets.anti"
        "construct_forms|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/construct_forms.s,construct_forms.anti"
        "construct_unknown|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/construct_unknown.s,construct_unknown.anti"
        "catch_place|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/catch_place.s,catch_place.anti"
        "failing|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/failing.s,failing.anti"
        "own_moves|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/own_moves.s,own_moves.anti"
        "own_params|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/own_params.s,own_params.anti"
        "lent_params|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/lent_params.s,lent_params.anti"
        "lent_slices|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/lent_slices.s,lent_slices.anti"
        "index_several|-S,-o,${LISTINGS}/index_several.s,index_several.anti"
        "for_patterns|-S,-o,${LISTINGS}/for_patterns.s,for_patterns.anti"
        "lent_derived|-S,-o,${LISTINGS}/lent_derived.s,lent_derived.anti"
        "owning_values|-S,-o,${LISTINGS}/owning_values.s,owning_values.anti"
        "destroy_locals|-S,-o,${LISTINGS}/destroy_locals.s,destroy_locals.anti"
        "lent_tuples|-S,-o,${LISTINGS}/lent_tuples.s,lent_tuples.anti"
        "walk_copy|-S,-o,${LISTINGS}/walk_copy.s,walk_copy.anti"
        "walk_lent|-S,-o,${LISTINGS}/walk_lent.s,walk_lent.anti"
        "fn_may_fail|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/fn_may_fail.s,fn_may_fail.anti"
        "concrete_signature|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/concrete_signature.s,concrete_signature.anti"
        "qualified_bodies|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/qualified_bodies.s,qualified_bodies.anti"
        "qualified_calls|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/qualified_calls.s,qualified_calls.anti"
        "abstract_call|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/abstract_call.s,abstract_call.anti"
        "link_framework|-S,-o,${LISTINGS}/link_framework.s,link_framework.anti"
        "link_framework_syntax|-S,-o,${LISTINGS}/link_framework_syntax.s,link_framework_syntax.anti"
        "inherits_body|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/inherits_body.s,inherits_body.anti"
        "fail_forms|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/fail_forms.s,fail_forms.anti"
        "fail_no_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/fail_no_import.s,fail_no_import.anti"
        "defaults|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/defaults.s,defaults.anti"
        "tuples|-S,-o,${LISTINGS}/tuples.s,tuples.anti"
        "switch_fallthrough|-S,-o,${LISTINGS}/switch_fallthrough.s,switch_fallthrough.anti"
        "variant_cover|-S,-o,${LISTINGS}/variant_cover.s,variant_cover.anti"
        "switch_str|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/switch_str.s,switch_str.anti"
        "switch_str_no_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/switch_str_no_import.s,switch_str_no_import.anti"
        "hex_bytes|-S,-o,${LISTINGS}/hex_bytes.s,hex_bytes.anti"
        "half|-S,-o,${LISTINGS}/half.s,half.anti"
        "format_literal|-S,-o,${LISTINGS}/format_literal.s,format_literal.anti"
        "format_spec|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/format_spec.s,format_spec.anti"
        "format_types|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/format_types.s,format_types.anti"
        "format_no_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/format_no_import.s,format_no_import.anti"
        "format_call|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/format_call.s,format_call.anti"
        "deserialize_alloc|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/deserialize_alloc.s,deserialize_alloc.anti"
        "deserialize_no_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/deserialize_no_import.s,deserialize_no_import.anti"
        "destroy_from|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/destroy_from.s,destroy_from.anti"
        "destroy_from_no_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/destroy_from_no_import.s,destroy_from_no_import.anti"
        "transient|-S,-o,${LISTINGS}/transient.s,transient.anti"
        "member_keywords|-S,-o,${LISTINGS}/member_keywords.s,member_keywords.anti"
        "simd_decl|-S,-o,${LISTINGS}/simd_decl.s,simd_decl.anti"
        "simd_ops|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/simd_ops.s,simd_ops.anti"
        "inject_type|-S,-o,${LISTINGS}/inject_type.s,inject_type.anti"
        "inject_literal|-S,-o,${LISTINGS}/inject_literal.s,inject_literal.anti"
        "inject_missing|-S,-o,${LISTINGS}/inject_missing.s,inject_missing.anti"
        "inject_wrong|-S,--inject,inject_wrong.Clock=inject_wrong.Console.get,-o,${LISTINGS}/inject_wrong.s,inject_wrong.anti"
        "inject_provider|-S,--inject,inject_provider.Logger=inject_provider.nowhere,--inject,inject_provider.Clock=inject_provider.ticks,-o,${LISTINGS}/inject_provider.s,inject_provider.anti"
        "inject_cycle|-S,--inject,inject_cycle.Logger=inject_cycle.Console.get,--inject,inject_cycle.Clock=inject_cycle.Watch.get,-o,${LISTINGS}/inject_cycle.s,inject_cycle.anti"
        "provides_lines|-S,-o,${LISTINGS}/provides_lines.s,provides_lines.anti"
        "plugin_calls|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/plugin_calls.s,plugin_calls.anti"
        "compound_wrapping|-S,-o,${LISTINGS}/compound_wrapping.s,compound_wrapping.anti"
        "long_float|-S,-o,${LISTINGS}/long_float.s,long_float.anti"
        "allow_names|-S,-o,${LISTINGS}/allow_names.s,allow_names.anti"
        "allow_places|-S,-o,${LISTINGS}/allow_places.s,allow_places.anti"
        "allow_unused|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/allow_unused.s,allow_unused.anti"
        "catch_none|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/catch_none.s,catch_none.anti"
        "nested_public|-S,-o,${LISTINGS}/nested_public.s,nested_public.anti"
        "nested_outside|-S,-o,${LISTINGS}/nested_outside.s,nested_outside.anti"
        "nested_forms|-S,-o,${LISTINGS}/nested_forms.s,nested_forms.anti"
        "nested_export|-S,-o,${LISTINGS}/nested_export.s,nested_export.anti"
        "hooks|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/hooks.s,hooks.anti"
        "closures|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/closures.s,closures.anti"
        "optional_values|-S,-o,${LISTINGS}/optional_values.s,optional_values.anti"
        "snapshots|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/snapshots.s,snapshots.anti"
        "synchronized|-S,-o,${LISTINGS}/synchronized.s,synchronized.anti"
        "concurrent|-S,-o,${LISTINGS}/concurrent.s,concurrent.anti"
        "concurrent_forms|-S,-o,${LISTINGS}/concurrent_forms.s,concurrent_forms.anti"
        "pointer_leak|-S,-o,${LISTINGS}/pointer_leak.s,pointer_leak.anti"
        "mutex_copy|-S,-o,${LISTINGS}/mutex_copy.s,mutex_copy.anti"
        "compare_swap|-S,-o,${LISTINGS}/compare_swap.s,compare_swap.anti"
        "pattern_malformed|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_malformed.s,pattern_malformed.anti"
        "pattern_exponential|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_exponential.s,pattern_exponential.anti"
        "pattern_import|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_import.s,pattern_import.anti"
        "pattern_level|--target,linux-x86_64,--cpu,v1,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_level,pattern_level.anti"
        "pattern_methods|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_methods.s,pattern_methods.anti"
        "pattern_bytes|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_bytes.s,pattern_bytes.anti"
        "generics|--front-end,generics.anti"
        "generic_syntax|--front-end,generic_syntax.anti"
        "constraints|--front-end,constraints.anti"
        "hashing|--front-end,hashing.anti"
        "direct_imports|--front-end,--runtime,${ANTIC_RUNTIME_DIR},direct_imports.anti"
        "walk_interfaces|--front-end,--runtime,${ANTIC_RUNTIME_DIR},walk_interfaces.anti"
        "generic_copies|-S,-o,${LISTINGS}/generic_copies.s,generic_copies.anti")
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
        "generics.types|--dump-types,${DUMP}/generics.anti"
        "constraints.types|--dump-types,${DUMP}/constraints.anti"
        "header/main.ir|--dump-ir,${DUMP}/header/main.anti"
        "size_of/main.ir|--dump-ir,${DUMP}/size_of/main.anti"
        "size_of/main.linux-arm64.sel|--dump-select,--target,linux-arm64,${DUMP}/size_of/main.anti"
        "c_long/main.linux-x86_64.sel|--dump-select,--target,linux-x86_64,${DUMP}/c_long/main.anti"
        "c_long/main.windows-x86_64.sel|--dump-select,--target,windows-x86_64,${DUMP}/c_long/main.anti"
        "devirt.ir|--dump-ir,${DUMP}/devirt.anti"
        "devirt.dev.ir|--dev,--dump-ir,${DUMP}/devirt.anti"
        "devirt.opt|--dump-opt,${DUMP}/devirt.anti"
        "devirt.dev.opt|--dev,--dump-opt,${DUMP}/devirt.anti"
        "copies_merge.opt|--dump-opt,${DUMP}/copies_merge.anti"
        "copies_merge.dev.opt|--dev,--dump-opt,${DUMP}/copies_merge.anti"
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
            floats structs strings fnptr half)
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
    # A copy of a generic in dev mode is link-once: a weak definition on
    # ELF and Mach-O and a COMDAT section on COFF.
    if(target MATCHES "^linux")
        set(once ".weak copies_merge.pick$3c$2acopies_merge.Person$3e")
    elseif(target MATCHES "^macos")
        set(once ".weak_definition _copies_merge.pick$3c$2acopies_merge.Person$3e")
    else()
        set(once ".text,\"xr\",discard,_A12copies_merge_pick$3c$2acopies_merge.Person$3e")
    endif()
    add_test(NAME asm_copies_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DTARGET=${target}"
            "-DTRIPLE=${triple}"
            "-DOPTIONS=--dev"
            "-DHOLDS=${once}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/copies_merge.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-copies"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    # The debug information names a copy as a person reads it, the DWARF
    # entry on ELF and Mach-O and the CodeView record on COFF, and the
    # symbol keeps the escaped name.
    add_test(NAME asm_debug_copies_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DTARGET=${target}"
            "-DTRIPLE=${triple}"
            "-DOPTIONS=-g"
            "-DHOLDS=.asciz \"copies_merge.pick<int>\"|pick$3cint$3e:"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/copies_merge.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-debug-copies"
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
    # The calls of the runtime that convert f16 below x86-64-v3.
    if(target MATCHES "x86_64$")
        add_test(NAME asm_half_v1_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DTARGET=${target}"
                "-DTRIPLE=${triple}"
                "-DOPTIONS=--cpu|v1"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/half.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-v1"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    endif()
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

