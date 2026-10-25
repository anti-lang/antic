# The coverage measurement: which lines of antic, anti and the runtime the
# suite reaches, by the source-based coverage of the pinned clang. The
# preset coverage of CMakePresets.json turns ANTIC_COVERAGE on, and
# ./c coverage builds that tree, runs its suite and writes the report.
# tools/coverage-report.cmake merges the profiles and runs llvm-cov.
# CMakeLists.txt includes this file.
#
# The measurement is no gate. No other tree takes the flags, and no test
# reads a number of the report. A coverage tree instruments the runtime
# of the host's system as well, see antic_coverage_runtime below.
option(ANTIC_COVERAGE
       "Instrument antic, anti, the unit tests and the runtime for coverage"
       OFF)

# The flags of a link under the measurement, which takes the profile
# runtime of the pinned clang.
set(ANTIC_COVERAGE_LINK_FLAGS -fprofile-instr-generate)

# The flags of a compile under the measurement, for sources under root.
#
# A compile under the measurement defines ANTI_COVERAGE as well. The seam
# of src/rt/fault.h then has the same shape without ANTI_FAULTS as with
# it, so the counts of antic and antic_faults, and of the runtime and the
# runtime of the fault tests, merge into one record per function. llvm-cov
# reads one record of a function, and the counts of the fault tests were
# lost before. src/rt/assert.c writes the counts of a program before it
# aborts.
#
# DESIGN: the coverage mapping names each file from the directory of its
# compile, which is the build tree and differs per directory of the build.
# The build maps the repository to a dot as well, so llvm-cov read
# build/coverage/src/antic/lexer.c and build/coverage/tests/src/rt/utf.c,
# two files that do not exist. With root as the directory of every
# compile and the map of root to a dot, a file is ./src/antic/lexer.c
# whoever compiled it, and tools/coverage-report.cmake runs llvm-cov in
# root.
function(antic_coverage_compile_flags out root)
    set(${out} -fprofile-instr-generate -fcoverage-mapping
        "-fcoverage-compilation-dir=${root}" "-ffile-prefix-map=${root}=."
        -DANTI_COVERAGE=1 PARENT_SCOPE)
endfunction()

# Where the tests of a coverage tree leave their raw profiles, and the
# two patterns of a file there, which the compile of an instrumented
# file names, so that a run writes its profile wherever it starts.
#
# ANTIC_COVERAGE_PATTERN is that of antic, anti and the unit tests. %m is
# the signature of the program, so each program has files of its own,
# and the 9 keeps nine of them per program. A run adds its counts to one
# of the nine under a lock of that file, so the directory stays small
# however many times the suite starts antic, and the tests of ctest -j
# wait on nine files and not on one.
#
# ANTIC_COVERAGE_RUNTIME_PATTERN is that of the runtime inside an Anti
# program. %p is the process, so every run of a program writes a file of
# its own. %1m merges a second process of the same number into that file
# when the signature is the same, and gives it a file of its own when the
# signature differs.
#
# DESIGN: the programs of the suite take no %m pool. The signature of %m
# is a sum of the sizes of the profile and the name of its first
# function, and two programs that link the same runtime functions in
# another order have the same signature and records that do not merge.
# The program that came second then wrote three errors of the profile
# runtime to its standard error and no profile, and trap_place_sorted,
# which compares that stream, failed. A file per process costs a few
# kilobytes per run of a program, since the profile holds the runtime
# functions the program links and no more.
#
# DESIGN: a compile names the pattern, and the tests set no
# LLVM_PROFILE_FILE. The pattern per compile replaced the variable, which
# ctest set for every test of the tree, and Q12 of "Answers of
# 2026-10-24" in docs/decisions.md keeps it. The variable reaches every
# process of a test, antic and the program alike, and a program has to
# write elsewhere than antic: antic takes a pool of nine files and a
# program a file per process, and one variable gives both one pattern. A test that sets the variable for a program it
# built itself, as profile_guided does, still sets it for that program,
# and every run collects, a part of ctest -I as much as the whole suite.
# The build itself runs antic on the standard library and leaves
# profiles there too, which coverage_clear removes before the suite runs.
set(ANTIC_COVERAGE_PROFILES "${CMAKE_BINARY_DIR}/profiles")
set(ANTIC_COVERAGE_PATTERN "${ANTIC_COVERAGE_PROFILES}/%9m.profraw")
set(ANTIC_COVERAGE_RUNTIME_PATTERN
    "${ANTIC_COVERAGE_PROFILES}/program-%p-%1m.profraw")

