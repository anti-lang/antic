# Refuse a Linux program that needs a glibc newer than the pinned sysroot.
#
#   cmake -DBINARY=<file> -DREADOBJ=<llvm-readobj> [-DMAX_GLIBC=2.35]
#         -P tools/check-libc.cmake
#   cmake -DLISTING=<file> [-DMAX_GLIBC=2.35] -P tools/check-libc.cmake
#
# DESIGN: a release links the pinned sysroot and never the libc of the
# machine that built it: musl for the static form, glibc 2.35 and the
# kernel headers of Ubuntu 22.04 for the dynamic one. The shipped antic
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
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED MAX_GLIBC)
    set(MAX_GLIBC "2.35")
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
                        "[-DMAX_GLIBC=2.35] -P tools/check-libc.cmake")
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
