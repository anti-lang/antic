# tools/sysroot-pins names glibc 2.35 and the kernel headers of Ubuntu
# 22.04, three packages of the jammy release pocket per processor.
# tools/get-sysroot.cmake unpacks them into sysroot/linux-<cpu>-glibc for
# the Linux link mode against glibc. Stand-in packages served from file://
# URLs take the place of the real ones in a copy of the script.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DLLVM_AR=<llvm-ar>
#         -P tests/run_glibc_sysroot.cmake

# The pins name the release pocket of jammy, whose files never change.
file(STRINGS "${ROOT}/tools/sysroot-pins" pins REGEX "^GLIBC_")
foreach(line IN LISTS pins)
    string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
    list(GET pair 0 key)
    list(GET pair 1 value)
    set("${key}" "${value}")
endforeach()
if(NOT GLIBC_X86_64_URL STREQUAL "https://archive.ubuntu.com/ubuntu/pool/main" OR
   NOT GLIBC_AARCH64_URL STREQUAL "https://ports.ubuntu.com/ubuntu-ports/pool/main")
    message(FATAL_ERROR "tools/sysroot-pins takes glibc from ${GLIBC_X86_64_URL} "
                        "and ${GLIBC_AARCH64_URL}, not the Ubuntu archive")
endif()
foreach(arch X86_64 AARCH64)
    set(deb amd64)
    if(arch STREQUAL "AARCH64")
        set(deb arm64)
    endif()
    foreach(package LIBC_DEV LIBC HEADERS)
        set(name "${GLIBC_${arch}_${package}}")
        set(digest "${GLIBC_${arch}_${package}_DIGEST}")
        if(NOT name MATCHES "^(g/glibc/libc6(-dev)?_2\\.35-0ubuntu3|l/linux/linux-libc-dev_5\\.15\\.0-25\\.25)_${deb}\\.deb$")
            message(FATAL_ERROR "GLIBC_${arch}_${package} is '${name}', not a "
                                "package of the jammy release for ${deb}")
        endif()
        if(NOT digest MATCHES "^[0-9a-f]+$")
            message(FATAL_ERROR "GLIBC_${arch}_${package}_DIGEST is '${digest}'")
        endif()
        string(LENGTH "${digest}" length)
        if(NOT length EQUAL 64)
            message(FATAL_ERROR "GLIBC_${arch}_${package}_DIGEST is '${digest}'")
        endif()
    endforeach()
endforeach()

# Write the stand-in package <name> whose data holds the files after it,
# each holding its own path.
function(stand_in name)
    set(stage "${WORK}/stage-${name}")
    file(REMOVE_RECURSE "${stage}")
    foreach(path IN LISTS ARGN)
        file(WRITE "${stage}/data/${path}" "${path}\n")
    endforeach()
    file(WRITE "${stage}/debian-binary" "2.0\n")
    file(GLOB top RELATIVE "${stage}/data" "${stage}/data/*")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf ../data.tar.zst --zstd
                            -- ${top}
                    WORKING_DIRECTORY "${stage}/data" COMMAND_ERROR_IS_FATAL ANY)
    file(COPY_FILE "${stage}/data.tar.zst" "${stage}/control.tar.zst")
    file(REMOVE "${WORK}/packages/${name}")
    file(MAKE_DIRECTORY "${WORK}/packages")
    execute_process(COMMAND "${LLVM_AR}" rc --format=gnu
                            "${WORK}/packages/${name}" debian-binary
                            control.tar.zst data.tar.zst
                    WORKING_DIRECTORY "${stage}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()

file(REMOVE_RECURSE "${WORK}")
stand_in(libc-dev.deb usr/include/features.h usr/lib/x86_64-linux-gnu/libc.so
         usr/share/doc/libc6-dev/copyright)
stand_in(libc.deb lib/x86_64-linux-gnu/libc.so.6 usr/share/doc/libc6/copyright)
stand_in(headers.deb usr/include/linux/futex.h
         usr/share/doc/linux-libc-dev/copyright)

# A copy of the script beside pins that name the stand-ins.
file(MAKE_DIRECTORY "${WORK}/tools")
file(COPY_FILE "${ROOT}/tools/get-sysroot.cmake" "${WORK}/tools/get-sysroot.cmake")
file(COPY_FILE "${ROOT}/tools/zig-stubs-pin" "${WORK}/tools/zig-stubs-pin")
file(STRINGS "${ROOT}/tools/sysroot-pins" kept REGEX "^[^G]|^G[^L]")
list(JOIN kept "\n" text)
string(APPEND text "\nGLIBC_X86_64_URL=file://${WORK}/packages\n")
foreach(pair LIBC_DEV=libc-dev.deb LIBC=libc.deb HEADERS=headers.deb)
    string(REPLACE "=" ";" pair "${pair}")
    list(GET pair 0 package)
    list(GET pair 1 name)
    file(SHA256 "${WORK}/packages/${name}" digest)
    string(APPEND text "GLIBC_X86_64_${package}=${name}\n"
                       "GLIBC_X86_64_${package}_DIGEST=${digest}\n")
endforeach()
file(WRITE "${WORK}/tools/sysroot-pins" "${text}")

function(get_sysroot result)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                            -DLLVM_BIN=bin -DTARGETS=linux-x86_64-glibc
                            -P "${WORK}/tools/get-sysroot.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                    ENCODING NONE)
    set(${result} "${status}" PARENT_SCOPE)
    set(output "${out}${err}" PARENT_SCOPE)
endfunction()

get_sysroot(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed:\n${output}")
endif()
set(tree "${WORK}/sysroot/linux-x86_64-glibc")
foreach(path usr/include/features.h usr/lib/x86_64-linux-gnu/libc.so
             lib/x86_64-linux-gnu/libc.so.6 usr/include/linux/futex.h)
    if(NOT EXISTS "${tree}/${path}")
        message(FATAL_ERROR "${tree} lacks ${path}")
    endif()
    file(READ "${tree}/${path}" text)
    if(NOT text STREQUAL "${path}\n")
        message(FATAL_ERROR "${tree}/${path} holds '${text}'")
    endif()
endforeach()
if(EXISTS "${tree}/debian-binary")
    message(FATAL_ERROR "the wrapper of a package landed in ${tree}")
endif()
foreach(pair glibc.txt=usr/share/doc/libc6/copyright
             linux-headers.txt=usr/share/doc/linux-libc-dev/copyright)
    string(REPLACE "=" ";" pair "${pair}")
    list(GET pair 0 licence)
    list(GET pair 1 source)
    file(READ "${WORK}/sysroot/licenses/${licence}" text)
    if(NOT text STREQUAL "${source}\n")
        message(FATAL_ERROR "licenses/${licence} is not the copyright of ${source}")
    endif()
endforeach()

# A package of another digest is refused.
file(APPEND "${WORK}/packages/libc.deb" "changed\n")
file(REMOVE_RECURSE "${WORK}/sysroot")
get_sysroot(status)
if(status EQUAL 0 OR NOT output MATCHES "expected")
    message(FATAL_ERROR "tools/get-sysroot.cmake took a changed package:\n${output}")
endif()
