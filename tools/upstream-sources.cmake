# The upstream source of every pinned component of the runtime archive.
#
# antic_write_sources(<path>) writes licenses/sources.txt of the runtime
# archive: one line per component with its name, its version and the URL
# of the exact upstream source package, from tools/sysroot-pins,
# tools/zig-stubs-pin and the pins of the native libraries. The name of a
# line is the name of the component's licence text in licenses/, so
# `anti license` prints the line after the text. glibc and the kernel
# headers are the two components whose licence asks for the source, and
# the record lists every other pinned component all the same, so a reader
# has one list.

include("${CMAKE_CURRENT_LIST_DIR}/pins.cmake")

# The source package of an Ubuntu binary package: <out_version> the
# version, which is the second field of the file name
# g/glibc/libc6_2.35-0ubuntu3_amd64.deb, and <out_url> the .dsc of the
# source package under <base>, which the directory of the pool names,
# g/glibc/glibc_2.35-0ubuntu3.dsc.
function(antic_deb_source out_version out_url base file)
    if(NOT file MATCHES "^(.*/([^_/]+))/([^_/]+)_([^_/]+)_[^_/]+\\.deb$")
        message(FATAL_ERROR "`${file}` is not the file name of a package of "
                            "the pool")
    endif()
    set(${out_version} "${CMAKE_MATCH_4}" PARENT_SCOPE)
    set(${out_url} "${base}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}_${CMAKE_MATCH_4}.dsc"
        PARENT_SCOPE)
endfunction()

function(antic_write_sources path)
    set(tools "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    antic_read_pins("${tools}/sysroot-pins")
    antic_read_pins("${tools}/zig-stubs-pin")
    string(CONCAT record
        "# The upstream source of each component of the runtime archive: the\n"
        "# name of its licence text in this directory, its version and the\n"
        "# URL of the exact source package. tools/upstream-sources.cmake\n"
        "# writes it from the pins.\n")
    string(APPEND record "musl ${MUSL_VERSION} ${MUSL_SOURCE_URL}\n")
    # One line per package of the glibc sysroots, under the name of its
    # licence text: glibc for the C library, linux-headers for the kernel
    # headers and the name of the binary package for the rest. The two
    # processors hold the same version of each, which the line gives once.
    separate_arguments(packages UNIX_COMMAND "${GLIBC_PACKAGES}")
    foreach(package IN LISTS packages)
        if(package STREQUAL "LIBC_DEV")
            continue()
        endif()
        foreach(arch X86_64 AARCH64)
            set(file "${GLIBC_${arch}_${package}}")
            if(file STREQUAL "")
                message(FATAL_ERROR "tools/sysroot-pins has no "
                                    "GLIBC_${arch}_${package}")
            endif()
            antic_deb_source(version_${arch} url_${arch}
                             "${GLIBC_${arch}_URL}" "${file}")
        endforeach()
        if(NOT version_X86_64 STREQUAL version_AARCH64)
            message(FATAL_ERROR "tools/sysroot-pins holds ${package} at "
                                "${version_X86_64} for x86_64 and at "
                                "${version_AARCH64} for arm64")
        endif()
        if(package STREQUAL "LIBC")
            set(name glibc)
        elseif(package STREQUAL "HEADERS")
            set(name linux-headers)
        else()
            get_filename_component(name "${GLIBC_X86_64_${package}}" NAME)
            string(REGEX REPLACE "_.*$" "" name "${name}")
        endif()
        string(APPEND record "${name} ${version_X86_64} ${url_X86_64}\n")
    endforeach()
    string(REPLACE "@TAG@" "${ZIG_TAG}" zig_url "${ZIG_URL}")
    string(APPEND record "zig ${ZIG_TAG} ${zig_url}\n")
    string(REPLACE "@VERSION@" "${MINGW_VERSION}" mingw_url "${MINGW_URL}")
    string(APPEND record "mingw-w64 ${MINGW_VERSION} ${mingw_url}\n")
    foreach(name pcre2 sqlite mbedtls miniaudio raylib mimalloc)
        string(TOUPPER "${name}" key)
        antic_read_pins("${tools}/${name}-pin")
        antic_pin_url(url "${name}")
        string(APPEND record "${name} ${${key}_VERSION} ${url}\n")
    endforeach()
    # DESIGN: file(WRITE) ends a line as the host does, which is CRLF on
    # Windows, and the line of the record that anti prints after a text
    # then carried a carriage return there. file(CONFIGURE) writes LF on
    # every host, so the record of a package is the same bytes whichever
    # host built it. The record holds no @ for @ONLY to fill.
    file(CONFIGURE OUTPUT "${path}" CONTENT "${record}" @ONLY
         NEWLINE_STYLE LF)
endfunction()
