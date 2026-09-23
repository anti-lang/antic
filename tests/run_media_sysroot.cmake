# tools/sysroot-pins names the X11 and OpenGL development packages of
# Ubuntu 22.04 that raylib compiles against on Linux, for amd64 and arm64,
# and src/native/get-media-sysroot.cmake unpacks them over the glibc
# sysroot. Stand-in packages served from file:// URLs take the place of the
# real ones in a copy of the script.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DLLVM_AR=<llvm-ar>
#         -P tests/run_media_sysroot.cmake

file(STRINGS "${ROOT}/tools/sysroot-pins" pins REGEX "^(GLIBC|MEDIA)_")
foreach(line IN LISTS pins)
    if(line MATCHES "^([^=]+)=(.*)$")
        set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    endif()
endforeach()
separate_arguments(packages UNIX_COMMAND "${MEDIA_PACKAGES}")

# Every library that raylib's X11 back end and OpenGL need is pinned, the
# headers and the library alike.
foreach(package LIBX11_DEV LIBX11 X11PROTO LIBXRANDR_DEV LIBXRANDR
                LIBXINERAMA_DEV LIBXINERAMA LIBXCURSOR_DEV LIBXCURSOR
                LIBXI_DEV LIBXI LIBXEXT_DEV LIBXEXT LIBXRENDER_DEV LIBXRENDER
                LIBXFIXES_DEV LIBXFIXES LIBGL_DEV LIBGL LIBGLX_DEV LIBGLX
                LIBGLVND)
    if(NOT package IN_LIST packages)
        message(FATAL_ERROR "MEDIA_PACKAGES lacks ${package}")
    endif()
endforeach()

# Each pin names a package of the jammy release pocket for its processor,
# or one for every processor, and a SHA-256 digest.
foreach(arch X86_64 AARCH64)
    set(deb amd64)
    if(arch STREQUAL "AARCH64")
        set(deb arm64)
    endif()
    foreach(package IN LISTS packages)
        set(name "${MEDIA_${arch}_${package}}")
        set(digest "${MEDIA_${arch}_${package}_DIGEST}")
        set(form "^[a-z0-9]+/[a-z0-9.+-]+/[a-z0-9.+-]+_[^_/]+_(${deb}|all)")
        if(NOT name MATCHES "${form}\\.deb$")
            message(FATAL_ERROR "MEDIA_${arch}_${package} is '${name}', not a "
                                "package for ${deb}")
        endif()
        string(LENGTH "${digest}" length)
        if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
            message(FATAL_ERROR "MEDIA_${arch}_${package}_DIGEST is '${digest}'")
        endif()
    endforeach()
endforeach()

# Write the stand-in package <name> whose data holds the files after it,
# each holding its own path, and the link usr/lib/x86_64-linux-gnu/libX.so
# to the absolute path of one of them.
function(stand_in name)
    set(stage "${WORK}/stage-${name}")
    file(REMOVE_RECURSE "${stage}")
    foreach(path IN LISTS ARGN)
        file(WRITE "${stage}/data/${path}" "${path}\n")
    endforeach()
    if(name STREQUAL "libx6_1_amd64.deb")
        file(MAKE_DIRECTORY "${stage}/data/usr/lib/x86_64-linux-gnu")
        file(CREATE_LINK "/lib/x86_64-linux-gnu/libX.so.6"
             "${stage}/data/usr/lib/x86_64-linux-gnu/libX.so" SYMBOLIC)
    endif()
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
stand_in(libx-dev_1_amd64.deb usr/include/X11/Xlib.h
         usr/share/doc/libx-dev/copyright)
stand_in(libx6_1_amd64.deb lib/x86_64-linux-gnu/libX.so.6
         usr/share/doc/libx6/copyright)

# A copy of the script beside pins that name the stand-ins, over a glibc
# sysroot that is already there.
file(MAKE_DIRECTORY "${WORK}/src/native" "${WORK}/tools")
file(COPY_FILE "${ROOT}/src/native/get-media-sysroot.cmake"
     "${WORK}/src/native/get-media-sysroot.cmake")
set(text "GLIBC_X86_64_URL=file://${WORK}/packages\nMEDIA_PACKAGES=DEV LIB\n")
foreach(pair DEV=libx-dev_1_amd64.deb LIB=libx6_1_amd64.deb)
    string(REPLACE "=" ";" pair "${pair}")
    list(GET pair 0 package)
    list(GET pair 1 name)
    file(SHA256 "${WORK}/packages/${name}" digest)
    string(APPEND text "MEDIA_X86_64_${package}=${name}\n"
                       "MEDIA_X86_64_${package}_DIGEST=${digest}\n")
endforeach()
file(WRITE "${WORK}/tools/sysroot-pins" "${text}")
set(tree "${WORK}/sysroot/linux-x86_64-glibc")
file(WRITE "${tree}/usr/include/features.h" "glibc\n")

function(get_media result)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                            -DLLVM_BIN=bin -DTARGETS=linux-x86_64-glibc
                            -P "${WORK}/src/native/get-media-sysroot.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                    ENCODING NONE)
    set(${result} "${status}" PARENT_SCOPE)
    set(output "${out}${err}" PARENT_SCOPE)
endfunction()

get_media(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "get-media-sysroot.cmake failed:\n${output}")
endif()
foreach(path usr/include/features.h usr/include/X11/Xlib.h
             lib/x86_64-linux-gnu/libX.so.6)
    if(NOT EXISTS "${tree}/${path}")
        message(FATAL_ERROR "${tree} lacks ${path}")
    endif()
endforeach()
if(EXISTS "${tree}/debian-binary")
    message(FATAL_ERROR "the wrapper of a package landed in ${tree}")
endif()

# The absolute link now points inside the sysroot.
set(link "${tree}/usr/lib/x86_64-linux-gnu/libX.so")
file(READ_SYMLINK "${link}" destination)
if(NOT destination STREQUAL "../../../lib/x86_64-linux-gnu/libX.so.6")
    message(FATAL_ERROR "${link} points to ${destination}")
endif()
file(READ "${link}" text)
if(NOT text STREQUAL "lib/x86_64-linux-gnu/libX.so.6\n")
    message(FATAL_ERROR "${link} reads '${text}'")
endif()
foreach(pair libx-dev libx6)
    file(READ "${WORK}/sysroot/licenses/${pair}.txt" text)
    if(NOT text STREQUAL "usr/share/doc/${pair}/copyright\n")
        message(FATAL_ERROR "licenses/${pair}.txt is not the copyright "
                            "of ${pair}")
    endif()
endforeach()

# A second run with the same pins leaves the tree alone.
file(WRITE "${tree}/usr/include/X11/Xlib.h" "kept\n")
get_media(status)
file(READ "${tree}/usr/include/X11/Xlib.h" text)
if(NOT status EQUAL 0 OR NOT text STREQUAL "kept\n")
    message(FATAL_ERROR "a second run unpacked the packages again:\n${output}")
endif()

# A package of another digest is refused.
file(APPEND "${WORK}/packages/libx6_1_amd64.deb" "changed\n")
file(REMOVE "${tree}/.media-packages")
file(REMOVE_RECURSE "${WORK}/sysroot/.download")
get_media(status)
if(status EQUAL 0 OR NOT output MATCHES "HASH mismatch")
    message(FATAL_ERROR "get-media-sysroot.cmake took a changed "
                        "package:\n${output}")
endif()
