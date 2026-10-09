# The upstream source of every pinned component of the runtime archive.
#
# antic_pin_url(<out> <name>) sets <out> to the URL of the source package
# that tools/<name>-pin names as <NAME>_URL, with @VERSION@ replaced by
# <NAME>_VERSION, @YEAR@ by <NAME>_YEAR and @NUMBER@ by the version in
# the form of sqlite.org. The download scripts of src/native/ and
# tools/get-raylib.cmake take the URL from here, and so does the record
# below, so each pin holds it once.
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

# Set every KEY=value line of <file> as a variable of the caller.
function(antic_read_pins file)
    file(STRINGS "${file}" pins REGEX "^[A-Z]")
    foreach(line IN LISTS pins)
        string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
        list(GET pair 0 key)
        list(GET pair 1 value)
        set("${key}" "${value}" PARENT_SCOPE)
    endforeach()
endfunction()

function(antic_pin_url out name)
    string(TOUPPER "${name}" key)
    antic_read_pins("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${name}-pin")
    foreach(field VERSION URL)
        if(NOT DEFINED "${key}_${field}" OR "${${key}_${field}}" STREQUAL "")
            message(FATAL_ERROR "tools/${name}-pin has no ${key}_${field}")
        endif()
    endforeach()
    set(url "${${key}_URL}")
    string(REPLACE "@VERSION@" "${${key}_VERSION}" url "${url}")
    if(url MATCHES "@YEAR@")
        if(NOT DEFINED "${key}_YEAR")
            message(FATAL_ERROR "tools/${name}-pin has no ${key}_YEAR")
        endif()
        string(REPLACE "@YEAR@" "${${key}_YEAR}" url "${url}")
    endif()
    if(url MATCHES "@NUMBER@")
        # X.Y.Z is written as X, then Y and Z in two digits each, then 00,
        # so 3.8.2 would be 3080200.
        if(NOT "${${key}_VERSION}" MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            message(FATAL_ERROR "${key}_VERSION is `${${key}_VERSION}`, "
                                "expected X.Y.Z")
        endif()
        set(number "${CMAKE_MATCH_1}")
        foreach(part "${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
            if(part LESS 10)
                string(APPEND number "0")
            endif()
            string(APPEND number "${part}")
        endforeach()
        string(APPEND number "00")
        string(REPLACE "@NUMBER@" "${number}" url "${url}")
    endif()
    if(url MATCHES "@[A-Z]+@")
        message(FATAL_ERROR "${key}_URL holds a name the pin does not fill: "
                            "${url}")
    endif()
    set(${out} "${url}" PARENT_SCOPE)
endfunction()

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
