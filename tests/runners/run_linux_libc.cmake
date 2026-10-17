# A Linux program of a release names no glibc symbol above the pinned
# 2.35. Run with cmake -P and these values:
#   CLANG     the pinned clang
#   LLVM_BIN  the directory of ld.lld and llvm-readobj
#   SYSROOT   the directory of tools/get-sysroot.cmake
#   CHECK     tools/check-libc.cmake
#   WORK      a directory for the files
#
# tools/pack-anti.cmake compiles and links antic and anti for a Linux host
# with antic_host_compile_options and antic_host_linux_link of
# tools/host-compile.cmake, static against the musl of the pinned
# sysroot, and reads the dynamic section of each before it writes the
# package. This compiles and links a stand-in with the same two functions
# for both Linux hosts and reads it the same way, so a change of the
# recipe that reaches the builder's libc fails here rather than in a
# published tarball.
#
# It also shows the refusal, on a listing of a program that needs a glibc
# of its own machine. No program of this tree needs one, so the listing is
# written here.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${SYSROOT}/linux-arm64/usr/lib/libc.a")
    message("SKIP: no Linux sysroot in ${SYSROOT}")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${root}/tools/warnings.cmake")
include("${root}/tools/host-compile.cmake")
file(READ "${root}/tools/version" version)
string(STRIP "${version}" version)
file(WRITE "${WORK}/tiny.c" "int main(void) { return 0; }\n")

# The compile and the link of tools/pack-anti.cmake, on one small source.
# A Linux host reads neither the resource directory of the clang nor the
# Apple SDK, so both stay empty.
function(link_stand_in host out)
    antic_host_compile_options(options "${host}" "${root}" "${SYSROOT}" ""
                               "" "${version}")
    set(object "${WORK}/tiny.${host}.o")
    set(program "${WORK}/tiny.${host}")
    execute_process(COMMAND "${CLANG}" ${options} ${ANTIC_C_WARNINGS}
                            -c -o "${object}" "${WORK}/tiny.c"
                    RESULT_VARIABLE failed ERROR_VARIABLE err ENCODING NONE)
    if(failed)
        message(FATAL_ERROR "${host}: the stand-in did not compile\n${err}")
    endif()
    antic_host_linux_link(command "${host}" "${SYSROOT}" "${LLVM_BIN}"
                          "${program}" "${object}")
    execute_process(COMMAND ${command}
                    RESULT_VARIABLE failed ERROR_VARIABLE err ENCODING NONE)
    if(failed)
        message(FATAL_ERROR "${host}: the stand-in did not link\n${err}")
    endif()
    set("${out}" "${program}" PARENT_SCOPE)
endfunction()

function(reads_clean binary what)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DBINARY=${binary}"
                            "-DREADOBJ=${LLVM_BIN}/llvm-readobj"
                            -P "${CHECK}"
                    RESULT_VARIABLE refused OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT refused EQUAL 0)
        message(FATAL_ERROR "${what} was refused\n${out}${err}")
    endif()
endfunction()

foreach(host linux-x86_64 linux-arm64)
    link_stand_in("${host}" program)
    reads_clean("${program}" "the ${host} stand-in")
endforeach()

# A program of a machine whose glibc is newer than the pinned one. The
# listing is the shape llvm-readobj -V writes for the versioned symbols.
file(WRITE "${WORK}/newer.txt"
     "  Version symbols {\n"
     "    Name: memcpy@GLIBC_2.17\n"
     "    Name: pthread_attr_getguardsize@GLIBC_2.34\n"
     "    Name: pidfd_spawnp@GLIBC_2.39\n"
     "  }\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DLISTING=${WORK}/newer.txt"
                        -P "${CHECK}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(refused EQUAL 0)
    message(FATAL_ERROR "a program that needs GLIBC_2.39 was not refused")
endif()
if(NOT "${out}${err}" MATCHES "GLIBC_2\\.39")
    message(FATAL_ERROR "the refusal does not name the version\n${out}${err}")
endif()
if("${out}${err}" MATCHES "GLIBC_2\\.34")
    message(FATAL_ERROR "the refusal names a version it allows\n${out}${err}")
endif()
