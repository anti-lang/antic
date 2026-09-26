# A Linux program of a release names no glibc symbol above the pinned
# 2.35. Run with cmake -P and these values:
#   CLANG     the pinned clang
#   LLVM_BIN  the directory of ld.lld and llvm-readobj
#   SYSROOT   the directory of tools/get-sysroot.cmake
#   CHECK     tools/check-libc.cmake
#   WORK      a directory for the files
#   PACKAGE   optional, a packed tree whose bin/antic is read as well
#
# tools/pack-anti.cmake links a Linux program against the pinned sysroot,
# static and against musl, and reads its dynamic section before it writes
# the package. This links a stand-in with that same recipe for both Linux
# hosts and reads it the same way, so a change to the recipe that reaches
# the builder's libc fails here rather than in a published tarball. The
# packed antic itself is read when PACKAGE names a tree, which a release
# run has and the suite has not.
#
# It also shows the refusal, on a listing of a program that needs a glibc
# of its own machine. No program of this tree needs one, so the listing is
# written here.

if(NOT EXISTS "${SYSROOT}/linux-arm64/usr/lib/libc.a")
    message("SKIP: no Linux sysroot in ${SYSROOT}")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")
file(WRITE "${WORK}/tiny.c" "int main(void) { return 0; }\n")

function(triple_of host out)
    if(host STREQUAL "linux-x86_64")
        set("${out}" x86_64-unknown-linux-musl PARENT_SCOPE)
    else()
        set("${out}" aarch64-unknown-linux-musl PARENT_SCOPE)
    endif()
endfunction()

# The link line of tools/pack-anti.cmake, on one small source.
function(link_stand_in host out)
    triple_of("${host}" triple)
    set(lib "${SYSROOT}/${host}/usr/lib")
    set(object "${WORK}/tiny.${host}.o")
    set(program "${WORK}/tiny.${host}")
    execute_process(COMMAND "${CLANG}" ${ANTIC_C_WARNINGS} --target=${triple}
                            --sysroot "${SYSROOT}/${host}" -O2
                            -c -o "${object}" "${WORK}/tiny.c"
                    RESULT_VARIABLE failed ERROR_VARIABLE err ENCODING NONE)
    if(failed)
        message(FATAL_ERROR "${host}: the stand-in did not compile\n${err}")
    endif()
    execute_process(COMMAND "${LLVM_BIN}/ld.lld" -static -pie
                            --no-dynamic-linker -o "${program}"
                            "${lib}/rcrt1.o" "${lib}/crti.o" "${object}"
                            "${lib}/libc.a" "${lib}/libclang_rt.builtins.a"
                            "${lib}/crtn.o"
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

if(DEFINED PACKAGE AND EXISTS "${PACKAGE}/bin/antic")
    reads_clean("${PACKAGE}/bin/antic" "the packed antic")
    reads_clean("${PACKAGE}/bin/anti" "the packed anti")
endif()

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
