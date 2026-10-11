# Check that llvm-mc, lld, llvm-ar, llvm-readobj, opt, llc, llvm-profdata
# and llvm-cov in LLVM_BIN are the LLVM release of tools/llvm-version, and
# fail otherwise. With DEST set, copy them and llvm-objdump there, without
# llvm-cov, which is how the runtime archive carries its eight pinned
# tools. Run with cmake -P and these values:
#   LLVM_BIN         the directory of the installed tools
#   DEST             optional directory of the runtime archive's tools
#   RUNTIME_ARCHIVE  set when LLVM_BIN is bin/ of the runtime archive
#
# DESIGN: llvm-cov is a tool of the build alone. It measures what the tests
# of this tree reach, and no package carries it, which Eddie decided on
# 2026-10-11 as decision 29 of docs/decisions.md. tools/pack-anti.cmake
# copies bin/ of the runtime archive whole into the package of this
# machine. So the copy below leaves llvm-cov out, and RUNTIME_ARCHIVE
# refuses one that stands there. The tests toolchain_pin and
# no_llvm_cov_packed hold it.

cmake_minimum_required(VERSION 3.21)

file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" pin)
string(STRIP "${pin}" pin)
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
set(checked llvm-mc llvm-ar llvm-readobj ld.lld ld64.lld lld-link opt llc
    llvm-profdata)
if(RUNTIME_ARCHIVE)
    if(EXISTS "${LLVM_BIN}/llvm-cov${exe}")
        message(FATAL_ERROR "${LLVM_BIN} holds llvm-cov${exe}, and no package "
                            "carries llvm-cov")
    endif()
else()
    list(APPEND checked llvm-cov)
endif()
set(failed "")
foreach(tool IN LISTS checked)
    execute_process(COMMAND "${LLVM_BIN}/${tool}${exe}" --version
        OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" found "${out}${err}")
    if(found STREQUAL pin)
        message(STATUS "${tool} ${found}")
    else()
        string(APPEND failed "${tool} has version '${found}', the pin is ${pin}\n")
    endif()
endforeach()
if(NOT failed STREQUAL "")
    message(FATAL_ERROR "${failed}")
endif()
if(DEFINED DEST)
    file(MAKE_DIRECTORY "${DEST}")
    foreach(tool llvm-mc llvm-ar llvm-objdump llvm-readobj lld ld.lld ld64.lld
            lld-link opt llc llvm-profdata)
        if(EXISTS "${LLVM_BIN}/${tool}${exe}")
            file(COPY "${LLVM_BIN}/${tool}${exe}" DESTINATION "${DEST}")
        endif()
    endforeach()
    file(WRITE "${DEST}/llvm-version" "${pin}\n")
endif()
