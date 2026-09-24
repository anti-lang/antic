# tools/get-sysroot.cmake turns every absolute symbolic link of a sysroot it
# builds into the relative link to the same path inside the sysroot, which
# lld follows on the host. The glibc sysroot then copies each link, which
# glibc_sysroot checks. Here the musl sysroot keeps its links: a stand-in
# Alpine package holds a header linked by its absolute path, and it lands
# as a relative link. Stand-in packages served from file:// URLs and a
# stand-in clang take the place of the real ones in a copy of the script.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_sysroot_links.cmake

file(REMOVE_RECURSE "${WORK}")
set(stage "${WORK}/stage")
file(WRITE "${stage}/apk/usr/include/stdio.h" "usr/include/stdio.h\n")
file(MAKE_DIRECTORY "${stage}/apk/usr/include/sys")
file(CREATE_LINK /usr/include/stdio.h "${stage}/apk/usr/include/sys/stdio.h"
     SYMBOLIC)
foreach(name crt1.o crti.o crtn.o rcrt1.o Scrt1.o libc.a)
    file(WRITE "${stage}/apk/usr/lib/${name}" "usr/lib/${name}\n")
endforeach()
file(MAKE_DIRECTORY "${WORK}/packages/x86_64")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf
                        "${WORK}/packages/x86_64/musl-dev-1.apk" -- usr
                WORKING_DIRECTORY "${stage}/apk" COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${stage}/source/musl-1/COPYRIGHT" "musl\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf
                        "${WORK}/packages/musl.tar.gz" -- musl-1
                WORKING_DIRECTORY "${stage}/source" COMMAND_ERROR_IS_FATAL ANY)
set(clang "${WORK}/clang")
file(WRITE "${clang}/lib/clang/23/lib/x86_64-unknown-linux-musl/libclang_rt.builtins.a"
     "builtins\n")
file(WRITE "${clang}/licenses/llvm.txt" "llvm\n")

# A copy of the script beside pins that name the stand-ins.
file(MAKE_DIRECTORY "${WORK}/tools")
file(COPY_FILE "${ROOT}/tools/get-sysroot.cmake" "${WORK}/tools/get-sysroot.cmake")
file(COPY_FILE "${ROOT}/tools/zig-stubs-pin" "${WORK}/tools/zig-stubs-pin")
file(SHA256 "${WORK}/packages/x86_64/musl-dev-1.apk" apk_digest)
file(SHA256 "${WORK}/packages/musl.tar.gz" source_digest)
file(WRITE "${WORK}/tools/sysroot-pins"
     "ALPINE_URL=file://${WORK}/packages\n"
     "MUSL_VERSION=1\n"
     "MUSL_APK=1\n"
     "MUSL_DEV_X86_64=${apk_digest}\n"
     "MUSL_SOURCE_URL=file://${WORK}/packages/musl.tar.gz\n"
     "MUSL_SOURCE_DIGEST=${source_digest}\n")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                        -DLLVM_BIN=bin -DTARGETS=linux-x86_64
                        "-DCLANG_DIR=${clang}"
                        -P "${WORK}/tools/get-sysroot.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed:\n${out}${err}")
endif()
set(link "${WORK}/sysroot/linux-x86_64/usr/include/sys/stdio.h")
if(NOT IS_SYMLINK "${link}")
    message(FATAL_ERROR "${link} is no link")
endif()
file(READ_SYMLINK "${link}" destination)
if(NOT destination STREQUAL "../stdio.h")
    message(FATAL_ERROR "${link} points to ${destination}")
endif()
file(READ "${link}" text)
if(NOT text STREQUAL "usr/include/stdio.h\n")
    message(FATAL_ERROR "${link} reads '${text}'")
endif()
