# tools/copy-llvm.cmake lays out bin/ of the runtime archive: every tool
# that antic_llvm_archive_tools of tools/pinned-tools.cmake names, and
# llvm-version. A tool missing from the directory of the LLVM tools is an
# error that names it. The copy of check-llvm.cmake took a tool only if it
# was there, so a missing llvm-objdump left a runtime archive without it
# and no error. The tools here are stand-ins of a few bytes, since the
# copy reads no version. Run with cmake -P and these values:
#   ROOT  the root of the repository
#   WORK  a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tools/pinned-tools.cmake")
antic_llvm_archive_tools(names)
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
set(source "${WORK}/llvm/bin")
set(dest "${WORK}/runtime/bin")
file(MAKE_DIRECTORY "${source}")
foreach(name IN LISTS names)
    if(NOT name STREQUAL "llvm-objdump")
        file(WRITE "${source}/${name}${exe}" "stand-in ${name}\n")
    endif()
endforeach()
file(WRITE "${source}/llvm-cov${exe}" "stand-in llvm-cov\n")

function(copy out_status out_text)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${source}"
                            "-DDEST=${dest}"
                            -P "${ROOT}/tools/copy-llvm.cmake"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err
                    RESULT_VARIABLE status ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

copy(status text)
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${text}")
if(status EQUAL 0 OR NOT flat MATCHES "llvm-objdump${exe} is missing")
    message(FATAL_ERROR "tools/copy-llvm.cmake did not refuse a directory "
                        "without llvm-objdump${exe}:\n${text}")
endif()
if(EXISTS "${dest}/llvm-version")
    message(FATAL_ERROR "tools/copy-llvm.cmake wrote llvm-version after it "
                        "found a tool missing")
endif()

file(WRITE "${source}/llvm-objdump${exe}" "stand-in llvm-objdump\n")
copy(status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/copy-llvm.cmake refused a whole "
                        "directory:\n${text}")
endif()
foreach(name IN LISTS names)
    if(NOT EXISTS "${dest}/${name}${exe}")
        message(FATAL_ERROR "tools/copy-llvm.cmake left out ${name}${exe}")
    endif()
endforeach()
file(READ "${ROOT}/tools/llvm-version" pin)
file(READ "${dest}/llvm-version" written)
string(STRIP "${pin}" pin)
if(NOT written STREQUAL "${pin}\n")
    message(FATAL_ERROR "llvm-version of the copy holds `${written}`, the "
                        "pin is `${pin}`")
endif()
if(EXISTS "${dest}/llvm-cov${exe}")
    message(FATAL_ERROR "tools/copy-llvm.cmake copied llvm-cov${exe}, which "
                        "no package carries")
endif()
file(REMOVE_RECURSE "${WORK}")
