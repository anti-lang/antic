# The coverage measurement: which lines of antic, anti and the runtime the
# suite reaches, by the source-based coverage of the pinned clang. The
# preset coverage of CMakePresets.json turns ANTIC_COVERAGE on, and
# ./c coverage builds that tree, runs its suite and writes the report.
# tools/coverage-report.cmake merges the profiles and runs llvm-cov.
# CMakeLists.txt includes this file.
#
# The measurement is no gate. No other tree takes the flags, and no test
# reads a number of the report.
option(ANTIC_COVERAGE
       "Instrument antic, anti and the unit tests for the coverage measurement"
       OFF)

# The flags of a link under the measurement, which takes the profile
# runtime of the pinned clang.
set(ANTIC_COVERAGE_LINK_FLAGS -fprofile-instr-generate)

# The flags of a compile under the measurement, for sources under root.
#
# DESIGN: the coverage mapping names each file from the directory of its
# compile, which is the build tree and differs per directory of the build.
# The build maps the repository to a dot as well, so llvm-cov read
# build/coverage/src/antic/lexer.c and build/coverage/tests/src/rt/utf.c,
# two files that do not exist. With root as the directory of every
# compile and the map of root to a dot, a file is ./src/antic/lexer.c
# whoever compiled it, no program holds a path of this machine, and
# tools/coverage-report.cmake runs llvm-cov in root.
function(antic_coverage_compile_flags out root)
    set(${out} -fprofile-instr-generate -fcoverage-mapping
        "-fcoverage-compilation-dir=${root}" "-ffile-prefix-map=${root}=."
        PARENT_SCOPE)
endfunction()

# Where the tests of a coverage tree leave their raw profiles, and the
# pattern of a file there. %m is the signature of the program, so each
# program has files of its own, and the 9 keeps nine of them per program.
# A run adds its counts to one of the nine under a lock of that file, so
# the directory stays small however many times the suite starts antic,
# and the tests of ctest -j wait on nine files and not on one.
# The build itself runs antic on the standard library, with no variable
# set, and that antic leaves default.profraw in the build tree, which no
# report reads: the measurement is of the suite.
set(ANTIC_COVERAGE_PROFILES "${CMAKE_BINARY_DIR}/profiles")
set(ANTIC_COVERAGE_PATTERN "${ANTIC_COVERAGE_PROFILES}/%9m.profraw")

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
        target_compile_options(${target} PRIVATE ${compile_flags})
        target_link_options(${target} PUBLIC ${ANTIC_COVERAGE_LINK_FLAGS})
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "EXECUTABLE")
            set_property(GLOBAL APPEND PROPERTY ANTIC_COVERAGE_PROGRAMS
                         ${target})
        endif()
    endforeach()
endfunction()

# The two targets of a coverage tree and the environment of its tests.
# coverage_clear removes the profiles of an earlier run, and
# coverage_report writes the report from the profiles on disk. Neither
# runs a test, so a suite that ran in parts is reported once, after its
# last part. coverage_report names the programs, so it builds one that is
# out of date first, and the profiles of the build before no longer fit
# it. CMakeLists.txt calls this after the last directory that instruments
# a target.
#
# DESIGN: ctest reads the file of TEST_INCLUDE_FILES before it starts a
# test, and the file sets LLVM_PROFILE_FILE for all of them. Every run of
# ctest in the tree then collects, a part of ctest -I as much as the whole
# suite, and the directory has this one definition. A test that sets the
# variable for a program it built itself, as profile_guided does, still
# sets it for that program.
function(antic_coverage_finish)
    if(NOT ANTIC_COVERAGE)
        return()
    endif()
    set(environment "${CMAKE_BINARY_DIR}/coverage-environment.cmake")
    file(WRITE "${environment}"
         "set(ENV{LLVM_PROFILE_FILE} \"${ANTIC_COVERAGE_PATTERN}\")\n")
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
                 TEST_INCLUDE_FILES "${environment}")

    get_property(programs GLOBAL PROPERTY ANTIC_COVERAGE_PROGRAMS)
    set(objects "")
    foreach(program IN LISTS programs)
        list(APPEND objects "$<TARGET_FILE:${program}>")
    endforeach()
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
endfunction()
