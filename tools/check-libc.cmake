# Refuse a Linux program that needs a glibc newer than the pinned sysroot.
#
#   cmake -DBINARY=<file> -DREADOBJ=<llvm-readobj> [-DMAX_GLIBC=<version>]
#         -P tools/check-libc.cmake
#   cmake -DLISTING=<file> [-DMAX_GLIBC=<version>] -P tools/check-libc.cmake
#
# MAX_GLIBC is the glibc of tools/sysroot-pins without it, the version of
# the package GLIBC_X86_64_LIBC names.
#
# DESIGN: a release links the pinned sysroot and never the libc of the
# machine that built it: musl for the static form, the glibc and the
# kernel headers of Ubuntu 22.04 of tools/sysroot-pins for the dynamic
# one. The shipped antic
# then runs on any Linux from Ubuntu 22.04 on. A program linked against
# the builder's glibc instead names the symbol versions of that machine,
# and it refuses to start on an older one with a message about a version
# of GLIBC that the library does not have. Nothing about the build says
# so, and the tarball is already published by the time a user finds out.
# The binary itself says it, in the versioned symbols of its dynamic
# section, so that is what this reads.
#
# A static musl program names no version at all and passes with nothing to
# report. LISTING reads a saved listing instead of running READOBJ, for the
# test that shows a refusal, since no program of this tree needs a glibc.
cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/pins.cmake")
if(NOT DEFINED MAX_GLIBC)
    # g/glibc/libc6_2.35-0ubuntu3_amd64.deb is the package of glibc 2.35.
    antic_pin_value(libc "${CMAKE_CURRENT_LIST_DIR}/sysroot-pins"
                    GLIBC_X86_64_LIBC)
    if(NOT libc MATCHES "/[^_/]+_([0-9.]+)-[^_/]+_[^_/]+\\.deb$")
        message(FATAL_ERROR "check-libc: GLIBC_X86_64_LIBC of "
                            "tools/sysroot-pins is `${libc}`, which names no "
                            "version of glibc")
    endif()
    set(MAX_GLIBC "${CMAKE_MATCH_1}")
endif()
if(DEFINED LISTING)
    file(READ "${LISTING}" versions)
elseif(DEFINED BINARY AND DEFINED READOBJ)
    if(NOT EXISTS "${BINARY}")
        message(FATAL_ERROR "check-libc: no such file ${BINARY}")
    endif()
    execute_process(COMMAND "${READOBJ}" -V "${BINARY}"
                    RESULT_VARIABLE status
                    OUTPUT_VARIABLE versions
                    ERROR_VARIABLE err
                    ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR
            "check-libc: ${READOBJ} -V failed on ${BINARY}\n${err}")
    endif()
else()
    message(FATAL_ERROR "usage: cmake -DBINARY=<file> -DREADOBJ=<tool> "
                        "[-DMAX_GLIBC=<version>] -P tools/check-libc.cmake")
endif()

string(REGEX MATCHALL "GLIBC_[0-9]+\\.[0-9]+(\\.[0-9]+)?" found "${versions}")
set(over "")
foreach(name IN LISTS found)
    string(REPLACE "GLIBC_" "" version "${name}")
    if(version VERSION_GREATER "${MAX_GLIBC}")
        list(APPEND over "${name}")
    endif()
endforeach()
list(REMOVE_DUPLICATES over)
if(NOT over STREQUAL "")
    list(SORT over)
    string(REPLACE ";" ", " printed "${over}")
    message(FATAL_ERROR
        "check-libc: ${BINARY}${LISTING} needs ${printed}, above the pinned "
        "glibc ${MAX_GLIBC}. It was linked against the libc of the machine "
        "that built it, not the sysroot of tools/get-sysroot.cmake, so it "
        "will not start on Ubuntu 22.04.")
endif()
