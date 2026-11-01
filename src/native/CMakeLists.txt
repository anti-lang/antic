# Builds the third-party libraries of the runtime archive as static
# libraries, one per target, from the pinned sources with the pinned clang.
# CMakeLists.txt at the top adds this directory after the cross builds of
# anti_rt and the tests. The targets, the sysroots of the runtime tree, the
# processor levels and llvm-ar come from there.
# Libraries are added in the order of docs/decisions.md: PCRE2, then Mbed
# TLS, miniaudio and raylib, with SQLite between PCRE2 and Mbed TLS as
# docs/anti-language-additions.md places it. media.cmake holds what the
# last two share. mimalloc, the C allocator of the musl targets, comes
# last.
#
# DESIGN: this is a subdirectory of the build of antic and no project of
# its own, so one configure builds every target that has a sysroot, as the
# cross builds of anti_rt do. See "Libraries and runtime" in docs/decisions.md.

# DESIGN: every native library of the runtime archive is built for the
# default level of its target, as docs/anti-language-additions.md settles
# under "CPU levels", and lies in lib/<target>/ beside the level
# directories of anti_rt. The levels come from tools/cpu-levels through
# antic_cpu_level and antic_cpu_march of the top-level file.

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "Native libraries: skipped, they need clang")
    return()
endif()

# DESIGN: every compile of a native library takes
# ANTIC_NATIVE_FP_OPTIONS, on every target and at every level it is built
# for. -ffp-contract=off fuses no multiply and add into one instruction,
# so a library gives the same results everywhere, as antic does, which
# never fuses them. Eddie decided this by Q7 of "Answers of 2026-11-01"
# in docs/decisions.md. It is no warning flag, so warnings.cmake does not
# hold it. antic_native_target and antic_media_target of media.cmake add
# it to the flags of a target, which the probes and the glue of
# anti.regex share. The test native_fp_contract reads every compile.
set(ANTIC_NATIVE_FP_OPTIONS -ffp-contract=off)

# The target triple and the flags of a compile for <target>, with the
# headers of its sysroot in the runtime tree: those of antic_target_triple
# and antic_target_c_options of tools/target-compile.cmake, which the cross
# builds of anti_rt take for the same target, and the -march of the
# default level.
#
# The code options of antic_linux_code_options of the top-level file go
# with them, as the runtime of the same target takes them, and
# ANTIC_NATIVE_FP_OPTIONS.
function(antic_native_target out_triple out_flags target)
    antic_target_triple(triple "${target}")
    antic_target_c_options(flags "${target}"
        "${ANTIC_RUNTIME_DIR}/sysroot/${target}" "${ANTIC_CLANG_RESOURCE_DIR}")
    antic_cpu_level(level "${target}")
    antic_cpu_march(march "${level}")
    antic_linux_code_options(code_options "${target}")
    list(APPEND flags "${march}" ${code_options} ${ANTIC_NATIVE_FP_OPTIONS})
    set(${out_triple} "${triple}" PARENT_SCOPE)
    set(${out_flags} "${flags}" PARENT_SCOPE)
endfunction()

# The two tests of a native library on <target>. <link>_<target> links
# SOURCE with OBJECTS for the target through antic, and on the host the
# test <run> also runs the program. Both work below WORK, and OPTIONS,
# separated by commas, go to antic.
function(antic_native_tests run link target)
    # OPTIONS is a list keyword, since the options of a target may be
    # empty, which a keyword of one value refuses with a warning.
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "SOURCE;WORK" "OBJECTS;OPTIONS")
    list(JOIN arg_OBJECTS "," objects)
    set(common "-DANTIC=$<TARGET_FILE:antic>" "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
        "-DSOURCE=${arg_SOURCE}" "-DOBJECTS=${objects}")
    if(NOT "${arg_OPTIONS}" STREQUAL "")
        list(APPEND common "-DOPTIONS=${arg_OPTIONS}")
    endif()
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME ${run}
            COMMAND "${CMAKE_COMMAND}" ${common} "-DWORK=${arg_WORK}/run"
                -P "${ANTIC_RUNNERS}/run_program.cmake")
    endif()
    add_test(NAME ${link}_${target}
        COMMAND "${CMAKE_COMMAND}" ${common} "-DWORK=${arg_WORK}/link"
            "-DTARGET=${target}"
            -P "${ANTIC_RUNNERS}/run_native_link.cmake")
endfunction()

# Copy the licence text <source> of the native library <name> to
# licenses/<name>.txt of the runtime tree, which the packer and anti
# license read, and note the name for the test native_licenses.
function(antic_native_license name source)
    configure_file("${source}" "${ANTIC_RUNTIME_DIR}/licenses/${name}.txt"
        COPYONLY)
    set_property(GLOBAL APPEND PROPERTY ANTIC_NATIVE_LICENSES "${name}")
endfunction()

# Copy the headers <file>... of the native library <name>, each a path
# below <base>, to include/<name>/ of the runtime tree under the same path,
# which the packer copies. A C program and anti bind --clang read them
# there.
# DESIGN: the headers of each library are the ones its own build installs,
# and include/<name>/ is the directory a C compile names with -I, so a
# header keeps the directory its includes name: mbedtls/ and psa/ of Mbed
# TLS. Eddie decided include/<library>/ on 2026-10-08 in
# docs/work-order-distribution.md.
function(antic_native_headers name base)
    foreach(file IN LISTS ARGN)
        configure_file("${base}/${file}"
            "${ANTIC_RUNTIME_DIR}/include/${name}/${file}" COPYONLY)
    endforeach()
endfunction()

# Note that the native library <name> compiles one object for <target>
# with the command <argument>..., which the test native_fp_contract reads.
# <target> may name a level after a `/`.
function(antic_native_compiled name target)
    list(JOIN ARGN " " command)
    set_property(GLOBAL APPEND PROPERTY ANTIC_NATIVE_COMPILES
        "${name}|${target}|${command}")
endfunction()

set(ANTIC_NATIVE_TARGETS "")
foreach(target IN LISTS ANTIC_TARGETS)
    if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/${target}")
        list(APPEND ANTIC_NATIVE_TARGETS "${target}")
    endif()
endforeach()

include(pcre2.cmake)
include(sqlite.cmake)
include(mbedtls.cmake)
include(media.cmake)
include(miniaudio.cmake)
include(raylib.cmake)
include(mimalloc.cmake)

# LICENSES/ of the repository holds the licence text of every library
# above, the same bytes the build copies from its pinned release.
get_property(antic_native_licenses GLOBAL PROPERTY ANTIC_NATIVE_LICENSES)
list(JOIN antic_native_licenses "," antic_native_licenses)
add_test(NAME native_licenses
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DNAMES=${antic_native_licenses}"
            -P "${ANTIC_RUNNERS}/run_native_licenses.cmake")

# Every compile of every library above carries -ffp-contract=off, by Q7 of
# "Answers of 2026-11-01" in docs/decisions.md. The configure step writes
# the commands, one per line, and the test reads them.
get_property(antic_native_compiles GLOBAL PROPERTY ANTIC_NATIVE_COMPILES)
list(JOIN antic_native_compiles "\n" antic_native_compiles)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/compiles.txt"
    "${antic_native_compiles}\n")
list(JOIN ANTIC_NATIVE_TARGETS "," antic_native_target_list)
add_test(NAME native_fp_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILES=${CMAKE_CURRENT_BINARY_DIR}/compiles.txt"
            "-DNAMES=${antic_native_licenses}"
            "-DTARGETS=${antic_native_target_list}"
            -P "${ANTIC_RUNNERS}/run_native_fp_contract.cmake")
