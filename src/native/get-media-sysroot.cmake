# Extend the glibc sysroot of each Linux target with the X11 and OpenGL
# development files that raylib's desktop back end compiles against.
#
#   cmake -DDEST=<dir> -DLLVM_BIN=<dir> -DTARGETS=<target>[;<target>]
#         -P src/native/get-media-sysroot.cmake
#
# A target is linux-x86_64-glibc or linux-arm64-glibc. When <dir>/<target>
# holds no glibc yet, tools/get-sysroot.cmake installs it first. The
# packages are the MEDIA_ lines of tools/sysroot-pins, from the pool that
# the GLIBC_<arch>_URL line names, each checked against its digest before
# it is unpacked. The copyright file of each goes to <dir>/licenses/.
#
# DESIGN: a stamp in the tree names the digests it was unpacked from, so a
# second run with the same pins does nothing, and a changed pin unpacks
# every package again. tools/get-sysroot.cmake removes the whole tree when
# it installs glibc, the stamp with it.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST OR NOT DEFINED LLVM_BIN OR NOT DEFINED TARGETS)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -DLLVM_BIN=<dir> "
                        "-DTARGETS=<target>[;<target>] "
                        "-P src/native/get-media-sysroot.cmake")
endif()
get_filename_component(DEST "${DEST}" ABSOLUTE)
set(tools_dir "${CMAKE_CURRENT_LIST_DIR}/../../tools")

file(STRINGS "${tools_dir}/sysroot-pins" pins REGEX "^(GLIBC|MEDIA)_")
foreach(line IN LISTS pins)
    if(line MATCHES "^([^=]+)=(.*)$")
        set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    endif()
endforeach()
if(NOT DEFINED MEDIA_PACKAGES)
    message(FATAL_ERROR "tools/sysroot-pins has no MEDIA_PACKAGES")
endif()
separate_arguments(packages UNIX_COMMAND "${MEDIA_PACKAGES}")

# Download url to file and stop unless its digest is the expected one.
function(fetch url file digest)
    set(have "")
    if(EXISTS "${file}")
        file(SHA256 "${file}" have)
    endif()
    if(NOT have STREQUAL digest)
        file(DOWNLOAD "${url}" "${file}" STATUS status
             EXPECTED_HASH "SHA256=${digest}")
        list(GET status 0 code)
        list(GET status 1 text)
        if(NOT code EQUAL 0)
            file(REMOVE "${file}")
            message(FATAL_ERROR "${url}: ${text}")
        endif()
    endif()
endfunction()

# DESIGN: a package names some links by an absolute path, as the loader
# /lib64/ld-linux-x86-64.so.2 of libc6 names /lib/x86_64-linux-gnu. On the
# host that path lies outside the sysroot, so lld finds nothing behind the
# link that libc.so names. Every absolute link of <root> becomes the
# relative link to the same file inside <root>.
function(relative_links root)
    file(GLOB_RECURSE entries LIST_DIRECTORIES true "${root}/*")
    foreach(entry IN LISTS entries)
        if(NOT IS_SYMLINK "${entry}")
            continue()
        endif()
        file(READ_SYMLINK "${entry}" destination)
        if(NOT destination MATCHES "^/")
            continue()
        endif()
        get_filename_component(directory "${entry}" DIRECTORY)
        file(RELATIVE_PATH relative "${directory}" "${root}${destination}")
        file(REMOVE "${entry}")
        file(CREATE_LINK "${relative}" "${entry}" SYMBOLIC)
    endforeach()
endfunction()

function(media_sysroot target arch)
    set(root "${DEST}/${target}")
    if(NOT EXISTS "${root}/usr/include/features.h")
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${DEST}"
                                "-DLLVM_BIN=${LLVM_BIN}" "-DTARGETS=${target}"
                                -P "${tools_dir}/get-sysroot.cmake"
                        RESULT_VARIABLE status)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "tools/get-sysroot.cmake failed for ${target}")
        endif()
    endif()
    set(want "")
    foreach(package IN LISTS packages)
        set(digest "${MEDIA_${arch}_${package}_DIGEST}")
        if(digest STREQUAL "" OR "${MEDIA_${arch}_${package}}" STREQUAL "")
            message(FATAL_ERROR "tools/sysroot-pins has no MEDIA_${arch}_${package}")
        endif()
        string(APPEND want "${digest}\n")
    endforeach()
    set(stamp "${root}/.media-packages")
    set(have "")
    if(EXISTS "${stamp}")
        file(READ "${stamp}" have)
    endif()
    if(have STREQUAL want)
        return()
    endif()
    set(work "${DEST}/.download/${target}-media")
    file(MAKE_DIRECTORY "${work}")
    foreach(package IN LISTS packages)
        set(name "${MEDIA_${arch}_${package}}")
        get_filename_component(asset "${name}" NAME)
        fetch("${GLIBC_${arch}_URL}/${name}" "${work}/${asset}"
              "${MEDIA_${arch}_${package}_DIGEST}")
        file(REMOVE_RECURSE "${work}/deb")
        file(ARCHIVE_EXTRACT INPUT "${work}/${asset}" DESTINATION "${work}/deb")
        file(GLOB data "${work}/deb/data.tar.*")
        list(LENGTH data count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "${asset} holds ${count} data archives, not one")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${data}" DESTINATION "${root}")
        # The name of the package is the part of the file before the
        # first underscore, and its copyright lies under that name.
        string(REGEX REPLACE "_.*$" "" debian "${asset}")
        if(EXISTS "${root}/usr/share/doc/${debian}/copyright")
            file(COPY_FILE "${root}/usr/share/doc/${debian}/copyright"
                 "${DEST}/licenses/${debian}.txt")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${work}/deb")
    relative_links("${root}")
    file(WRITE "${stamp}" "${want}")
endfunction()

file(MAKE_DIRECTORY "${DEST}/licenses")
foreach(target IN LISTS TARGETS)
    if(target STREQUAL "linux-x86_64-glibc")
        media_sysroot("${target}" X86_64)
    elseif(target STREQUAL "linux-arm64-glibc")
        media_sysroot("${target}" AARCH64)
    else()
        message(FATAL_ERROR "unknown target ${target}, not a glibc sysroot")
    endif()
    message(STATUS "${DEST}/${target}")
endforeach()
