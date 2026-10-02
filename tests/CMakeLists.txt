# Unit tests link the compiler's code directly. Program tests in
# tests/programs compile .anti files with antic and run the result.

# ANTIC_LLVM_MC and ANTIC_LLVM_AR, the llvm-mc and the llvm-ar of the
# pinned release, stand in tools/pinned-compiler.cmake, which runs before
# the first test reads either.

# The files of src/rt/ a test program compiles, by the names
# ANTIC_RUNTIME_SOURCES of CMakeLists.txt gives them. A name the runtime does
# not build stops the configure step, so no test compiles a file the
# runtime dropped or renamed. Each program of the runtime below takes the
# part its subject needs, with stand-ins for the rest.
function(antic_rt_sources out)
    set(paths "")
    foreach(name IN LISTS ARGN)
        if(NOT name IN_LIST ANTIC_RUNTIME_SOURCES)
            message(FATAL_ERROR "src/rt/${name}.c is no source of the runtime")
        endif()
        list(APPEND paths "${PROJECT_SOURCE_DIR}/src/rt/${name}.c")
    endforeach()
    set(${out} ${paths} PARENT_SCOPE)
endfunction()

# The unit tests link anti_core, every file of anti but its main, and the
# files of the runtime they test.
antic_rt_sources(unit_rt_sources assert atomic cpu f16 fs platform_posix
    platform_windows text)
add_executable(antic_unit_tests
    unit/test_main.c
    unit/pipeline.c
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
    unit/test_fmt.c
    unit/test_files.c
    unit/test_tool_platform.c
    unit/test_interface.c
    ${unit_rt_sources})
