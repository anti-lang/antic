# Check that the LLVM tools in LLVM_BIN are the LLVM release of
# tools/llvm-version, and fail otherwise. The tools are those of
# tools/pinned-tools.cmake: the nine of ANTIC_LLVM_TOOLS and the names of
# lld in a directory that tools/get-llvm.cmake installed, and those of
# antic_llvm_archive_tools in bin/ of the runtime archive. The target
# llvm_tools of CMakeLists.txt runs it before tools/copy-llvm.cmake, which
# lays out that directory. Run with cmake -P and these values:
#   LLVM_BIN         the directory of the installed tools
#   RUNTIME_ARCHIVE  set when LLVM_BIN is bin/ of the runtime archive
#
# DESIGN: llvm-cov and llvm-mc, ANTIC_LLVM_UNPACKED, are tools of the
# build alone. llvm-cov measures what the tests of this tree reach, and
# llvm-mc assembles the output of `antic -S` for the tests. No package
# carries either, which Eddie decided on 2026-10-11 as decisions 29 and 2
# of docs/decisions.md.
# tools/pack-anti.cmake copies bin/ of the runtime archive whole into the
# package of this machine. So the copy leaves both out, and RUNTIME_ARCHIVE
# refuses one that stands there. The tests toolchain_pin,
# no_llvm_cov_packed and no_llvm_mc_packed hold it.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/pinned-tools.cmake")
file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" pin)
string(STRIP "${pin}" pin)
include("${CMAKE_CURRENT_LIST_DIR}/host.cmake")
antic_machine_host(machine)
string(REGEX REPLACE "-.*$" "" os "${machine}")
antic_executable_suffix(exe "${machine}")
if(RUNTIME_ARCHIVE)
    foreach(tool IN LISTS ANTIC_LLVM_UNPACKED)
        if(EXISTS "${LLVM_BIN}/${tool}${exe}")
            message(FATAL_ERROR "${LLVM_BIN} holds ${tool}${exe}, and no "
                                "package carries ${tool}")
        endif()
    endforeach()
    antic_llvm_archive_tools(names)
else()
    set(names ${ANTIC_LLVM_TOOLS} ${ANTIC_LLD_NAMES})
endif()
antic_llvm_tool_problems(problems "${LLVM_BIN}" "${pin}" "${os}" ${names})
if(problems)
    list(JOIN problems "\n" text)
    message(FATAL_ERROR "${text}")
endif()
list(LENGTH names count)
message(STATUS "${count} LLVM tools of ${pin} in ${LLVM_BIN}")
