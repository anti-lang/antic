# TOOL, a tool of the build that no package carries, reaches no directory
# that the packer copies into a package. Run with cmake -P and these
# values:
#   ROOT  the root of the repository
#   TOOL  a tool of ANTIC_LLVM_UNPACKED of tools/pinned-tools.cmake
#   WORK  a directory this run writes into
#
# DESIGN: llvm-cov measures the tests of this tree, and llvm-mc assembles
# the output of `antic -S` for the tests. No package carries either, which
# Eddie decided on 2026-10-11 as decisions 29 and 2 of docs/decisions.md.
# tools/pack-anti.cmake copies a whole bin/ into a package: bin/ of the
# runtime archive for this machine, and the directory that
# tools/get-llvm.cmake -DHOST=<host> lays out for every other host. The
# tools archive of a host holds both, so each of the two has to keep them
# out.
#
# The tools here are stand-ins of a few bytes, and .installed names the
# digest of the pin, so tools/get-llvm.cmake downloads nothing.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/pins.cmake")

file(REMOVE_RECURSE "${WORK}")

# The directory of another host. One Windows host stands for both, since
# its tools carry a suffix.
foreach(host linux-arm64 windows-x86_64)
    set(exe "")
    if(host MATCHES "^windows-")
        set(exe ".exe")
    endif()
    antic_pin_value(digest "${ROOT}/tools/llvm-pin" "${host}-digest")
    set(dest "${WORK}/${host}")
    file(MAKE_DIRECTORY "${dest}/bin")
    file(WRITE "${dest}/.installed" "${digest}\n")
    foreach(tool lld llvm-ar ${TOOL})
        file(WRITE "${dest}/bin/${tool}${exe}" "stand-in ${tool} of ${host}\n")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DHOST=${host}"
                            "-DDEST=${dest}"
                            -P "${ROOT}/tools/get-llvm.cmake"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err
                    RESULT_VARIABLE status ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT "${out}${err}" MATCHES "is installed in")
        message(FATAL_ERROR "tools/get-llvm.cmake -DHOST=${host} gave "
                            "${status} on the stand-in tree:\n${out}${err}")
    endif()
    # The script ran to its end, and the tools the package carries stayed.
    foreach(name llvm-version "llvm-ar${exe}" "ld.lld${exe}")
        if(NOT EXISTS "${dest}/bin/${name}")
            message(FATAL_ERROR "tools/get-llvm.cmake -DHOST=${host} left no "
                                "bin/${name}")
        endif()
    endforeach()
    if(EXISTS "${dest}/bin/${TOOL}${exe}")
        message(FATAL_ERROR "tools/get-llvm.cmake -DHOST=${host} left "
                            "bin/${TOOL}${exe}, which the packer would copy "
                            "into the package of ${host}")
    endif()
endforeach()

# bin/ of the runtime archive. The check of the runtime archive refuses
# the tool there before it asks any tool for its version.
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
else()
    set(exe "")
endif()
set(runtime_bin "${WORK}/runtime/bin")
file(MAKE_DIRECTORY "${runtime_bin}")
file(WRITE "${runtime_bin}/${TOOL}${exe}" "stand-in ${TOOL}\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${runtime_bin}"
                        -DRUNTIME_ARCHIVE=ON
                        -P "${ROOT}/tools/check-llvm.cmake"
                OUTPUT_VARIABLE out ERROR_VARIABLE err
                RESULT_VARIABLE status ENCODING NONE)
# CMake wraps the text of a message at the width it sees, and a long path
# moves the break, so a line break and the spaces after it may stand
# between two words of the refusal. They read as one space here, T138 of
# docs/audit/tests-findings.md.
string(REGEX REPLACE "[ \t]*\n[ \t]*" " " flat "${err}")
if(status EQUAL 0 OR NOT flat MATCHES "holds ${TOOL}")
    message(FATAL_ERROR "tools/check-llvm.cmake accepted ${TOOL} in bin/ of "
                        "the runtime archive:\n${out}${err}")
endif()
