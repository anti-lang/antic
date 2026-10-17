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
# DESIGN: llvm-cov is a tool of the build alone. It measures what the tests
# of this tree reach, and no package carries it, which Eddie decided on
# 2026-10-11 as decision 29 of docs/decisions.md. tools/pack-anti.cmake
# copies bin/ of the runtime archive whole into the package of this
# machine. So the copy leaves llvm-cov out, and RUNTIME_ARCHIVE refuses one
# that stands there. The tests toolchain_pin and no_llvm_cov_packed hold
# it.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/pinned-tools.cmake")
file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" pin)
string(STRIP "${pin}" pin)
set(os linux)
set(exe "")
if(CMAKE_HOST_WIN32)
    set(os windows)
    set(exe ".exe")
endif()
if(RUNTIME_ARCHIVE)
    if(EXISTS "${LLVM_BIN}/llvm-cov${exe}")
        message(FATAL_ERROR "${LLVM_BIN} holds llvm-cov${exe}, and no package "
                            "carries llvm-cov")
    endif()
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
