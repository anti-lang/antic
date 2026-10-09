# The record of the upstream sources of the runtime archive,
# licenses/sources.txt beside the licence texts. One line per component:
# its name, its version and the URL of the exact upstream source package,
# which the CMake build writes from the pins. Run with cmake -P and these
# values:
#   ROOT     the repository
#   SOURCES  the sources.txt to check
#
# Every line gives three fields and reaches its source over HTTPS, and
# names a component whose licence text stands beside the file. The line
# of glibc and the one of the kernel headers name the version of
# tools/sysroot-pins in their URL, musl names its version there as well,
# and the line of every pinned library carries the version of its pin.
# The lines end in LF on every host, so the file of a package built on
# Windows is the file of a package built anywhere else, and the line
# `anti license` prints after a text carries no carriage return.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${SOURCES}")
    message(FATAL_ERROR "${SOURCES} is missing")
endif()
# file(READ) reads the file as text, which on Windows turns CRLF into LF,
# so the bytes are read as hex and searched by pairs.
file(READ "${SOURCES}" raw HEX)
string(REGEX REPLACE "(..)" "\\1;" bytes "${raw}")
if("0d" IN_LIST bytes)
    message(FATAL_ERROR "${SOURCES} holds a carriage return")
endif()
get_filename_component(licenses "${SOURCES}" DIRECTORY)
file(STRINGS "${SOURCES}" lines)
set(names "")
foreach(line IN LISTS lines)
    if(line MATCHES "^#" OR line STREQUAL "")
        continue()
    endif()
    if(NOT line MATCHES "^([^ ]+) ([^ ]+) (https://[^ ]+)$")
        message(FATAL_ERROR "${SOURCES} holds `${line}`, which is not "
                            "`<name> <version> <https URL>`")
    endif()
    set(name "${CMAKE_MATCH_1}")
    if(name IN_LIST names)
        message(FATAL_ERROR "${SOURCES} names ${name} twice")
    endif()
    list(APPEND names "${name}")
    if(NOT EXISTS "${licenses}/${name}.txt")
        message(FATAL_ERROR "${SOURCES} names ${name}, and ${licenses} holds "
                            "no ${name}.txt")
    endif()
    set("line_${name}" "${line}")
endforeach()

# Set <out> to the value of <key> in tools/<pin>.
function(read_pin out pin key)
    file(STRINGS "${ROOT}/tools/${pin}" line REGEX "^${key}=")
    string(REGEX REPLACE "^${key}=" "" value "${line}")
    if(value STREQUAL "")
        message(FATAL_ERROR "tools/${pin} has no ${key}")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# The line of <name> holds <version> as its version and, when <in_url> is
# ON, in its URL.
function(expect name version in_url)
    if(NOT DEFINED "line_${name}")
        message(FATAL_ERROR "${SOURCES} has no line for ${name}")
    endif()
    set(line "${line_${name}}")
    string(REGEX REPLACE "^[^ ]+ ([^ ]+) .*$" "\\1" got "${line}")
    if(NOT got STREQUAL version)
        message(FATAL_ERROR "${SOURCES} gives ${name} the version ${got}, and "
                            "its pin ${version}")
    endif()
    string(REGEX REPLACE "^[^ ]+ [^ ]+ (.*)$" "\\1" url "${line}")
    string(FIND "${url}" "${version}" at)
    if(in_url AND at EQUAL -1)
        message(FATAL_ERROR "the URL of ${name} in ${SOURCES} does not name "
                            "${version}: ${url}")
    endif()
endfunction()

# The version of an Ubuntu package is the second field of its file name,
# libc6_2.35-0ubuntu3_amd64.deb.
function(deb_version out key)
    read_pin(deb sysroot-pins "${key}")
    if(NOT deb MATCHES "^.*/[^_/]+_([^_/]+)_[^_/]+\\.deb$")
        message(FATAL_ERROR "${key} of tools/sysroot-pins is `${deb}`, not "
                            "the file name of a package")
    endif()
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

deb_version(glibc GLIBC_X86_64_LIBC)
expect(glibc "${glibc}" ON)
deb_version(headers GLIBC_X86_64_HEADERS)
expect(linux-headers "${headers}" ON)
read_pin(musl sysroot-pins MUSL_VERSION)
expect(musl "${musl}" ON)
read_pin(zig zig-stubs-pin ZIG_TAG)
expect(zig "${zig}" ON)
foreach(name pcre2 sqlite mbedtls miniaudio raylib mimalloc)
    string(TOUPPER "${name}" key)
    read_pin(version "${name}-pin" "${key}_VERSION")
    expect("${name}" "${version}" OFF)
endforeach()