target_link_libraries(antic_unit_tests PRIVATE anti_core ${CMAKE_DL_LIBS})
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
# One test per file of the unit tests, unit_<name> for test_<name>.c, each
# in a directory of its own for the files it writes.
get_target_property(unit_sources antic_unit_tests SOURCES)
foreach(source IN LISTS unit_sources)
    if(source MATCHES "^unit/test_(.+)\\.c$" AND
       NOT CMAKE_MATCH_1 STREQUAL "main")
        set(group "${CMAKE_MATCH_1}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/unit/${group}")
        add_test(NAME unit_${group} COMMAND antic_unit_tests ${group}
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/unit/${group}")
    endif()
endforeach()

# The allocation helpers of anti when the memory is refused, in a program
# of its own, since each ends the process.
add_executable(files_oom_tests unit/test_files_oom.c)
target_link_libraries(files_oom_tests PRIVATE anti_core)
target_include_directories(files_oom_tests PRIVATE ../src/anti)
add_test(NAME files_oom
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:files_oom_tests>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/files-oom"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_files_oom.cmake")

# The allocation helpers of antic when the memory is refused or a size
# overflows, in a program of its own, since each ends the process.
add_executable(alloc_oom_tests unit/test_alloc_oom.c)
target_link_libraries(alloc_oom_tests PRIVATE antic_core)
add_test(NAME alloc_oom
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:alloc_oom_tests>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/alloc-oom"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_alloc_oom.cmake")

# antic allocates through src/antic/alloc.c alone, and the front end
# includes no header of the IR for its memory.
# The programs of tests/ that link antic_core take their memory from
# src/antic/alloc.c as well.
set(core_test_sources
    "$<TARGET_PROPERTY:antic_unit_tests,SOURCES>"
    "$<TARGET_PROPERTY:files_oom_tests,SOURCES>"
    "$<TARGET_PROPERTY:alloc_oom_tests,SOURCES>"
    "$<TARGET_PROPERTY:symbols_probe,SOURCES>")
add_test(NAME one_allocator
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DUNIT_SOURCES=$<JOIN:${core_test_sources},|>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_one_allocator.cmake")

# Every host branch of antic, anti and the runtime stands in their
# platform layers, src/antic/platform.c and src/anti/platform.c with their
# headers and src/rt/platform.h with its two files.
add_test(NAME platform_layer
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_platform_layer.cmake")

# Every compiler extension of the runtime stands in its platform layer or
# in src/rt/atomic.c.
add_test(NAME rt_extensions
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_extensions.cmake")

# The runtime calls the UTF-16 entry points of Windows alone.
add_test(NAME rt_wide_text
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_wide_text.cmake")

# anti reaches antic through driver.h, antic.h and the shared helpers
# alone, M02 of the audit.
add_test(NAME anti_interface
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_interface.cmake")

# The suffix antic gives an object of the host target, the one that
# src/antic/target.c names: .obj on Windows and .o elsewhere. Every dev
# object a test links is named with it.
if(ANTIC_HOST_TARGET MATCHES "^windows-")
    set(anti_object ".obj")
else()
    set(anti_object ".o")
endif()

# DESIGN: the dev objects of the standard library are compiled once per
# suite, each by the test dev_object_<name> into anti_<name>_dev of this
# directory with the fixture <name>_dev_object. A test that links one names
# the module alone, by its path under std/anti/ such as lang or
# collection/list, and antic_dev_inputs gives the objects and the fixtures
# from that one list, so the two cannot disagree.
set(ANTIC_DEV_MODULES lang regex mem text reflect runtime collection
    collection/pool collection/tree collection/queue collection/list
    collection/deque collection/ring collection/grid collection/sorted
    collection/map collection/set collection/concurrent
    collection/synchronized)
foreach(module IN LISTS ANTIC_DEV_MODULES)
    string(REPLACE "/" "_" name "${module}")
    add_test(NAME dev_object_${name}
        COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
                -I "${ANTIC_RUNTIME_DIR}/std"
                -o "${CMAKE_CURRENT_BINARY_DIR}/anti_${name}_dev"
                "${ANTIC_RUNTIME_DIR}/std/anti/${module}.antl")
    set_tests_properties(dev_object_${name} PROPERTIES
        FIXTURES_SETUP ${name}_dev_object)
endforeach()

# Set objects to the dev objects of the modules after the two names, and
# fixtures to the fixtures that compile them.
function(antic_dev_inputs objects fixtures)
    set(found_objects "")
    set(found_fixtures "")
    foreach(module IN LISTS ARGN)
        if(NOT module IN_LIST ANTIC_DEV_MODULES)
            message(FATAL_ERROR "no test compiles the dev object of ${module}")
        endif()
        string(REPLACE "/" "_" name "${module}")
        list(APPEND found_objects
            "${CMAKE_CURRENT_BINARY_DIR}/anti_${name}_dev${anti_object}")
        list(APPEND found_fixtures ${name}_dev_object)
    endforeach()
    set(${objects} ${found_objects} PARENT_SCOPE)
    set(${fixtures} ${found_fixtures} PARENT_SCOPE)
endfunction()

# A program test of run_program.cmake, which builds source with antic,
# llvm-mc and the runtime archive of this tree, runs it and compares the
# result with the expected file beside it.
#   antic_program_test(<name> <source> WORK <dir> [EXPECTED <file>]
#                      [OPTIONS <option>...] [DEV <module>...]
#                      [OBJECTS <object>...] [UNSET <variable>...])
# OPTIONS, OBJECTS and UNSET reach the script as run_program.cmake names
# them. DEV names the modules whose dev objects the link takes, which
# antic_dev_inputs resolves.
function(antic_program_test name source)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "WORK;EXPECTED"
                          "OPTIONS;DEV;OBJECTS;UNSET")
    if(NOT arg_WORK)
        message(FATAL_ERROR "antic_program_test(${name}) names no WORK")
    endif()
    antic_dev_inputs(dev_objects dev_fixtures ${arg_DEV})
    list(JOIN arg_OPTIONS "," options)
    set(objects ${dev_objects} ${arg_OBJECTS})
    list(JOIN objects "," objects)
    set(optional "")
    if(arg_EXPECTED)
        list(APPEND optional "-DEXPECTED=${arg_EXPECTED}")
    endif()
    if(arg_UNSET)
        list(JOIN arg_UNSET "," unset)
        list(APPEND optional "-DUNSET=${unset}")
    endif()
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOPTIONS=${options}"
            "-DOBJECTS=${objects}"
            ${optional}
            "-DWORK=${arg_WORK}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    if(dev_fixtures)
        set_property(TEST ${name} PROPERTY FIXTURES_REQUIRED ${dev_fixtures})
    endif()
endfunction()

# A program of library modules under com/example and a program module,
# through run_modules.cmake, once against the library files in release mode
# and once from an object per module in dev mode: <name>_release and
# <name>_dev.
#   antic_modules_test(<name> ROOT <dir> LIBRARIES <module>...
#                      PROGRAM <module> WORK <prefix> [STD <module>...]
#                      [OPTIONS <option>...] [LIB_OPTIONS <option>...]
#                      [TIMEOUT <seconds>])
# STD names the modules of the standard library the program reaches,
# whose dev objects the dev link takes from antic_dev_inputs.
function(antic_modules_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ROOT;PROGRAM;WORK;TIMEOUT"
                          "LIBRARIES;STD;OPTIONS;LIB_OPTIONS")
    antic_dev_inputs(std_objects std_fixtures ${arg_STD})
    list(JOIN std_objects "," std_objects)
    list(JOIN arg_LIBRARIES "," libraries)
    list(JOIN arg_OPTIONS "," options)
    list(JOIN arg_LIB_OPTIONS "," lib_options)
    foreach(mode release dev)
        set(objects "")
        if(mode STREQUAL "dev")
            set(objects "${std_objects}")
        endif()
        add_test(NAME ${name}_${mode}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DROOT=${arg_ROOT}"
                "-DLIBRARIES=${libraries}"
                "-DLIB_OPTIONS=${lib_options}"
                "-DOPTIONS=${options}"
                "-DSTD_OBJECTS=${objects}"
                "-DPROGRAM=${arg_PROGRAM}"
                "-DMODE=${mode}"
                "-DWORK=${arg_WORK}-${mode}"
                "-DOBJECT=${anti_object}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_modules.cmake")
        if(mode STREQUAL "dev" AND std_fixtures)
            set_property(TEST ${name}_${mode} PROPERTY
                FIXTURES_REQUIRED ${std_fixtures})
        endif()
        if(arg_TIMEOUT)
            set_tests_properties(${name}_${mode} PROPERTIES
                TIMEOUT ${arg_TIMEOUT})
        endif()
    endforeach()
endfunction()

# A dump of antic compared byte for byte with an expected file, through
# run_dump.cmake. The name says the stage and the target:
# dump_<stage>_<subject>, with _<target> after it for a dump of one target.
#   antic_dump_test(<stage> <subject> <expected> [TARGET <target>]
#                   [ARGS <argument>...])
# tokens, ast, types, ir, opt, select and alloc give --dump-<stage>, and
# asm gives -S into a file of its own, which the test compares. ARGS are
# the other arguments of antic, the source among them.
set(ANTIC_DUMPS "${CMAKE_CURRENT_BINARY_DIR}/dumps")
file(MAKE_DIRECTORY "${ANTIC_DUMPS}")
function(antic_dump_test stage subject expected)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "TARGET" "ARGS")
    set(name dump_${stage}_${subject})
    set(arguments "")
    if(arg_TARGET)
        string(APPEND name _${arg_TARGET})
        set(arguments --target ${arg_TARGET})
    endif()
    set(output "")
    if(stage STREQUAL "asm")
        set(file "${ANTIC_DUMPS}/${name}.s")
        list(PREPEND arguments -S -o "${file}")
        set(output "-DOUTPUT=${file}")
    elseif(stage MATCHES "^(tokens|ast|types|ir|opt|select|alloc)$")
        list(PREPEND arguments --dump-${stage})
    else()
        message(FATAL_ERROR "antic dumps no stage ${stage}")
    endif()
    list(APPEND arguments ${arg_ARGS})
    list(JOIN arguments "," command)
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=${command}"
            ${output}
            "-DEXPECTED=${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endfunction()

# llvm-objdump and llvm-readobj of the runtime archive, with the suffix of
# an executable of the host, .exe on Windows. Every test names them by
# these two. Spelled without the suffix, the file was missing on Windows,
# and each test that reads a library there skipped.
set(ANTIC_RUNTIME_OBJDUMP
    "${ANTIC_RUNTIME_DIR}/bin/llvm-objdump${CMAKE_EXECUTABLE_SUFFIX}")
set(ANTIC_RUNTIME_READOBJ
    "${ANTIC_RUNTIME_DIR}/bin/llvm-readobj${CMAKE_EXECUTABLE_SUFFIX}")

# The signal table of the runtime, in a program of its own. src/rt/signal.h
# shares its name with the header of C, so the include path of the unit
# tests would hide the one of C from src/rt/signal.c. The test starts
# threads, and so does the reader of the platform layer.
find_package(Threads REQUIRED)
antic_rt_sources(signal_sources signal atomic utf platform_posix
    platform_windows)
add_executable(rt_signal_tests unit/test_signal.c ${signal_sources})
target_link_libraries(rt_signal_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
add_test(NAME rt_signal COMMAND rt_signal_tests)

# rt.configure while another thread reads the keys, in a program of its own
# with stand-ins for the loader. The runtime of an Anti program carries no
# sanitizer, and this program does in the sanitizer builds.
antic_rt_sources(conf_sources conf assert toml atomic fs hash utf
    platform_posix platform_windows)
add_executable(rt_conf_tests unit/test_conf_race.c ${conf_sources})
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
antic_rt_sources(plugin_sources plugin assert loaded atomic digest text toml
    utf platform_posix platform_windows)
add_executable(rt_plugin_tests unit/test_plugin_race.c ${plugin_sources})
target_link_libraries(rt_plugin_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_plugin_tests PRIVATE
    "PLUGIN_ONE=\"$<TARGET_FILE:plugin_table_one>\""
    "PLUGIN_TWO=\"$<TARGET_FILE:plugin_table_two>\"")
add_dependencies(rt_plugin_tests plugin_table_one plugin_table_two)
add_test(NAME rt_plugin_race COMMAND rt_plugin_tests)

# The platform layer of the runtime on the host. Both platform files go
# in, and the one of the other system compiles to nothing. The test starts
# threads of the layer. The library of
# an empty plugin table is copied into a directory whose name is outside
# ASCII, which a loader that takes the ANSI code page cannot open.
antic_rt_sources(platform_sources utf platform_posix platform_windows)
add_executable(rt_platform_tests unit/test_platform.c ${platform_sources})
target_link_libraries(rt_platform_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
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
# The arguments after the name of the program are the ones
# test_platform.c reads back, one with a blank and one outside ASCII.
add_test(NAME rt_platform COMMAND rt_platform_tests one "two words" "ü")

# The loader against a table of a plugin with one value damaged at a time.
add_library(plugin_bad SHARED unit/plugin_bad.c)
add_executable(rt_plugin_table_tests unit/test_plugin_table.c
    ${plugin_sources})
target_link_libraries(rt_plugin_table_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_plugin_table_tests PRIVATE
    "PLUGIN_BAD=\"$<TARGET_FILE:plugin_bad>\"")
add_dependencies(rt_plugin_table_tests plugin_bad)
add_test(NAME rt_plugin_table COMMAND rt_plugin_table_tests)

# add_expect_test(<name> STATUS <status> [DIRECTORY <dir>]
#                 EXPECTED <file> | PATTERN <regex>
#                 COMMAND <program> <arguments>...)
# registers a test of run_expect.cmake, which checks the exit status of
# the program beside its output. Every test of a refusal or of a warning
# goes through it, and none sets PASS_REGULAR_EXPRESSION, under which
# ctest ignores the status. The command line of the test carries no `;`,
# no `,` inside an argument and no line break, so a pattern holds neither
# and a test that needs one compares a file with EXPECTED.
function(add_expect_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "STATUS;DIRECTORY;EXPECTED;PATTERN"
                          "COMMAND")
    foreach(argument IN LISTS arg_COMMAND)
        if(argument MATCHES ",")
            message(FATAL_ERROR "${name}: the argument `${argument}` holds a `,`")
        endif()
    endforeach()
    if(arg_PATTERN MATCHES "[;\n]")
        message(FATAL_ERROR "${name}: the pattern holds a `;` or a line break")
    endif()
    list(JOIN arg_COMMAND "," command)
    set(values "-DCOMMAND=${command}" "-DSTATUS=${arg_STATUS}")
    if(DEFINED arg_DIRECTORY)
        list(APPEND values "-DDIRECTORY=${arg_DIRECTORY}")
    endif()
    if(DEFINED arg_EXPECTED)
        list(APPEND values "-DEXPECTED=${arg_EXPECTED}")
    endif()
    if(DEFINED arg_PATTERN)
        list(APPEND values "-DPATTERN=${arg_PATTERN}")
    endif()
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}" ${values}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_expect.cmake")
endfunction()

# run_expect.cmake refuses a program that prints the message of a refusal
# and exits 0, which a pattern of ctest lets pass, and no CMake file of
# the repository sets such a pattern.
add_test(NAME expect_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DEXPECT=${CMAKE_CURRENT_SOURCE_DIR}/run_expect.cmake"
        "-DROOT=${CMAKE_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/expect-runner"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_expect_self.cmake")

# The runtime archive holds the licence text of every component.
add_expect_test(runtime_licenses STATUS 0
    PATTERN "^BSD Zero Clause License"
    COMMAND "${CMAKE_COMMAND}" -E cat "${ANTIC_RUNTIME_DIR}/licenses/anti_rt.txt")

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
    add_test(NAME cpu_level_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DLIBRARY=${library}"
            "-DTARGET=${target}"
            "-DLEVEL=${level}"
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
                "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
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

# The warning flags of the build stand in tools/warnings.cmake and
# src/native/warnings.cmake alone, as docs/c-guidelines.md requires.
add_test(NAME warning_set
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warning_set.cmake")

# Rule 25: every symbol a runtime library of the archive defines starts
# with anti_rt_, on every target and level.
add_test(NAME rt_names
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_names.cmake")
set_tests_properties(rt_names PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# Each declaration of antl_io.h, parser_parser.h, sema_checker.h and
# lower_lowerer.h stands under the file that defines it, and the comment
# of each header names every file.
add_test(NAME header_sections
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_header_sections.cmake")

# A name that the tools and the runtime both spell, the markers of the
# notice, the index of plugins, the suffix of a symbols archive and the
# sysroot directory, stands in one header.
add_test(NAME names_once
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_names_once.cmake")

# The layout rule of "Repository layout" in CLAUDE.md, over git ls-files.
add_test(NAME repo_layout
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_repo_layout.cmake")

# The pre-commit hook refuses staged .md files that fail the docs-style
# checker, and the configure step installed it in this checkout.
add_test(NAME pre_commit_hook
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DHOOKS=${ANTI_HOOKS_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/pre-commit"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pre_commit.cmake")
set_tests_properties(pre_commit_hook PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# The version stands in tools/version alone, and both programs print
# that one. Step 10 of the release reads them from a fresh install.
# Each prints its line and nothing else, which the files below hold.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_version"
     "antic ${PROJECT_VERSION}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/expect/anti_version"
     "anti ${PROJECT_VERSION}\n")
add_expect_test(antic_version STATUS 0
    EXPECTED "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_version"
    COMMAND "$<TARGET_FILE:antic>" --version)
add_expect_test(anti_version STATUS 0
    EXPECTED "${CMAKE_CURRENT_BINARY_DIR}/expect/anti_version"
    COMMAND "$<TARGET_FILE:anti>" --version)

# The host target name has two spellings: ANTIC_HOST_TARGET here and the
# detection in src/antic/target.c. This test fails when they disagree.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_host_target"
     "${ANTIC_HOST_TARGET}\n")
add_expect_test(antic_host_target STATUS 0
    EXPECTED "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_host_target"
    COMMAND "$<TARGET_FILE:antic>" --print-host-target)

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

# A module that writes no function writes no empty CodeView subsection of
# symbols, which lld-link warns of.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME codeview_empty_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DSTD=${ANTIC_RUNTIME_DIR}/std"
            "-DSOURCE=${ANTIC_RUNTIME_DIR}/std/anti/collection/tree.antl"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/codeview-empty"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_codeview_empty.cmake")
endforeach()

# Every tool a build of this tree runs comes from the pinned release, and
# antic and anti were linked by the pinned lld. A reader's build takes the
# tools of the machine on purpose.
if(NOT ANTIC_SYSTEM_COMPILER)
    add_test(NAME pinned_tools
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK=${PROJECT_SOURCE_DIR}/tools/pinned-tools.cmake"
            "-DCACHE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            "-DCLANG_DIR=${ANTIC_CLANG_DIR}"
            "-DLLVM_DIR=${ANTIC_LLVM_DIR}"
            "-DOS=${ANTIC_HOST_OS}"
            "-DPROGRAMS=$<TARGET_FILE:antic>,$<TARGET_FILE:anti>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pinned_tools.cmake")
endif()

file(GLOB program_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/*.anti")
foreach(source IN LISTS program_sources)
    get_filename_component(name "${source}" NAME_WE)
    antic_program_test(program_${name}
        "${source}"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs")
endforeach()
# The program whose calls release mode makes direct, in dev mode, which
# keeps every call through a table.
antic_program_test(program_devirtualise_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/devirtualise.anti"
    OPTIONS "--dev"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# Snapshots in dev mode, where each module is an object of its own.
antic_program_test(program_snapshots_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/snapshots.anti"
    OPTIONS "--dev"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# Qualified bodies in dev mode, where every call through an interface
# reads a table this module wrote.
foreach(name IN ITEMS qualified_bodies qualified_chain qualified_single)
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
endforeach()
# The wrapping and saturating operators, mul_high and the flags form in
# dev mode, whose checks trap on the plain operators and on none of these.
foreach(name IN ITEMS wrapping saturating compound_wrapping mul_high flags
        carry_chain variants)
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
endforeach()
# The two forms of a walk in dev mode, whose loops over a collection
# carry the check of its changes.
antic_program_test(program_walk_forms_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/walk_forms.anti"
    OPTIONS "--dev"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# The default `==` and hash of a class without the field lists, which
# --no-reflect drops. The compiler writes both as code.
antic_program_test(program_eq_compiled_no_reflect
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/eq_compiled.anti"
    OPTIONS "--no-reflect"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-no-reflect")
# The default hash in dev mode, which writes it in each module apart, and
# the patterns of `for`.
foreach(name IN ITEMS hash_builtin hash_default for_patterns lent_derived
        lent_tuples owning_values destroy_locals temporaries owning_destruct
        statement_temps literal_moves variant_owning generic_moves caller_params
        owning_locals owned_buffers)
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
endforeach()
# The copies of generics in dev mode, which compiles each module apart and
# prints what the release build of the same program prints.
foreach(name IN ITEMS generic_copies generic_constants generic_methods
        generic_nested generic_sync generic_hooks generic_may_fail
        generic_fn_args generic_variants construct_keep_own walk_generic
        hash_generic generic_static walk_owned_parts dropped_failing
        handler_exit_temps)
    set(dev "")
    if(name STREQUAL "generic_may_fail" OR name STREQUAL "generic_fn_args" OR
       name STREQUAL "walk_generic" OR name STREQUAL "hash_generic" OR
       name STREQUAL "walk_owned_parts" OR name STREQUAL "dropped_failing" OR
       name STREQUAL "handler_exit_temps")
        set(dev lang)
    endif()
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        DEV ${dev}
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
endforeach()
# Locking and channels in dev mode, whose checks run inside every `sync`
# block and every arm of a `select`. sync_exits fails with the errors of
# anti.lang, so its dev link names the object of anti.lang.
foreach(name IN ITEMS sync_exits sync_workers channels select_two
        channel_workers synchronized concurrent sync_pairs sync_operators
        lock_free_queue)
    set(dev "")
    if(name STREQUAL "sync_exits")
        set(dev lang)
    endif()
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        DEV ${dev}
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
    set_tests_properties(program_${name}_dev PROPERTIES TIMEOUT 120)
endforeach()
# The pattern literals in dev mode, where the object of each module runs
# the constructor that compiles its own literals. The link names the
# objects of anti.lang and anti.regex, whose classes the registry holds.
antic_program_test(program_pattern_literals_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/pattern_literals.anti"
    OPTIONS "--dev"
    DEV lang regex
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# The methods of `str` in dev mode, where the functions of anti.regex that
# the checker calls in their place stand in the object of anti.regex.
antic_program_test(program_pattern_replace_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/pattern_replace.anti"
    OPTIONS "--dev"
    DEV lang regex
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# The registry of the classes in dev mode, where the module that links
# writes it. The program takes its allocator from anti.mem. A dev link
# names the object of anti.mem on the command line, and that of anti.lang,
# which anti.mem imports.
antic_program_test(program_deserialize_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/deserialize.anti"
    OPTIONS "--dev"
    DEV lang mem
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
# The methods of `[]byte` and `patch` in dev mode, where the byte
# literals compile in the constructor of the program's module. The
# conversions call anti.text, whose object imports anti.mem.
foreach(name IN ITEMS pattern_bytes pattern_patch)
    antic_program_test(program_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti"
        OPTIONS "--dev"
        DEV lang regex text mem
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-dev")
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
    antic_program_test(program_${source}_${case_name}
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${source}.anti"
        OPTIONS "${case_options}"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/programs/${case_expected}"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-hooks/${case_name}")
endforeach()
# `trace` follows the mode: a dev build runs the marked code's call hooks
# as --trace does, and --no-trace turns them off again, which gives the
# output of release.
foreach(case "dev|--dev|hook_trace.trace.expected"
        "dev_no_trace|--dev,--no-trace|hook_trace.expected")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 case_name)
    list(GET parts 1 case_options)
    list(GET parts 2 case_expected)
    antic_program_test(program_hook_trace_${case_name}
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/hook_trace.anti"
        OPTIONS "${case_options}"
        DEV lang
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/programs/${case_expected}"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-hooks/${case_name}")
endforeach()

# Ownership without a field list: delete, destroy and dup call the
# teardown and the copy that the compiler writes for each class.
antic_program_test(program_delete_owned_no_reflect
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/delete_owned.anti"
    OPTIONS "--no-reflect"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-no-reflect")

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
    set(abi_wchar_x86_64_options EXPECTED "${abi_wchar_x86_64_expected}")
    foreach(case structs wchar simd)
        antic_program_test(program_abi_${case}_macos-x86_64
            "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_${case}.anti"
            OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_${case}_x86_64>,,>"
            OPTIONS "${ANTIC_X86_64_OPTIONS}"
            ${abi_${case}_x86_64_options}
            WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64")
    endforeach()
    if(EXISTS "${ANTIC_RAYLIB_DIR}/src/raymath.h")
        abi_x86_64_object(abi_raymath_x86_64 abi/raymath.c)
        target_include_directories(abi_raymath_x86_64 PRIVATE
            "${ANTIC_RAYLIB_DIR}/src")
        antic_program_test(program_abi_raymath_macos-x86_64
            "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.anti"
            OPTIONS "${ANTIC_X86_64_OPTIONS}"
            OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_raymath_x86_64>,,>"
            EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected"
            WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64")
    endif()
    # DESIGN: a program that holds anti.regex links PCRE2, which stands at
    # the default level x86-64-v3 alone, and a program below it is refused
    # at link. Rosetta runs no v3 program, so the programs of patterns run
    # on the host target alone. errors/pattern_level.anti holds the
    # refusal.
    set(rosetta_sources ${program_sources})
    list(FILTER rosetta_sources EXCLUDE REGEX "/(pattern_[a-z]+|regex_compile|eq_handles)\\.anti$")
    foreach(source IN LISTS rosetta_sources)
        get_filename_component(name "${source}" NAME_WE)
        antic_program_test(program_${name}_macos-x86_64
            "${source}"
            OPTIONS "${ANTIC_X86_64_OPTIONS}"
            WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-x86_64")
    endforeach()
    set_tests_properties(program_args_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_GREETING=héllo")
    set_tests_properties(program_nullable_macos-x86_64 PROPERTIES
        ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")
    foreach(name IN ITEMS sync_exits sync_workers channels select_two
            channel_workers parallel_join synchronized concurrent sync_pairs
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

# macOS malloc fills fresh memory with 0xaa under MallocScribble, so an
# element of `alloc(T, n)` in tests/programs/alloc_owning.anti that the
# compiler failed to zero holds a table there, as the heap of the Linux VM
# gave it one. Other systems ignore the variable.
set_tests_properties(program_alloc_owning PROPERTIES
    ENVIRONMENT "MallocScribble=1")
if(TEST program_alloc_owning_macos-x86_64)
    set_tests_properties(program_alloc_owning_macos-x86_64 PROPERTIES
        ENVIRONMENT "MallocScribble=1")
endif()

# The workers of tests/programs/channel_workers.anti wait on each other, so
# each needs a thread of the pool. Its .args file passes --anti.threads=4,
# which every run of the program takes. A program that leaves a mutex
# locked, a channel waiting or a waiter of the pool asleep stops, and the
# timeout reports it. tests/programs/parallel_join.anti passes
# --anti.threads=6 for the joiner, the job it waits for and a chunk.
foreach(name IN ITEMS sync_exits sync_workers channels select_two
        channel_workers parallel_join synchronized concurrent sync_pairs
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
        set(expected EXPECTED "${win32_expected}")
    endif()
    antic_program_test(std_${name}
        "${source}"
        ${expected}
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std")
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
set(collection_dev lang mem text collection)
antic_program_test(std_collection_parts_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/std/collection_parts.anti"
    OPTIONS "--dev"
    DEV ${collection_dev}
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")

# The collections on handles and the heap and the sequences in dev mode,
# and copies of collections that fields of the program reach.
# Each module below anti.collection writes an object of its own, and a
# program links the objects of every module it reaches.
foreach(case "pool|pool" "tree|pool,tree" "queue|queue" "list|list"
        "deque|deque" "ring|ring" "grid|grid" "fields|list,map,pool")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 modules)
    string(REPLACE "," ";" modules "${modules}")
    list(TRANSFORM modules PREPEND "collection/")
    antic_program_test(std_collection_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/collection_${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} ${modules}
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# Shared<T> in dev mode. Its copies come from the library file of anti.mem
# into the module of the program, beside those of anti.collection.
antic_program_test(std_shared_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/std/shared.anti"
    OPTIONS "--dev"
    DEV ${collection_dev}
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
# The sorted collections in dev mode, whose copies come from the library
# file of anti.collection.sorted and whose base comes from anti.collection.
# The link names no object of anti.reflect, which no collection imports.
foreach(name IN ITEMS sorted_map sorted_set sorted_order sorted_trap)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/sorted
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The maps of anti.collection.map and the sets of anti.collection.set in
# dev mode, with the object of each module beside those of the modules it
# imports. The sets stand on the tables of the maps.
foreach(name IN ITEMS map map_tuple hash_map set hash_set)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/map collection/set
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The collections threads share, of anti.collection.concurrent, in dev
# mode. The module stands on the maps and on the error of a versioned set
# in anti.collection.pool, so the link names the objects of both.
foreach(name IN ITEMS concurrent_map concurrent_map_threads
        concurrent_map_equal spsc_ring spsc_ring_threads)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/map collection/pool
            collection/concurrent
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The collections behind one lock each, of anti.collection.synchronized, in
# dev mode. The module stands on the list, the maps, the sets, the pool and
# the walk of anti.collection.concurrent, so the link names the object of
# each.
foreach(name IN ITEMS sync_list sync_list_threads sync_map sync_map_threads
        sync_set sync_set_threads sync_pool sync_pool_threads sync_equal)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/list collection/map collection/set
            collection/pool collection/concurrent collection/synchronized
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# `dup` and the hash of every collection in dev mode. Each program holds
# every collection, so the link names the object of every collection
# module.
foreach(name IN ITEMS dup hash equals)
    antic_program_test(std_collection_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/collection_${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/list collection/deque
            collection/ring collection/grid collection/map collection/set
            collection/sorted collection/pool collection/tree
            collection/queue collection/concurrent
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The runs with threads spin while the ring is full or empty and wait on the
# locks of the maps and of the synchronized collections. A side that stops
# for good leaves the other spinning or waiting, and the timeout reports it.
foreach(name IN ITEMS std_concurrent_map_threads std_spsc_ring_threads
        std_concurrent_map_threads_dev std_spsc_ring_threads_dev
        std_concurrent_map_equal std_concurrent_map_equal_dev)
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endforeach()
foreach(kind IN ITEMS list map set pool)
    set_tests_properties(std_sync_${kind}_threads std_sync_${kind}_threads_dev
        PROPERTIES TIMEOUT 120)
endforeach()
set_tests_properties(std_sync_equal std_sync_equal_dev PROPERTIES TIMEOUT 120)

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
antic_program_test(std_userdirs_no_home
    "${CMAKE_CURRENT_SOURCE_DIR}/std/userdirs.anti"
    EXPECTED "${no_home}"
    UNSET "HOME,LOCALAPPDATA,XDG_CONFIG_HOME,XDG_DATA_HOME,XDG_CACHE_HOME"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-no-home")

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
antic_program_test(program_abi_structs
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_structs.anti"
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_structs>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")

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
antic_program_test(program_abi_simd
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_simd.anti"
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_simd>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")

add_library(abi_wchar OBJECT abi/wchar.c)
target_compile_options(abi_wchar PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_wchar PRIVATE -mmacosx-version-min=11.0)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_wchar PRIVATE -fno-sanitize=all)
endif()
abi_wchar_expected(abi_wchar_expected "")
antic_program_test(program_abi_wchar
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_wchar.anti"
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_wchar>,,>"
    EXPECTED "${abi_wchar_expected}"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")

# A user receives these files. None of them may name a directory of the
# machine that built it.
# An ordinary build maps its paths away with -ffile-prefix-map. A
# sanitizer writes the path of the module into its own metadata, which no
# flag maps, so the test runs on a build without one.
string(FIND "${CMAKE_C_FLAGS}" "-fsanitize" antic_sanitized)
if(antic_sanitized EQUAL -1)
    add_test(NAME no_paths
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_no_paths.cmake")
endif()

add_test(NAME cmake_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" -DNAME=cmake
            "-DREADERS=tools/install.sh$<SEMICOLON>tools/install.ps1"
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
            "-DCLANG=${ANTIC_CLANG_DIR}" "-DLLVM=${ANTIC_LLVM_DIR}"
            "-DSYSROOT=${ANTIC_SYSROOT_DIR}" "-DRAYLIB=${ANTIC_RAYLIB_DIR}"
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

# `anti run --memory-checks` and `anti test --memory-checks` put the report
# of each memory error through Anti's symbolizer, and the dev cache keeps
# the objects with and without the checks apart.
add_test(NAME anti_memory_checks
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks.anti"
        "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks_tests.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-memory-checks"
        "-DHOST=${ANTIC_HOST_TARGET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_memory_checks.cmake")
set_tests_properties(anti_memory_checks PROPERTIES TIMEOUT 300
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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
        "-DCHECK_PDB=${PROJECT_SOURCE_DIR}/tools/check-pdb.cmake"
        "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build"
        "-DOBJECT=${anti_object}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build.cmake")

# The package name of the manifest reaches every compile of `anti check`,
# `anti build` in both modes, `anti test` and `anti doc`, so a module that
# uses an internal item of another module of its package passes all four.
add_test(NAME anti_internal
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-internal"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_internal.cmake")

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
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
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
        "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
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
# A walk whose pick of a package changes drops the requirements of the
# old pick, and the packages only it needed.
add_test(NAME anti_build_resolve
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-resolve"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_resolve.cmake")
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
# A failed write of `anti fmt` leaves the source as it was. The limit on
# the size of a written file is a POSIX one, and Windows has none.
if(NOT WIN32)
    add_test(NAME anti_fmt_write
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-fmt-write"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_fmt_write.cmake")
endif()

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
target_link_libraries(antl_rename PRIVATE antic_core)
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
set(anti_bind_cases header clang escaped_path raymath raylib api
    api_malformed clang_malformed)
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
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
            "-DCLANG_DIR=${ANTIC_CLANG_DIR}"
            "-DCLANG_VERSION=${ANTI_CLANG_VERSION}"
            "-DCLANG_TAG=${ANTI_CLANG_TAG}"
            "-DCLANG_PAGE=${ANTI_CLANG_PAGE}"
            "-DHOST=${ANTIC_HOST_TARGET}"
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
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" -DNAME=raylib
            -DSCRIPT=tools/get-raylib.cmake -DFILES=src/native/raylib.cmake
            -DHASH=SHA256 -DPARTS=2
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin.cmake")
# The check of the pins refuses a copy of a version or a digest and passes
# a text one dot short of the version.
add_test(NAME native_pin_self
    COMMAND "${CMAKE_COMMAND}"
            "-DPIN=${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin.cmake"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/native-pin-self"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin_self.cmake")
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
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
            "-DSDK=${ANTIC_MACOS_SDK}"
            "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/release"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_dry_run.cmake")
set_tests_properties(release_dry_run PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# A small raylib binding: the raymath functions of the pinned raylib
# release, which tools/get-raylib.cmake downloads. tools/raylib-pin holds
# the version, so the default of ANTIC_RAYLIB_DIR cannot name another one.
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
    antic_program_test(program_abi_raymath
        "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.anti"
        OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_raymath>,,>"
        EXPECTED "${raymath_expected}"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")
else()
    # Fail rather than skip, so a missing raylib cannot pass unnoticed.
    add_test(NAME program_abi_raymath
        COMMAND "${CMAKE_COMMAND}" -E echo
            "raylib not found in ${ANTIC_RAYLIB_DIR}: run tools/get-raylib.cmake")
    set_tests_properties(program_abi_raymath PROPERTIES
        FAIL_REGULAR_EXPRESSION "raylib not found")
endif()

# Diagnostics. A refusal of a program exits with 1, and its messages
# equal the .err file beside it.
add_expect_test(error_missing_semicolon STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/missing_semicolon.err"
    COMMAND "$<TARGET_FILE:antic>" -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/missing_semicolon.s"
            missing_semicolon.anti)

# The runtime's module path belongs to no compilation, the anti root needs
# --anti-internal for a library, and a library path of one segment warns.
add_expect_test(error_runtime_module STATUS 1
    PATTERN "^antic: .*/errors/anti/rt\\.anti: the module path `anti\\.rt` is the runtime's"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${CMAKE_CURRENT_SOURCE_DIR}/errors"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/rt.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/anti/rt.anti")

add_expect_test(error_reserved_library STATUS 1
    PATTERN "^antic: .*/modules/anti/text\\.anti: the module path `anti\\.text` is reserved for the language's own libraries"
    COMMAND "$<TARGET_FILE:antic>" -c -I "${CMAKE_CURRENT_SOURCE_DIR}/modules"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_text.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/anti/text.anti")

add_test(NAME antl_internal
    COMMAND antic -c --anti-internal -I "${CMAKE_CURRENT_SOURCE_DIR}/modules"
            -o "${CMAKE_CURRENT_BINARY_DIR}/anti_text.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/anti/text.anti")

# An `anti.regex` whose functions take fewer parameters, or other types,
# than the calls the checker writes. The checker refuses each call and
# reads no parameter the function lacks.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/damaged/anti")
add_test(NAME antl_regex_damaged
    COMMAND antic -c --anti-internal -I "${CMAKE_CURRENT_SOURCE_DIR}/modules"
            -o "${CMAKE_CURRENT_BINARY_DIR}/damaged/anti/regex.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/anti/regex.anti")
set_tests_properties(antl_regex_damaged PROPERTIES
    FIXTURES_SETUP regex_damaged)
add_expect_test(error_regex_damaged STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/regex_damaged.err"
    COMMAND "$<TARGET_FILE:antic>" --front-end --runtime "${ANTIC_RUNTIME_DIR}"
            -I "${CMAKE_CURRENT_BINARY_DIR}/damaged" regex_damaged.anti)
set_tests_properties(error_regex_damaged PROPERTIES
    FIXTURES_REQUIRED regex_damaged)
# A group of `patch` is compared as the u64 its literal holds.
add_expect_test(error_patch_group_wide STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/patch_group_wide.err"
    COMMAND "$<TARGET_FILE:antic>" --front-end --runtime "${ANTIC_RUNTIME_DIR}"
            patch_group_wide.anti)

# A `may fail` function without a `fail` and without a `try` is a warning,
# not an error, because an interface function may fail in one
# implementation and not in another. A dev build prints it and carries on.
add_expect_test(warning_never_fails STATUS 0
    PATTERN "never_fails.anti:3:21: warning: `close_it` may fail and never does \\[never-fails\\]"
    COMMAND "$<TARGET_FILE:antic>" --runtime "${ANTIC_RUNTIME_DIR}" --dev -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/never_fails.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/never_fails.anti")

# A release build accepts no warning, and --warnings-as-errors gives a dev
# build the same behaviour. The warning is then an error with its name.
foreach(mode "release|-S" "strict|--dev,--warnings-as-errors,-S")
    string(REPLACE "|" ";" parts "${mode}")
    list(GET parts 0 name)
    list(GET parts 1 flags)
    string(REPLACE "," ";" flags "${flags}")
    add_expect_test(error_never_fails_${name} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/never_fails.err"
        COMMAND "$<TARGET_FILE:antic>" --runtime "${ANTIC_RUNTIME_DIR}" ${flags}
                -o "${CMAKE_CURRENT_BINARY_DIR}/never_fails_${name}.s"
                never_fails.anti)
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
add_expect_test(warning_note STATUS 0
    PATTERN "note.anti:2:8: warning: the pub item `warm` has a `//#` note and no `///` comment"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings -S -o "${CMAKE_CURRENT_BINARY_DIR}/note.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
add_test(NAME silent_note
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/note_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
set_tests_properties(silent_note PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

add_expect_test(warning_dropped STATUS 0
    PATTERN "dropped.anti:3:2: warning: the `/\\*\\*` comment is dropped, because no item or field follows it"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings -S -o "${CMAKE_CURRENT_BINARY_DIR}/dropped.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
add_test(NAME silent_dropped
    COMMAND antic -S -o "${CMAKE_CURRENT_BINARY_DIR}/dropped_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
set_tests_properties(silent_dropped PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

# A doc comment names each type the compiler declares in `anti.lang`
# without a warning.
add_test(NAME silent_doc_declared
    COMMAND antic --doc-warnings -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/doc_declared.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/doc_declared.anti")
set_tests_properties(silent_doc_declared PROPERTIES
    FAIL_REGULAR_EXPRESSION "warning")

# A doc comment inside `{ }` of `f"..."` is dropped with a warning, as one
# inside any other expression is.
add_test(NAME warning_format_doc
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/errors"
        "-DCOMMAND=--doc-warnings,--runtime,${ANTIC_RUNTIME_DIR},-S,-o,${CMAKE_CURRENT_BINARY_DIR}/format_doc.s,format_doc.anti"
        "-DNEEDLES=format_doc.anti:10:15: warning: the `///` comment is dropped, because no item or field follows it [doc-dropped];format_doc.anti:12:13: warning: the `/**` comment is dropped, because no item or field follows it [doc-dropped]"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warnings.cmake")

# A doc comment names the functions of a class of a library without a
# warning, as it names those of a struct.
add_test(NAME silent_doc_library_class
    COMMAND antic --doc-warnings --runtime "${ANTIC_RUNTIME_DIR}" -S
            -o "${CMAKE_CURRENT_BINARY_DIR}/doc_library_class.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/doc_library_class.anti")
set_tests_properties(silent_doc_library_class PROPERTIES
    FAIL_REGULAR_EXPRESSION "warning")

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

add_expect_test(warning_one_segment STATUS 0
    PATTERN "geometry.anti:1:1: warning: the module path `geometry` has one segment, which is for a program's own files \\[single-segment-path\\]"
    COMMAND "$<TARGET_FILE:antic>" -c -o "${CMAKE_CURRENT_BINARY_DIR}/geometry.antl"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/geometry.anti")

add_expect_test(error_no_main STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/no_main.err"
    COMMAND "$<TARGET_FILE:antic>" --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/no_main" no_main.anti)

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
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
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

# Paths outside ASCII on the command line, in the place of antic, in the
# data directory of the user and in a project of anti. Windows reads each
# through UTF-16.
add_test(NAME unicode_paths
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/unicode-paths"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_unicode_paths.cmake")

# A Windows executable carries the CodeView record of the PDB that
# lld-link wrote beside it. It holds no path of the machine that linked it
# either, because the link passes /PDBALTPATH:%_PDB%.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME pdb_guid_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
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
            "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOPTIONS=${framework_options}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/framework/core_foundation.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/framework"
            "-DTARGET=${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_framework.cmake")
    set_tests_properties(framework_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# A shared library takes the frameworks, the `link linux` libraries, the
# glibc mode and the memory checks of a program of its module.
add_test(NAME shared_inputs
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
        "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/shared-inputs"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_shared_inputs.cmake")

# The platform linker of the host links the first executable of chapter 3.
if(NOT WIN32)
    antic_program_test(program_platform_linker
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
        OPTIONS "--linker,platform"
        WORK "${CMAKE_CURRENT_BINARY_DIR}/platform")
endif()

# The platform linker links only for the operating system it runs on.
if(ANTIC_HOST_OS STREQUAL "linux")
    set(foreign_target macos-arm64)
else()
    set(foreign_target linux-x86_64)
endif()
add_expect_test(error_foreign_link STATUS 2
    PATTERN "^antic: linking for ${foreign_target} with the platform linker needs a host with the same operating system"
    COMMAND "$<TARGET_FILE:antic>" --target ${foreign_target} --linker platform
            --runtime "${ANTIC_RUNTIME_DIR}" --llvm-mc "${ANTIC_LLVM_MC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/return42"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")

# The back end folds an array length from size_of and names the target.
add_expect_test(error_array_length STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/zero_length.err"
    COMMAND "$<TARGET_FILE:antic>" -S --target linux-x86_64
            -o "${CMAKE_CURRENT_BINARY_DIR}/errors/zero_length.s"
            zero_length.anti)

add_expect_test(error_missing_runtime STATUS 2
    PATTERN "^antic: linking needs --runtime"
    COMMAND "$<TARGET_FILE:antic>" -o "${CMAKE_CURRENT_BINARY_DIR}/errors/return42"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")

# Dumps of compiler stages, compared with expected files in tests/dump.
# scale.tokens is the token listing that chapter 1 prints.
antic_dump_test(tokens scale
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.tokens"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti")

# A doc comment is one token, printed with its first source line.
antic_dump_test(tokens docs
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/docs.tokens"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/docs.anti")

add_expect_test(error_lexical STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/lexical.err"
    COMMAND "$<TARGET_FILE:antic>" -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/lexical.s"
            lexical.anti)

# A source of 64 MiB passes the front end, and one a byte longer is
# refused when it is read, before the lexer sees it.
add_test(NAME error_source_cap
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/source-cap"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_source_cap.cmake")

# The lexer refuses a NUL byte outside a literal, wherever it stands
# among the blocks the reader takes, and takes one inside a bytes
# literal.
add_test(NAME error_source_nul
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DNUL=${CMAKE_CURRENT_SOURCE_DIR}/errors/nul_byte.bin"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/source-nul"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_source_nul.cmake")

# scale.ast is the syntax tree listing that chapter 1 prints.
antic_dump_test(ast scale
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.ast"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti")

# `fallthrough;` is a statement of its own in the tree of an arm.
antic_dump_test(ast switch_fallthrough
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/switch_fallthrough.ast"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/switch_fallthrough.anti")

# Type parameters and type arguments in the tree: `>>` closes two lists,
# and `<` in an expression opens a list by the rule of C#.
antic_dump_test(ast generics_syntax
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/generics_syntax.ast"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/generics_syntax.anti")

# An `f"..."` is one node, with the value of each `{expr}` below it.
antic_dump_test(ast format
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/format.ast"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/format.anti")

# `alloc T { }` holds its literal alone and `for 0..3` binds no name, in
# the tree and in the typed tree.
foreach(stage ast types)
    antic_dump_test(${stage} alloc_loops
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/alloc_loops.${stage}"
        ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/alloc_loops.anti")
endforeach()
# Two mistakes produce exactly two messages.
add_expect_test(error_syntax STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/syntax.err"
    COMMAND "$<TARGET_FILE:antic>" -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/syntax.s"
            syntax.anti)

# scale.types is the typed tree listing that chapter 1 prints.
antic_dump_test(types scale
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.types"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/scale.anti")

# main.ir is the IR listing that chapter 1 prints for main.anti.
antic_dump_test(ir main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.ir"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")

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
antic_modules_test(hooks_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/hooks"
    LIBRARIES store
    PROGRAM client
    STD lang
    OPTIONS --trace --trace writes
    LIB_OPTIONS --trace
    WORK "${CMAKE_CURRENT_BINARY_DIR}/hooks-modules")

# A program of two modules holds one descriptor and one table per class,
# so `is`, `==`, `equals` and `reflect.new` agree whichever module made
# the object, in release mode and in dev mode
antic_modules_test(shared_class
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/shared"
    LIBRARIES points
    PROGRAM client
    STD lang mem text reflect
    WORK "${CMAKE_CURRENT_BINARY_DIR}/shared")
# A chain of classes over three modules, each naming its base with the
# module it comes from
antic_modules_test(inherits_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/chain"
    LIBRARIES animal dog
    PROGRAM chain
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/chain")
# Ownership over two modules: the teardown and the copy of a class call
# those of the base, the inline field and the elements that another module
# declares
antic_modules_test(owned_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/owned"
    LIBRARIES parts
    PROGRAM crate
    WORK "${CMAKE_CURRENT_BINARY_DIR}/owned")

# Generics over a module boundary: a program and a second library use the
# generics of a library with their own arguments. Each makes its copies
# from the checked tree the library file carries, and a copy two modules
# make is one type and one symbol. Release mode and dev mode print the
# same.
antic_modules_test(generics_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generics"
    LIBRARIES gen mid
    PROGRAM client
    STD lang mem text
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generics")

# Direct imports. A program and a library name the items of another
# library without its module name, and reach the module by its name as
# well. A generic among them has its copy made in the program.
antic_modules_test(direct_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/direct"
    LIBRARIES shapes frame
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/direct")

# `may fail` over a module boundary: the library file records the flag, so
# the program handles the error, `try` forwards it and `undo` runs on the
# fail path alone.
antic_modules_test(failing_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/failing"
    LIBRARIES counter
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/failing")

# Default values over a module boundary: the library file carries the
# default of each parameter, and `here` is the position of the call in the
# program.
antic_modules_test(default_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/defaults"
    LIBRARIES greeter
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/defaults")

# A tuple over a module boundary: the library file carries the tuple of
# each signature, so the two modules name one type and the program takes
# the elements apart.
antic_modules_test(tuple_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/tuples"
    LIBRARIES pairs
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/tuples")

# A variant of a library, which another module builds and switches on
# with every case named. It tests one with `is` and `if let` and passes
# it back inside a struct of the library.
antic_modules_test(variant_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/variants"
    LIBRARIES geo
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/variants")

# A public class of a library whose layout holds types nested in it,
# which another module lays out, builds and calls.
antic_modules_test(nested_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/nested"
    LIBRARIES people
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/nested")

# The hash of the types of a library and its generics that hash and order,
# which another module uses and hashes inside a struct of its own.
antic_modules_test(hashing_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/hashing"
    LIBRARIES keys
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/hashing")

# The operators of a library reach the private fields and functions of its
# classes, a generic one among them, in the copies another module makes.
antic_modules_test(access_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/access"
    LIBRARIES ledger
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/access")

# A generic class with operator hooks in a library, which another module
# compares, adds, indexes and walks. The library file carries the mark of
# each hook, in the class and at module level.
antic_modules_test(generic_hooks_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_hooks"
    LIBRARIES row
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-hooks")

# A collection, its iterator and a struct with the language hooks in a
# library, which another module walks, indexes and collects.
antic_modules_test(iteration_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/iteration"
    LIBRARIES bag
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/iteration")

# Closures and functions passed to the parameters of a library.
# The library file carries each parameter in its form. A `concurrent` one
# reaches the workers of `parallel`, and a `keep` one stores a C function
# pointer.
antic_modules_test(closures_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/closures"
    LIBRARIES walk
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/closures")

# A Mutex in a struct of a library and channels that pass between the
# program and a library, which drains one and selects over two.
antic_modules_test(sync_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/sync"
    LIBRARIES queue
    PROGRAM client
    TIMEOUT 120
    WORK "${CMAKE_CURRENT_BINARY_DIR}/sync")

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

antic_dump_test(ir modules
    "${MODULES}/main.ir"
    ARGS -I "${LIBS}" "${MODULES}/main.anti")
set_tests_properties(dump_ir_modules PROPERTIES FIXTURES_REQUIRED twice_antl)

# Dev mode: one object per module, linked into the same program.
add_test(NAME dev_modules
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DMODULES=${MODULES}"
        "-DLIBS=${LIBS}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/dev"
        "-DOBJECT=${anti_object}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dev.cmake")
set_tests_properties(dev_modules PROPERTIES FIXTURES_REQUIRED twice_antl)

add_executable(poke_byte modules/poke_byte.c)
target_link_libraries(poke_byte PRIVATE antic_core)
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
        set(flag --dev)
    endif()
    add_expect_test(error_worker_reach_${mode} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/worker_reach.err"
        COMMAND "$<TARGET_FILE:antic>" ${flag} -S -I "${LIBS}"
                -o "${CMAKE_CURRENT_BINARY_DIR}/worker_reach.s" worker_reach.anti)
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
add_expect_test(error_closures_modules STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/closures_modules.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/closures"
            -o "${CMAKE_CURRENT_BINARY_DIR}/closures_modules.s" closures_modules.anti)
set_tests_properties(error_closures_modules PROPERTIES
    FIXTURES_REQUIRED walk_antl)

# The `final` marks of a library reach its importer: a replacement of a
# `final fn` and a class below a `final class` are refused there.
file(MAKE_DIRECTORY "${LIBS}/sealed/com/example")
add_test(NAME antl_sealed
    COMMAND antic -c -I "${MODULES}/sealed"
            -o "${LIBS}/sealed/com/example/sealed.antl"
            "${MODULES}/sealed/com/example/sealed.anti")
set_tests_properties(antl_sealed PROPERTIES FIXTURES_SETUP sealed_antl)
add_expect_test(error_final_modules STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/final_modules.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/sealed"
            -o "${CMAKE_CURRENT_BINARY_DIR}/final_modules.s" final_modules.anti)
set_tests_properties(error_final_modules PROPERTIES
    FIXTURES_REQUIRED sealed_antl)

# The `own` and `lent` parameters of a library reach the importer through
# the library file.
file(MAKE_DIRECTORY "${LIBS}/owned/com/example")
add_test(NAME antl_parts
    COMMAND antic -c -I "${MODULES}/owned"
            -o "${LIBS}/owned/com/example/parts.antl"
            "${MODULES}/owned/com/example/parts.anti")
set_tests_properties(antl_parts PROPERTIES FIXTURES_SETUP parts_antl)
add_expect_test(error_own_modules STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/own_modules.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/owned"
            -o "${CMAKE_CURRENT_BINARY_DIR}/own_modules.s" own_modules.anti)
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
add_expect_test(error_concurrent_modules STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/concurrent_modules.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/sync"
            -o "${CMAKE_CURRENT_BINARY_DIR}/concurrent_modules.s" concurrent_modules.anti)
set_tests_properties(error_concurrent_modules PROPERTIES
    FIXTURES_REQUIRED queue_antl)

# The private fields and functions of a library's classes stay closed to
# the operators and functions of the importer.
file(MAKE_DIRECTORY "${LIBS}/access/com/example")
add_test(NAME antl_ledger
    COMMAND antic -c -I "${MODULES}/access"
            -o "${LIBS}/access/com/example/ledger.antl"
            "${MODULES}/access/com/example/ledger.anti")
set_tests_properties(antl_ledger PROPERTIES FIXTURES_SETUP ledger_antl)
add_expect_test(error_operator_access_modules STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/operator_access_modules.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/access"
            -o "${CMAKE_CURRENT_BINARY_DIR}/operator_access_modules.s" operator_access_modules.anti)
set_tests_properties(error_operator_access_modules PROPERTIES
    FIXTURES_REQUIRED ledger_antl)

# A qualified base that the module does not declare as a public class
add_expect_test(error_inherits_module STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/inherits_module.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/inherits_module.s" inherits_module.anti)
set_tests_properties(error_inherits_module PROPERTIES
    FIXTURES_REQUIRED board_antl)

add_expect_test(error_missing_library STATUS 1
    PATTERN "twice\\.antl needs module `com\\.example\\.scale`"
    COMMAND "$<TARGET_FILE:antic>" --dump-ir "${MODULES}/main.anti"
            "${LIBS}/com/example/twice.antl")
set_tests_properties(error_missing_library PROPERTIES
    FIXTURES_REQUIRED twice_antl)

add_expect_test(error_library_cycle STATUS 1
    PATTERN "twice\\.antl depends on `com\\.example\\.scale`, so the import forms a cycle"
    COMMAND "$<TARGET_FILE:antic>" -c -I "${MODULES}" -o "${LIBS}/cycle.antl"
            "${MODULES}/com/example/scale.anti"
            "${LIBS}/com/example/twice.antl")
set_tests_properties(error_library_cycle PROPERTIES
    FIXTURES_REQUIRED twice_antl)

add_test(NAME antl_one
    COMMAND antic -c -I "${MODULES}" -o "${LIBS}/com/example/one.antl"
            "${MODULES}/com/example/one.anti")
set_tests_properties(antl_one PROPERTIES FIXTURES_SETUP one_antl)

# The first library loads, and the second one is stuck on the cycle.
add_expect_test(error_library_cycle_later STATUS 1
    PATTERN "twice\\.antl depends on `com\\.example\\.scale`, so the import forms a cycle"
    COMMAND "$<TARGET_FILE:antic>" -c -I "${MODULES}" -o "${LIBS}/cycle.antl"
            "${MODULES}/com/example/scale.anti"
            "${LIBS}/com/example/one.antl" "${LIBS}/com/example/twice.antl")
set_tests_properties(error_library_cycle_later PROPERTIES
    FIXTURES_REQUIRED "one_antl;twice_antl")

add_expect_test(error_duplicate_library STATUS 1
    PATTERN "scale\\.antl and .*scale\\.antl both hold module `com\\.example\\.scale`"
    COMMAND "$<TARGET_FILE:antic>" --dump-ir "${MODULES}/main.anti"
            "${LIBS}/com/example/scale.antl" "${LIBS}/com/example/scale.antl")
set_tests_properties(error_duplicate_library PROPERTIES
    FIXTURES_REQUIRED scale_antl)

# The five behaviours of `assert`: a release build without it, a forced
# build with it, dev mode keeping it and --no-asserts dropping it.
if(ANTIC_LLVM_MC)
    add_test(NAME asserts
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/asserts.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asserts"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asserts.cmake")
endif()

# Every dev-mode check: bounds, overflow, a narrowing `as`, division,
# shifts and a walk of a collection that changes, one test per program in
# a build that keeps the check and a build that drops it. A program whose
# operation traps by itself without the check is built in release mode and
# not run, which runs gives as OFF. DEV names the modules whose dev objects
# the dev build links. checks_override runs the options that decide
# instead of the mode.
function(antic_check_test name phrase pattern runs)
    cmake_parse_arguments(PARSE_ARGV 4 arg "" "" "DEV")
    antic_dev_inputs(objects fixtures ${arg_DEV})
    list(JOIN objects "," objects)
    add_test(NAME check_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/${name}.anti"
            "-DPHRASE=${phrase}"
            "-DPATTERN=${pattern}"
            "-DRUNS=${runs}"
            "-DOBJECTS=${objects}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/checks"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_check.cmake")
    if(fixtures)
        set_property(TEST check_${name} PROPERTY FIXTURES_REQUIRED ${fixtures})
    endif()
endfunction()
if(ANTIC_LLVM_MC)
    antic_check_test(bounds_array "index out of bounds"
        "bounds_array\\.anti:[0-9]+: index out of bounds: index 5, length 4"
        ON)
    antic_check_test(bounds_slice "index out of bounds"
        "bounds_slice\\.anti:[0-9]+: index out of bounds: index 4, length 2"
        ON)
    antic_check_test(bounds_simd "index out of bounds"
        "bounds_simd\\.anti:[0-9]+: index out of bounds: index 7, length 6"
        ON)
    antic_check_test(bounds_str "index out of bounds"
        "bounds_str\\.anti:[0-9]+: index out of bounds: index 7, length 3"
        ON)
    antic_check_test(overflow_add "overflow in"
        "overflow_add\\.anti:[0-9]+: overflow in \\+: left 9223372036854775807, right 1"
        ON)
    antic_check_test(overflow_sub "overflow in"
        "overflow_sub\\.anti:[0-9]+: overflow in -: left -9223372036854775807, right 2"
        ON)
    antic_check_test(overflow_mul "overflow in"
        "overflow_mul\\.anti:[0-9]+: overflow in \\*: left 4000000000, right 4000000000"
        ON)
    antic_check_test(overflow_mul32 "overflow in"
        "overflow_mul32\\.anti:[0-9]+: overflow in \\*: left 100000, right 100000"
        ON)
    antic_check_test(overflow_carry "overflow in"
        "overflow_carry\\.anti:[0-9]+: overflow in \\+: left 9223372036854775806, right 1"
        ON)
    antic_check_test(narrow "out of range"
        "narrow\\.anti:[0-9]+: value out of range for i8: value 300"
        ON)
    antic_check_test(narrow_sign "out of range"
        "narrow_sign\\.anti:[0-9]+: value out of range for u64: value -1"
        ON)
    antic_check_test(narrow_char "out of range"
        "narrow_char\\.anti:[0-9]+: value out of range for char: value 1114112"
        ON)
    antic_check_test(narrow_surrogate "out of range"
        "narrow_surrogate\\.anti:[0-9]+: value out of range for char: value 55296"
        ON)
    antic_check_test(narrow_enum "not declared by"
        "narrow_enum\\.anti:[0-9]+: value not declared by Kind: value 7"
        ON)
    antic_check_test(divide "division by zero"
        "divide\\.anti:[0-9]+: division by zero in /: left 10"
        OFF)
    antic_check_test(remainder "division by zero"
        "remainder\\.anti:[0-9]+: division by zero in %: left 10"
        OFF)
    antic_check_test(shift_wide "shift count out of range"
        "shift_wide\\.anti:[0-9]+: shift count out of range for <<: count 64, width 64"
        ON)
    antic_check_test(shift_negative "shift count out of range"
        "shift_negative\\.anti:[0-9]+: shift count out of range for >>: count -1, width 64"
        ON)
    antic_check_test(walk_changed "was changed while"
        "walk_changed\\.anti:68: `bag` was changed while `for` walked it: changed at walk_changed\\.anti:70"
        ON DEV lang)
    antic_check_test(collection_changed "was changed while"
        "collection_changed\\.anti:82: `pair` was changed while `for` walked it: changed at collection_changed\\.anti:84"
        ON DEV lang mem text collection)
    antic_check_test(list_changed "was changed while"
        "list_changed\\.anti:12: `people` was changed while `for` walked it: changed at list_changed\\.anti:14"
        ON DEV lang mem text collection collection/list)
    antic_check_test(list_bounds "index out of bounds"
        "\\.anti:[0-9]+: index out of bounds: index 5, length 4"
        ON DEV lang mem text collection collection/list)
    antic_check_test(grid_bounds "index out of bounds"
        "\\.anti:[0-9]+: index out of bounds: index 3, length 3"
        ON DEV lang mem text collection collection/grid)
    antic_check_test(sorted_changed "was changed while"
        "sorted_changed\\.anti:13: `ages` was changed while `for` walked it: changed at sorted_changed\\.anti:15"
        ON DEV lang mem text collection reflect collection/sorted)
    antic_check_test(map_changed "was changed while"
        "map_changed\\.anti:12: `ages` was changed while `for` walked it: changed at map_changed\\.anti:14"
        ON DEV lang mem text collection collection/map)
    antic_check_test(set_changed "was changed while"
        "set_changed\\.anti:12: `seen` was changed while `for` walked it: changed at set_changed\\.anti:13"
        ON DEV lang mem text collection collection/map collection/set)
    add_test(NAME checks_override
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/traps"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/checks-override"
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
add_test(NAME trap_table
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/table.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trap_table.cmake")

# The programs of tests/traps that stop with the message of <name>.err.
# parallel_size: the results of `parallel` take one value per chunk, and
# a count of them that no size holds stops the program before a worker
# runs. alloc_literal, alloc_construct and singleton_memory: out of memory
# is fatal for `alloc T { }`, `alloc T(args)` and the first `get` of a
# singleton, which stop with the failure routine instead of writing
# through a null pointer. pattern_limit: a search with a pattern literal
# that reaches the match limit stops with the pattern and the line.
# pattern_group_count: a match whose `count` the program raised reads no
# group past those of its pattern, and stops with the count of the
# pattern.
foreach(name IN ITEMS parallel_size alloc_literal alloc_construct
        singleton_memory pattern_limit pattern_group_count)
    add_test(NAME trap_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/${name}.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trap.cmake")
endforeach()

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

# --memory-checks reports a read after free, a double free, a write past a
# heap block and a leak on every host but windows-arm64, which has no
# runtime of AddressSanitizer. Every host checks that windows-arm64
# refuses the option.
add_test(NAME memory_checks
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        "-DHOST=${ANTIC_HOST_TARGET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_memory_checks.cmake")
set_tests_properties(memory_checks PROPERTIES TIMEOUT 120
    SKIP_REGULAR_EXPRESSION "SKIP: ")
# --memory-checks finds a pointer into a List kept across a push that grows
# the list, as a read after free.
add_test(NAME memory_checks_list
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks_list.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        "-DHOST=${ANTIC_HOST_TARGET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_memory_checks_list.cmake")
set_tests_properties(memory_checks_list PROPERTIES TIMEOUT 120
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
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
# the dev object of anti.lang.
set(TRACE "${CMAKE_CURRENT_SOURCE_DIR}/trace")
foreach(case
        "stack|stack_trace|||stack_trace.pattern||0"
        "stack_g|stack_trace|-g||stack_trace.g.pattern||0"
        "stack_dev|stack_trace|--dev||stack_trace.pattern||0"
        "forged|forged|||forged.pattern||0"
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
        "handlers|handler_reports|--trace,--trace,writes||handler_reports.pattern|handler_reports.errors.pattern|0"
        "start|start|--trace|--anti.trace=calls|start.pattern|start.errors.pattern|0"
        "start_none|start|--trace||start.none.pattern||0")
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
        set(std lang)
    endif()
    antic_dev_inputs(std_objects std_fixtures ${std})
    list(JOIN std_objects "," std_objects)
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
            "-DSTD_OBJECTS=${std_objects}"
            "-DSTATUS=${status}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/trace-${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trace.cmake")
    if(std_fixtures)
        set_property(TEST trace_${name} PROPERTY
            FIXTURES_REQUIRED ${std_fixtures})
    endif()
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
        "exit_status|status||4294967299|empty.pattern||3"
        "help|report||--anti.help|help.pattern||0"
        "configure|configure||${CONF}/backtrace.toml|configure.pattern||0"
        "configure_dev|configure|--dev|${CONF}/no-backtrace.toml|configure.plain.pattern||0"
        "configure_missing|configure||${CONF}/nowhere.toml|empty.pattern|missing.pattern|70"
        "configure_nul|configure_nul||${CONF}/basic.toml|empty.pattern|configure-nul.pattern|70"
        "configure_injections|configure||${CONF}/injection.toml|empty.pattern|configure-injections.pattern|70"
        "configure_injections_include|configure||${CONF}/include-injection.toml|empty.pattern|configure-injections.pattern|70"
        "file_missing|report||--anti.conf=${CONF}/nowhere.toml|empty.pattern|missing.pattern|70"
        "include_missing|report||--anti.conf=${CONF}/missing.toml|empty.pattern|missing.pattern|70"
        "include_cycle|report||--anti.conf=${CONF}/cycle.toml|empty.pattern|cycle.pattern|70"
        "unknown_option|report||--anti.nonsense|empty.pattern|unknown-option.pattern|70"
        "unknown_key|report||--anti.conf=${CONF}/unknown-key.toml|empty.pattern|unknown-key.pattern|70"
        "bad_value|report||--anti.conf=${CONF}/bad-value.toml|empty.pattern|bad-value.pattern|70"
        "nul_value|report||--anti.conf=${CONF}/nul-value.toml|empty.pattern|nul-value.pattern|70"
        "not_toml|report||--anti.conf=${CONF}/not-toml.toml|empty.pattern|not-toml.pattern|70"
        "cut_string|report||--anti.conf=${CONF}/cut.toml|empty.pattern|cut.pattern|70"
        "open_table|report||--anti.conf=${CONF}/open-table.toml|empty.pattern|open-table.pattern|70"
        "no_value|report||--anti.conf=${CONF}/no-value.toml|empty.pattern|no-value.pattern|70"
        "include_not_toml|report||--anti.conf=${CONF}/include-not-toml.toml|empty.pattern|not-toml.pattern|70"
        "configure_not_toml|configure||${CONF}/not-toml.toml|empty.pattern|not-toml.pattern|70"
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
        set(std lang runtime)
    endif()
    antic_dev_inputs(std_objects std_fixtures ${std})
    list(JOIN std_objects "," std_objects)
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
            "-DSTD_OBJECTS=${std_objects}"
            "-DSTATUS=${status}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/conf-${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trace.cmake")
    if(std_fixtures)
        set_property(TEST conf_${name} PROPERTY
            FIXTURES_REQUIRED ${std_fixtures})
    endif()
endforeach()

# The runtime and `anti symbols` stop a chain of includes at one depth,
# with one message.
add_test(NAME conf_include_depth
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CONF}/report.anti"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/conf-include-depth"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_conf_include_depth.cmake")

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
    add_expect_test(inject_manifest_${mode} STATUS 0
        DIRECTORY "${INJECT}/project"
        PATTERN "1 tests in 1 modules, 0 modules failed"
        COMMAND "$<TARGET_FILE:anti>" test ${extra}
                --work "${CMAKE_CURRENT_BINARY_DIR}/inject-manifest-${mode}"
                -I src --runtime "${ANTIC_RUNTIME_DIR}"
                --llvm-mc "${ANTIC_LLVM_MC}" src/com/example/app.anti)
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
    add_expect_test(inject_standard_${mode} STATUS 0
        DIRECTORY "${INJECT}/standard"
        PATTERN "1 tests in 1 modules, 0 modules failed"
        COMMAND "$<TARGET_FILE:anti>" test ${extra}
                --work "${CMAKE_CURRENT_BINARY_DIR}/inject-standard-${mode}"
                -I src --runtime "${ANTIC_RUNTIME_DIR}"
                --llvm-mc "${ANTIC_LLVM_MC}" src/com/example/standard.anti)
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
antic_rt_sources(probe_sources symbols)
add_executable(symbols_probe unit/symbols_probe.c ${probe_sources})
target_link_libraries(symbols_probe PRIVATE antic_core)
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
                "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
                "-DTARGET=${ANTIC_HOST_TARGET}"
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
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
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
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
                "-DLLVM_READOBJ=${ANTIC_RUNTIME_READOBJ}"
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
antic_dump_test(opt main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")

# The selected machine code of main.anti that chapter 12 prints.
antic_dump_test(select main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.arm64.sel"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")
antic_dump_test(select main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.x86_64.sel"
    TARGET linux-x86_64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")

# The allocated machine code of main.anti that chapter 13 prints.
antic_dump_test(alloc main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.arm64.alloc"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")
antic_dump_test(alloc main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.x86_64.alloc"
    TARGET linux-x86_64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")

antic_dump_test(alloc twice
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/twice.arm64.alloc"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/twice.anti")
antic_dump_test(alloc cell
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/cell.arm64.alloc"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/cell.anti")

# Spilling: more values live across a call than callee-saved registers.
antic_dump_test(alloc spill12
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/spill12.arm64.alloc"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/spill12.anti")
antic_dump_test(alloc spill7
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/spill7.x86_64.alloc"
    TARGET linux-x86_64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/spill7.anti")

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
    antic_dump_test(alloc ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/${expected}.alloc"
        TARGET ${target}
        ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.anti")
endforeach()

# The three ARM64 conventions pass these structs alike. The Windows frame
# adds its unwind directives, so it has a file of its own.
foreach(target macos-arm64 linux-arm64 windows-arm64)
    set(expected structs.arm64.alloc)
    if(target STREQUAL "windows-arm64")
        set(expected structs.windows-arm64.alloc)
    endif()
    antic_dump_test(alloc structs
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/${expected}"
        TARGET ${target}
        ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.anti")
endforeach()

# The refusals of the programs of tests/errors, error_<name>. A message
# names the file alone, so each runs in the directory of its program.
set(LISTINGS "${CMAKE_CURRENT_BINARY_DIR}/listings")
file(MAKE_DIRECTORY "${LISTINGS}")
foreach(case "layout|-S,--target,linux-x86_64,-o,${LISTINGS}/layout.s,layout.anti"
        "layout_unit|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_unit.s,layout_unit.anti"
        "layout_size|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_size.s,layout_size.anti"
        "layout_shift|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_shift.s,layout_shift.anti"
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
        "if_let_moves|-S,-o,${LISTINGS}/if_let_moves.s,if_let_moves.anti"
        "owning_destruct|-S,-o,${LISTINGS}/owning_destruct.s,owning_destruct.anti"
        "literal_moves|-S,-o,${LISTINGS}/literal_moves.s,literal_moves.anti"
        "variant_owning|-S,-o,${LISTINGS}/variant_owning.s,variant_owning.anti"
        "generic_moves|-S,-o,${LISTINGS}/generic_moves.s,generic_moves.anti"
        "caller_params|-S,-o,${LISTINGS}/caller_params.s,caller_params.anti"
        "deref_new|-S,-o,${LISTINGS}/deref_new.s,deref_new.anti"
        "collection_copy|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/collection_copy.s,collection_copy.anti"
        "collection_objects|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/collection_objects.s,collection_objects.anti"
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
        "final_replace|-S,-o,${LISTINGS}/final_replace.s,final_replace.anti"
        "worker_walk|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/worker_walk.s,worker_walk.anti"
        "assert_call|-S,-o,${LISTINGS}/assert_call.s,assert_call.anti"
        "qualified_bodies|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/qualified_bodies.s,qualified_bodies.anti"
        "qualified_calls|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/qualified_calls.s,qualified_calls.anti"
        "abstract_call|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/abstract_call.s,abstract_call.anti"
        "quiet_constant|-S,-o,${LISTINGS}/quiet_constant.s,quiet_constant.anti"
        "link_framework|-S,-o,${LISTINGS}/link_framework.s,link_framework.anti"
        "link_framework_syntax|-S,-o,${LISTINGS}/link_framework_syntax.s,link_framework_syntax.anti"
        "link_linux|-S,-o,${LISTINGS}/link_linux.s,link_linux.anti"
        "link_linux_syntax|-S,-o,${LISTINGS}/link_linux_syntax.s,link_linux_syntax.anti"
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
        "no_hooks_plugins|-S,--no-hooks,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/no_hooks_plugins.s,no_hooks_plugins.anti"
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
        "worker_params|-S,-o,${LISTINGS}/worker_params.s,worker_params.anti"
        "literal_type_name|-S,-o,${LISTINGS}/literal_type_name.s,literal_type_name.anti"
        "optional_values|-S,-o,${LISTINGS}/optional_values.s,optional_values.anti"
        "snapshots|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/snapshots.s,snapshots.anti"
        "synchronized|-S,-o,${LISTINGS}/synchronized.s,synchronized.anti"
        "sync_pairs|-S,-o,${LISTINGS}/sync_pairs.s,sync_pairs.anti"
        "operator_access|-S,-o,${LISTINGS}/operator_access.s,operator_access.anti"
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
        "copy_names|--front-end,copy_names.anti"
        "generic_syntax|--front-end,generic_syntax.anti"
        "constraints|--front-end,constraints.anti"
        "hashing|--front-end,hashing.anti"
        "equality|--front-end,equality.anti"
        "enum_values|--front-end,enum_values.anti"
        "switch_arm_values|--front-end,switch_arm_values.anti"
        "overloads|--front-end,overloads.anti"
        "no_reflect|--front-end,--no-reflect,--runtime,${ANTIC_RUNTIME_DIR},no_reflect.anti"
        "direct_imports|--front-end,--runtime,${ANTIC_RUNTIME_DIR},direct_imports.anti"
        "walk_interfaces|--front-end,--runtime,${ANTIC_RUNTIME_DIR},walk_interfaces.anti"
        "handles|--front-end,--runtime,${ANTIC_RUNTIME_DIR},handles.anti"
        "sorted|--front-end,--runtime,${ANTIC_RUNTIME_DIR},sorted.anti"
        "shared_handles|--front-end,--runtime,${ANTIC_RUNTIME_DIR},shared_handles.anti"
        "generic_copies|-S,-o,${LISTINGS}/generic_copies.s,generic_copies.anti"
        "copies_deep|-S,-o,${LISTINGS}/copies_deep.s,copies_deep.anti"
        "copies_many|-S,-o,${LISTINGS}/copies_many.s,copies_many.anti")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 arguments)
    string(REPLACE "," ";" arguments "${arguments}")
    add_expect_test(error_${name} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/${name}.err"
        COMMAND "$<TARGET_FILE:antic>" ${arguments})
endforeach()
# A program of tests/errors that no test reads is refused.
add_test(NAME errors_registered
    COMMAND "${CMAKE_COMMAND}" "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_errors_registered.cmake")
set(DUMP "${CMAKE_CURRENT_SOURCE_DIR}/dump")
# Each case is the stage, the subject, the target or nothing, the expected
# file under tests/dump and the other arguments of antic.
foreach(case "types|symbolic||symbolic.types|${DUMP}/symbolic.anti"
        "types|generics||generics.types|${DUMP}/generics.anti"
        "types|constraints||constraints.types|${DUMP}/constraints.anti"
        "ir|header_main||header/main.ir|${DUMP}/header/main.anti"
        "ir|size_of_main||size_of/main.ir|${DUMP}/size_of/main.anti"
        "select|size_of_main|linux-arm64|size_of/main.linux-arm64.sel|${DUMP}/size_of/main.anti"
        "select|c_long_main|linux-x86_64|c_long/main.linux-x86_64.sel|${DUMP}/c_long/main.anti"
        "select|c_long_main|windows-x86_64|c_long/main.windows-x86_64.sel|${DUMP}/c_long/main.anti"
        "ir|devirt||devirt.ir|${DUMP}/devirt.anti"
        "ir|devirt_dev||devirt.dev.ir|--dev,${DUMP}/devirt.anti"
        "opt|devirt||devirt.opt|${DUMP}/devirt.anti"
        "opt|devirt_dev||devirt.dev.opt|--dev,${DUMP}/devirt.anti"
        "opt|devirt_plugin||devirt_plugin.opt|--lib,shared,--no-runtime,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/devirt_plugin.anti"
        "opt|copies_merge||copies_merge.opt|${DUMP}/copies_merge.anti"
        "opt|copies_merge_dev||copies_merge.dev.opt|--dev,${DUMP}/copies_merge.anti"
        "ir|forward||forward.ir|${DUMP}/forward.anti"
        "opt|forward||forward.opt|${DUMP}/forward.anti"
        "ir|probe||probe.ir|${CMAKE_CURRENT_SOURCE_DIR}/abi/probe.anti"
        "ir|sizes||sizes.ir|${CMAKE_CURRENT_SOURCE_DIR}/programs/sizes.anti"
        "alloc|spillweight|linux-x86_64|spillweight.linux-x86_64.alloc|${DUMP}/spillweight.anti"
        "alloc|sizes|linux-x86_64|sizes.linux-x86_64.alloc|${CMAKE_CURRENT_SOURCE_DIR}/programs/sizes.anti"
        "ir|bitfields||bitfields.ir|${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "alloc|bitfields|linux-x86_64|bitfields.linux-x86_64.alloc|${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "alloc|bitfields|windows-x86_64|bitfields.windows-x86_64.alloc|${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "opt|measure||measure.opt|${DUMP}/measure.anti"
        "alloc|measure|linux-arm64|measure.linux-arm64.alloc|${DUMP}/measure.anti"
        "asm|scale_dev|macos-arm64|scale.dev.macos-arm64.s|--dev,-I,${MODULES},${MODULES}/com/example/scale.anti"
        "asm|scale_dev|windows-x86_64|scale.dev.windows-x86_64.s|--dev,-I,${MODULES},${MODULES}/com/example/scale.anti"
        "asm|main_dev|linux-x86_64|main.dev.linux-x86_64.s|--dev,-I,${LIBS},${MODULES}/main.anti"
        "asm|doubling|linux-x86_64|doubling.linux-x86_64.s|--lib,shared,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
        "asm|doubling|macos-arm64|doubling.macos-arm64.s|--lib,shared,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
        "asm|doubling|windows-arm64|doubling.windows-arm64.s|--lib,shared,-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 stage)
    list(GET parts 1 subject)
    list(GET parts 2 target)
    list(GET parts 3 expected)
    list(GET parts 4 arguments)
    string(REPLACE "," ";" arguments "${arguments}")
    set(target_argument "")
    if(NOT target STREQUAL "")
        set(target_argument TARGET ${target})
    endif()
    antic_dump_test(${stage} ${subject} "${DUMP}/${expected}"
        ${target_argument} ARGS ${arguments})
endforeach()
set_tests_properties(dump_asm_main_dev_linux-x86_64 PROPERTIES
    FIXTURES_REQUIRED twice_antl)

# Calls through function pointers in the IR of chapter 20.
antic_dump_test(opt fnptr
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/fnptr.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/fnptr.anti")

# String literals as globals in the IR of chapter 19.
antic_dump_test(opt strings
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/strings.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/strings.anti")

# Aggregates in the IR of chapter 18.
antic_dump_test(opt structs
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/structs.anti")

# The parameter extensions of chapter 15 in the IR.
antic_dump_test(opt arm64
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/arm64.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/arm64.anti")

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
    antic_dump_test(asm ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.${target}.s"
        TARGET ${target}
        ARGS "${path}")
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

antic_dump_test(select loop
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.arm64.sel"
    TARGET linux-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.anti")
antic_dump_test(select loop
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.x86_64.sel"
    TARGET macos-x86_64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/loop.anti")

# The optimizer examples of chapter 10. Each NAME.anti in tests/opt has its
# IR before optimization in NAME.ir and after optimization in NAME.opt.
file(GLOB opt_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/opt/*.anti")
foreach(source ${opt_sources})
    get_filename_component(name "${source}" NAME_WE)
    get_filename_component(dir "${source}" DIRECTORY)
    antic_dump_test(ir ${name} "${dir}/${name}.ir" ARGS "${source}")
    antic_dump_test(opt ${name} "${dir}/${name}.opt" ARGS "${source}")
endforeach()

# Semantic analysis reports every error it finds.
add_expect_test(error_types STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/types.err"
    COMMAND "$<TARGET_FILE:antic>" -S -o "${CMAKE_CURRENT_BINARY_DIR}/errors/types.s"
            types.anti)

