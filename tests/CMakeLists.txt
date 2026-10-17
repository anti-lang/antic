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
antic_rt_sources(unit_rt_sources assert atomic cpu f16 fs mem platform_posix
    platform_windows text)
add_executable(antic_unit_tests
    unit/test_main.c
    unit/pipeline.c
    unit/test_target.c
    unit/test_cpu.c
    unit/test_llvm_target.c
    unit/test_abi.c
    unit/test_llvm_emit.c
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
    unit/test_link.c
    unit/test_userdirs.c
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
    unit/test_regex.c
    unit/test_pattern.c
    unit/test_bind.c
    unit/test_deps.c
    unit/test_zip.c
    unit/test_syms.c
    unit/test_fmt.c
    unit/test_files.c
    unit/test_tool_platform.c
    unit/test_interface.c
    unit/test_contract.c
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
# The messages two groups write to the standard error, which the group
# itself does not read: the zip reader and writer, and the build id of a
# program.
foreach(group zip syms)
    add_test(NAME unit_${group}_messages
        COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:antic_unit_tests>"
            "-DGROUP=${group}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/unit/${group}.err"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/unit-messages/${group}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_unit_messages.cmake")
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

antic_coverage(antic_unit_tests files_oom_tests alloc_oom_tests)

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

# Each file of the runtime that M321 of docs/audit/summary.md named has
# one header that declares what it defines, and str.h holds the layout of
# a str.
add_test(NAME rt_headers
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_headers.cmake")

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
# collection/list. antic_dev_object registers the test of a module, and
# antic_dev_inputs gives the objects and the fixtures of the modules that
# have one, so the two cannot disagree.
function(antic_dev_object module)
    string(REPLACE "/" "_" name "${module}")
    if(TEST dev_object_${name})
        return()
    endif()
    add_test(NAME dev_object_${name}
        COMMAND antic --dev --llvm-mc "${ANTIC_LLVM_MC}"
                --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
                -I "${ANTIC_RUNTIME_DIR}/std"
                -o "${CMAKE_CURRENT_BINARY_DIR}/anti_${name}_dev"
                "${ANTIC_RUNTIME_DIR}/std/anti/${module}.antl")
    set_tests_properties(dev_object_${name} PROPERTIES
        FIXTURES_SETUP ${name}_dev_object)
endfunction()
foreach(module lang regex mem text reflect runtime collection
        collection/pool collection/tree collection/queue collection/list
        collection/deque collection/ring collection/grid collection/sorted
        collection/map collection/set collection/concurrent
        collection/synchronized)
    antic_dev_object(${module})
endforeach()

# Set objects to the dev objects of the modules after the two names, and
# fixtures to the fixtures that compile them.
function(antic_dev_inputs objects fixtures)
    set(found_objects "")
    set(found_fixtures "")
    foreach(module IN LISTS ARGN)
        string(REPLACE "/" "_" name "${module}")
        if(NOT TEST dev_object_${name})
            message(FATAL_ERROR "no test compiles the dev object of ${module}")
        endif()
        list(APPEND found_objects
            "${CMAKE_CURRENT_BINARY_DIR}/anti_${name}_dev${anti_object}")
        list(APPEND found_fixtures ${name}_dev_object)
    endforeach()
    set(${objects} ${found_objects} PARENT_SCOPE)
    set(${fixtures} ${found_fixtures} PARENT_SCOPE)
endfunction()

# DESIGN: ANTIC_TEST_LTO runs the suite behind --lto, as the step
# runtime-lto of docs/work-order-llvm-optimization.md asks. Empty by
# default, so the suite builds as a user does. Set to full or thin, it
# gives each program test of antic_program_test, antic_modules_test and
# antic_llvm_program_test that builds in release mode --lto in that mode.
# A test whose options hold one that --lto refuses keeps them alone.
set(ANTIC_TEST_LTO "" CACHE STRING
    "full or thin: build the program tests of release mode with --lto")
if(NOT ANTIC_TEST_LTO MATCHES "^(|full|thin)$")
    message(FATAL_ERROR "ANTIC_TEST_LTO is full, thin or empty, not ${ANTIC_TEST_LTO}")
endif()
# Set out to the options after it, with --lto of ANTIC_TEST_LTO added. An
# option may stand in a word of options joined by commas.
function(antic_test_lto out)
    set(options ${ARGN})
    if(ANTIC_TEST_LTO AND NOT options MATCHES
       "(^|[;,])(--dev|-S|-c|--lib|--linker|--lto|--memory-checks)([;,]|$)")
        list(APPEND options --lto ${ANTIC_TEST_LTO})
    endif()
    set(${out} ${options} PARENT_SCOPE)
endfunction()

# A program test of run_program.cmake, which builds source with antic,
# llvm-mc and the runtime archive of this tree, runs it and compares the
# result with the expected file beside it.
#   antic_program_test(<name> <source> WORK <dir> [EXPECTED <file>]
#                      [OPTIONS <option>...] [DEV <module>...]
#                      [OBJECTS <object>...] [UNSET <variable>...]
#                      [EMULATOR <program>] [SKIP_RUN <reason>]
#                      [TIMEOUT <seconds>])
# OPTIONS, OBJECTS, UNSET, EMULATOR, SKIP_RUN and TIMEOUT reach the script
# as run_program.cmake names them. DEV names the modules whose dev objects
# the link takes, which antic_dev_inputs resolves.
function(antic_program_test name source)
    cmake_parse_arguments(PARSE_ARGV 2 arg ""
                          "WORK;EXPECTED;EMULATOR;SKIP_RUN;TIMEOUT"
                          "OPTIONS;DEV;OBJECTS;UNSET")
    if(NOT arg_WORK)
        message(FATAL_ERROR "antic_program_test(${name}) names no WORK")
    endif()
    antic_dev_inputs(dev_objects dev_fixtures ${arg_DEV})
    antic_test_lto(options ${arg_OPTIONS})
    list(JOIN options "," options)
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
    if(arg_EMULATOR)
        list(APPEND optional "-DEMULATOR=${arg_EMULATOR}")
    endif()
    if(arg_SKIP_RUN)
        list(APPEND optional "-DSKIP_RUN=${arg_SKIP_RUN}")
    endif()
    if(arg_TIMEOUT)
        list(APPEND optional "-DTIMEOUT=${arg_TIMEOUT}")
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
    if(arg_SKIP_RUN)
        set_tests_properties(${name} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endif()
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
    list(JOIN arg_LIB_OPTIONS "," lib_options)
    foreach(mode release dev)
        set(objects "")
        set(options ${arg_OPTIONS})
        if(mode STREQUAL "dev")
            set(objects "${std_objects}")
        else()
            antic_test_lto(options ${options})
        endif()
        list(JOIN options "," options)
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
# tokens, ast, types, ir, opt and llvm give --dump-<stage>. ARGS are the
# other arguments of antic, the source among them.
function(antic_dump_test stage subject expected)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "TARGET" "ARGS")
    set(name dump_${stage}_${subject})
    set(arguments "")
    if(arg_TARGET)
        string(APPEND name _${arg_TARGET})
        set(arguments --target ${arg_TARGET})
    endif()
    set(version "")
    if(stage MATCHES "^(tokens|ast|types|ir|opt|llvm)$")
        list(PREPEND arguments --dump-${stage})
        # The LLVM text names the version of antic, which the expected
        # file writes as VERSION.
        if(stage STREQUAL "llvm")
            set(version "-DVERSION=${PROJECT_VERSION}")
        endif()
    else()
        message(FATAL_ERROR "antic dumps no stage ${stage}")
    endif()
    list(APPEND arguments ${arg_ARGS})
    list(JOIN arguments "," command)
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DCOMMAND=${command}"
            ${version}
            "-DEXPECTED=${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dump.cmake")
endfunction()

# llvm-objdump, llvm-readobj and llvm-mc of the runtime archive, with the
# suffix of an executable of the host, .exe on Windows. Every test names
# them by these three. Spelled without the suffix, the file was missing on
# Windows, and each test that reads a library there skipped.
set(ANTIC_RUNTIME_OBJDUMP
    "${ANTIC_RUNTIME_DIR}/bin/llvm-objdump${CMAKE_EXECUTABLE_SUFFIX}")
set(ANTIC_RUNTIME_READOBJ
    "${ANTIC_RUNTIME_DIR}/bin/llvm-readobj${CMAKE_EXECUTABLE_SUFFIX}")
set(ANTIC_RUNTIME_MC
    "${ANTIC_RUNTIME_DIR}/bin/llvm-mc${CMAKE_EXECUTABLE_SUFFIX}")

# The signal table of the runtime, in a program of its own. The test
# starts threads, and so does the reader of the platform layer.
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
antic_rt_sources(conf_sources conf inject assert toml atomic fs hash utf
    platform_posix platform_windows)
add_executable(rt_conf_tests unit/test_conf_race.c ${conf_sources})
target_link_libraries(rt_conf_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
target_compile_definitions(rt_conf_tests PRIVATE
    ANTIC_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME rt_conf_race COMMAND rt_conf_tests)

# M315. Object.deserialize against malformed and cut-off text, in a
# program of its own with stand-ins for the classes, the registry and the
# loader. The runtime of an Anti program carries no sanitizer, and this
# program does in the sanitizer builds.
antic_rt_sources(deserialize_sources registry object json utf f16 text cast
    mem assert atomic fs platform_posix platform_windows)
add_executable(rt_deserialize_tests unit/test_deserialize.c
    ${deserialize_sources})
target_link_libraries(rt_deserialize_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
add_test(NAME rt_deserialize COMMAND rt_deserialize_tests)

# The loader of the runtime under threads that load at once, in a program
# of its own with stand-ins for the rest of the runtime. It loads two
# libraries that each carry an empty table of a plugin.
foreach(which one two)
    add_library(plugin_table_${which} SHARED unit/plugin_table.c)
endforeach()
antic_rt_sources(plugin_sources plugin assert loaded atomic digest text toml
    utf platform_posix platform_windows object json cast mem)
add_executable(rt_plugin_tests unit/test_plugin_race.c ${plugin_sources})
target_link_libraries(rt_plugin_tests PRIVATE Threads::Threads
    ${CMAKE_DL_LIBS})
# Fifteen copies of the first library, each in a directory of its own,
# fill every slot beside it.
set(plugin_copies "${CMAKE_CURRENT_BINARY_DIR}/plugin-copies")
foreach(copy RANGE 1 15)
    add_custom_command(TARGET rt_plugin_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${plugin_copies}/${copy}"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:plugin_table_one>"
            "${plugin_copies}/${copy}/$<TARGET_FILE_NAME:plugin_table_one>"
        VERBATIM)
endforeach()
target_compile_definitions(rt_plugin_tests PRIVATE
    "PLUGIN_ONE=\"$<TARGET_FILE:plugin_table_one>\""
    "PLUGIN_TWO=\"$<TARGET_FILE:plugin_table_two>\""
    "PLUGIN_COPIES=\"${plugin_copies}\""
    "PLUGIN_NAME=\"$<TARGET_FILE_NAME:plugin_table_one>\"")
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
# S312 and M301. The text builder, the copy of an object and the copy of
# an owned buffer when the C library refuses their memory, in a program of
# its own, since each case ends the process. The files of the runtime it
# links build with malloc and realloc named after the stand-ins of the
# test, which refuse on request.
antic_rt_sources(oom_sources text object json cast assert mem atomic utf
    fs platform_posix platform_windows)
add_library(rt_oom_runtime OBJECT ${oom_sources})
target_compile_definitions(rt_oom_runtime PRIVATE
    malloc=refused_malloc realloc=refused_realloc)
add_executable(rt_oom_tests unit/test_rt_oom.c
    $<TARGET_OBJECTS:rt_oom_runtime>)
target_include_directories(rt_oom_tests PRIVATE ../src/rt)
target_link_libraries(rt_oom_tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
add_test(NAME rt_oom
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:rt_oom_tests>"
        "-DCASES=take,append,fill,append_count,fill_count,copy_buffer,dup"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/rt-oom"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_oom.cmake")
# T139. The listing of anti.fs when the C library refuses its memory.
add_test(NAME rt_oom_fs
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:rt_oom_tests>"
        "-DCASES=fs_list"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/rt-oom-fs"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_oom.cmake")
# The programs of the runtime above compile its files themselves, so a
# coverage tree measures src/rt/ through them.
antic_coverage(rt_signal_tests rt_conf_tests rt_plugin_tests
    rt_platform_tests rt_plugin_table_tests rt_oom_runtime rt_oom_tests)

# add_expect_test(<name> STATUS <status> [DIRECTORY <dir>]
#                 [TIMEOUT <seconds>]
#                 EXPECTED <file> | PATTERN <regex>
#                 COMMAND <program> <arguments>...)
# registers a test of run_expect.cmake, which checks the exit status of
# the program beside its output. Every test of a refusal or of a warning
# goes through it, and none sets PASS_REGULAR_EXPRESSION, under which
# ctest ignores the status. The command line of the test carries no `;`,
# no `,` inside an argument and no line break, so a pattern holds neither
# and a test that needs one compares a file with EXPECTED.
function(add_expect_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg ""
                          "STATUS;DIRECTORY;TIMEOUT;EXPECTED;PATTERN"
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
    if(DEFINED arg_TIMEOUT)
        list(APPEND values "-DTIMEOUT=${arg_TIMEOUT}")
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
# Beside the texts, licenses/sources.txt records the upstream source of
# every pinned component: name, version and the URL of the exact source
# package, written from the pins.
add_test(NAME runtime_sources
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DSOURCES=${ANTIC_RUNTIME_DIR}/licenses/sources.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_sources.cmake")

# The processor levels. src/antic/cpu.c holds the table for antic and
# tools/cpu-levels holds it for the build, so the two are compared.
add_test(NAME cpu_levels_pin
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLEVELS=${PROJECT_SOURCE_DIR}/tools/cpu-levels"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_pin.cmake")

# The data layout of every target and the CPU attributes of every level that
# the LLVM back end writes are the ones the pinned clang writes.
add_test(NAME llvm_datalayout_pin
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCLANG=${ANTIC_CLANG_DIR}/bin/clang"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm_datalayout_pin"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_datalayout_pin.cmake")

# The text of the LLVM back end passes the verifier of opt for every program
# of tests/dump and tests/programs, in release and dev mode, one test per
# target. From the step emit-wide on the translator takes every operation
# of the IR, so a program it refuses fails the test.
# DESIGN: one test per target, since each runs antic twice per program. As
# one test the twelve runs per program took over nine minutes under ASan,
# whose start alone costs a fifth of a second.
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    add_test(NAME llvm_verify_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DOPT=${ANTIC_OPT}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/dump,${CMAKE_CURRENT_SOURCE_DIR}/programs"
            "-DTARGETS=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm_verify/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_verify.cmake")
endforeach()
# run_llvm_verify.cmake fails for an antic that a signal ends, and for a
# refused file that its list of the files that are no program lacks.
add_test(NAME llvm_verify_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_verify.cmake"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPT=${ANTIC_OPT}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DTARGET=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm_verify_runner"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_verify_self.cmake")

# A program of tests/programs built with antic, which runs opt and llc of
# the runtime archive and links, and run on a target the host runs. MODE is release or dev, and OPTIONS the options of antic
# beside them. SOURCE names a program elsewhere than tests/programs,
# OBJECTS the objects of C it links and EXPECTED its expected output. The
# test is llvm_program_<name>_<target>_<mode>.
function(antic_llvm_program_test name target mode)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "SOURCE;OBJECTS;EXPECTED"
                          "OPTIONS")
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.anti")
    if(arg_SOURCE)
        set(source "${arg_SOURCE}")
    endif()
    set(extra "")
    if(arg_OBJECTS)
        list(APPEND extra "-DOBJECTS=${arg_OBJECTS}")
    endif()
    if(arg_EXPECTED)
        list(APPEND extra "-DEXPECTED=${arg_EXPECTED}")
    endif()
    set(options ${arg_OPTIONS})
    if(target STREQUAL "macos-x86_64")
        list(APPEND options --cpu v1)
    endif()
    if(mode STREQUAL "release")
        antic_test_lto(options ${options})
    endif()
    list(JOIN options "," options)
    add_test(NAME llvm_program_${name}_${target}_${mode}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DTARGET_NAME=${target}"
            "-DMODE=${mode}"
            "-DOPTIONS=${options}"
            ${extra}
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm_programs"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_program.cmake")
endfunction()

# The scalar programs of tests/programs, in release and dev mode, for
# macos-arm64 and for macos-x86_64 under Rosetta, which take --cpu v1 as
# every macos-x86_64 program of a macos-arm64 host does. scalar_ops and
# llvm_division build in dev mode with --no-checks, so the defined result
# stands behind each check. asserts calls
# the runtime from its assertions in dev mode, and runs in release mode
# alone. llvm_division is the test llvm_division of
# docs/work-order-llvm-back-end.md. The six programs of the wide
# operations run in both modes from the step emit-wide on, and llvm_flags,
# the test llvm_flags, runs in release mode as the work order gives it.
set(llvm_targets "")
if(ANTIC_HOST_TARGET STREQUAL "macos-arm64")
    set(llvm_targets macos-arm64 macos-x86_64)
elseif(ANTIC_HOST_TARGET STREQUAL "macos-x86_64")
    set(llvm_targets macos-x86_64)
endif()
foreach(target IN LISTS llvm_targets)
    foreach(name IN ITEMS return42 return_pieces deep_nesting long_float
            asserts scalar_ops llvm_division llvm_wrap)
        antic_llvm_program_test(${name} ${target} release)
    endforeach()
    foreach(name IN ITEMS return42 return_pieces deep_nesting long_float)
        antic_llvm_program_test(${name} ${target} dev)
    endforeach()
    foreach(name IN ITEMS scalar_ops llvm_division llvm_wrap)
        antic_llvm_program_test(${name} ${target} dev OPTIONS --no-checks)
    endforeach()
    foreach(name IN ITEMS flags saturating mul_high carry_chain wrapping
            simd)
        antic_llvm_program_test(${name} ${target} release)
        antic_llvm_program_test(${name} ${target} dev)
    endforeach()
    antic_llvm_program_test(llvm_flags ${target} release)
endforeach()

# The tool run of the LLVM back end: --keep-llvm, the bitcode of release
# mode alone and -S. A failed opt or llc prints its own message and then
# the line of antic. --opt and --llc name the tool, here one that refuses
# the options antic passes: llc for opt, and llvm-mc for llc, since opt
# takes the options of llc. The tests run on the hosts of llvm_targets,
# and the step vm adds the others.
if(llvm_targets)
    add_test(NAME llvm_run
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm_run"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_run.cmake")
    add_expect_test(llvm_opt_failed STATUS 1
        PATTERN "llc: error: -passes is for .mir file only.*antic: opt failed"
        COMMAND $<TARGET_FILE:antic>
                --runtime "${ANTIC_RUNTIME_DIR}" --opt "${ANTIC_LLC}"
                -o "${CMAKE_CURRENT_BINARY_DIR}/llvm_opt_failed"
                "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
    add_expect_test(llvm_llc_failed STATUS 1
        PATTERN "llvm-mc: Unknown command line argument .*antic: llc failed"
        COMMAND $<TARGET_FILE:antic> --dev
                --runtime "${ANTIC_RUNTIME_DIR}" --llc "${ANTIC_LLVM_MC}"
                -o "${CMAKE_CURRENT_BINARY_DIR}/llvm_llc_failed"
                "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
endif()

# The test llvm_inline_lines: a release build with -g inlines a small
# callee, and the line table names the line of the callee in the code of
# its caller.
if(llvm_targets)
    add_test(NAME llvm_inline_lines
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/llvm_inline_lines.anti"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-inline-lines"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_inline_lines.cmake")
endif()

# The test of the step tables of docs/work-order-llvm-optimization.md: a
# release build calls the function of an object of a known class
# directly after opt, across the call of the `created` hook. Every host
# builds for every target, so each spelling of the name is read on every
# host. The Windows VM found the COFF one missing.
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    add_test(NAME direct_calls_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOPT=${ANTIC_OPT}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/table_hook.anti"
            "-DFUNCTION=table_hook.main"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/direct-calls/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_direct_calls.cmake")
    set_tests_properties(direct_calls_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# A COFF host of plugins exports every name its plugin imports, and the
# plugin lists as many places as its table counts. The VM run runs both.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME llvm_coff_plugin_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-coff-plugin/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_coff_plugin.cmake")
    set_tests_properties(llvm_coff_plugin_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()
# A library that keeps an `f16` converts it through two functions of the
# runtime below x86-64-v3, and takes them from its host as it takes every
# other one.
add_test(NAME llvm_coff_plugin_f16
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DTARGET=windows-x86_64"
        "-DCPU=v2"
        "-DLIBRARY=halves"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-coff-plugin/f16"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_coff_plugin.cmake")
set_tests_properties(llvm_coff_plugin_f16 PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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
# levels below it write the SSE forms. v2 adds no instruction that llc
# writes for the program, so it differs from v1 in the runtime alone.
set(cpu_sse "\taddsd\t|\taddss\t|\tmulss\t|\tucomisd\t|\tcvtss2sd\t|\tmovsd\t")
set(cpu_avx "\tvaddsd\t|\tvaddss\t|\tvmulss\t|\tvucomisd\t|\tvcvtss2sd\t|\tvmovsd\t")
foreach(case "v1,${cpu_sse},${cpu_avx}" "v2,${cpu_sse},${cpu_avx}"
        "v3,${cpu_avx},${cpu_sse}")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 level)
    list(GET parts 1 uses)
    list(GET parts 2 avoids)
    add_test(NAME cpu_level_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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

# The runtime keeps the frame record of every function that calls another
# on Linux and macOS, so a trace walks through its frames.
add_test(NAME runtime_frames
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/runtime-frames.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_frames.cmake")
set_tests_properties(runtime_frames PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# The runtime of linux-arm64 is the same on every host: neither its
# objects nor its bitcode take the outline atomics that the GCC of a Linux
# host brought.
add_test(NAME runtime_hosts
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_AR=${ANTIC_LLVM_AR}"
        "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/runtime-hosts"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_hosts.cmake")
set_tests_properties(runtime_hosts PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# The atomic operations of a program are LLVM atomic instructions on every
# target, and each target and level writes the instructions its runtime
# holds: one instruction with LSE, a load-store exclusive loop on armv8.0,
# and the lock prefix on x86_64. No call of the runtime remains.
add_test(NAME llvm_atomics
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/atomics.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-atomics"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_atomics.cmake")
set(atomic_lse "\tcasal\t|\tldaddal\t|\tswpal\t|\tcasalb\t|\tcasalh\t")
set(atomic_exclusive "\tldaxr\t|\tstlxr\t|\tldaxrb\t|\tstlxrh\t")
set(atomic_x86 "\tlock\t\t|xadd|cmpxchg|\txchg")
set(atomic_calls "anti_rt_atomic|__aarch64_")
foreach(case "linux-arm64,armv8.0,${atomic_exclusive},${atomic_lse}"
        "linux-arm64,armv8.2,${atomic_lse},${atomic_exclusive}"
        "linux-arm64,armv8.5,${atomic_lse},${atomic_exclusive}"
        "windows-arm64,armv8.2,${atomic_lse},${atomic_exclusive}"
        "macos-arm64,armv8.5,${atomic_lse},${atomic_exclusive}"
        "linux-x86_64,v1,${atomic_x86},"
        "linux-x86_64,v3,${atomic_x86},"
        "macos-x86_64,v3,${atomic_x86},"
        "windows-x86_64,v3,${atomic_x86},")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 uses)
    list(GET parts 3 avoids)
    if(avoids STREQUAL "")
        set(avoids "${atomic_calls}")
    else()
        set(avoids "${avoids}|${atomic_calls}")
    endif()
    add_test(NAME atomics_${target}_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DTARGET=${target}"
            "-DLEVEL=${level}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/atomics.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/atomics-level"
            "-DUSES=${uses}"
            "-DAVOIDS=${avoids}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpu_level.cmake")
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
# for each, and v1 and v2, which lack F16C, call the runtime. No call
# reaches the conversions of compiler-rt.
set(f16_call "anti_rt_f16_to_f32|anti_rt_f32_to_f16")
set(f16_f16c "\tvcvtph2ps\t|\tvcvtps2ph\t")
set(f16_fcvt "\tfcvt\ts|\tfcvt\th")
set(f16_rt "__extendhfsf2|__truncsfhf2")
foreach(case "linux-x86_64,v1,${f16_call},cvtph2ps|cvtps2ph|${f16_rt}"
        "linux-x86_64,v2,${f16_call},cvtph2ps|cvtps2ph|${f16_rt}"
        "linux-x86_64,v3,${f16_f16c},${f16_call}|${f16_rt}"
        "windows-x86_64,v1,${f16_call},cvtph2ps|cvtps2ph|${f16_rt}"
        "windows-x86_64,v3,${f16_f16c},${f16_call}|${f16_rt}"
        "linux-arm64,armv8.0,${f16_fcvt},${f16_call}|${f16_rt}"
        "macos-arm64,armv8.5,${f16_fcvt},${f16_call}|${f16_rt}")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 uses)
    list(GET parts 3 avoids)
    add_test(NAME f16_${target}_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
foreach(case "linux-x86_64,v3,vaddps[^\n]*%ymm,1"
        "linux-x86_64,v1,addps[^\n]*%xmm,2"
        "linux-x86_64,v2,addps[^\n]*%xmm,2"
        "macos-arm64,armv8.5,fadd.4s\tv,2"
        "linux-arm64,armv8.0,fadd\tv[0-9]+.4s,2")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 level)
    list(GET parts 2 pattern)
    list(GET parts 3 count)
    add_test(NAME simd_width_${target}_${level}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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

# Every script of `cmake -P` names the minimum version of CMake, so CMake 3
# sets the policies CMake 4 takes for granted.
add_test(NAME script_policies
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_script_policies.cmake")
add_test(NAME script_policies_runner
    COMMAND "${CMAKE_COMMAND}"
            "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_script_policies.cmake"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/script-policies"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_script_policies_self.cmake")
# Anti runs no CI, so no file stands under .github/workflows/.
add_test(NAME no_workflows
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_no_workflows.cmake")
# The tool that fetched the CRT and SDK of Microsoft is named nowhere but in
# the history of docs/decisions.md and docs/reports/, and no file of the
# code names the variable of the installers that went with it.
add_test(NAME no_sdk_fetcher
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_no_sdk_fetcher.cmake")
# One function downloads for the whole build.
add_test(NAME downloads_once
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_downloads_once.cmake")

# bin/ of the runtime archive is built by the root CMakeLists.txt from
# the directory of the pinned LLVM tools, and by no file of tests/.
add_test(NAME llvm_tools_root
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_tools_root.cmake")

# The LLVM tools stand in one list, and tools/check-llvm.cmake checks
# them through the one check.
add_test(NAME llvm_tool_list
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_tool_list.cmake")

# The copy into bin/ of the runtime archive takes every tool of the list
# and refuses a directory that lacks one.
add_test(NAME llvm_tools_copy
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-tools-copy"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_tools_copy.cmake")

# A message at a position has one printer in antic, and the platform
# layer names the tool that runs.
add_test(NAME message_forms
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_message_forms.cmake")

# The warning flags of the build stand in tools/warnings.cmake and
# src/native/warnings.cmake alone, as docs/c-guidelines.md requires.
add_test(NAME warning_set
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_warning_set.cmake")

# No header of ours hides a header of the C library on an include path.
add_test(NAME header_names
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_header_names.cmake")

# Rule 25: every symbol a runtime library of the archive defines starts
# with anti_rt_, on every target and level.
add_test(NAME rt_names
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rt_names.cmake")
set_tests_properties(rt_names PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

# Each declaration of antl_io.h, driver_parts.h, parser_parser.h,
# sema_checker.h, lower_lowerer.h and whole_parts.h stands under the file
# that defines it, and the comment of each header names every file.
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

# The usage. `--help`, and `-h` of anti, print it on the standard output
# and end with 0, and the tool without an argument prints the same bytes
# on the standard error and ends with 2.
add_test(NAME antic_help
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=$<TARGET_FILE:antic>,--help"
            -DNAME=antic -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
add_test(NAME anti_help
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=$<TARGET_FILE:anti>,--help"
            -DNAME=anti -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
add_test(NAME anti_help_short
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=$<TARGET_FILE:anti>,-h"
            -DNAME=anti -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
# R155 of "Rules of 2026-10-13" in docs/decisions.md: `--help` of anti
# with a second word prints the usage and ends with 0.
add_test(NAME anti_help_and_a_word
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=$<TARGET_FILE:anti>,--help,extra"
            -DNAME=anti -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")

# A command line that a tool refuses with its usage: 2, nothing on the
# standard output and the bytes of `--help` on the standard error. antic
# names an option it does not know and a second input before the usage.
# The name of a case is its test, and the words after it are the arguments.
function(add_usage_test tool name)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "FIRST" "ARGS")
    list(JOIN arg_ARGS "," words)
    set(values "-DCOMMAND=$<TARGET_FILE:${tool}>,${words}" -DNAME=${tool}
        -DREFUSED=ON)
    if(DEFINED arg_FIRST)
        list(APPEND values "-DFIRST=${arg_FIRST}")
    endif()
    add_test(NAME ${tool}_usage_${name}
        COMMAND "${CMAKE_COMMAND}" ${values}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
endfunction()
set(usage_source "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
add_usage_test(antic option_unknown FIRST "antic: unknown option --bogus"
    ARGS --bogus "${usage_source}")
add_usage_test(antic second_input FIRST "antic: one input file only"
    ARGS "${usage_source}" "${usage_source}")
add_usage_test(antic options_without_an_input ARGS -S --dev)
# R141 of "Rules of 2026-10-13" in docs/decisions.md: the usage of antic
# lists the options of the modes, the checks and the hooks.
add_test(NAME antic_usage_lists_options
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=$<TARGET_FILE:antic>,--help"
            "-DLISTS=--help|--no-reflect|--asserts|--no-asserts|--checks|--no-checks|--no-hooks|--trace|--no-trace"
            -DNAME=antic -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
# R142 of "Rules of 2026-10-13" in docs/decisions.md: `--help` of antic
# among other arguments prints the usage and ends with 0, and antic
# compiles nothing.
add_test(NAME antic_help_among_arguments
    COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:antic>,-S,--dev,--help,${usage_source}"
            -DNAME=antic -P "${CMAKE_CURRENT_SOURCE_DIR}/run_usage.cmake")
foreach(case
        "command_unknown|bogus"
        "option_unknown|--bogus"
        "version_and_a_word|--version|extra"
        "new_without_a_name|new"
        "new_with_two_names|new|com.example.one|com.example.two"
        "new_with_an_option|new|--bogus"
        "build_option_unknown|build|--bogus"
        "build_word|build|extra"
        "build_lib_kind|build|--lib|bundled"
        "check_option_unknown|check|--bogus"
        "check_targets_value|check|--targets|some"
        "doc_option_unknown|doc|--bogus"
        "fmt_option_unknown|fmt|--bogus"
        "test_option_unknown|test|--bogus"
        "bind_without_an_input|bind"
        "bind_option_unknown|bind|--bogus|api.json"
        "bind_two_inputs|bind|api.json|other.json"
        "bind_header_and_an_input|bind|--header|api.antl|api.json"
        "symbols_without_a_command|symbols"
        "symbols_command_unknown|symbols|bogus|--conf|app.toml"
        "symbols_option_unknown|symbols|check|--conf|app.toml|--bogus"
        "symbols_inventory_without_conf|symbols|inventory"
        "symbols_inventory_with_symbols|symbols|inventory|--conf|app.toml|--symbols|symbols.zip"
        "symbols_check_with_from|symbols|check|--conf|app.toml|--from|archives"
        "symbols_check_with_a_trace|symbols|check|--conf|app.toml|trace.txt"
        "symbols_resolve_with_from|symbols|resolve|trace.txt|--symbols|symbols.zip|--from|archives"
        "symbols_resolve_with_conf|symbols|resolve|trace.txt|--symbols|symbols.zip|--conf|app.toml"
        "symbols_resolve_without_symbols|symbols|resolve|trace.txt"
        "symbols_resolve_two_traces|symbols|resolve|trace.txt|other.txt|--symbols|symbols.zip"
        "sdk_without_a_command|sdk"
        "sdk_command_unknown|sdk|bogus"
        "sdk_export_option_unknown|sdk|export|--bogus"
        "sdk_import_without_a_bundle|sdk|import"
        "license_option_unknown|license|--bogus")
    string(REPLACE "|" ";" parts "${case}")
    list(POP_FRONT parts name)
    add_usage_test(anti ${name} ARGS ${parts})
endforeach()
# An option of anti that takes a value, as the last word. By R155 of
# "Rules of 2026-10-13" in docs/decisions.md anti writes one line that
# names the option and ends with 2, as antic does.
foreach(case
        "build|--target" "build|--cpu" "build|--lto" "build|--lib"
        "build|--profile-use" "build|--runtime" "build|--llvm-mc"
        "build|--llvm-ar"
        "check|--targets" "check|--work" "check|--runtime" "check|-I"
        "doc|-o" "doc|--work" "doc|--runtime" "doc|-I"
        "test|--work" "test|--runtime" "test|--llvm-mc" "test|-I"
        "bind|--header" "bind|--clang" "bind|-o" "bind|--runtime"
        "bind|--module" "bind|--target" "bind|-I" "bind|-D"
        "symbols|--conf" "symbols|--from" "symbols|--out" "symbols|--symbols"
        "sdk_export|--sdk" "sdk_export|-o" "sdk_import|--sysroot"
        "license|--from" "license|--from-archive" "license|--runtime")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 command)
    list(GET parts 1 option)
    string(REGEX REPLACE "^-+" "" word "${option}")
    string(REPLACE "-" "_" word "${word}")
    if(option STREQUAL "-o")
        set(word "output")
    elseif(option STREQUAL "-I")
        set(word "root")
    elseif(option STREQUAL "-D")
        set(word "define")
    endif()
    set(words ${command})
    if(command STREQUAL "symbols")
        set(words symbols inventory)
    elseif(command STREQUAL "sdk_export")
        set(words sdk export)
    elseif(command STREQUAL "sdk_import")
        set(words sdk import bundle.tar.xz)
    endif()
    set(line "anti: ${option} needs a value")
    file(WRITE
         "${CMAKE_CURRENT_BINARY_DIR}/expect/anti_option_${command}_${word}_without_a_value"
         "${line}\n")
    add_expect_test(anti_option_${command}_${word}_without_a_value STATUS 2
        EXPECTED "${CMAKE_CURRENT_BINARY_DIR}/expect/anti_option_${command}_${word}_without_a_value"
        COMMAND "$<TARGET_FILE:anti>" ${words} ${option})
endforeach()

# A wrong value of an option of antic, and an option that takes a value as
# the last word. antic writes one line and ends with 2.
function(add_antic_refusal name line)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_option_${name}"
         "${line}\n")
    add_expect_test(antic_option_${name} STATUS 2
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/programs"
        EXPECTED "${CMAKE_CURRENT_BINARY_DIR}/expect/antic_option_${name}"
        COMMAND "$<TARGET_FILE:antic>" ${ARGN})
endfunction()
add_antic_refusal(linker_value "antic: --linker takes lld or platform"
    --linker gold return42.anti)
add_antic_refusal(inject_value
    "antic: --inject takes an interface and its provider, as --inject Interface=Provider, found Logger"
    --inject Logger return42.anti)
add_antic_refusal(target_value "antic: unknown target plan9"
    --runtime "${ANTIC_RUNTIME_DIR}" --target plan9 return42.anti)
add_antic_refusal(input_suffix "antic: return42.expected: expected a .anti file"
    --runtime "${ANTIC_RUNTIME_DIR}" return42.expected)
add_antic_refusal(lto_memory_checks_macos
    "antic: --lto links no macOS program of --memory-checks"
    --runtime "${ANTIC_RUNTIME_DIR}" --target macos-arm64 --lto full
    --memory-checks -o "${CMAKE_CURRENT_BINARY_DIR}/lto_memory_checks"
    return42.anti)
foreach(option --lib --linker --lto --framework --linux-lib --inject
        --dependency --attribution -I --llvm-ar --profile-use
        --package-name --package-version --license --license-text -o
        --target --cpu --llvm-mc --opt --llc --runtime)
    string(REGEX REPLACE "^-+" "" word "${option}")
    string(REPLACE "-" "_" word "${word}")
    if(option STREQUAL "-o")
        set(word "output")
    elseif(option STREQUAL "-I")
        set(word "root")
    endif()
    add_antic_refusal(${word}_without_a_value
        "antic: ${option} needs a value" return42.anti ${option})
endforeach()

# Options of antic that the driver reads together, one test per case of
# tests/run_antic_options.cmake, whose header names what each shows.
set(antic_option_cases checks_by_mode checks_last_decides asserts_by_mode
    asserts_text_dropped
    warn_undocumented
    strict_library strict_front_end strict_dev bundle_static bundle_shared
    output_assembly output_library output_program keep_llvm
    output_static output_shared output_dev_library output_plugin
    output_native_library output_native_plugin linker_lld)
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/linux-x86_64")
    list(APPEND antic_option_cases soname_linux)
endif()
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/windows-x86_64")
    list(APPEND antic_option_cases windows_library_reproducible)
endif()
foreach(case IN LISTS antic_option_cases)
    add_test(NAME antic_option_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            -DCASE=${case}
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DEXE=${CMAKE_EXECUTABLE_SUFFIX}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/antic-options/${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antic_options.cmake")
endforeach()
# --cpu beside --target, for each target: its three levels, its default
# and the three levels of the other architecture, which it refuses.
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    add_test(NAME antic_option_target_cpu_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            -DCASE=target_cpu
            -DTARGET=${target}
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/antic-options/target_cpu_${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antic_options.cmake")
endforeach()

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

# The LLVM tools in bin/ of the runtime archive, which the target
# llvm_tools of the root CMakeLists.txt copies there, are those of LLVM
# tools/llvm-version. The directory holds no llvm-cov, which toolchain_pin
# refuses there.
add_test(NAME toolchain_pin
    COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${ANTIC_RUNTIME_DIR}/bin"
        -DRUNTIME_ARCHIVE=ON
        -P "${PROJECT_SOURCE_DIR}/tools/check-llvm.cmake")

# A module that writes no function writes no empty CodeView subsection of
# symbols, which lld-link warns of. codeview_empty_refuses_<target> feeds
# the check an object with an empty one, which it must refuse.
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME codeview_empty_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
            "-DSTD=${ANTIC_RUNTIME_DIR}/std"
            "-DSOURCE=${ANTIC_RUNTIME_DIR}/std/anti/collection/tree.antl"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/codeview-empty"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_codeview_empty.cmake")
    add_test(NAME codeview_empty_refuses_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DCLANG=${CMAKE_C_COMPILER}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
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

    # antic and anti compile for every Linux and Windows host with the
    # options of a package and the warning set.
    foreach(host linux-x86_64 linux-arm64 windows-x86_64 windows-arm64)
        add_test(NAME host_sources_${host}
            COMMAND "${CMAKE_COMMAND}"
                "-DROOT=${PROJECT_SOURCE_DIR}"
                "-DHOST=${host}"
                "-DCLANG=${CMAKE_C_COMPILER}"
                "-DSYSROOT=${ANTIC_SYSROOT_DIR}"
                "-DPCRE2_INCLUDE=${ANTIC_PCRE2_INCLUDE}"
                "-DVERSION=${PROJECT_VERSION}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_host_sources.cmake")
    endforeach()
endif()

file(GLOB program_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/*.anti")
# DESIGN: a program named llvm_* checks a result of release mode that
# "Defined results in release mode" of docs/work-order-llvm-back-end.md
# defines, such as a division by zero. It runs through
# antic_llvm_program_test, in the modes and with the options it needs.
list(FILTER program_sources EXCLUDE REGEX "/llvm_[a-z_]+\\.anti$")
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
# The facts of arithmetic with the checks of a dev build in a release
# build, where the checked operations carry nsw.
antic_program_test(program_arith_facts_checks
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/arith_facts.anti"
    OPTIONS "--checks"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-checks")
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
# The default to_text and type_name without the field lists, which give
# the class name as they give it with them.
antic_program_test(program_object_defaults_no_reflect
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/object_defaults.anti"
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
# did not ask, and leaves the marked code alone. --no-hooks beside --trace
# drops the call hooks that --trace turned on, which gives the output of
# release.
# The value of --trace names a module as well, which reaches every class
# of it.
foreach(case "hooks|no_hooks|--no-hooks|hooks.no-hooks.expected"
        "hook_trace|trace|--trace|hook_trace.trace.expected"
        "hook_trace|writes|--trace,--trace,writes|hook_trace.writes.expected"
        "hook_trace|pattern|--trace,Quiet|hook_trace.pattern.expected"
        "hook_trace|trace_no_hooks|--trace,--no-hooks|hook_trace.expected"
        "hook_trace|module|--trace,hook_trace|hook_trace.module.expected"
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

# DESIGN: every arm64 host runs the x86_64 programs of its own system under
# emulation, so the Mac and the two VMs of docs/vm-setup.md run the
# programs of all six targets between them. Anti runs no CI and no machine
# here has an x86_64 processor. macos-arm64 runs macos-x86_64 under
# Rosetta, linux-arm64 runs linux-x86_64 under qemu-x86_64 and
# windows-arm64 runs windows-x86_64 under the x64 emulation of Windows.
# Rosetta has no AVX2, so the programs of macos-x86_64 take --cpu v1 and
# link the v1 runtime of the archive, as any --cpu v1 build does. qemu and
# Windows emulate x86-64-v3, and their programs take the default level. A
# linux-arm64 host without qemu-x86_64 runs no linux-x86_64 program. Each
# include directory is one joined argument, since CMake drops a repeated
# -isystem from the options of a target.
set(ANTIC_EMULATED_TARGET "")
set(ANTIC_EMULATOR "")
set(emulated_c_flags "")
if(ANTIC_HOST_TARGET STREQUAL "macos-arm64")
    set(ANTIC_EMULATED_TARGET macos-x86_64)
    set(ANTIC_X86_64_OPTIONS "--target,macos-x86_64,--cpu,v1")
    set(emulated_c_flags -arch x86_64 -mmacosx-version-min=11.0)
elseif(ANTIC_HOST_TARGET STREQUAL "linux-arm64")
    find_program(ANTIC_QEMU_X86_64 qemu-x86_64)
    if(ANTIC_QEMU_X86_64)
        set(ANTIC_EMULATED_TARGET linux-x86_64)
        set(ANTIC_EMULATOR "${ANTIC_QEMU_X86_64}")
    else()
        message(STATUS "No qemu-x86_64: the suite runs no linux-x86_64 program")
    endif()
    set(ANTIC_X86_64_OPTIONS "--target,linux-x86_64")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
            OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(emulated_c_flags --target=x86_64-linux-musl -fPIC -nostdinc
            "-isystem${resource}/include"
            "-isystem${ANTIC_RUNTIME_DIR}/sysroot/linux-x86_64/usr/include")
    endif()
elseif(ANTIC_HOST_TARGET STREQUAL "windows-arm64")
    set(ANTIC_EMULATED_TARGET windows-x86_64)
    set(ANTIC_X86_64_OPTIONS "--target,windows-x86_64")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
            OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE)
        antic_windows_triple(triple windows-x86_64)
        antic_windows_compile_options(emulated_c_flags
            "${ANTIC_RUNTIME_DIR}/sysroot/windows-x86_64" "${resource}")
        list(PREPEND emulated_c_flags "--target=${triple}")
    endif()
endif()

# The same C files compiled for the emulated target, which its ABI
# programs link. The host runs them, so antic meets clang on the rules of
# x86_64 as well.
# DESIGN: the object takes no runtime library of Microsoft. On a Windows
# host CMake would add the directive of msvcrtd for a Debug build, which
# names a library the sysroot of mingw-w64 does not hold and which the
# link of the program against the sysroot then refuses.
function(abi_x86_64_object name source)
    add_library(${name} OBJECT "${source}")
    target_compile_options(${name} PRIVATE ${emulated_c_flags}
        -fno-sanitize=all ${ANTIC_REFERENCE_C_FLAGS})
    set_target_properties(${name} PROPERTIES MSVC_RUNTIME_LIBRARY "")
endfunction()

if(ANTIC_EMULATED_TARGET)
    set(emulated "${ANTIC_EMULATED_TARGET}")
    set(emulator "")
    if(ANTIC_EMULATOR)
        set(emulator EMULATOR "${ANTIC_EMULATOR}")
    endif()
    if(emulated_c_flags)
        abi_x86_64_object(abi_structs_x86_64 abi/structs.c)
        abi_x86_64_object(abi_wchar_x86_64 abi/wchar.c)
        abi_x86_64_object(abi_simd_x86_64 abi/simd.c)
        abi_wchar_expected(abi_wchar_x86_64_expected _X86_64
            ${emulated_c_flags})
        set(abi_structs_x86_64_options "")
        set(abi_simd_x86_64_options "")
        set(abi_wchar_x86_64_options EXPECTED "${abi_wchar_x86_64_expected}")
        foreach(case structs wchar simd)
            antic_program_test(program_abi_${case}_${emulated}
                "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_${case}.anti"
                OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_${case}_x86_64>,,>"
                OPTIONS "${ANTIC_X86_64_OPTIONS}"
                ${abi_${case}_x86_64_options}
                ${emulator}
                WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64")
        endforeach()
        if(EXISTS "${ANTIC_RAYLIB_SOURCE}/src/raymath.h")
            abi_x86_64_object(abi_raymath_x86_64 abi/raymath.c)
            target_include_directories(abi_raymath_x86_64 PRIVATE
                "${ANTIC_RAYLIB_SOURCE}/src")
            antic_program_test(program_abi_raymath_${emulated}
                "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.anti"
                OPTIONS "${ANTIC_X86_64_OPTIONS}"
                OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_raymath_x86_64>,,>"
                EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected"
                ${emulator}
                WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-x86_64")
        endif()
    endif()
    # DESIGN: a program that holds anti.regex links PCRE2, which stands at
    # the default level x86-64-v3 alone, and a program below it is refused
    # at link. Rosetta runs no v3 program, so on macos-arm64 the programs
    # of patterns run on the host target alone. errors/pattern_level.anti
    # holds the refusal.
    set(emulated_sources ${program_sources})
    if(emulated STREQUAL "macos-x86_64")
        list(FILTER emulated_sources EXCLUDE REGEX "/(pattern_[a-z]+|regex_compile|eq_handles)\\.anti$")
    endif()
    foreach(source IN LISTS emulated_sources)
        get_filename_component(name "${source}" NAME_WE)
        antic_program_test(program_${name}_${emulated}
            "${source}"
            OPTIONS "${ANTIC_X86_64_OPTIONS}"
            ${emulator}
            WORK "${CMAKE_CURRENT_BINARY_DIR}/programs-x86_64")
    endforeach()
    set_tests_properties(program_args_${emulated} PROPERTIES
        ENVIRONMENT "ANTI_TEST_GREETING=héllo")
    set_tests_properties(program_nullable_${emulated} PROPERTIES
        ENVIRONMENT "ANTI_TEST_NULLABLE=set from C")
    foreach(name IN ITEMS sync_exits sync_workers channels select_two
            channel_workers parallel_join synchronized concurrent sync_pairs
        lock_free_queue)
        set_tests_properties(program_${name}_${emulated} PROPERTIES
            TIMEOUT 120)
    endforeach()
    set_tests_properties(program_by_unsigned_${emulated} PROPERTIES
        TIMEOUT 60)
    # DESIGN: the runtime of AddressSanitizer for x86_64 runs under
    # Rosetta, so the Mac runs the programs of --memory-checks for
    # macos-x86_64 that link at v1. traps/memory_checks.anti stays out,
    # since its clean case compiles a pattern literal and anti.regex is
    # built for x86-64-v3 alone. qemu-x86_64 and the x64 emulation of
    # Windows stop that runtime, as the block after memory_checks says.
    if(emulated STREQUAL "macos-x86_64")
        add_test(NAME memory_checks_list_${emulated}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks_list.anti"
                "-DOPTIONS=${ANTIC_X86_64_OPTIONS}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/memory-checks-x86_64"
                "-DHOST=${ANTIC_HOST_TARGET}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_memory_checks_list.cmake")
        antic_program_test(std_builder_room_memory_checks_${emulated}
            "${CMAKE_CURRENT_SOURCE_DIR}/std/builder_room.anti"
            OPTIONS "${ANTIC_X86_64_OPTIONS},--memory-checks"
            WORK "${CMAKE_CURRENT_BINARY_DIR}/memory-checks-x86_64")
        set_tests_properties(memory_checks_list_${emulated}
            std_builder_room_memory_checks_${emulated} PROPERTIES TIMEOUT 120
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endif()
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
if(ANTIC_EMULATED_TARGET)
    set_tests_properties(program_alloc_owning_${ANTIC_EMULATED_TARGET}
        PROPERTIES ENVIRONMENT "MallocScribble=1")
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
# NAME.win32.expected for the Windows targets, and one whose output names
# the C libraries of musl brings NAME.musl.expected for the Linux hosts,
# whose programs link musl. One whose output follows
# the inlining of release mode brings NAME.lto.expected for the suite
# behind ANTIC_TEST_LTO, where the LTO of lld inlines across the runtime
# and the program as well. A trace then names the caller, as the entry on
# the inlining of release mode in docs/decisions.md says.
foreach(source IN LISTS std_sources)
    get_filename_component(name "${source}" NAME_WE)
    set(expected "")
    set(win32_expected "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.win32.expected")
    set(lto_expected "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.lto.expected")
    set(musl_expected "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.musl.expected")
    if(ANTIC_HOST_TARGET MATCHES "^windows-" AND EXISTS "${win32_expected}")
        set(expected EXPECTED "${win32_expected}")
    endif()
    if(ANTIC_HOST_TARGET MATCHES "^linux-" AND EXISTS "${musl_expected}")
        set(expected EXPECTED "${musl_expected}")
    endif()
    if(ANTIC_TEST_LTO AND EXISTS "${lto_expected}")
        set(expected EXPECTED "${lto_expected}")
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

# The copies of the failures of anti.collection in dev mode and under
# --memory-checks, which reports a read of the bytes of a message after the
# error that made them is gone.
antic_program_test(std_error_copies_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/std/error_copies.anti"
    OPTIONS "--dev"
    DEV ${collection_dev} collection/pool
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
antic_program_test(std_error_copies_memory_checks
    "${CMAKE_CURRENT_SOURCE_DIR}/std/error_copies.anti"
    OPTIONS --memory-checks
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-memory-checks")
# A copy of a file sink of anti.log under --memory-checks, which reports a
# write to the file after the original closed it.
antic_program_test(std_file_sink_dup_memory_checks
    "${CMAKE_CURRENT_SOURCE_DIR}/std/file_sink_dup.anti"
    OPTIONS --memory-checks
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-memory-checks")
if(ANTIC_HOST_TARGET STREQUAL "windows-arm64")
    set_tests_properties(std_error_copies_memory_checks
        std_file_sink_dup_memory_checks PROPERTIES DISABLED TRUE)
endif()

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
foreach(name IN ITEMS map map_tuple hash_map set hash_set map_growth)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/map collection/set
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The collections threads share, of anti.collection.concurrent, in dev
# mode, and the copy of the walk they share with the synchronized ones. The
# module stands on the maps, so the link names their object.
foreach(name IN ITEMS concurrent_map concurrent_map_threads
        concurrent_map_equal spsc_ring spsc_ring_threads snapshot_dup)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/map collection/concurrent
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# The collections behind one lock each, of anti.collection.synchronized, in
# dev mode. The module stands on the list, the maps, the sets and the pool,
# so the link names the object of each.
foreach(name IN ITEMS sync_list sync_list_threads sync_map sync_map_threads
        sync_set sync_set_threads sync_pool sync_pool_threads sync_equal)
    antic_program_test(std_${name}_dev
        "${CMAKE_CURRENT_SOURCE_DIR}/std/${name}.anti"
        OPTIONS "--dev"
        DEV ${collection_dev} collection/list collection/map collection/set
            collection/pool collection/synchronized
        WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
endforeach()
# A change that rewrites the key of an entry, in both kinds of thread-safe
# map, in dev mode.
antic_program_test(std_map_key_rewrite_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/std/map_key_rewrite.anti"
    OPTIONS "--dev"
    DEV ${collection_dev} collection/list collection/map collection/set
        collection/pool collection/concurrent collection/synchronized
    WORK "${CMAKE_CURRENT_BINARY_DIR}/std-dev")
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

# M330. Every `alloc(T, n)` of src/std/ stops the program when the memory
# runs out, and no module fails for it.
add_test(NAME std_out_of_memory
    COMMAND "${CMAKE_COMMAND}"
        "-DROOT=${ANTIC_STD_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_std_out_of_memory.cmake")

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
set_target_properties(abi_structs PROPERTIES MSVC_RUNTIME_LIBRARY "")
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
set_target_properties(abi_simd PROPERTIES MSVC_RUNTIME_LIBRARY "")
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

# M302. The message of a SystemError keeps its text after the next error
# of its thread. tests/abi/system_error.c sets the codes each error reads.
add_library(abi_system_error OBJECT abi/system_error.c)
set_target_properties(abi_system_error PROPERTIES MSVC_RUNTIME_LIBRARY "")
target_compile_options(abi_system_error PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_system_error PRIVATE -mmacosx-version-min=11.0)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_system_error PRIVATE -fno-sanitize=all)
endif()
antic_program_test(program_abi_system_error
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_system_error.anti"
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_system_error>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")

add_library(abi_wchar OBJECT abi/wchar.c)
set_target_properties(abi_wchar PROPERTIES MSVC_RUNTIME_LIBRARY "")
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

# Every C type against C, in both directions: the functions of
# tests/abi/ctypes.c take and return each of `c_char` to `c_wchar`,
# `c_float` and `c_double`, and call the functions the program exports. The
# program runs as a release build, as a dev build and under
# --memory-checks, with one expected output.
add_library(abi_ctypes OBJECT abi/ctypes.c)
set_target_properties(abi_ctypes PROPERTIES MSVC_RUNTIME_LIBRARY "")
target_compile_options(abi_ctypes PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
if(APPLE)
    target_compile_options(abi_ctypes PRIVATE -mmacosx-version-min=11.0)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(abi_ctypes PRIVATE -fno-sanitize=all)
endif()
antic_program_test(program_abi_ctypes
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_ctypes.anti"
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_ctypes>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi")
antic_program_test(program_abi_ctypes_dev
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_ctypes.anti"
    OPTIONS --dev
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_ctypes>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-dev")
antic_program_test(program_abi_ctypes_memory_checks
    "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_ctypes.anti"
    OPTIONS --memory-checks
    OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_ctypes>,,>"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/abi-memory-checks")
if(ANTIC_HOST_TARGET STREQUAL "windows-arm64")
    set_tests_properties(program_abi_ctypes_memory_checks PROPERTIES
        DISABLED TRUE)
endif()

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
# The Windows sysroot of mingw-w64: the headers, the import libraries
# that llvm-dlltool writes from the .def files for the processor of each
# target, the builtins of the pinned clang and the licence, laid out from
# a stand-in source release by the pinned clang and the pinned llvm-ar.
if(NOT ANTIC_SYSTEM_COMPILER)
    add_test(NAME windows_sysroot
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/windows_sysroot"
                "-DCLANG_DIR=${ANTIC_CLANG_DIR}"
                "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_windows_sysroot.cmake")
endif()

# A configure with -DANTI_DEPS_DIR reads the pinned downloads of another
# directory, and a copy of the sources builds against this tree's.
add_test(NAME deps_dir
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/deps_dir"
            "-DDEPS=${ANTIC_DEPS_DIR}" "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DCLANG=${ANTIC_CLANG_DIR}" "-DLLVM=${ANTIC_LLVM_DIR}"
            "-DSYSROOT=${ANTIC_SYSROOT_DIR}" "-DRAYLIB=${ANTIC_RAYLIB_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_deps_dir.cmake")

# The configure of a fresh checkout installs the sysroots and raylib from
# their archives, as decision 3 of "Decisions of 2026-10-11" asks.
if(NOT ANTIC_SYSTEM_COMPILER)
    add_test(NAME first_build
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DTREE=${CMAKE_BINARY_DIR}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/first_build"
                "-DGENERATOR=${CMAKE_GENERATOR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_first_build.cmake")
endif()

# A configure leaves the names of lld alone while the suite runs them.
add_test(NAME llvm_names
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DLLVM=${ANTIC_LLVM_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-names"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_llvm_names.cmake")

# llvm-cov stays out of every directory the packer copies into a package.
add_test(NAME no_llvm_cov_packed
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/no-llvm-cov-packed"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_no_llvm_cov_packed.cmake")

# The report of the coverage measurement: tools/coverage-report.cmake
# merges the profiles of two runs of a small program and writes the
# summary and the annotated sources of the three directories it measures.
# The program takes the flags of tools/coverage.cmake, as a coverage tree
# gives them to antic.
# DESIGN: the test stands on a Mac alone. The measurement runs on the
# development Mac, and the script that links a program of the host here,
# as release_dry_run does, names the Apple SDK.
if(APPLE)
    set(coverage_work "${CMAKE_CURRENT_BINARY_DIR}/coverage-report")
    antic_coverage_compile_flags(coverage_flags "${coverage_work}/root")
    add_test(NAME coverage_report
        COMMAND "${CMAKE_COMMAND}"
                "-DCC=${CMAKE_C_COMPILER}"
                "-DSDK=${ANTIC_MACOS_SDK}"
                "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
                "-DCOMPILE_FLAGS=${coverage_flags}"
                "-DLINK_FLAGS=${ANTIC_COVERAGE_LINK_FLAGS}"
                "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                "-DSCRIPT=${CMAKE_SOURCE_DIR}/tools/coverage-report.cmake"
                "-DWORK=${coverage_work}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_coverage_report.cmake")
endif()

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

# The constructs of the flow area in a `tests` block and in a `fixtures`
# block, which no program of tests/matrix reaches: every build but
# `anti test` drops both blocks. docs/notes/test-matrix.md names the cells.
add_test(NAME anti_test_flow
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/flow"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-flow"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

# The constructs of the values area in a `tests` block and in a `fixtures`
# block, as anti_test_flow holds those of the flow area.
add_test(NAME anti_test_values
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/values"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-values"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

# The constructs of the ownership area in a `tests` block and in a
# `fixtures` block, as anti_test_flow holds those of the flow area.
add_test(NAME anti_test_ownership
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/ownership"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-ownership"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

# The constructs of the errors area in a `tests` block and in a `fixtures`
# block, as anti_test_flow holds those of the flow area. A test takes no
# `may fail`, which error_tests_may_fail holds.
add_test(NAME anti_test_errors
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/errors"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-errors"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

# The constructs of the threads area in a `tests` block and in a `fixtures`
# block, as anti_test_flow holds those of the flow area. The workers stand
# at module level.
add_test(NAME anti_test_threads
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/threads"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-threads"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

# The constructs of the classes area in a `tests` block and in a
# `fixtures` block, as anti_test_flow holds those of the flow area. The
# classes stand at module level, since a block holds functions alone.
add_test(NAME anti_test_classes
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DMODULES=${CMAKE_CURRENT_SOURCE_DIR}/anti-test"
        "-DMODULE=com/example/classes"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-test-classes"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_test_module.cmake")

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

# A Windows program of `anti build --memory-checks` has the DLL of
# AddressSanitizer beside it in dist/.
add_test(NAME anti_memory_dll
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-memory-dll"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_memory_dll.cmake")
set_tests_properties(anti_memory_dll PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")

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

# The link of the symbols archive lays an ELF program out as the release
# link does. Under LTO the debug CFI of an Anti function moved into
# .eh_frame when the runtime brought unwind tables, and every address
# after it shifted.
foreach(target linux-x86_64 linux-arm64)
    add_test(NAME symbols_layout_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-symbols"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/symbols-layout-${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_symbols_layout.cmake")
    set_tests_properties(symbols_layout_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

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

# A program of musl names musl and mimalloc in its notice, and a program
# of glibc or of macOS names neither. `anti license --from` prints the
# notice and NOTICE.txt of `anti build` holds the same.
add_test(NAME license_notice
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DMUSL_VERSION=${ANTIC_MUSL_VERSION}"
        "-DMIMALLOC_VERSION=${ANTIC_MIMALLOC_VERSION}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/license-notice"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_license_notice.cmake")
set_tests_properties(license_notice PROPERTIES TIMEOUT 120
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# The four forms of `anti license` beside --from, one test each: the
# plain form over licenses/ of the runtime archive, --project over a
# project that links two packages, --project --notice and the NOTICE.txt
# of `anti build`, and --from-archive over a static archive of `--lib
# static`.
foreach(form plain project notice archive)
    set(name "license_${form}")
    if(form STREQUAL "notice")
        set(name license_project_notice)
    elseif(form STREQUAL "archive")
        set(name license_from_archive)
    endif()
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DFORM=${form}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DVERSION=${PROJECT_VERSION}"
            "-DMUSL_VERSION=${ANTIC_MUSL_VERSION}"
            "-DMIMALLOC_VERSION=${ANTIC_MIMALLOC_VERSION}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/license-form-${form}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_license_forms.cmake")
    set_tests_properties(${name} PROPERTIES TIMEOUT 180
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

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
# What `anti build` delivers and caches, one test per case of
# tests/run_anti_build_cases.cmake, whose header names what each shows. A
# Windows library links from every host whose runtime archive holds the
# sysroot. The curl of the redirect cases is a shell script.
set(anti_build_cases path_target path_target_cpu lock_dropped
    cache_strip_docs cache_version cache_license cache_license_text
    cache_attribution cache_dependency cache_import_checked
    cache_import_layout)
if(NOT ANTIC_HOST_OS STREQUAL "windows")
    list(APPEND anti_build_cases redirect_to_http redirect_followed
         redirect_loop)
endif()
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/windows-x86_64")
    list(APPEND anti_build_cases windows_import_library)
endif()
foreach(case IN LISTS anti_build_cases)
    add_test(NAME anti_build_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            -DCASE=${case}
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DOTHER=${anti_build_other}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-cases/${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_cases.cmake")
endforeach()

# The messages of anti, one test per message. A table of the directory of
# its command holds the cases of each, and anti_messages.cmake those of
# `anti new`, `anti license` and `anti sdk`, which have no directory.
# antic_messages.cmake holds the messages of the driver of antic that
# stand at no position of a source, as `antic_message_<case>`.
# run_anti_messages.cmake states the form of a case and prints the names
# of a table, so a case is registered by standing in its table.
foreach(pair "anti_build|anti-build/messages.cmake"
             "anti_test|anti-test/messages.cmake"
             "anti_symbols|anti-symbols/messages.cmake"
             "anti_check|check/messages.cmake"
             "anti_fmt|fmt/messages.cmake"
             "anti_doc|doc/messages.cmake"
             "anti_bind|bind/messages.cmake"
             "anti|anti_messages.cmake"
             "antic|antic_messages.cmake")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 command)
    list(GET pair 1 table)
    # The table of antic runs antic, and every other one anti.
    set(tool anti)
    if(command STREQUAL "antic")
        set(tool antic)
    endif()
    set(table "${CMAKE_CURRENT_SOURCE_DIR}/${table}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${table}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_messages.cmake")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DLIST=ON "-DTABLE=${table}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_messages.cmake"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE names
        ERROR_VARIABLE errors)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${table} gives no names:\n${errors}")
    endif()
    string(STRIP "${names}" names)
    string(REPLACE "\n" ";" names "${names}")
    foreach(name IN LISTS names)
        # DESIGN: antic keeps the text of --license-text when it refuses
        # the file for a NUL byte, and LeakSanitizer reports the block. A
        # line of tests/known-failures.txt names a test that fails in
        # every tree, and this one fails under AddressSanitizer alone. The
        # case therefore runs in a tree without it, and the report
        # docs/reports/2026-10-13-tests-coverage-rest.md holds the defect.
        string(FIND "${CMAKE_C_FLAGS}" "-fsanitize=address" with_asan)
        if(command STREQUAL "antic" AND
           name STREQUAL "license_text_with_a_nul_byte" AND
           NOT with_asan EQUAL -1)
            continue()
        endif()
        add_test(NAME ${command}_message_${name}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTI=$<TARGET_FILE:${tool}>"
                "-DTABLE=${table}"
                "-DCASE=${name}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/messages/${command}-${name}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DHOST=${ANTIC_HOST_TARGET}"
                "-DEXE=${CMAKE_EXECUTABLE_SUFFIX}"
                "-DVERSION=${PROJECT_VERSION}"
                "-DCLANG_VERSION=${ANTI_CLANG_VERSION}"
                "-DCLANG_TAG=${ANTI_CLANG_TAG}"
                "-DCLANG_PAGE=${ANTI_CLANG_PAGE}"
                "-DCLANG_DIR=${ANTIC_CLANG_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_messages.cmake")
    endforeach()
endforeach()

# An option of anti beside the run without it, one test per case of
# tests/run_anti_options.cmake, whose header names what each shows.
# --soname writes the version of a library of macOS and of Linux and
# changes nothing of one of Windows. A Mac alone links for macOS, and the
# other two link from every host whose runtime archive holds the sysroot.
set(anti_option_cases strip_docs check_root_library check_root_source
    bind_define_name bind_define_value bind_define_shim release_path
    library_root)
if(ANTIC_HOST_OS STREQUAL "macos")
    list(APPEND anti_option_cases soname_macos)
endif()
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/linux-x86_64")
    list(APPEND anti_option_cases soname_linux soname_linux_dist)
endif()
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/windows-x86_64")
    list(APPEND anti_option_cases soname_windows)
endif()
# --target all links a program for each of the six targets.
set(anti_option_six ON)
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    if(NOT EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/${target}")
        set(anti_option_six OFF)
    endif()
endforeach()
if(anti_option_six)
    list(APPEND anti_option_cases target_all target_all_cpu targets_default
         symbols_target bind_target)
endif()
# The inventory finds a program by the notice of its file, which
# anti_symbols reads on the hosts other than Windows.
if(NOT ANTIC_HOST_OS STREQUAL "windows")
    list(APPEND anti_option_cases symbols_from)
endif()
foreach(case IN LISTS anti_option_cases)
    add_test(NAME anti_option_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DANTIC=$<TARGET_FILE:antic>"
            -DCASE=${case}
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
            "-DCLANG_DIR=${ANTIC_CLANG_DIR}"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-options/${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_options.cmake")
endforeach()

# A program run from dist/ and built again at once, fifteen times. Windows
# maps a program that just ran for a moment, and the copy into dist/
# failed there. A turn takes about two seconds on the Windows VM.
if(ANTIC_HOST_TARGET MATCHES "^windows-")
    add_test(NAME anti_build_rerun
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-rerun"
            "-DTURNS=15"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_rerun.cmake")
endif()

# A program that runs from dist/ while `anti build` builds it again. The
# build replaces it, the running program ends as it began, and the new
# program runs. Linux refuses a write into a running program.
add_test(NAME anti_build_running
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/anti-build"
        "-DHOST=${ANTIC_HOST_TARGET}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/anti-build-running"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_build_running.cmake")

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

# antic runs its passes on a stack of its own size, whatever stack the
# main thread has. The limit of the main thread is a POSIX one too.
if(NOT WIN32)
    add_test(NAME antic_stack
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DOPT=${ANTIC_OPT}"
            "-DLLC=${ANTIC_LLC}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/antic-stack"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_antic_stack.cmake")
endif()

# A variant of 65536 cases takes a tag of 32 bits. The runner writes the
# source and runs the program.
add_test(NAME variant_wide_tag
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/variant-wide-tag"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_variant_wide_tag.cmake")

# A `type` alias of a function type leaves the functions after it as they
# stand. The source is in the canonical form, so fmt_canonical passes over
# it until `anti fmt` agrees: finding T025 of docs/audit/tests-findings.md.
add_expect_test(fmt_alias_of_function STATUS 0
    PATTERN "^$"
    COMMAND "$<TARGET_FILE:anti>" fmt --check
            "${CMAKE_CURRENT_SOURCE_DIR}/fmt/alias_of_function.anti")

# Every source of the standard library and of the tests stands in the
# canonical form, so that `anti fmt` over the checkout writes nothing.
add_test(NAME fmt_canonical
    COMMAND "${CMAKE_COMMAND}"
        "-DANTI=$<TARGET_FILE:anti>"
        "-DSTD=${CMAKE_SOURCE_DIR}/src/std"
        "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_fmt_canonical.cmake")
# run_fmt_canonical.cmake fails for an anti that ends by a signal and
# prints nothing.
add_test(NAME fmt_canonical_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_fmt_canonical.cmake"
        "-DSTD=${CMAKE_SOURCE_DIR}/src/std"
        "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/fmt-canonical-runner"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_fmt_canonical_self.cmake")
set_tests_properties(fmt_canonical_runner PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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

# The page of a module that holds one item per form the fixture above
# lacks: the constants of each kind, the modifiers of a struct and of a
# class, the fields of a class and the signatures of a function. Its doc
# text holds the names anti doc links and the ones it leaves as text. The
# user docs are HTML, and --private shows the fields that are not `pub`.
foreach(pair "html||com.example.forms.html|forms.expected.html"
             "private|--markdown\\|--private|com.example.forms.md|forms.private.expected.md")
    string(REPLACE "\\|" "!" pair "${pair}")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 case)
    list(GET pair 1 options)
    list(GET pair 2 page)
    list(GET pair 3 expected)
    string(REPLACE "!" "|" options "${options}")
    add_test(NAME anti_doc_forms_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/doc/src"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/doc/src/com/example/forms.anti|${CMAKE_CURRENT_SOURCE_DIR}/doc/src/com/example/limits.anti"
            "-DPAGE=${page}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/doc/${expected}"
            "-DOPTIONS=${options}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/doc-forms-${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_doc_page.cmake")
endforeach()

# The pages of two modules of doc text. prose holds the forms of the doc
# subset that no other page holds: an item of a list in two lines, an
# empty code span, a bracket that starts no link, a link whose scheme
# stands in capitals, a link to a page beside this one and a code block
# that never closes. Its developer's page in Markdown holds the note of the
# module and the note of an item. wrapped holds a link that anti fmt
# breaks after a word of its text.
foreach(pair "prose_html||prose|com.example.prose.html|prose.expected.html"
             "prose_dev|--markdown\\|--dev|prose|com.example.prose.md|prose.dev.expected.md"
             "wrapped_link||wrapped|com.example.wrapped.html|wrapped.expected.html")
    string(REPLACE "\\|" "!" pair "${pair}")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 case)
    list(GET pair 1 options)
    list(GET pair 2 module)
    list(GET pair 3 page)
    list(GET pair 4 expected)
    string(REPLACE "!" "|" options "${options}")
    # The code block of prose never closes, which R179 of "Rules of
    # 2026-10-13" warns of.
    set(warning "")
    if(module STREQUAL "prose")
        set(warning "-DWARNING=the doc comment of `add` holds a fence that nothing closes")
    endif()
    add_test(NAME anti_doc_${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTI=$<TARGET_FILE:anti>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}/doc/src"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/doc/src/com/example/${module}.anti"
            ${warning}
            "-DPAGE=${page}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/doc/${expected}"
            "-DOPTIONS=${options}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/doc-${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_anti_doc_page.cmake")
endforeach()

# anti bind --header writes the header that antic --lib writes for the
# same module, from the library file alone. The other cases bind
# tests/bind/layout.h and raymath through clang and a part of
# raylib_api.json, and check the refusals. forms binds tests/bind/forms.h
# and values the macros of tests/bind/values.h. functions binds the
# functions of tests/bind/functions.h, and huge_enum and nested_enum each
# bind a header of one enum. api_parts, api_callbacks, api_literals,
# api_types and api_words each bind a description of tests/bind in the
# format of rlparser. words binds the names of tests/bind/words.h that are
# words of Anti, unsigned_bitfield the bitfield of tests/bind/bits.h and
# fixed_enum the enums of tests/bind/fixed.h.
# api_typedef_chain and clang_typedef_chain each bind a chain of aliases
# that names every link three times. clang_of_the_tree runs a copy of anti in a tree whose deps/clang is the
# pinned clang, with no clang on PATH.
set(anti_bind_cc ${ANTIC_TEST_CC})
if(APPLE)
    list(APPEND anti_bind_cc -mmacosx-version-min=11.0)
endif()
set(anti_bind_cases header clang escaped_path raymath raylib api
    api_malformed clang_malformed forms values functions huge_enum
    nested_enum api_parts api_callbacks api_literals api_types
    api_words words unsigned_bitfield fixed_enum api_typedef_chain
    clang_of_the_tree)
if(NOT WIN32)
    # The cases refusals, ast_malformed and clang_typedef_chain stand a
    # shell script in for clang.
    list(APPEND anti_bind_cases refusals ast_malformed clang_typedef_chain)
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
            "-DRAYLIB=${ANTIC_RAYLIB_SOURCE}"
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
# the sysroots of every target, which every package carries.
set(package_missing "")
foreach(dir linux-x86_64 linux-arm64 macos-arm64 macos-x86_64
        windows-x86_64 windows-arm64 licenses)
    if(NOT EXISTS "${ANTIC_SYSROOT_DIR}/${dir}")
        list(APPEND package_missing "${dir}")
    endif()
endforeach()
if(package_missing)
    # Fail rather than skip, so a missing sysroot cannot pass unnoticed.
    list(JOIN package_missing " " package_missing)
    add_test(NAME package_keys
        COMMAND "${CMAKE_COMMAND}" -E echo
            "sysroots not found in ${ANTIC_SYSROOT_DIR}: ${package_missing}: run tools/get-sysroot.cmake")
    set_tests_properties(package_keys PROPERTIES
        FAIL_REGULAR_EXPRESSION "sysroots not found")
else()
    add_test(NAME package_keys
        COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DANTIC=$<TARGET_FILE:antic>" "-DANTI=$<TARGET_FILE:anti>"
                "-DHOST=${ANTIC_HOST_TARGET}"
                "-DSYSROOT=${ANTIC_SYSROOT_DIR}" "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DCLANG=${CMAKE_C_COMPILER}"
                "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                "-DRAYLIB=${ANTIC_RAYLIB_SOURCE}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/package"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package.cmake")
    # PERF DECISION: the package carries the LLVM tools, and xz compresses
    # its 530 MB of them in about three minutes. ctest runs the tests of
    # a fresh tree in the order of their declaration, which put this one
    # last and alone at the end of the suite. The cost puts it first, so
    # the rest of the suite runs beside it.
    set_tests_properties(package_keys PROPERTIES COST 300)
endif()
# The release script names the directory of the raylib download, never the
# version of the pin.
add_test(NAME raylib_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" -DNAME=raylib
            -DSCRIPT=src/native/get-source.cmake
            -DCALLER=src/native/raylib-source.cmake
            -DFILES=src/native/raylib.cmake,tools/release.sh
            -DHASH=SHA256 -DPARTS=2
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin.cmake")
# The check of the pins refuses a copy of a version or a digest and passes
# a text one dot short of the version.
add_test(NAME native_pin_self
    COMMAND "${CMAKE_COMMAND}"
            "-DPIN=${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin.cmake"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/native-pin-self"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_native_pin_self.cmake")
# antic takes its tools, its libraries and its sysroots from the runtime
# archive alone, and refuses what the archive lacks by its path rather
# than taking it from the search path or from LIB.
add_test(NAME own_tools
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DANTIC=$<TARGET_FILE:antic>" "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOPT=${ANTIC_OPT}" "-DLLC=${ANTIC_LLC}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/own_tools"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_own_tools.cmake")
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

# The installers download the package alone: no CMake, no sysroot step
# and no script of the package, and the check of an install is the
# --version of its two programs.
add_test(NAME installer_alone
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/installer-alone"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installer_alone.cmake")
set_tests_properties(installer_alone PROPERTIES
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
# run_release_dry_run.cmake fails with the output of a fixture that does
# not compile or link, where it reported the whole test skipped.
add_test(NAME release_dry_run_runner
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_release_dry_run.cmake"
            "-DCC=${CMAKE_C_COMPILER}"
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
            "-DSDK=${ANTIC_MACOS_SDK}"
            "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/release-runner"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_release_dry_run_self.cmake")
set_tests_properties(release_dry_run_runner PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# A small raylib binding: the raymath functions of the pinned raylib
# release, which src/native/raylib-source.cmake fetches.
if(EXISTS "${ANTIC_RAYLIB_SOURCE}/src/raymath.h")
    add_library(abi_raymath OBJECT abi/raymath.c)
    set_target_properties(abi_raymath PROPERTIES MSVC_RUNTIME_LIBRARY "")
    target_compile_options(abi_raymath PRIVATE ${ANTIC_REFERENCE_C_FLAGS})
    target_include_directories(abi_raymath PRIVATE "${ANTIC_RAYLIB_SOURCE}/src")
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
            "raylib not found in ${ANTIC_RAYLIB_SOURCE}")
    set_tests_properties(program_abi_raymath PROPERTIES
        FAIL_REGULAR_EXPRESSION "raylib not found")
endif()

# The programs of tests/abi under the LLVM back end, in release mode as the
# program_abi_* tests run, for macos-arm64 and for macos-x86_64 under
# Rosetta. Each links the objects of C of its native test and prints what
# that test prints, so they check abi_classify of src/antic/abi.c against
# clang. abi_simd holds the simd operations its structs need.
foreach(target IN LISTS llvm_targets)
    set(suffix "")
    set(wchar_expected "${abi_wchar_expected}")
    if(NOT target STREQUAL ANTIC_HOST_TARGET)
        set(suffix _x86_64)
        set(wchar_expected "${abi_wchar_x86_64_expected}")
    endif()
    foreach(case structs simd wchar raymath)
        if(NOT TARGET abi_${case}${suffix})
            continue()
        endif()
        set(expected "")
        if(case STREQUAL "wchar")
            set(expected EXPECTED "${wchar_expected}")
        elseif(case STREQUAL "raymath")
            set(expected EXPECTED
                "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_raymath.expected")
        endif()
        antic_llvm_program_test(abi_${case} ${target} release
            SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/abi/abi_${case}.anti"
            OBJECTS "$<JOIN:$<TARGET_OBJECTS:abi_${case}${suffix}>,,>"
            ${expected})
    endforeach()
endforeach()

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
# A call the checker writes against a library of the language that lacks
# what the call names, as an older library file would. A test without a
# tree reads the library of regex_damaged, and every other one the library
# of its own tree under tests/modules.
#   <program>|<tree>|<module>
foreach(case
        "lang_error_maker|lang_code_alone|lang"
        "lang_error_origin|lang_no_origin|lang"
        "lang_trace_capture|lang_no_capture|lang"
        "regex_find_all_damaged||"
        "regex_group_older||"
        "regex_older||"
        "regex_patch_bytes_older||"
        "regex_patch_older||"
        "regex_split_damaged||"
        "text_builder_older|text_older|text"
        "text_equal_older|text_older|text"
        "text_to_bytes_older|text_older|text")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 tree)
    list(GET parts 2 module)
    set(root "${CMAKE_CURRENT_BINARY_DIR}/damaged")
    set(fixture regex_damaged)
    if(tree)
        set(root "${CMAKE_CURRENT_BINARY_DIR}/older/${tree}")
        set(fixture ${tree}_antl)
        if(NOT TEST antl_${tree})
            file(MAKE_DIRECTORY "${root}/anti")
            add_test(NAME antl_${tree}
                COMMAND antic -c --anti-internal
                        -I "${CMAKE_CURRENT_SOURCE_DIR}/modules/${tree}"
                        -o "${root}/anti/${module}.antl"
                        "${CMAKE_CURRENT_SOURCE_DIR}/modules/${tree}/anti/${module}.anti")
            set_tests_properties(antl_${tree} PROPERTIES
                FIXTURES_SETUP ${fixture})
        endif()
    endif()
    add_expect_test(error_${name} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/${name}.err"
        COMMAND "$<TARGET_FILE:antic>" --front-end
                --runtime "${ANTIC_RUNTIME_DIR}" -I "${root}" ${name}.anti)
    set_tests_properties(error_${name} PROPERTIES
        FIXTURES_REQUIRED ${fixture})
endforeach()
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
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings -S
            --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/note.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
add_test(NAME silent_note
    COMMAND antic -S --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/note_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/note.anti")
set_tests_properties(silent_note PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

add_expect_test(warning_dropped STATUS 0
    PATTERN "dropped.anti:3:2: warning: the `/\\*\\*` comment is dropped, because no item or field follows it"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings -S
            --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/dropped.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
# A `//#!` note that stands in a function is dropped like a doc comment.
add_expect_test(warning_dropped_module_note STATUS 0
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped_module_note.err"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings --front-end
            dropped_module_note.anti)
add_test(NAME silent_dropped
    COMMAND antic -S --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/dropped_silent.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/dropped.anti")
set_tests_properties(silent_dropped PROPERTIES FAIL_REGULAR_EXPRESSION "warning")

# A doc comment names each type the compiler declares in `anti.lang`
# without a warning.
add_test(NAME silent_doc_declared
    COMMAND antic --doc-warnings -S
            --opt "${ANTIC_OPT}" --llc "${ANTIC_LLC}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/doc_declared.s"
            "${CMAKE_CURRENT_SOURCE_DIR}/errors/doc_declared.anti")
set_tests_properties(silent_doc_declared PROPERTIES
    FAIL_REGULAR_EXPRESSION "warning")

# A doc comment names an alias, a module path, a case and a field of a case
# without a warning, and a star or a `<` that opens no markup gets none.
add_expect_test(silent_doc_names STATUS 0
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "^$"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings --front-end
            --runtime "${ANTIC_RUNTIME_DIR}" doc_names.anti)
# The whole path of the one module a file imports. No other import stands
# before it whose first part answers for the name.
add_expect_test(silent_doc_whole_path STATUS 0
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "^$"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings --front-end
            --runtime "${ANTIC_RUNTIME_DIR}" doc_whole_path.anti)
add_expect_test(silent_doc_plain_marks STATUS 0
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "^$"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings --front-end
            doc_plain_marks.anti)
# Emphasis before each character that may follow it, and the two forms of
# an HTML tag that hold a `/`.
add_expect_test(warning_doc_emphasis_closers STATUS 0
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/doc_emphasis_closers.err"
    COMMAND "$<TARGET_FILE:antic>" --doc-warnings --front-end
            doc_emphasis_closers.anti)

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
        "-DDOCUMENTS=${PROJECT_SOURCE_DIR}/docs/anti-syntax-overview.md"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/overview"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_document_examples.cmake")

# Every page of the developer's guide, through the same runner and its
# marks: every `anti` block compiles, a program prints its `text output`
# block, a `refused` block fails with the error it names, and an `llvm`
# or `asm` excerpt stands in what antic writes for the block it names.
add_test(NAME guide_examples
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DDOCUMENTS=${PROJECT_SOURCE_DIR}/docs/site/developers-guide"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/guide"
        "-DLLVM_OPT=${ANTIC_OPT}"
        "-DLLVM_MC=${ANTIC_RUNTIME_MC}"
        "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_document_examples.cmake")

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
    # The step runtime-lto of docs/work-order-llvm-optimization.md: lld
    # links the bitcode of the program with the runtime as bitcode of
    # each mode for every target.
    foreach(mode full thin)
        add_test(NAME cross_link_lto_${mode}_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cross-lto-${mode}"
                "-DTARGET=${target}"
                "-DFORMAT=${format}"
                "-DOPTIONS=--lto,${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cross_link.cmake")
        set_tests_properties(cross_link_lto_${mode}_${target} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endforeach()

# Every link drops the code a program never calls. A hello-world program
# calls anti_rt_write of src/rt/io.c and never anti_rt_exit of the same
# file, so the second must be gone from the executable of every target.
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    add_test(NAME dead_code_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/link-identity/hello.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/dead-code"
            "-DTARGET=${target}"
            "-DKEPT=anti_rt_write"
            "-DDROPPED=anti_rt_exit"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dead_code.cmake")
    set_tests_properties(dead_code_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# Two functions and two descriptors whose contents could be folded keep
# addresses of their own in a release program of every target, in the
# default LTO link and in the object link of --lto none.
set(distinct_source "${CMAKE_CURRENT_SOURCE_DIR}/link-identity/distinct.anti")
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    foreach(lto full none)
        if(lto STREQUAL "full")
            set(name distinct_addresses_${target})
        else()
            set(name distinct_addresses_lto_${lto}_${target})
        endif()
        add_test(NAME ${name}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${distinct_source}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/distinct-addresses/${lto}"
                "-DTARGET=${target}"
                "-DLTO=${lto}"
                "-DROOT=${CMAKE_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_distinct_addresses.cmake")
        set_tests_properties(${name} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endforeach()

# Every object of a program and of its runtime carries the
# address-significance table that the safe folding of lld reads.
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    add_test(NAME address_tables_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DSOURCE=${distinct_source}"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/address-tables/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_address_tables.cmake")
    set_tests_properties(address_tables_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# A windows-x86_64 program holds no 32-bit absolute address, and runs on a
# Windows host.
add_test(NAME windows_addresses
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/link-identity/hello.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/windows-addresses"
        "-DROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_windows_addresses.cmake")
set_tests_properties(windows_addresses PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

# A release build links the runtime as bitcode through full LTO unless it
# is told otherwise or cannot.
add_test(NAME lto_default
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
        "-DHOSTING=${CMAKE_CURRENT_SOURCE_DIR}/programs/inject_default.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/lto-default"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_lto_default.cmake")

# --lto takes one of its two modes, and links a program in release mode
# alone. A runtime archive without the runtime as bitcode of the mode is
# refused before anything compiles.
add_expect_test(lto_mode STATUS 2
    PATTERN "^antic: --lto takes full, thin or none, not fast"
    COMMAND $<TARGET_FILE:antic> --runtime "${ANTIC_RUNTIME_DIR}"
            --lto fast -o "${CMAKE_CURRENT_BINARY_DIR}/lto_mode"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
foreach(case "dev|--dev" "assembly|-S" "library|-c" "lib|--lib|static"
        "platform|--linker|platform")
    string(REPLACE "|" ";" parts "${case}")
    list(POP_FRONT parts what)
    add_expect_test(lto_refused_${what} STATUS 2
        PATTERN "^antic: --lto links a program in release mode, without --dev, -S, -c, --lib or --linker platform"
        COMMAND $<TARGET_FILE:antic> --runtime "${ANTIC_RUNTIME_DIR}"
                --lto full ${parts}
                -o "${CMAKE_CURRENT_BINARY_DIR}/lto_refused_${what}"
                "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")
endforeach()
# The .def file of a Windows program that hosts plugins lists the COFF
# symbols of its object, which --lto makes bitcode.
if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/windows-x86_64")
    add_expect_test(lto_refused_windows_host STATUS 1
        PATTERN "^antic: --lto links no Windows program that hosts plugins"
        COMMAND $<TARGET_FILE:antic> --runtime "${ANTIC_RUNTIME_DIR}"
                --target windows-x86_64 --lto full
                -o "${CMAKE_CURRENT_BINARY_DIR}/lto_refused_windows_host.exe"
                "${CMAKE_CURRENT_SOURCE_DIR}/programs/inject_default.anti")
endif()
# `anti build` and `anti run` pass --lto to the one antic call of release
# mode, so the option needs --release and a program.
add_expect_test(anti_lto_word STATUS 2
    PATTERN "^anti: --lto takes full, thin or none, not fast"
    COMMAND $<TARGET_FILE:anti> build --release --lto fast)
add_expect_test(anti_lto_dev STATUS 2
    PATTERN "^anti: --lto builds a program with --release, without --lib"
    COMMAND $<TARGET_FILE:anti> run --lto none)
add_expect_test(lto_no_bitcode STATUS 2
    PATTERN "^antic: --lto thin needs .*/lib/${ANTIC_HOST_TARGET}/[^/]+/bitcode/thin/[^/]+, the runtime as bitcode"
    COMMAND $<TARGET_FILE:antic> --runtime "${CMAKE_CURRENT_BINARY_DIR}"
            --lto thin -o "${CMAKE_CURRENT_BINARY_DIR}/lto_no_bitcode"
            "${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti")

# A release links its macOS binaries against the pinned Apple SDK, and an
# SDK that changed under that name is refused.
add_test(NAME macos_sdk
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DCLANG=${ANTIC_CLANG_DIR}/bin/clang"
            "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/macossdk"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_macos_sdk.cmake")
set_tests_properties(macos_sdk PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: ")
# run_macos_sdk.cmake skips for a missing SDK alone, and fails for one
# that changed under the pinned name.
add_test(NAME macos_sdk_runner
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}"
            "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_macos_sdk.cmake"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/macossdk-runner"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_macos_sdk_self.cmake")
set_tests_properties(macos_sdk_runner PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP: ")

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
# build and in one with -g, and holds no path of the build. Release mode
# inlines inner, outer and put, which then have no record of their own, as
# the entry on inlined frames in docs/decisions.md says. skipped keeps its
# frame for the skip it passes.
set(pdb_names "stack_trace.skipped;stack_trace.main")
foreach(target windows-x86_64 windows-arm64)
    add_test(NAME pdb_names_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/trace/stack_trace.anti"
            "-DNAMES=${pdb_names}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/pdb-names"
            "-DTARGET=${target}"
            "-DROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_pdb_names.cmake")
    set_tests_properties(pdb_names_${target} PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# antic writes the same LLVM text for every test program on every host.
add_test(NAME emit_identity
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DPROGRAMS=${CMAKE_CURRENT_SOURCE_DIR}/programs"
        "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/emit-identity/programs.sha256"
        "-DVERSION=${PROJECT_VERSION}"
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

# The groups of a float literal, a character literal and a reserved word.
antic_dump_test(tokens kinds
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/kinds.tokens"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/kinds.anti")

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

# R153 of "Rules of 2026-10-13" in docs/decisions.md: under --tests the
# tree holds the functions of the `tests` and the `fixtures` block.
antic_dump_test(ast test_blocks
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/test_blocks.ast"
    ARGS --tests "${CMAKE_CURRENT_SOURCE_DIR}/dump/test_blocks.anti")

# The nodes of the parsed tree that no other dump holds. The checker would
# refuse nodes.anti, which the parser takes.
antic_dump_test(ast nodes
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/nodes.ast"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/nodes.anti")

# The nodes the checker writes into the tree: an atomic read and write,
# `p?.x`, `to_slice`, the built-ins of a simd struct, `mul_high`, a call
# whose error the `catch` of its block takes and the bound names of the
# arms of a `switch` and of a `select`.
antic_dump_test(types checked_nodes
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/checked_nodes.types"
    ARGS -I "${ANTIC_RUNTIME_DIR}/std"
         "${CMAKE_CURRENT_SOURCE_DIR}/dump/checked_nodes.anti")

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

# The pattern of `--trace` names a package above the module of a class
# that asks for no call hooks. The library is built with `--trace com`,
# and the program without the option.
antic_modules_test(trace_package_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/trace_package"
    LIBRARIES plain
    PROGRAM client
    STD lang
    LIB_OPTIONS --trace com
    WORK "${CMAKE_CURRENT_BINARY_DIR}/trace-package-modules")

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

# The constructs of the flow area in the functions of a library module,
# plain and generic. The library file carries the checked tree of each
# generic one, and the client compiles its copy.
# docs/notes/test-matrix.md names the cells.
antic_modules_test(flow_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/flow"
    LIBRARIES flow
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/flow")

# The types of the area types in a library module: a packed struct, an
# aligned one, bitfields, a union, an enum over a base, two variants, two
# aliases, a generic packed struct and the marks `keep` and `concurrent`
# of a parameter. The client reads, writes and measures each across the
# module boundary. docs/notes/test-matrix.md names the cells.
antic_modules_test(types_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/types"
    LIBRARIES types
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/types")

# The constructs of the values area in the functions of a library module,
# plain and generic, as flow_modules holds those of the flow area. The
# library writes `f"..."` and a pattern literal, so its dev link takes the
# dev objects of anti.text and anti.regex.
antic_modules_test(values_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/values"
    LIBRARIES values
    PROGRAM client
    STD lang mem text regex
    WORK "${CMAKE_CURRENT_BINARY_DIR}/values")

# The constructs of the ownership area in the functions of a library
# module, plain and generic, as flow_modules holds those of the flow area.
# The client makes, copies and deletes an object of a class of the library
# as well, so the teardown and the copy of that class run across the
# module boundary.
antic_modules_test(ownership_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/ownership"
    LIBRARIES ownership
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/ownership")

# The constructs of the errors area in the functions of a library module,
# plain and generic, as flow_modules holds those of the flow area. The
# client handles the errors of the library and tests the class of one that
# the library declares.
antic_modules_test(errors_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/errors"
    LIBRARIES errors
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/errors-modules")

# The constructs of the threads area in the functions of a library module,
# plain and generic, as flow_modules holds those of the flow area. The
# client runs four workers of the library: two named through a direct
# import, and two named by their module after the arrow.
antic_modules_test(threads_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/threads"
    LIBRARIES threads
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/threads-modules")

# The constructs of the classes area in a library module: the classes with
# their members, and each construct in a function, plain and generic, as
# flow_modules holds those of the flow area. The client inherits a class
# of the library and implements an interface of it, and reaches the
# constants, the static fields, the operators and the generics from there.
antic_modules_test(classes_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/classes"
    LIBRARIES classes
    PROGRAM client
    STD lang mem reflect text
    WORK "${CMAKE_CURRENT_BINARY_DIR}/classes-modules")

# A public static field and a public constant of a class of a library
# module, reached from the client as `counter.Counter.total` and
# `counter.Counter.STEP`. T076 of docs/audit/tests-findings.md.
antic_modules_test(class_members_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/class_members"
    LIBRARIES counter
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/class-members-modules")

# A constant of a class read in a generic function of a library module,
# whose copy the client compiles. T029 of docs/audit/tests-findings.md,
# which generic_atomic_modules shows for a static field.
antic_modules_test(generic_class_constant_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_class_constant"
    LIBRARIES limits
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-class-constant-modules")

# The forms of a library file that no other library holds: a constant that
# is the negative and one that is the complement of a size, a struct that
# names its own copy, a singleton, a `provides` line and an `inject final`
# field.
antic_modules_test(library_forms_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/library_forms"
    LIBRARIES parts
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/library-forms-modules")

# `dispatch`, `join` and `join_all` in a generic function of a library
# module, whose copy the client compiles. T061 of
# docs/audit/tests-findings.md.
antic_modules_test(generic_dispatch_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_dispatch"
    LIBRARIES jobs
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-dispatch-modules")

# The origin of a `fail` and the position of a `here` in a generic function
# of a library module, whose copy the client compiles. T043 of
# docs/audit/tests-findings.md.
antic_modules_test(generic_origin_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_origin"
    LIBRARIES origin
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-origin")

# A generic function and a generic class of a library module that change
# a `static atomic` field. The client makes its copies from the library
# file. T029 of docs/audit/tests-findings.md.
antic_modules_test(generic_atomic_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_atomic"
    LIBRARIES tally
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic_atomic")

# A class of a library module built with arguments under the name of its
# module, `built.Built(4)` and `alloc built.Built(5)`. T030 of
# docs/audit/tests-findings.md.
antic_modules_test(qualified_arguments_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/qualified_arguments"
    LIBRARIES built
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/qualified_arguments")

# A Mutex in a struct of a library and channels that pass between the
# program and a library, which drains one and selects over two.
antic_modules_test(sync_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/sync"
    LIBRARIES queue
    PROGRAM client
    TIMEOUT 120
    WORK "${CMAKE_CURRENT_BINARY_DIR}/sync")

# The area modules of docs/notes/test-matrix.md: what crosses the boundary
# of a module. Each test below holds one side of that boundary, and the
# note names the test of each cell.

# Every kind of item a module makes public, reached by the name of the
# module, by an alias of the module and by a direct import.
antic_modules_test(items_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/items"
    LIBRARIES items
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/items-modules")

# Every kind of item at the level `internal`, reached from a module of the
# same package, and the protected members of a class of the library from a
# class below it.
antic_modules_test(levels_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/levels"
    LIBRARIES levels
    PROGRAM near
    OPTIONS --package-name com.example
    LIB_OPTIONS --package-name com.example
    WORK "${CMAKE_CURRENT_BINARY_DIR}/levels-modules")

# One function of a library per kind of value, which takes the kind and
# gives it back to another module.
antic_modules_test(kinds_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/kinds"
    LIBRARIES kinds
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/kinds-modules")

# The default of a bitfield of a class of a library, which a client writes
# into the unit the bitfields share.
antic_modules_test(bitfield_defaults_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/bitfield_defaults"
    LIBRARIES bits
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/bitfield-defaults-modules")

# A function of a library as the default of a field of its class.
antic_modules_test(function_default_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/function_default"
    LIBRARIES handlers
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/function-default-modules")

# The default of a field of a class of a library, one field per kind of
# value a default takes, read by a client that names no field.
antic_modules_test(field_defaults_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/field_defaults"
    LIBRARIES defaults
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/field-defaults-modules")

# The defaults of a class of a library over a length computed from
# `size_of`, by R043 of "Rules of 2026-10-13". The library file holds the
# value once, and the client fills the elements of its target.
antic_modules_test(default_lengths_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/default_lengths"
    LIBRARIES rooms
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/default-lengths-modules")

# A `construct` that fails tears down the base whose `construct` finished,
# by R114 and R025 of "Rules of 2026-10-13", where the base is a class of
# the library and where the failing class is one.
antic_modules_test(construct_fails_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/construct_fails"
    LIBRARIES parts
    STD lang
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/construct-fails-modules")

# The defaults of a generic class of a library that name its constant
# parameter, by R045 of "Rules of 2026-10-13". Each copy of the client
# takes them with its own number in place.
antic_modules_test(generic_defaults_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_defaults"
    LIBRARIES rows
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-defaults-modules")

# The default of a field whose type is an optional value, in a class of a
# library. T085 of docs/audit/tests-findings.md.
antic_modules_test(optional_default_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/optional_default"
    LIBRARIES holder
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/optional-default-modules")

# A literal of a case of a generic variant of a library, written under the
# name of the module with its type arguments. T091 of
# docs/audit/tests-findings.md.
antic_modules_test(generic_variant_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_variant"
    LIBRARIES maybe
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-variant-modules")

# A diamond of four modules: two libraries import a third, and the program
# imports the two. The program holds one descriptor per class, one static
# field, one singleton and one copy of a generic per type argument, and
# three modules declare a struct and a class of one name.
antic_modules_test(diamond_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/diamond"
    LIBRARIES base left right
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/diamond-modules")

# Generics of a library whose bodies reach the private items of their
# module. The client compiles the copies.
antic_modules_test(generic_private_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/generic_private"
    LIBRARIES hidden
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/generic-private-modules")

# Every kind of item that `export` marks, reached from a module of Anti as
# a public item of the library.
antic_modules_test(exports_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/exports"
    LIBRARIES exports
    PROGRAM client
    STD lang
    WORK "${CMAKE_CURRENT_BINARY_DIR}/exports-modules")

# The constants of a library where the language takes a constant
# expression: the length of an array, a constant, the value of an enum, the
# width of a bitfield, `align(N)`, the defaults of a parameter and of a
# field, and the arms of a `switch`.
antic_modules_test(constants_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/constants"
    LIBRARIES limits
    PROGRAM client
    WORK "${CMAKE_CURRENT_BINARY_DIR}/constants-modules")

# A constant of a library as the argument of a type parameter `N: int`.
# T094 of docs/audit/tests-findings.md.
antic_modules_test(constant_argument_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/constants"
    LIBRARIES limits
    PROGRAM argument
    WORK "${CMAKE_CURRENT_BINARY_DIR}/constant-argument-modules")

# The items library once more, written with --strip-docs. A library file
# without its doc text gives the client the same program.
antic_modules_test(items_stripped_modules
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/modules/items"
    LIBRARIES items
    PROGRAM client
    LIB_OPTIONS --strip-docs
    WORK "${CMAKE_CURRENT_BINARY_DIR}/items-stripped-modules")

# The library files of the refusals below, each tree under its own
# directory of ${LIBS}. The libraries of a tree compile in the order given,
# each after the ones it imports.
#   antic_library_files(<tree> [OPTIONS <option>...] LIBRARIES <module>...)
# The fixture <tree>_antl stands when the last one is written.
function(antic_library_files tree)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "LIBRARIES;OPTIONS")
    file(MAKE_DIRECTORY "${LIBS}/${tree}/com/example")
    set(before "")
    list(LENGTH arg_LIBRARIES count)
    set(at 0)
    foreach(library IN LISTS arg_LIBRARIES)
        math(EXPR at "${at} + 1")
        add_test(NAME antl_${tree}_${library}
            COMMAND antic -c -I "${MODULES}/${tree}" -I "${LIBS}/${tree}"
                    --runtime "${ANTIC_RUNTIME_DIR}" ${arg_OPTIONS}
                    -o "${LIBS}/${tree}/com/example/${library}.antl"
                    "${MODULES}/${tree}/com/example/${library}.anti")
        set(fixture ${tree}_antl_${library})
        if(at EQUAL count)
            set(fixture ${tree}_antl)
        endif()
        set_tests_properties(antl_${tree}_${library} PROPERTIES
            FIXTURES_SETUP ${fixture})
        if(before)
            set_tests_properties(antl_${tree}_${library} PROPERTIES
                FIXTURES_REQUIRED ${before})
        endif()
        set(before ${fixture})
    endforeach()
endfunction()
antic_library_files(levels OPTIONS --package-name com.example
    LIBRARIES levels)
antic_library_files(diamond LIBRARIES base left right)
antic_library_files(items LIBRARIES items)
antic_library_files(exports LIBRARIES exports)
antic_library_files(generic_limits LIBRARIES gen)
antic_library_files(externs LIBRARIES first second)

# The refusals at the boundary of a module, one message each. A case names
# the program of tests/errors, the tree of its library files and the
# package of the build. An internal item is asked for from the package
# org.other, and every other case names no package.
foreach(case
        "modules_constant_assigned|items|"
        "modules_constraint_with_an_interface_not_met|items|"
        "modules_direct_import_clash|diamond|"
        "modules_export_symbol_twice|exports|"
        "modules_function_as_a_type|items|"
        "modules_generic_function_of_a_class|generic_limits|"
        "modules_generic_function_of_a_generic_class|generic_limits|"
        "modules_generic_without_its_two_arguments|items|"
        "modules_internal_alias|levels|org.other"
        "modules_internal_class|levels|org.other"
        "modules_internal_class_as_a_base|levels|org.other"
        "modules_internal_constant|levels|org.other"
        "modules_internal_constraint|levels|org.other"
        "modules_internal_direct_import|levels|org.other"
        "modules_internal_enum|levels|org.other"
        "modules_internal_extern_function|levels|org.other"
        "modules_internal_function|levels|org.other"
        "modules_internal_generic_function|levels|org.other"
        "modules_internal_generic_struct|levels|org.other"
        "modules_internal_interface|levels|org.other"
        "modules_internal_singleton|levels|org.other"
        "modules_internal_struct|levels|org.other"
        "modules_internal_struct_as_a_type|levels|org.other"
        "modules_internal_union|levels|org.other"
        "modules_internal_variant|levels|org.other"
        "modules_member_private_field|levels|"
        "modules_member_private_field_from_a_class_below|levels|"
        "modules_member_private_field_in_a_literal|levels|"
        "modules_member_private_function|levels|"
        "modules_member_private_function_from_a_class_below|levels|"
        "modules_member_private_static_function|levels|"
        "modules_member_protected_field|levels|"
        "modules_member_protected_field_in_a_literal|levels|"
        "modules_member_protected_field_of_another_class|levels|"
        "modules_member_protected_function|levels|"
        "modules_member_protected_static_function|levels|"
        "modules_module_of_a_dependency|diamond|"
        "modules_private_alias|levels|"
        "modules_private_class|levels|"
        "modules_private_class_as_a_base|levels|"
        "modules_private_constant|levels|"
        "modules_private_constraint|levels|"
        "modules_private_direct_import|levels|"
        "modules_private_enum|levels|"
        "modules_private_extern_function|levels|"
        "modules_private_function|levels|"
        "modules_private_generic_function|levels|"
        "modules_private_generic_struct|levels|"
        "modules_private_interface|levels|"
        "modules_private_struct|levels|"
        "modules_private_struct_as_a_type|levels|"
        "modules_private_union|levels|"
        "modules_private_variant|levels|"
        "modules_singleton_as_a_base|items|"
        "modules_singleton_copied_by_dup|items|"
        "modules_type_called|items|"
        "modules_type_nested_in_a_generic_class|generic_limits|"
        "modules_type_nested_in_a_generic_class_passed_on|generic_limits|"
        "modules_variadic_function_as_a_value|items|")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 tree)
    list(GET parts 2 package)
    set(options "")
    if(package)
        set(options --package-name ${package})
    endif()
    add_expect_test(error_${name} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/${name}.err"
        COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/${tree}"
                --runtime "${ANTIC_RUNTIME_DIR}" ${options}
                -o "${CMAKE_CURRENT_BINARY_DIR}/${name}.s" ${name}.anti)
    set_tests_properties(error_${name} PROPERTIES
        FIXTURES_REQUIRED ${tree}_antl)
endforeach()

# R127 of "Rules of 2026-10-13": one C function that two declarations of
# one program give two signatures. The first case holds two library
# files, whose message names the second of them, and the second a library
# file and the module that compiles.
add_expect_test(error_modules_extern_function_with_two_signatures STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "second\\.antl declares `extern fn labs` with another signature than a library before it"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/externs"
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_extern_function_with_two_signatures.s"
            modules_extern_function_with_two_signatures.anti)
add_expect_test(error_modules_extern_function_with_another_signature_than_a_library STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/modules_extern_function_with_another_signature_than_a_library.err"
    COMMAND "$<TARGET_FILE:antic>" -S -I "${LIBS}/externs"
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_extern_function_with_another_signature_than_a_library.s"
            modules_extern_function_with_another_signature_than_a_library.anti)
set_tests_properties(error_modules_extern_function_with_two_signatures
    error_modules_extern_function_with_another_signature_than_a_library
    PROPERTIES FIXTURES_REQUIRED externs_antl)

# A library file whose own import no search root holds. The command names
# the file of the left side of the diamond and no root, so the base is
# missing, and the driver says which module the file needs.
add_expect_test(error_modules_missing_dependency STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "left\\.antl needs module `com\\.example\\.base`"
    COMMAND "$<TARGET_FILE:antic>" -S --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_missing_dependency.s"
            modules_missing_dependency.anti
            "${LIBS}/diamond/com/example/left.antl")
set_tests_properties(error_modules_missing_dependency PROPERTIES
    FIXTURES_REQUIRED diamond_antl)

# What the driver refuses of a library for C: a module with `main`, a kind
# of library that is neither `static` nor `shared`, and `--soname` without
# the version it writes into the name. A wrong value of an option ends
# antic with 2, as a wrong command line does.
add_expect_test(error_modules_library_with_main STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "a library for C has no function `main`"
    COMMAND "$<TARGET_FILE:antic>" --lib static
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_library_with_main.a"
            modules_library_with_main.anti)
# R131 of "Rules of 2026-10-13": a library for C of a module that exports
# nothing gives the warning `nothing-exported`, which the release build of
# the library refuses. tests/antic_messages.cmake holds the clause that
# silences it.
add_expect_test(error_modules_library_exports_nothing STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/modules_library_exports_nothing.err"
    COMMAND "$<TARGET_FILE:antic>" --lib static
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_library_exports_nothing.a"
            modules_library_exports_nothing.anti)
add_expect_test(error_modules_library_kind STATUS 2
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "--lib takes static or shared[^a-z]+not bundled"
    COMMAND "$<TARGET_FILE:antic>" --lib bundled
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_library_kind.a"
            modules_library_options.anti)
add_expect_test(error_modules_soname_without_a_version STATUS 1
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    PATTERN "--soname needs --package-version"
    COMMAND "$<TARGET_FILE:antic>" --lib shared --soname
            --runtime "${ANTIC_RUNTIME_DIR}"
            -o "${CMAKE_CURRENT_BINARY_DIR}/modules_soname${plugin_suffix}"
            modules_library_options.anti)

# An import that forms a cycle. The library file of `one` imports `two`,
# which stood without an import when `one` was compiled, and `two` now
# imports `one`. The message stands at the import. T088 of
# docs/audit/tests-findings.md.
file(MAKE_DIRECTORY "${LIBS}/cycle/first/com/example"
     "${LIBS}/cycle/second/com/example")
add_test(NAME antl_cycle_two
    COMMAND antic -c -I "${MODULES}/cycle/first"
            -o "${LIBS}/cycle/first/com/example/two.antl"
            "${MODULES}/cycle/first/com/example/two.anti")
set_tests_properties(antl_cycle_two PROPERTIES FIXTURES_SETUP cycle_two)
add_test(NAME antl_cycle_one
    COMMAND antic -c -I "${MODULES}/cycle" -I "${LIBS}/cycle/first"
            -o "${LIBS}/cycle/second/com/example/one.antl"
            "${MODULES}/cycle/com/example/one.anti")
set_tests_properties(antl_cycle_one PROPERTIES
    FIXTURES_REQUIRED cycle_two FIXTURES_SETUP cycle_one)
add_expect_test(error_modules_import_cycle STATUS 1
    DIRECTORY "${MODULES}/cycle"
    EXPECTED "${MODULES}/cycle/two.err"
    COMMAND "$<TARGET_FILE:antic>" -c -I "${MODULES}/cycle"
            -I "${LIBS}/cycle/second"
            -o "${CMAKE_CURRENT_BINARY_DIR}/cycle_two.antl"
            com/example/two.anti)
set_tests_properties(error_modules_import_cycle PROPERTIES
    FIXTURES_REQUIRED cycle_one)

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
    # R087 of "Rules of 2026-10-13": a count of chunks below zero.
    antic_check_test(parallel_count_below_zero "count of chunks below zero"
        "parallel_count_below_zero\\.anti:[0-9]+: count of chunks below zero: value -3"
        ON)
    # The checks on the narrow types, in a compound assignment and in a copy
    # of a generic function. docs/notes/test-matrix.md names them under
    # "Values".
    antic_check_test(overflow_add_i8 "overflow in"
        "overflow_add_i8\\.anti:[0-9]+: overflow in \\+: left 127, right 1"
        ON)
    antic_check_test(overflow_sub_i16 "overflow in"
        "overflow_sub_i16\\.anti:[0-9]+: overflow in -: left -32768, right 1"
        ON)
    antic_check_test(overflow_mul_i8 "overflow in"
        "overflow_mul_i8\\.anti:[0-9]+: overflow in \\*: left 100, right 2"
        ON)
    antic_check_test(overflow_compound "overflow in"
        "overflow_compound\\.anti:[0-9]+: overflow in \\+: left 9223372036854775807, right 1"
        ON)
    antic_check_test(overflow_generic "overflow in"
        "overflow_generic\\.anti:[0-9]+: overflow in \\+: left 127, right 1"
        ON)
    antic_check_test(narrow_unsigned "out of range"
        "narrow_unsigned\\.anti:[0-9]+: value out of range for i8: value 200"
        ON)
    antic_check_test(narrow_u64 "out of range"
        "narrow_u64\\.anti:[0-9]+: value out of range for int: value 9223372036854775808"
        ON)
    antic_check_test(narrow_to_byte "out of range"
        "narrow_to_byte\\.anti:[0-9]+: value out of range for byte: value -1"
        ON)
    antic_check_test(divide_byte "division by zero"
        "divide_byte\\.anti:[0-9]+: division by zero in /: left 200"
        ON)
    antic_check_test(remainder_i8 "division by zero"
        "remainder_i8\\.anti:[0-9]+: division by zero in %: left -7"
        ON)
    antic_check_test(divide_compound "division by zero"
        "divide_compound\\.anti:[0-9]+: division by zero in /: left 10"
        ON)
    antic_check_test(shift_wide_byte "shift count out of range"
        "shift_wide_byte\\.anti:[0-9]+: shift count out of range for <<: count 8, width 8"
        ON)
    antic_check_test(shift_right_wide_i16 "shift count out of range"
        "shift_right_wide_i16\\.anti:[0-9]+: shift count out of range for >>: count 16, width 16"
        ON)
    antic_check_test(shift_compound "shift count out of range"
        "shift_compound\\.anti:[0-9]+: shift count out of range for <<: count 64, width 64"
        ON)
    # R031 of "Rules of 2026-10-13": the least value divided by -1, its
    # remainder by -1 and unary `-` of it stop a dev build with the
    # overflow check.
    antic_check_test(overflow_divide_least "overflow in"
        "overflow_divide_least\\.anti:[0-9]+: overflow in /: left -9223372036854775808, right -1"
        ON)
    antic_check_test(overflow_remainder_least_i8 "overflow in"
        "overflow_remainder_least_i8\\.anti:[0-9]+: overflow in %: left -128, right -1"
        ON)
    antic_check_test(overflow_divide_least_compound "overflow in"
        "overflow_divide_least_compound\\.anti:[0-9]+: overflow in /: left -32768, right -1"
        ON)
    antic_check_test(overflow_negate_least "overflow in"
        "overflow_negate_least\\.anti:[0-9]+: overflow in -: value -9223372036854775808"
        ON)
    antic_check_test(overflow_negate_least_i8 "overflow in"
        "overflow_negate_least_i8\\.anti:[0-9]+: overflow in -: value -128"
        ON)
    # R032: `<<%` by a count below zero stops a dev build. A count at or
    # above the width gives 0 and is no failure.
    antic_check_test(shift_wrap_negative "shift count out of range"
        "shift_wrap_negative\\.anti:[0-9]+: shift count out of range for <<%: count -1, width 64"
        ON)
    antic_check_test(shift_wrap_negative_compound "shift count out of range"
        "shift_wrap_negative_compound\\.anti:[0-9]+: shift count out of range for <<%: count -1, width 8"
        ON)
    # R048: a value that does not fit the width of a bitfield stops a dev
    # build where it is stored, in a write, a compound assignment and a
    # literal.
    antic_check_test(bitfield_write "out of range"
        "bitfield_write\\.anti:[0-9]+: value out of range for the 3 bits of a: value 13"
        ON)
    antic_check_test(bitfield_write_signed "out of range"
        "bitfield_write_signed\\.anti:[0-9]+: value out of range for the 3 bits of b: value -5"
        ON)
    antic_check_test(bitfield_compound "out of range"
        "bitfield_compound\\.anti:[0-9]+: value out of range for the 3 bits of a: value 8"
        ON)
    antic_check_test(bitfield_literal "out of range"
        "bitfield_literal\\.anti:[0-9]+: value out of range for the 20 bits of b: value 524288"
        ON)
    antic_check_test(bitfield_class "out of range"
        "bitfield_class\\.anti:[0-9]+: value out of range for the 9 bits of kind: value 512"
        ON)
    # Decision 24 of "Decisions of 2026-10-11": a dev build checks the
    # bounds of `xs[lo..hi]` as it checks an index.
    antic_check_test(slice_range "out of bounds"
        "slice_range\\.anti:[0-9]+: .*out of bounds.*length 3"
        ON)
    # The same check on a slice of a slice, against the length of the
    # slice, and on bounds that give no slice: the low one above the high
    # one, and the low one below zero.
    antic_check_test(slice_of_slice_range "out of bounds"
        "slice_of_slice_range\\.anti:[0-9]+: .*out of bounds.*length 2"
        ON)
    antic_check_test(slice_range_reversed "out of bounds"
        "slice_range_reversed\\.anti:[0-9]+: .*out of bounds"
        ON)
    antic_check_test(slice_range_negative "out of bounds"
        "slice_range_negative\\.anti:[0-9]+: .*out of bounds"
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
    # M304: `reserve` and `shrink` move the room a walk holds, and each
    # counts the move as a change of the collection.
    antic_check_test(list_reserve_changed "was changed while"
        "list_reserve_changed\\.anti:14: `xs` was changed while `for` walked it: changed at list_reserve_changed\\.anti:16"
        ON DEV lang mem text collection collection/list)
    antic_check_test(list_shrink_changed "was changed while"
        "list_shrink_changed\\.anti:14: `xs` was changed while `for` walked it: changed at list_shrink_changed\\.anti:16"
        ON DEV lang mem text collection collection/list)
    antic_check_test(deque_reserve_changed "was changed while"
        "deque_reserve_changed\\.anti:14: `xs` was changed while `for` walked it: changed at deque_reserve_changed\\.anti:16"
        ON DEV lang mem text collection collection/deque)
    antic_check_test(deque_shrink_changed "was changed while"
        "deque_shrink_changed\\.anti:14: `xs` was changed while `for` walked it: changed at deque_shrink_changed\\.anti:16"
        ON DEV lang mem text collection collection/deque)
    antic_check_test(map_reserve_changed "was changed while"
        "map_reserve_changed\\.anti:14: `ages` was changed while `for` walked it: changed at map_reserve_changed\\.anti:16"
        ON DEV lang mem text collection collection/map)
    antic_check_test(map_shrink_changed "was changed while"
        "map_shrink_changed\\.anti:14: `ages` was changed while `for` walked it: changed at map_shrink_changed\\.anti:16"
        ON DEV lang mem text collection collection/map)
    antic_check_test(hash_map_reserve_changed "was changed while"
        "hash_map_reserve_changed\\.anti:14: `ages` was changed while `for` walked it: changed at hash_map_reserve_changed\\.anti:16"
        ON DEV lang mem text collection collection/map)
    antic_check_test(hash_map_shrink_changed "was changed while"
        "hash_map_shrink_changed\\.anti:14: `ages` was changed while `for` walked it: changed at hash_map_shrink_changed\\.anti:16"
        ON DEV lang mem text collection collection/map)
    antic_check_test(bitset_reserve_changed "was changed while"
        "bitset_reserve_changed\\.anti:14: `seen` was changed while `for` walked it: changed at bitset_reserve_changed\\.anti:16"
        ON DEV lang mem text collection collection/map collection/set)
    antic_check_test(bitset_shrink_changed "was changed while"
        "bitset_shrink_changed\\.anti:14: `seen` was changed while `for` walked it: changed at bitset_shrink_changed\\.anti:16"
        ON DEV lang mem text collection collection/map collection/set)
    antic_check_test(sorted_reserve_changed "was changed while"
        "sorted_reserve_changed\\.anti:14: `ages` was changed while `for` walked it: changed at sorted_reserve_changed\\.anti:16"
        ON DEV lang mem text collection reflect collection/sorted)
    antic_check_test(sorted_shrink_changed "was changed while"
        "sorted_shrink_changed\\.anti:14: `ages` was changed while `for` walked it: changed at sorted_shrink_changed\\.anti:16"
        ON DEV lang mem text collection reflect collection/sorted)
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
# A Windows link writes CodeView, which neither reads, so a Windows host
# runs no test of them even where a gdb stands on its path. A host without
# a debugger reports the test skipped and names the reason.
find_program(ANTIC_LLDB NAMES lldb DOC "the debugger of macOS")
find_program(ANTIC_GDB NAMES gdb DOC "the debugger of Linux")
if(NOT WIN32 AND NOT ANTIC_LLDB AND NOT ANTIC_GDB)
    add_test(NAME debug_info
        COMMAND "${CMAKE_COMMAND}" -E echo
            "SKIP: neither lldb nor gdb is on the path")
    set_tests_properties(debug_info PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP: ")
elseif(ANTIC_LLVM_MC AND NOT WIN32)
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
            "-DOBJECT=${anti_object}"
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
# pattern. chan_capacity: a channel made for no value stops the program.
# send_closed and send_closed_waiting: a `send` on a closed channel stops
# it, one that waits on a full channel included. take_in_refused,
# error_text_refused and trace_names_refused: the text that `take_in`
# makes, the text of an error and the names of a trace stop the program
# when they find no memory, by decision 4 of "Decisions of 2026-10-11".
# The room_ programs: a collection asked for room whose size no `int`
# holds stops the program as memory that runs out, finding S316, in a
# product, a sum, a doubling and a shift of the room of each collection.
foreach(name IN ITEMS parallel_size alloc_literal alloc_construct
        singleton_memory pattern_limit pattern_group_count chan_capacity
        send_closed send_closed_waiting take_in_refused error_text_refused
        trace_names_refused room_list room_list_reserve room_deque_reserve
        room_grid room_map room_map_reserve room_hash_map room_sorted
        room_pool room_pool_shift room_bitset)
    add_test(NAME trap_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/${name}.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trap.cmake")
endforeach()
# The place_ programs: a place function of a collection given a place
# that holds no element stops the program with `Error.fatal`, finding
# S315, one program for each class that fills the functions.
foreach(name IN ITEMS place_list place_list_next place_grid place_queue
        place_map place_map_next place_hash_map place_bitset place_pool
        place_tree place_sorted place_sorted_set)
    add_test(NAME trap_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/${name}.anti"
            "-DSTATUS=1"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trap.cmake")
endforeach()
# fatal_none: `catch fatal` on a `none` pointer runs the function of
# `error.on_fatal`, prints the error and exits with 1, in release mode,
# where the guard calls `Error.fatal` directly and ends with unreachable.
add_test(NAME trap_fatal_none
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/fatal_none.anti"
        "-DSTATUS=1"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_trap.cmake")

# The size of a Mutex per target: four bytes on Linux and macOS, eight on
# Windows.
add_test(NAME mutex_size
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DOPT=${ANTIC_OPT}"
        "-DLLC=${ANTIC_LLC}"
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

# The lock orders of one thread: lock_orders holds a pair in opposite
# orders, a ring of three locks, a lock that `destroy` ended and the hidden
# lock of an object that its thread takes again. lock_order_once holds the
# two orders of a pair more than once, which a dev build reports once.
# check_join_twice: a second `join` of one job that the checker cannot see
# traps in a dev build, by decision 23 of "Decisions of 2026-10-11".
# DESIGN: run_dev_report.cmake stops a program after ANTIC_REPORT_SECONDS,
# so a program that hangs fails its test as an ordinary failure, which
# WILL_FAIL of tests/known-failures.txt expects.
set(ANTIC_REPORT_SECONDS 20)
foreach(name IN ITEMS lock_orders lock_order_once)
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/${name}.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
            "-DTIMEOUT=${ANTIC_REPORT_SECONDS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dev_report.cmake")
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endforeach()
add_test(NAME check_join_twice
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/join_twice.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        "-DTIMEOUT=${ANTIC_REPORT_SECONDS}"
        "-DPATTERN=join_twice\\.anti:[0-9]+: .*join"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dev_report.cmake")
set_tests_properties(check_join_twice PROPERTIES TIMEOUT 120)
# check_sync_held: a `sync` on a Mutex that its thread holds already, which
# the checker cannot see, traps in a dev build, by R093 of "Rules of
# 2026-10-13". The release build waits for itself and is not run.
add_test(NAME check_sync_held
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/sync_held.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        "-DTIMEOUT=${ANTIC_REPORT_SECONDS}"
        "-DPATTERN=anti: sync_held\\.anti:7 takes a Mutex that its thread holds since sync_held\\.anti:16"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dev_report.cmake")
set_tests_properties(check_sync_held PROPERTIES TIMEOUT 120)

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
# --memory-checks finds a read of a bitfield after free, as it finds the
# read of a plain field.
add_test(NAME memory_checks_bitfield
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/traps/memory_checks_bitfield.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/traps"
        "-DHOST=${ANTIC_HOST_TARGET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_memory_checks_bitfield.cmake")
set_tests_properties(memory_checks_bitfield PROPERTIES TIMEOUT 120
    SKIP_REGULAR_EXPRESSION "SKIP: ")
# text.Builder appends in place while its room holds the bytes and the NUL.
# Under --memory-checks a fresh block holds no zeros, so a missing NUL or a
# write past the room fails here where a plain run passes by chance.
if(NOT ANTIC_HOST_TARGET STREQUAL "windows-arm64")
    add_test(NAME std_builder_room_memory_checks
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/std/builder_room.anti"
            "-DOPTIONS=--memory-checks"
            "-DOBJECTS="
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/std-memory-checks"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_program.cmake")
    set_tests_properties(std_builder_room_memory_checks PROPERTIES TIMEOUT 120)
endif()
# DESIGN: the runtime of AddressSanitizer for x86_64 stops under
# qemu-x86_64, in its allocator, and under the x64 emulation of Windows,
# where it cannot intercept the heap functions. Rosetta runs it, and the
# block of the emulated target registers the programs of macos-x86_64. On
# linux-arm64 and windows-arm64 the three programs of --memory-checks are
# therefore linked for the emulated target and not run, and each test
# reports itself skipped with the reason. The runtimes of x86_64 last ran
# on real x86_64 hardware on 2026-10-08, as
# docs/reports/2026-10-08-llvm-closed.md reports.
if(ANTIC_EMULATED_TARGET MATCHES "^(linux|windows)-x86_64$")
    foreach(case "memory_checks,traps/memory_checks.anti"
            "memory_checks_list,traps/memory_checks_list.anti"
            "std_builder_room_memory_checks,std/builder_room.anti")
        string(REPLACE "," ";" parts "${case}")
        list(GET parts 0 name)
        list(GET parts 1 source)
        antic_program_test(${name}_${ANTIC_EMULATED_TARGET}
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            OPTIONS "${ANTIC_X86_64_OPTIONS},--memory-checks"
            SKIP_RUN "the AddressSanitizer runtime of x86_64 stops under emulation"
            WORK "${CMAKE_CURRENT_BINARY_DIR}/memory-checks-x86_64")
        set_tests_properties(${name}_${ANTIC_EMULATED_TARGET} PROPERTIES
            TIMEOUT 120 SKIP_REGULAR_EXPRESSION "SKIP: ")
    endforeach()
endif()

# A program of musl links mimalloc of the runtime archive as its C
# allocator in every mode, and a program of --memory-checks does not.
foreach(target linux-x86_64 linux-arm64)
    add_test(NAME musl_allocator_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/c_allocator.anti"
            "-DTARGET=${target}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/musl-allocator/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_musl_allocator.cmake")
    set_tests_properties(musl_allocator_${target} PROPERTIES TIMEOUT 120
        SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# A program of musl reads no MIMALLOC_* variable: run with two of them,
# it prints what it prints without them. A Linux host runs the program of
# its own target, and linux-arm64 the one of linux-x86_64 under qemu.
foreach(target linux-x86_64 linux-arm64)
    set(emulator "")
    if(target STREQUAL ANTIC_EMULATED_TARGET)
        set(emulator "${ANTIC_EMULATOR}")
    endif()
    add_test(NAME mimalloc_environment_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/abi/c_allocator.anti"
            "-DTARGET=${target}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DEMULATOR=${emulator}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mimalloc-environment/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_mimalloc_environment.cmake")
    set_tests_properties(mimalloc_environment_${target} PROPERTIES
        TIMEOUT 120 SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

# Under --lto none every function of an ARM64 program and of its runtime
# starts on a 16-byte boundary.
foreach(target macos-arm64 linux-arm64 windows-arm64)
    add_test(NAME function_alignment_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DREADOBJ=${ANTIC_RUNTIME_READOBJ}"
            "-DOBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/devirtualise.anti"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/function-alignment/${target}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_function_alignment.cmake")
    set_tests_properties(function_alignment_${target} PROPERTIES
        TIMEOUT 120 SKIP_REGULAR_EXPRESSION "SKIP: ")
endforeach()

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

# The same probe over the types of the area types of
# docs/notes/test-matrix.md: packed and aligned types with fields at odd
# offsets, unions, the tag and the union of a variant, an enum, an array
# of arrays, an optional value, a tuple and a slice.
if(NOT WIN32)
    add_test(NAME abi_probe_types
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/abi"
            "-DNAME=probe_types"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/probe-types"
            "-DCC=${ANTIC_TEST_CC}"
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_probe.cmake")
    # run_probe.cmake reads the exit status of both probes.
    add_test(NAME abi_probe_runner
        COMMAND "${CMAKE_COMMAND}"
            "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_probe.cmake"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/probe-runner"
            "-DCC=${ANTIC_TEST_CC}"
            "-DHOST_LINK=${ANTIC_HOST_LINK_OPTIONS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_probe_self.cmake")
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
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DMUSL_VERSION=${ANTIC_MUSL_VERSION}"
            "-DMIMALLOC_VERSION=${ANTIC_MIMALLOC_VERSION}"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/dump/licenses.notice"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_licenses.cmake")
endif()

# Stack traces and the origin of an error. A trace holds addresses that
# differ from run to run, so run_trace.cmake matches the output against
# patterns. Each case is a name, the source in tests/trace, the options of
# antic and the arguments of the program. The pattern files of the output
# and of the error output follow, then the exit status. A dev build links
# the dev object of anti.lang. Release mode inlines small functions, as
# the entry on inlined frames in docs/decisions.md says, so a release case
# takes NAME.release.pattern where its trace names other frames than the
# one of dev mode.
set(TRACE "${CMAKE_CURRENT_SOURCE_DIR}/trace")
foreach(case
        "stack|stack_trace|||stack_trace.release.pattern||0"
        "stack_g|stack_trace|-g||stack_trace.g.pattern||0"
        "stack_dev|stack_trace|--dev||stack_trace.pattern||0"
        "forged|forged|||forged.pattern||0"
        "copies|copies|||copies.release.pattern||0"
        "copies_g|copies|-g||copies.release.pattern||0"
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
# --anti.conf, ANTI_CONF or rt.configure names, the includes of that file
# and the file of the logger that the key `logger` names. A case is a name, the source in tests/conf, the options of antic
# and the arguments of the program, then the pattern files of the output
# and of the error output and the exit status. A pattern file NAME.pattern
# has NAME.win32.pattern beside it where a Windows program prints `;`,
# the separator of a list of directories there. The runner is the one of
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
        "hash_seed_text|report||--anti.conf=${CONF}/seed-text.toml|empty.pattern|seed-text.pattern|70"
        "logger_unknown_kind|logger||--anti.logger=${CONF}/logger-unknown.toml|logger-done.pattern|logger-unknown.pattern|0"
        "logger_register|logger||--anti.logger=${CONF}/logger-first.toml,register|logger-first.pattern||0"
        "logger_sinks|logger||--anti.logger=${CONF}/logger-sinks.toml,register|logger-sinks.pattern||0"
        "logger_ninth_sink|logger||--anti.logger=${CONF}/logger-ninth.toml,register|logger-first.pattern|logger-ninth.errors.pattern|0"
        "logger_unopened|logger||--anti.logger=${CONF}/logger-unopened.toml|logger-done.pattern|logger-unopened.pattern|0")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 source)
    list(GET parts 2 options)
    list(GET parts 3 arguments)
    list(GET parts 4 expected)
    list(GET parts 5 errors)
    list(GET parts 6 status)
    string(REGEX REPLACE "[.]pattern$" ".win32.pattern" win32 "${expected}")
    if(ANTIC_HOST_TARGET MATCHES "^windows-" AND EXISTS "${CONF}/${win32}")
        set(expected "${win32}")
    endif()
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
# replacement at start, and --anti.inspect lists what it injects. The
# program imports nothing, and the case plugin shows that its start opens
# the library that --anti.inject names through the loader of the runtime,
# which every program links, S328 of docs/audit/summary.md. The runner
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

# A plugin whose interface takes and gives every kind of value: the host
# calls each function through the table of the interface, hands the
# library a closure and an object of its own, and ends an object the
# library made.
add_test(NAME plugin_kinds
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-kinds"
        "-DHOST=kinds_host"
        "-DLIBRARY=busy"
        "-DINTERFACE=kinds"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/kinds_host.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")

# The objects of a library that `live` counts and `unload` waits for: a
# local object of a class of the library is gone when its function
# returns, with a `destruct` and without one. T092 of
# docs/audit/tests-findings.md.
add_test(NAME plugin_locals
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-locals"
        "-DHOST=locals_host"
        "-DLIBRARY=locals"
        "-DINTERFACE=tasks"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/locals_host.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")

# An unload while another thread runs the code of the library: while
# `reflect.new` runs the `construct` of a class of the library, and while
# `delete` runs the `destruct` of an object of it. Each is refused. S307
# and S308 of docs/audit/summary.md.
add_test(NAME plugin_stages
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-stages"
        "-DHOST=stages_host"
        "-DLIBRARY=stalls"
        "-DINTERFACE=stages"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/stages_host.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")

# A trace whose innermost frame lies in a library, read again after the
# unload of the library: its build id, the name of its function and the
# text of the trace. S306 of docs/audit/summary.md.
add_test(NAME plugin_trace
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-trace"
        "-DHOST=trace_host"
        "-DLIBRARY=tracer"
        "-DINTERFACE=traced"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/trace_host.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")

# A library that uses two modules of the standard library which its host
# imports and does not use itself. The library resolves them against the
# host. T093 of docs/audit/tests-findings.md.
add_test(NAME plugin_unused
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DLLVM_MC=${ANTIC_LLVM_MC}"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
        "-DSUFFIX=${plugin_suffix}"
        "-DEXE=${plugin_exe}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-unused"
        "-DHOST=stdlib_host"
        "-DLIBRARY=stdlib"
        "-DINTERFACE=tasks"
        "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/stdlib_host.expected"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")
# R133 of "Rules of 2026-10-13" in docs/decisions.md: a library that uses
# two modules of the standard library which its host does not import fails
# to load, and the host handles the failure. A Windows plugin links against
# the import library of its host, where the same lack stops the link of
# the library, so the test runs on the other hosts.
if(NOT ANTIC_HOST_OS STREQUAL "windows")
    add_test(NAME plugin_module_not_imported
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
            "-DSUFFIX=${plugin_suffix}"
            "-DEXE=${plugin_exe}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-module-not-imported"
            "-DHOST=bare_host"
            "-DLIBRARY=stdlib"
            "-DINTERFACE=tasks"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/bare_host.expected"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")
endif()
# R133 of "Rules of 2026-10-13": the message of such a load names the
# module the host lacks. The library uses `anti.time` alone, which imports
# nothing, so one module is missing on every host.
if(NOT ANTIC_HOST_OS STREQUAL "windows")
    add_test(NAME plugin_module_named
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
            "-DSUFFIX=${plugin_suffix}"
            "-DEXE=${plugin_exe}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-module-named"
            "-DHOST=named_host"
            "-DLIBRARY=clock"
            "-DINTERFACE=tasks"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/named_host.expected"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")
endif()
# A provided class that the loader cannot build without arguments: one
# whose `construct` takes one, and one that holds such a class in a field
# no default fills. `instance` gives `none` for each, and the link of the
# library succeeds.
foreach(library needy holder)
    add_test(NAME plugin_refused_${library}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/plugin"
            "-DSUFFIX=${plugin_suffix}"
            "-DEXE=${plugin_exe}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/plugin-refused-${library}"
            "-DHOST=refused_host"
            "-DLIBRARY=${library}"
            "-DINTERFACE=tasks"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/plugin/refused_host.expected"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_plugin_kinds.cmake")
endforeach()
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
antic_coverage(symbols_probe)
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

# read_build_id of tests/build_id.cmake refuses a file with no build line,
# with two, or with an id that is not 64 digits.
add_test(NAME build_id_reader
    COMMAND "${CMAKE_COMMAND}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/build-id-reader"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_build_id_reader.cmake")
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

# The test llvm_build_id of docs/work-order-llvm-back-end.md: under the
# LLVM back end a -g build and a plain build of one program give one build
# id, since the digest leaves out every span of the text that -g added.
# The program has several functions, so -g adds a subprogram and lines to
# each. COFF writes subprograms in the plain build too.
if(NOT WIN32)
    foreach(target ${ANTIC_HOST_TARGET} windows-x86_64)
        add_test(NAME llvm_build_id_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/trace/stack_trace.anti"
                "-DOTHER=${CMAKE_CURRENT_SOURCE_DIR}/programs/return42.anti"
                "-DOPTIONS=--target|${target}"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/llvm-build-id/${target}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_build_id.cmake")
    endforeach()
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
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DMUSL_VERSION=${ANTIC_MUSL_VERSION}"
            "-DMIMALLOC_VERSION=${ANTIC_MIMALLOC_VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_licenses.cmake")
endif()

# Libraries for C, built with antic --lib and used from C programs on the
# host. Windows compiles the programs with the pinned clang against the
# mingw-w64 sysroot of the runtime archive and links them with lld-link.

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
            generics optional forms views never never_cpp ctypes
            ctypes_shared ctypes_bundle tight tight_aligned)
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
        set_tests_properties(clib_${case} PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP: ")
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

# A bundled library of each macOS target on every host. The pinned clang
# compiles the C programs against the sysroot of the runtime archive and
# ld64.lld links them, as antic links an Anti program. A Mac runs them,
# the ones of macos-x86_64 under Rosetta at v1, and every other host
# links them alone. clib_bundle_twice_<target> links two bundled libraries
# into one program and expects the duplicate symbol of their markers.
if(ANTIC_LLVM_AR AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(target macos-arm64 macos-x86_64)
        set(cross ON)
        set(options "")
        if(target STREQUAL ANTIC_HOST_TARGET)
            set(cross OFF)
        elseif(target STREQUAL ANTIC_EMULATED_TARGET)
            set(cross OFF)
            set(options "--cpu|v1")
        endif()
        foreach(case bundle twice)
            set(name clib_bundle_${target})
            if(case STREQUAL "twice")
                set(name clib_bundle_twice_${target})
            endif()
            add_test(NAME ${name}
                COMMAND "${CMAKE_COMMAND}"
                    "-DANTIC=$<TARGET_FILE:antic>"
                    "-DLLVM_MC=${ANTIC_LLVM_MC}"
                    "-DLLVM_AR=${ANTIC_LLVM_AR}"
                    "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                    "-DSOURCES=${CMAKE_CURRENT_SOURCE_DIR}/clib"
                    "-DDUMP=${CMAKE_CURRENT_SOURCE_DIR}/dump"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/clib-${target}"
                    "-DCASE=${case}"
                    "-DCC=${CMAKE_C_COMPILER}"
                    "-DCXX=${CMAKE_C_COMPILER}"
                    "-DTARGET=${target}"
                    "-DOPTIONS=${options}"
                    "-DLLVM_OBJDUMP=${ANTIC_RUNTIME_OBJDUMP}"
                    -DSYSROOT=ON
                    -DCROSS=${cross}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_clib.cmake")
            set_tests_properties(${name} PROPERTIES
                SKIP_REGULAR_EXPRESSION "SKIP: ")
        endforeach()
    endforeach()
endif()

# A function that may fail and returns a tuple takes one out pointer, the
# last parameter, and not one per element. The header of clib_tuples
# pins the same signature on the C side.
add_test(NAME ir_tuple_out
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DCOMMAND=--runtime,${ANTIC_RUNTIME_DIR},-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/tuples.anti"
        "-DLINE=export fn com.example.tuples.tuples_divide(%0: i64, %1: i64, %2: ptr nonnull deref(size_of (int, int))) -> ptr {"
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
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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

# The LLVM text of a program of control flow alone: one target of each
# object format, a release build and two objects of one module.
antic_dump_test(llvm control_flow
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/control_flow.macos-arm64.ll"
    TARGET macos-arm64
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/control_flow.anti")
foreach(target linux-x86_64 windows-x86_64)
    antic_dump_test(llvm control_flow
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/control_flow.${target}.ll"
        TARGET ${target}
        ARGS --dev "${CMAKE_CURRENT_SOURCE_DIR}/dump/control_flow.anti")
endforeach()

# main.opt is the optimized IR listing that chapter 1 prints.
antic_dump_test(opt main
    "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.opt"
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/main.anti")

# The LLVM text of the programs of the chapters on selection, register
# allocation, the back ends and emission, one golden per program and
# target, the programs and targets whose machine code had a golden.
foreach(case "main,macos-arm64" "main,macos-x86_64" "main,linux-x86_64"
        "main,linux-arm64" "main,windows-x86_64" "main,windows-arm64"
        "twice,macos-arm64" "cell,macos-arm64" "spill12,macos-arm64"
        "spill7,linux-x86_64" "sum,linux-x86_64" "sum,macos-arm64"
        "args,linux-x86_64" "args,windows-x86_64" "args,macos-arm64"
        "args,linux-arm64" "x86,linux-x86_64" "x86,linux-arm64"
        "arm64,macos-arm64" "arm64,linux-arm64"
        "floatcalls,linux-x86_64" "floatcalls,windows-x86_64"
        "floatcalls,macos-arm64" "floatcalls,windows-arm64"
        "floatcalls,linux-arm64" "floats,linux-x86_64" "floats,macos-arm64"
        "structs,linux-x86_64" "structs,windows-x86_64" "structs,macos-arm64"
        "structs,linux-arm64" "structs,windows-arm64"
        "fnptr,linux-x86_64" "fnptr,linux-arm64" "fnptr,macos-arm64"
        "loop,linux-arm64" "loop,macos-x86_64"
        "spillweight,linux-x86_64" "measure,linux-arm64"
        "strings,linux-x86_64" "strings,macos-arm64" "strings,windows-x86_64")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 source)
    list(GET parts 1 target)
    antic_dump_test(llvm ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.${target}.ll"
        TARGET ${target}
        ARGS "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.anti")
endforeach()
foreach(case "sizes,linux-x86_64" "bitfields,linux-x86_64"
        "bitfields,windows-x86_64" "letters,macos-arm64" "letters,linux-x86_64")
    string(REPLACE "," ";" parts "${case}")
    list(GET parts 0 source)
    list(GET parts 1 target)
    antic_dump_test(llvm ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/dump/${source}.${target}.ll"
        TARGET ${target}
        ARGS "${CMAKE_CURRENT_SOURCE_DIR}/programs/${source}.anti")
endforeach()

# The refusals of the programs of tests/errors, error_<name>. A message
# names the file alone, so each runs in the directory of its program.
set(LISTINGS "${CMAKE_CURRENT_BINARY_DIR}/listings")
file(MAKE_DIRECTORY "${LISTINGS}")
foreach(case "layout|-S,--target,linux-x86_64,-o,${LISTINGS}/layout.s,layout.anti"
        "layout_unit|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_unit.s,layout_unit.anti"
        "layout_size|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_size.s,layout_size.anti"
        "layout_shift|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_shift.s,layout_shift.anti"
        "layout_divide|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_divide.s,layout_divide.anti"
        "layout_sum|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_sum.s,layout_sum.anti"
        "layout_span|-S,--target,linux-x86_64,-o,${LISTINGS}/layout_span.s,layout_span.anti"
        "frame_limit|-S,--runtime,${ANTIC_RUNTIME_DIR},--target,linux-x86_64,-o,${LISTINGS}/frame_limit.s,frame_limit.anti"
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
        "refused_base_uses|-S,-o,${LISTINGS}/refused_base_uses.s,refused_base_uses.anti"
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
        "inject_cycle_outside|-S,--inject,inject_cycle_outside.Store=inject_cycle_outside.Disk.get,--inject,inject_cycle_outside.Logger=inject_cycle_outside.Console.get,--inject,inject_cycle_outside.Clock=inject_cycle_outside.Watch.get,-o,${LISTINGS}/inject_cycle_outside.s,inject_cycle_outside.anti"
        "inject_plugin_empty|-S,--inject,inject_plugin_empty.Logger=plugin:,-o,${LISTINGS}/inject_plugin_empty.s,inject_plugin_empty.anti"
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
        "pattern_exponential_assertions|-S,--runtime,${ANTIC_RUNTIME_DIR},-o,${LISTINGS}/pattern_exponential_assertions.s,pattern_exponential_assertions.anti"
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
        "copies_deep_front_end|--front-end,copies_deep_front_end.anti"
        "copies_many|-S,-o,${LISTINGS}/copies_many.s,copies_many.anti"
        "never_result|--front-end,never_result.anti"
        "call_count_of_a_function_value|--front-end,call_count_of_a_function_value.anti"
        "call_gives_no_value|--front-end,call_gives_no_value.anti"
        "copy_nests_deep|--front-end,copy_nests_deep.anti"
        "copy_nests_deep_field|--front-end,copy_nests_deep_field.anti"
        "enum_value_called|--front-end,enum_value_called.anti"
        "function_not_concurrent|--front-end,function_not_concurrent.anti"
        "function_not_kept|--front-end,function_not_kept.anti"
        "iter_gives_no_iterator|--front-end,iter_gives_no_iterator.anti"
        "lent_result_no_pointer|--front-end,lent_result_no_pointer.anti"
        "moved_into_no_name|--front-end,moved_into_no_name.anti"
        "patch_argument_count|--front-end,patch_argument_count.anti"
        "patch_finds_an_int|--front-end,patch_finds_an_int.anti"
        "plugin_instance_of_a_value|--front-end,--runtime,${ANTIC_RUNTIME_DIR},plugin_instance_of_a_value.anti"
        "plugin_instance_of_a_long_path|--front-end,--runtime,${ANTIC_RUNTIME_DIR},plugin_instance_of_a_long_path.anti"
        "simd_lanes_compared|--front-end,simd_lanes_compared.anti"
        "switch_refused_returns|--front-end,switch_refused_returns.anti"
        "tests_setup|--front-end,--tests,tests_setup.anti"
        "tests_may_fail|--front-end,--tests,--runtime,${ANTIC_RUNTIME_DIR},tests_may_fail.anti"
        "tests_block_holds_a_struct|--front-end,--tests,tests_block_holds_a_struct.anti"
        "tests_block_twice|--front-end,--tests,tests_block_twice.anti"
        "tests_block_without_a_brace|--front-end,--tests,tests_block_without_a_brace.anti"
        "tests_block_function_without_a_name|--front-end,--tests,tests_block_function_without_a_name.anti"
        "fixtures_block_holds_a_struct|--front-end,--tests,fixtures_block_holds_a_struct.anti"
        "fixtures_block_twice|--front-end,--tests,fixtures_block_twice.anti"
        "hex_escape_short|--front-end,hex_escape_short.anti"
        "tuple_element_hexadecimal|--front-end,tuple_element_hexadecimal.anti"
        "assert_message_is_a_variable|--front-end,assert_message_is_a_variable.anti")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 arguments)
    string(REPLACE "," ";" arguments "${arguments}")
    add_expect_test(error_${name} STATUS 1
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
        EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/${name}.err"
        COMMAND "$<TARGET_FILE:antic>" ${arguments})
endforeach()
# An `if` whose statement stands at the last level the parser takes.
# DESIGN: finding T103 of docs/audit/tests-findings.md. antic 0.2.0 never
# ends on this source and takes about two gigabytes of memory per second,
# so the test stops it after two seconds, where a matrix test would let it
# run for sixty. An antic that refuses the source needs a few
# milliseconds.
add_expect_test(error_if_at_the_depth_limit STATUS 1
    TIMEOUT 2
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/errors"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/errors/if_at_the_depth_limit.err"
    COMMAND "$<TARGET_FILE:antic>" --front-end if_at_the_depth_limit.anti)
# A program of tests/errors that no test reads is refused.
add_test(NAME errors_registered
    COMMAND "${CMAKE_COMMAND}" "-DTESTS=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_errors_registered.cmake")
set(DUMP "${CMAKE_CURRENT_SOURCE_DIR}/dump")
# Each case is the stage, the subject, the target or nothing, the expected
# file under tests/dump and the other arguments of antic.
foreach(case "types|symbolic||symbolic.types|${DUMP}/symbolic.anti"
        "types|symbolic_operators||symbolic_operators.types|${DUMP}/symbolic_operators.anti"
        "types|lent_optional||lent_optional.types|${DUMP}/lent_optional.anti"
        "types|generics||generics.types|${DUMP}/generics.anti"
        "types|constraints||constraints.types|${DUMP}/constraints.anti"
        "ir|header_main||header/main.ir|${DUMP}/header/main.anti"
        "ir|size_of_main||size_of/main.ir|${DUMP}/size_of/main.anti"
        "llvm|size_of_main|linux-arm64|size_of/main.linux-arm64.ll|${DUMP}/size_of/main.anti"
        "llvm|c_long_main|linux-x86_64|c_long/main.linux-x86_64.ll|${DUMP}/c_long/main.anti"
        "llvm|c_long_main|windows-x86_64|c_long/main.windows-x86_64.ll|${DUMP}/c_long/main.anti"
        "llvm|wchar_constant|macos-arm64|wchar_constant.macos-arm64.ll|${DUMP}/wchar_constant.anti"
        "llvm|wchar_constant|linux-arm64|wchar_constant.linux-arm64.ll|${DUMP}/wchar_constant.anti"
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
        "ir|bitfields||bitfields.ir|${CMAKE_CURRENT_SOURCE_DIR}/programs/bitfields.anti"
        "opt|measure||measure.opt|${DUMP}/measure.anti"
        "llvm|scale_dev|macos-arm64|scale.dev.macos-arm64.ll|--dev,-I,${MODULES},${MODULES}/com/example/scale.anti"
        "llvm|scale_dev|windows-x86_64|scale.dev.windows-x86_64.ll|--dev,-I,${MODULES},${MODULES}/com/example/scale.anti"
        "llvm|main_dev|linux-x86_64|main.dev.linux-x86_64.ll|--dev,-I,${LIBS},${MODULES}/main.anti"
        "llvm|doubling|linux-x86_64|doubling.linux-x86_64.ll|--lib,shared,--runtime,${ANTIC_RUNTIME_DIR},-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
        "llvm|doubling|macos-arm64|doubling.macos-arm64.ll|--lib,shared,--runtime,${ANTIC_RUNTIME_DIR},-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
        "llvm|doubling|windows-arm64|doubling.windows-arm64.ll|--lib,shared,--runtime,${ANTIC_RUNTIME_DIR},-I,${CMAKE_CURRENT_SOURCE_DIR}/clib,${CMAKE_CURRENT_SOURCE_DIR}/clib/com/example/doubling.anti"
        "llvm|fatal_guard|macos-arm64|fatal_guard.dev.macos-arm64.ll|--dev,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/fatal_guard.anti"
        "llvm|effects|linux-x86_64|effects.dev.linux-x86_64.ll|--dev,--cpu,v2,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/effects.anti"
        "llvm|tables|macos-arm64|tables.macos-arm64.ll|--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/tables.anti"
        "llvm|params|macos-arm64|params.dev.macos-arm64.ll|--dev,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/params.anti"
        "llvm|arith|macos-arm64|arith.dev.macos-arm64.ll|--dev,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/arith.anti"
        "llvm|tbaa|macos-arm64|tbaa.dev.macos-arm64.ll|--dev,--runtime,${ANTIC_RUNTIME_DIR},${DUMP}/tbaa.anti")
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
set_tests_properties(dump_llvm_main_dev_linux-x86_64 PROPERTIES
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
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
        set(once "\t.weak\tcopies_merge.pick$3c$2acopies_merge.Person$3e")
    elseif(target MATCHES "^macos")
        set(once "\t.weak_definition\t_copies_merge.pick$3c$2acopies_merge.Person$3e")
    else()
        set(once ".text,\"xr\",discard,_A12copies_merge_pick$3c$2acopies_merge.Person$3e")
    endif()
    add_test(NAME asm_copies_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
    # symbol keeps the escaped name. Release mode inlines the copy, so the
    # build is a dev build.
    add_test(NAME asm_debug_copies_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DTARGET=${target}"
            "-DTRIPLE=${triple}"
            "-DOPTIONS=--dev|-g"
            "-DHOLDS=.asciz\t\"copies_merge.pick<int>\"|pick$3cint$3e:"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/copies_merge.anti"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-debug-copies"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    # A shared library for C, with the constructor section of the target.
    add_test(NAME asm_clib_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
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
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DTARGET=${target}"
                "-DTRIPLE=${triple}"
                "-DOPTIONS=--cpu|v1"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dump/half.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/asm-v1"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_asm.cmake")
    endif()
endforeach()

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


# The benchmarks of tests/bench: five Anti programs, each with a C twin
# that the pinned clang compiles at -O2. They take seconds and their times
# vary with the machine, so they stand outside the suite and outside the
# default build. `cmake --build build/host --target bench` builds the C
# twins, then run_bench.cmake compiles and runs every program and prints
# the table into build/host/tests/bench.md as well.
set(bench_programs scalar_loop objects builder simd_loop map_work)
set(bench_executables "")
foreach(name IN LISTS bench_programs)
    add_executable(bench_${name} EXCLUDE_FROM_ALL bench/${name}.c)
    if(MSVC)
        target_compile_options(bench_${name} PRIVATE /O2)
    else()
        target_compile_options(bench_${name} PRIVATE -O2)
    endif()
    list(APPEND bench_executables bench_${name})
endforeach()
# The twin of map_work that reads its seed at run time, which run C of
# tests/bench/ablate/run.py times beside the twin.
add_executable(bench_map_work_rtseed EXCLUDE_FROM_ALL bench/map_work_rtseed.c)
if(MSVC)
    target_compile_options(bench_map_work_rtseed PRIVATE /O2)
else()
    target_compile_options(bench_map_work_rtseed PRIVATE -O2)
endif()
list(APPEND bench_executables bench_map_work_rtseed)
# mixed_work of tests/bench/ablate, the program of condition C1 in
# docs/work-order-llvm-optimization.md, has no C twin. run.py holds every
# build to the line of its baseline, and this test holds the baseline to
# the right line in release mode, so a fact that changes it fails the
# suite and not only a measurement.
antic_program_test(program_mixed_work
    "${CMAKE_CURRENT_SOURCE_DIR}/bench/ablate/mixed_work.anti"
    WORK "${CMAKE_CURRENT_BINARY_DIR}/bench_ablate")
# The same program through the LTO of lld in each mode of --lto, the step
# runtime-lto of the work order. It imports six modules of the standard
# library, so the runtime and the program meet in many places.
foreach(mode full thin)
    antic_program_test(program_mixed_work_lto_${mode}
        "${CMAKE_CURRENT_SOURCE_DIR}/bench/ablate/mixed_work.anti"
        OPTIONS --lto ${mode}
        WORK "${CMAKE_CURRENT_BINARY_DIR}/bench_ablate_lto_${mode}")
endforeach()
# The same program built with --profile-generate, run, merged by
# llvm-profdata and built again with --profile-use, the step pgo of the
# work order. Each build takes the default LTO of a release build and
# passes the test of the program.
add_test(NAME profile_guided
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/bench/ablate/mixed_work.anti"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/profile_guided"
        "-DREFUSALS=ON"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_profile.cmake")
set_tests_properties(profile_guided PROPERTIES TIMEOUT 300)
# A program with a profile of its own run in each link of a release
# build, for the host and for the target it emulates.
set(profile_targets "${ANTIC_HOST_TARGET}")
if(ANTIC_EMULATED_TARGET)
    list(APPEND profile_targets "${ANTIC_EMULATED_TARGET}")
endif()
foreach(target IN LISTS profile_targets)
    set(options "")
    set(emulator "")
    if(target STREQUAL ANTIC_EMULATED_TARGET)
        set(options "${ANTIC_X86_64_OPTIONS}")
        set(emulator "${ANTIC_EMULATOR}")
    endif()
    foreach(lto full thin none)
        set(mode "${lto}")
        if(lto STREQUAL "full")
            set(mode "")
        endif()
        add_test(NAME profile_lto_${lto}_${target}
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/programs/loops.anti"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/profile-lto/${lto}-${target}"
                "-DOPTIONS=${options}"
                "-DEMULATOR=${emulator}"
                "-DLTO=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_profile.cmake")
        set_tests_properties(profile_lto_${lto}_${target} PROPERTIES
            TIMEOUT 300)
    endforeach()
endforeach()
add_custom_target(bench
    COMMAND "${CMAKE_COMMAND}"
        "-DANTIC=$<TARGET_FILE:antic>"
        "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DOPT=${ANTIC_OPT}"
        "-DLLC=${ANTIC_LLC}"
        "-DBENCH=${CMAKE_CURRENT_SOURCE_DIR}/bench"
        "-DC_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/bench"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bench.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_bench.cmake"
    DEPENDS antic anti_std ${bench_executables}
    USES_TERMINAL
    VERBATIM)

# The matrix of constructs and contexts in tests/matrix, by decision 28 of
# "Decisions of 2026-10-11" in docs/decisions.md. A file there registers
# its tests by its name, and no list of this file names it.
#   <name>.anti with <name>.expected is a program. It runs in dev mode, in
#   release mode and with --memory-checks, as matrix_<name>_dev,
#   matrix_<name>_release and matrix_<name>_memory_checks. Each compares
#   the whole output with the one expected file, so the three agree.
#   <name>.anti with <name>.err is a refusal, matrix_<name>_refused,
#   through the runner of tests/errors: antic exits with 1 and prints the
#   bytes of the .err file.
# A <name>.args beside a program holds its arguments, as run_program.cmake
# reads them. The test matrix_files refuses every other file.
#
# DESIGN: ctest counts a test that its own time limit stops as failed
# whatever WILL_FAIL says, and a test that its runner fails as one that
# WILL_FAIL expects. A matrix test may show a defect that keeps antic or
# the program running, and tests/known-failures.txt must be able to list
# it. The runner therefore stops antic and the program after
# ANTIC_MATRIX_SECONDS each, and the limit of ctest stands above the two.
set(ANTIC_MATRIX_SECONDS 60)
math(EXPR matrix_program_limit "2 * ${ANTIC_MATRIX_SECONDS} + 30")
math(EXPR matrix_refusal_limit "${ANTIC_MATRIX_SECONDS} + 30")
set(MATRIX "${CMAKE_CURRENT_SOURCE_DIR}/matrix")
set(MATRIX_WORK "${CMAKE_CURRENT_BINARY_DIR}/matrix")
file(MAKE_DIRECTORY "${MATRIX_WORK}/refused")

# Set out to the modules of the standard library whose dev objects a dev
# build of source links: every module under `anti.` that source imports and
# every module those import, each by its path under std/anti/.
function(antic_dev_imports out source)
    set(modules "")
    set(pending "${source}")
    while(pending)
        list(POP_FRONT pending file)
        file(STRINGS "${file}" imports REGEX "^import anti\\.")
        foreach(line IN LISTS imports)
            string(REGEX MATCH "^import anti((\\.[a-z0-9_]+)+)" found "${line}")
            string(SUBSTRING "${CMAKE_MATCH_1}" 1 -1 module)
            string(REPLACE "." "/" module "${module}")
            if(module IN_LIST modules)
                continue()
            endif()
            set(module_source "${PROJECT_SOURCE_DIR}/src/std/anti/${module}.anti")
            if(NOT EXISTS "${module_source}")
                message(FATAL_ERROR "${file} imports `${found}`, and "
                                    "${module_source} does not exist")
            endif()
            list(APPEND modules "${module}")
            list(APPEND pending "${module_source}")
        endforeach()
    endwhile()
    set(${out} "${modules}" PARENT_SCOPE)
endfunction()

# The three tests of the program tests/matrix/<name>.anti.
# DESIGN: the dev build links the dev object of every module of the
# standard library the program imports, which antic_dev_imports reads from
# the source when the tree is configured. The source is therefore an input
# of the configure step, and a new import configures the tree again.
function(antic_matrix_program name)
    set(source "${MATRIX}/${name}.anti")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    antic_dev_imports(modules "${source}")
    foreach(module IN LISTS modules)
        antic_dev_object(${module})
    endforeach()
    antic_program_test(matrix_${name}_dev "${source}"
        OPTIONS --dev
        DEV ${modules}
        TIMEOUT ${ANTIC_MATRIX_SECONDS}
        WORK "${MATRIX_WORK}/dev")
    antic_program_test(matrix_${name}_release "${source}"
        TIMEOUT ${ANTIC_MATRIX_SECONDS}
        WORK "${MATRIX_WORK}/release")
    antic_program_test(matrix_${name}_memory_checks "${source}"
        OPTIONS --memory-checks
        TIMEOUT ${ANTIC_MATRIX_SECONDS}
        WORK "${MATRIX_WORK}/memory-checks")
    set_tests_properties(matrix_${name}_dev matrix_${name}_release
        matrix_${name}_memory_checks PROPERTIES TIMEOUT ${matrix_program_limit})
    # windows-arm64 has no runtime of AddressSanitizer and refuses the
    # option, which the test memory_checks holds. The test stays
    # registered there, so a line of tests/known-failures.txt names a test
    # on every host.
    if(ANTIC_HOST_TARGET STREQUAL "windows-arm64")
        set_tests_properties(matrix_${name}_memory_checks PROPERTIES
            DISABLED TRUE)
    endif()
endfunction()

# The refusal of tests/matrix/<name>.anti. A message names the file alone,
# so antic runs in the directory of the program, and it runs the whole
# build, so a refusal of any pass counts.
function(antic_matrix_refusal name)
    add_expect_test(matrix_${name}_refused STATUS 1
        TIMEOUT ${ANTIC_MATRIX_SECONDS}
        DIRECTORY "${MATRIX}"
        EXPECTED "${MATRIX}/${name}.err"
        COMMAND "$<TARGET_FILE:antic>" --runtime "${ANTIC_RUNTIME_DIR}"
                -o "${MATRIX_WORK}/refused/${name}" "${name}.anti")
    set_tests_properties(matrix_${name}_refused PROPERTIES
        TIMEOUT ${matrix_refusal_limit})
endfunction()

file(GLOB matrix_sources CONFIGURE_DEPENDS "${MATRIX}/*.anti")
# The expected files decide what a source registers, so a new one
# configures the tree again as a new source does.
file(GLOB matrix_results CONFIGURE_DEPENDS "${MATRIX}/*.expected"
     "${MATRIX}/*.err")
foreach(source IN LISTS matrix_sources)
    get_filename_component(name "${source}" NAME_WE)
    set(expected "${MATRIX}/${name}.expected")
    set(refused "${MATRIX}/${name}.err")
    if(expected IN_LIST matrix_results AND NOT refused IN_LIST matrix_results)
        antic_matrix_program(${name})
    elseif(refused IN_LIST matrix_results AND NOT expected IN_LIST matrix_results)
        antic_matrix_refusal(${name})
    endif()
endforeach()
# R050 of "Rules of 2026-10-13": the Universal C Runtime has no memory above
# the alignment of malloc that `free` releases, so `alloc(T, n)` of a type
# aligned above 16 stands at 16 on a Windows host. The entry on R050 under
# "What the rules run added" in docs/decisions.md holds it. The tests stay
# registered there, as the tests of --memory-checks do on windows-arm64.
if(ANTIC_HOST_TARGET MATCHES "^windows-")
    set_tests_properties(matrix_types_alloc_of_a_type_aligned_above_16_dev
        matrix_types_alloc_of_a_type_aligned_above_16_release
        matrix_types_alloc_of_a_type_aligned_above_16_memory_checks
        PROPERTIES DISABLED TRUE)
endif()
# Programs of the classes area once more under an option that takes a part
# of the class away. --no-reflect drops the field lists, and the default
# `equals` and `hash`, `is`, `as` and `dup` give what they give with them.
# --no-hooks drops the hook sites, and `construct` and `destruct` run as
# they run with them. Each takes the expected output of its matrix tests.
foreach(name equality field_kinds conversions copies
        call_through_a_function_field)
    antic_program_test(matrix_classes_${name}_no_reflect
        "${MATRIX}/classes_${name}.anti"
        OPTIONS "--no-reflect"
        WORK "${MATRIX_WORK}/no-reflect")
endforeach()
foreach(name objects construct_order)
    antic_program_test(matrix_classes_${name}_no_hooks
        "${MATRIX}/classes_${name}.anti"
        OPTIONS "--no-hooks"
        WORK "${MATRIX_WORK}/no-hooks")
endforeach()
# The `changed` hook of the first and the second field of a class and of
# its base, under --trace writes. The matrix runs the program without the
# option, where it prints no line of the hook, so this case has an expected
# file of its own beside those of hook_trace.
antic_program_test(matrix_classes_changed_hook_of_the_second_field_trace_writes
    "${MATRIX}/classes_changed_hook_of_the_second_field.anti"
    OPTIONS "--trace,--trace,writes"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/programs/hook_changed_fields.writes.expected"
    WORK "${MATRIX_WORK}/trace-writes")
# A write to a bitfield of a traced class calls `changed` with the record
# of the bitfield under --trace writes. T114 of
# docs/audit/tests-findings.md.
antic_program_test(matrix_classes_changed_hook_of_a_bitfield_trace_writes
    "${MATRIX}/classes_changed_hook_of_a_bitfield.anti"
    OPTIONS "--trace,--trace,writes"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/programs/hook_changed_bitfield.writes.expected"
    WORK "${MATRIX_WORK}/trace-writes-bitfield")
# A write to a `transient` field of a traced class calls no `changed`,
# since the field list of its descriptor holds no record of the field. The
# write to the field before it calls the hook.
antic_program_test(matrix_classes_changed_hook_of_a_transient_field_trace_writes
    "${MATRIX}/classes_changed_hook_of_a_transient_field.anti"
    OPTIONS "--trace,--trace,writes"
    EXPECTED "${CMAKE_CURRENT_SOURCE_DIR}/programs/hook_changed_transient.writes.expected"
    WORK "${MATRIX_WORK}/trace-writes-transient")
# `join` of a job without a result and `join_all` call the runtime without
# the `joined` hook under --no-hooks.
antic_program_test(matrix_threads_jobs_no_hooks
    "${MATRIX}/threads_jobs.anti"
    OPTIONS "--no-hooks"
    WORK "${MATRIX_WORK}/no-hooks")

# A source with neither file, or with both, registers nothing above.
add_test(NAME matrix_files
    COMMAND "${CMAKE_COMMAND}" "-DMATRIX=${MATRIX}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_matrix_files.cmake")

# The two guards of the test run. Every message of antic and of anti has
# a test that expects it, and every option of antic and every command and
# option of anti a test that passes it. What has none yet stands in
# diagnostics-open.txt and options-open.txt, and a line leaves its list in
# the commit that adds the test. docs/notes/test-matrix.md states both.
add_test(NAME diagnostics_covered
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DOPEN=${CMAKE_CURRENT_SOURCE_DIR}/diagnostics-open.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_diagnostics_covered.cmake")
add_test(NAME diagnostics_covered_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_diagnostics_covered.cmake"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/diagnostics-covered"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_diagnostics_covered_self.cmake")
# options_covered reads the command line of every test of this tree, which
# ctest prints, so it sees an option that a variable of this file holds.
add_test(NAME options_covered
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DOPEN=${CMAKE_CURRENT_SOURCE_DIR}/options-open.txt"
            "-DANTIC=$<TARGET_FILE:antic>" "-DANTI=$<TARGET_FILE:anti>"
            "-DCTEST=${CMAKE_CTEST_COMMAND}" "-DTREE=${CMAKE_BINARY_DIR}"
            "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/options-covered/tree"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_options_covered.cmake")
add_test(NAME options_covered_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_options_covered.cmake"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/options-covered"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_options_covered_self.cmake")

# A runner that prints SKIP: and returns ends with status 0, which ctest
# counts as a pass unless the test sets SKIP_REGULAR_EXPRESSION.
# skips_registered reads every test of the tree and refuses one whose
# runner prints the line without the property.
add_test(NAME skips_registered
    COMMAND "${CMAKE_COMMAND}"
        "-DCTEST=${CMAKE_CTEST_COMMAND}" "-DTREE=${CMAKE_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_skips_registered.cmake")
add_test(NAME skips_registered_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_skips_registered.cmake"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/skips-registered"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_skips_registered_self.cmake")

# antic allocates through src/antic/alloc.c alone, and the front end
# includes no header of the IR for its memory. The programs of tests/ that
# link antic_core, itself or through anti_core, take their memory from
# src/antic/alloc.c as well. The list comes from the targets of this
# directory, so the test stands after every add_executable of the file.
get_property(core_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(core_test_sources "")
foreach(target IN LISTS core_test_targets)
    get_target_property(type ${target} TYPE)
    get_target_property(libraries ${target} LINK_LIBRARIES)
    if(NOT type STREQUAL "EXECUTABLE" OR NOT libraries)
        continue()
    endif()
    if("antic_core" IN_LIST libraries OR "anti_core" IN_LIST libraries)
        get_target_property(sources ${target} SOURCES)
        list(APPEND core_test_sources ${sources})
    endif()
endforeach()
list(JOIN core_test_sources "|" core_test_sources)
add_test(NAME one_allocator
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DUNIT_SOURCES=${core_test_sources}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_one_allocator.cmake")

# The known failures, by decision 30 of "Decisions of 2026-10-11" in
# docs/decisions.md. tests/known-failures.txt lists the tests that show a
# defect, each with the id of its finding in docs/audit/tests-findings.md.
# Every listed test is expected to fail, so one that passes fails the
# suite. This block stands after every add_test of this file, since a
# line may name any of them.
#
# DESIGN: WILL_FAIL is set when the tree is configured, and the test
# known_failures runs later. The list is an input of the configure step, so
# a build configures again after a change of it. The configure step also
# writes the names of the tests and of those it marked into the build tree,
# and known_failures compares them with the list it reads, so a suite that
# runs on a tree configured with another list fails and says why.
include("${CMAKE_CURRENT_SOURCE_DIR}/known_failures.cmake")
set(KNOWN_FAILURES "${CMAKE_CURRENT_SOURCE_DIR}/known-failures.txt")
set(KNOWN_FAILURES_WORK "${CMAKE_CURRENT_BINARY_DIR}/known-failures")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${KNOWN_FAILURES}")
add_test(NAME known_failures
    COMMAND "${CMAKE_COMMAND}"
        "-DLIST=${KNOWN_FAILURES}"
        "-DFINDINGS=${PROJECT_SOURCE_DIR}/docs/audit/tests-findings.md"
        "-DTESTS=${KNOWN_FAILURES_WORK}/tests.txt"
        "-DMARKED=${KNOWN_FAILURES_WORK}/marked.txt"
        "-DSYSTEM=${ANTIC_HOST_OS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_known_failures.cmake")
# run_known_failures.cmake refuses every list that disagrees with the
# table or with the tree.
add_test(NAME known_failures_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_known_failures.cmake"
        "-DWORK=${KNOWN_FAILURES_WORK}/runner"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_known_failures_self.cmake")
known_failures_read("${KNOWN_FAILURES}" known_names known_ids known_hosts
                    known_wrong)
get_property(known_tests DIRECTORY PROPERTY TESTS)
set(known_marked "")
foreach(name systems IN ZIP_LISTS known_names known_hosts)
    known_failures_expected(expected "${systems}" "${ANTIC_HOST_OS}")
    if(expected AND TEST ${name} AND NOT name IN_LIST known_marked)
        set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE)
        list(APPEND known_marked ${name})
    endif()
endforeach()
list(JOIN known_tests "\n" known_tests)
list(JOIN known_marked "\n" known_marked)
file(WRITE "${KNOWN_FAILURES_WORK}/tests.txt" "${known_tests}\n")
file(WRITE "${KNOWN_FAILURES_WORK}/marked.txt" "${known_marked}\n")