# Instrument each target of the arguments in a coverage tree, and do
# nothing in any other. A program among them is one that the report reads.
#
# DESIGN: the flags go on the targets named here and never into
# CMAKE_C_FLAGS, where the flags of a sanitizer stand. tests/ compiles
# objects with the compiler of the build that an Anti program links, and
# such a program links no profile runtime. The link flag is PUBLIC, so a
# program of tests/ that links antic_core or anti_core takes the profile
# runtime that their objects call.
function(antic_coverage)
    if(NOT ANTIC_COVERAGE)
        return()
    endif()
    antic_coverage_compile_flags(compile_flags "${CMAKE_SOURCE_DIR}")
    foreach(target IN LISTS ARGN)
        target_compile_options(${target} PRIVATE ${compile_flags}
            "-fprofile-instr-generate=${ANTIC_COVERAGE_PATTERN}")
        target_link_options(${target} PUBLIC ${ANTIC_COVERAGE_LINK_FLAGS})
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "EXECUTABLE")
            set_property(GLOBAL APPEND PROPERTY ANTIC_COVERAGE_PROGRAMS
                         ${target})
        endif()
    endforeach()
endfunction()

# The two targets of a coverage tree.
# coverage_clear removes the profiles of an earlier run, and
# coverage_report writes the report from the profiles on disk. Neither
# runs a test, so a suite that ran in parts is reported once, after its
# last part. coverage_report names the programs and the objects of the
# runtime, so it builds one that is out of date first, and the profiles
# of the build before no longer fit it. CMakeLists.txt calls this after
# the last directory that instruments a target.
function(antic_coverage_finish)
    if(NOT ANTIC_COVERAGE)
        return()
    endif()
    get_property(programs GLOBAL PROPERTY ANTIC_COVERAGE_PROGRAMS)
    set(objects "")
    foreach(program IN LISTS programs)
        list(APPEND objects "$<TARGET_FILE:${program}>")
    endforeach()
    get_property(runtime GLOBAL PROPERTY ANTIC_COVERAGE_RUNTIME_OBJECTS)
    list(APPEND objects ${runtime})
    list(JOIN objects "," objects)
    add_custom_target(coverage_clear
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ANTIC_COVERAGE_PROFILES}"
        VERBATIM)
    add_custom_target(coverage_report
        COMMAND "${CMAKE_COMMAND}"
                "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                "-DROOT=${CMAKE_SOURCE_DIR}"
                "-DPROFILES=${ANTIC_COVERAGE_PROFILES}"
                "-DOUT=${CMAKE_BINARY_DIR}"
                "-DOBJECTS=${objects}"
                -P "${CMAKE_SOURCE_DIR}/tools/coverage-report.cmake"
        VERBATIM)
    get_property(builds GLOBAL PROPERTY ANTIC_COVERAGE_RUNTIME_BUILDS)
    if(builds)
        add_dependencies(coverage_report ${builds})
    endif()
endfunction()

# The runtime of a coverage tree, so that the measurement reaches src/rt/
# inside the programs of the suite as well.
#
# DESIGN: the runtime of each target of the host's system, the targets
# whose programs the suite runs, compiles with the flags above, its
# objects and its bitcode alike, at every level, and so does the glue of
# anti.regex in src/native/pcre2.cmake. The library of the runtime and
# its bitcode archives then take the members of the profile runtime of
# the pinned clang, which writes the counts of a program at its exit. antic
# names the profile runtime in a link of --profile-generate alone, so
# the members travel inside the library, and every program of the suite
# links them without an option of its own. The runtime of every other
# target, and every runtime of a tree that is no coverage tree, stays
# as it is, so the archive of a normal build and of a package does not
# change.
#
# antic_coverage_runtime(<flags> <members> <target> <profile runtime>)
# sets <flags> to the compile flags of the runtime of <target> and
# <members> to the objects of <profile runtime> that its archives take,
# both empty outside a coverage tree and for a target of another system.
function(antic_coverage_runtime flags_out members_out target profile)
    set(${flags_out} "" PARENT_SCOPE)
    set(${members_out} "" PARENT_SCOPE)
    if(NOT ANTIC_COVERAGE OR NOT target MATCHES "^${ANTIC_HOST_OS}-")
        return()
    endif()
    if(profile STREQUAL "" OR NOT EXISTS "${profile}")
        message(FATAL_ERROR "The coverage tree instruments the runtime of "
            "${target}, which needs the profile runtime of the pinned clang, "
            "and the clang in ${ANTIC_CLANG_RESOURCE_DIR} has none for it.")
    endif()
    antic_coverage_compile_flags(flags "${CMAKE_SOURCE_DIR}")
    list(APPEND flags
         "-fprofile-instr-generate=${ANTIC_COVERAGE_RUNTIME_PATTERN}")
    get_property(members GLOBAL PROPERTY ANTIC_COVERAGE_MEMBERS_${target})
    if(NOT members)
        set(dir "${CMAKE_BINARY_DIR}/coverage-profile/${target}")
        file(REMOVE_RECURSE "${dir}")
        file(MAKE_DIRECTORY "${dir}")
        antic_coverage_slice(archive "${profile}" "${target}" "${dir}")
        execute_process(COMMAND "${ANTIC_LLVM_AR}" x "--output=${dir}/members"
                "${archive}"
            RESULT_VARIABLE status ERROR_VARIABLE errors)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "llvm-ar x ${archive} gave ${status}\n${errors}")
        endif()
        file(GLOB members "${dir}/members/*")
        list(SORT members)
        set_property(GLOBAL PROPERTY ANTIC_COVERAGE_MEMBERS_${target}
                     ${members})
    endif()
    set(${flags_out} ${flags} PARENT_SCOPE)
    set(${members_out} ${members} PARENT_SCOPE)
