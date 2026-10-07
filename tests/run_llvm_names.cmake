# tools/get-llvm.cmake leaves a name of lld alone that already holds lld.
# Every configure runs the script on a shared directory of the LLVM tools,
# and deps_dir configures a copy while the suite links with ld.lld of that
# directory. The script wrote ld.lld, ld64.lld and lld-link again on each
# run, and on anti-linux linux_libc then could not start ld.lld. Run with
# cmake -P and these values:
#   ROOT  the root of the repository
#   LLVM  the directory of the installed LLVM tools, whose .installed
#         names the archive of this host
#   WORK  a directory this run writes into
#
# The tools here are stand-ins of a few bytes, so check-llvm.cmake at the
# end of the script refuses them. The run is read for the files it wrote,
# not for its status.

cmake_minimum_required(VERSION 3.21)

if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
set(dest "${WORK}/llvm")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${dest}/bin")
file(COPY_FILE "${LLVM}/.installed" "${dest}/.installed")
file(WRITE "${dest}/bin/lld${exe}" "lld\n")
foreach(name ld.lld ld64.lld lld-link)
    file(WRITE "${dest}/bin/${name}${exe}" "lld\n")
endforeach()
# A name that holds something else is written as lld.
file(WRITE "${dest}/bin/lld-link${exe}" "old\n")

foreach(name ld.lld ld64.lld)
    file(TIMESTAMP "${dest}/bin/${name}${exe}" "before_${name}" "%s" UTC)
endforeach()
# The timestamp counts seconds, so a write in the same second would not
# show.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.5)

execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${dest}"
                        -P "${ROOT}/tools/get-llvm.cmake"
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT "${out}${err}" MATCHES "is installed in")
    message(FATAL_ERROR "the stand-in tree did not count as installed:\n"
                        "${out}${err}")
endif()

foreach(name ld.lld ld64.lld)
    file(TIMESTAMP "${dest}/bin/${name}${exe}" after "%s" UTC)
    if(NOT after STREQUAL "${before_${name}}")
        message(FATAL_ERROR "get-llvm.cmake wrote ${name}${exe}, which "
                            "already held lld")
    endif()
endforeach()
file(READ "${dest}/bin/lld-link${exe}" link)
if(NOT link STREQUAL "lld\n")
    message(FATAL_ERROR "get-llvm.cmake left lld-link${exe} as `${link}`")
endif()
