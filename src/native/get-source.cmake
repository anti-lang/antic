# The fetch of a pinned source of the runtime archive: the native
# libraries and raylib.
#
# antic_get_source(<out> <name> <dest> <algorithm> <marker>) fetches the
# source that tools/<name>-pin names into <dest>. The archive of the URL
# of the pin is checked against <NAME>_DIGEST with <algorithm>, SHA256 or
# SHA3_256, and unpacked beside it, and <out> is the unpacked directory.
# The archive is named after the URL when its file name starts with
# <name>-, as sqlite-amalgamation-3530400.zip does, and <name>-<version>
# with the suffix of the URL otherwise, since the archive of a tag of
# GitHub is named after the tag alone. Either way the directory it unpacks
# to has the name of the archive without its suffix. <marker>, a file
# below that directory, says the tree is there: the tree is read and never
# written, so one that holds it is not unpacked again. antic_download of
# tools/download.cmake downloads the archive.
#
# PINS <file> FIELDS <url> <digest> read the URL and the digest from the
# rows <url> and <digest> of tools/<file> in place of tools/<name>-pin,
# whose <NAME>_VERSION <file> then holds. libm.cmake fetches the
# source of musl so, from the pins of tools/sysroot-pins.
#
# CMakeLists.txt at the top includes this file, for the recipes of this
# directory and the sources of raylib and musl.
cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/download.cmake")

function(antic_get_source out name dest algorithm marker)
    cmake_parse_arguments(PARSE_ARGV 5 arg "" "PINS" "FIELDS")
    set(tools "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../tools")
    include("${tools}/pins.cmake")
    string(TOUPPER "${name}" key)
    if(arg_PINS)
        antic_read_pins("${tools}/${arg_PINS}")
        list(GET arg_FIELDS 0 url_field)
        list(GET arg_FIELDS 1 digest_field)
        set(${key}_URL "${${url_field}}")
        set(${key}_DIGEST "${${digest_field}}")
    else()
        antic_read_pins("${tools}/${name}-pin")
    endif()
    if("${${key}_DIGEST}" STREQUAL "")
        message(FATAL_ERROR "tools/${name}-pin has no ${key}_DIGEST")
    endif()
    antic_pin_url(url "${name}")
    get_filename_component(dest "${dest}" ABSOLUTE)
    if(NOT url MATCHES "/([^/]+)(\\.tar\\.[a-z0-9]+|\\.zip)$")
        message(FATAL_ERROR "${key}_URL names `${url}`, which is no .tar or "
                            ".zip archive")
    endif()
    set(tree "${CMAKE_MATCH_1}")
    set(suffix "${CMAKE_MATCH_2}")
    string(FIND "${tree}" "${name}-" at)
    if(NOT at EQUAL 0)
        set(tree "${name}-${${key}_VERSION}")
    endif()
    set(archive "${dest}/${tree}${suffix}")
    antic_download("${url}" "${archive}" "${algorithm}" "${${key}_DIGEST}")
    if(NOT EXISTS "${dest}/${tree}/${marker}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dest}")
        if(NOT EXISTS "${dest}/${tree}/${marker}")
            message(FATAL_ERROR "${archive} unpacked no ${tree}/${marker}")
        endif()
    endif()
    set(${out} "${dest}/${tree}" PARENT_SCOPE)
endfunction()