endfunction()

# The archive of the processor of <target> inside <file>, in <out>. The
# profile runtime of macOS is one universal file for both processors,
# which llvm-ar does not read, so the slice of the processor goes to a
# file of its own in <dir>. Any other file is the archive already.
#
# DESIGN: the slice is a range of the bytes of the file. The pinned
# release holds no llvm-lipo, a tool of the host is refused and CMake
# writes no byte of value zero, so two pinned tools copy the range: the
# directive .incbin of llvm-mc puts it into the one section of an ELF
# object, and ld.lld with --oformat binary writes the bytes of that
# section alone.
function(antic_coverage_slice out file target dir)
    file(READ "${file}" magic HEX LIMIT 8)
    if(NOT magic MATCHES "^cafebabe")
        set(${out} "${file}" PARENT_SCOPE)
        return()
    endif()
    # The cputype of fat_arch in <mach/machine.h>.
    if(target MATCHES "arm64")
        set(cpu 0100000c)
    else()
        set(cpu 01000007)
    endif()
    string(SUBSTRING "${magic}" 8 8 count)
    math(EXPR count "0x${count}")
    foreach(index RANGE 1 ${count})
        # A fat_arch is five big-endian words after the header of two:
        # cputype, cpusubtype, offset, size and align.
        math(EXPR at "8 + (${index} - 1) * 20")
        file(READ "${file}" entry HEX OFFSET ${at} LIMIT 20)
        string(SUBSTRING "${entry}" 0 8 type)
        if(NOT type STREQUAL cpu)
            continue()
        endif()
        string(SUBSTRING "${entry}" 16 8 offset)
        string(SUBSTRING "${entry}" 24 8 size)
        math(EXPR offset "0x${offset}")
        math(EXPR size "0x${size}")
        file(WRITE "${dir}/slice.s"
             ".section .slice,\"a\"\n.incbin \"${file}\", ${offset}, ${size}\n")
        set(slice "${dir}/slice.a")
        get_filename_component(bin "${ANTIC_LLVM_MC}" DIRECTORY)
        antic_executable_suffix(exe "${ANTIC_HOST_TARGET}")
        execute_process(
            COMMAND "${ANTIC_LLVM_MC}" -triple x86_64-unknown-linux-gnu
                    -filetype=obj "${dir}/slice.s" -o "${dir}/slice.o"
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(
            COMMAND "${bin}/ld.lld${exe}" --oformat binary
                    -o "${slice}" "${dir}/slice.o"
            COMMAND_ERROR_IS_FATAL ANY)
        set(${out} "${slice}" PARENT_SCOPE)
        return()
    endforeach()
    message(FATAL_ERROR "${file} holds no slice for ${target}")
endfunction()

# Name the objects of the runtime of <target> at <level>, built by the
# target <build>, as programs of the report. The runtime of the host at
# its default level stands for all of them, since the levels and the
# bitcode compile one source with the flags above, which gives each
# function the same record. The glue of anti.regex stands at the default
# level of its target alone.
function(antic_coverage_runtime_objects target level build)
    if(NOT ANTIC_COVERAGE OR NOT target STREQUAL ANTIC_HOST_TARGET
       OR NOT level STREQUAL ANTIC_CPU_LEVEL_${target})
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY ANTIC_COVERAGE_RUNTIME_OBJECTS ${ARGN})
    set_property(GLOBAL APPEND PROPERTY ANTIC_COVERAGE_RUNTIME_BUILDS ${build})
endfunction()
