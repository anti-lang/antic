# The one download of the build.
#
# antic_download(<url> <file> [<algorithm> <digest>]) downloads <url> to
# <file> and stops on a failure, with nothing left at <file>. With a
# digest, SHA256 or SHA3_256 as <algorithm>, a file already on disk that
# has the digest is kept, and a download is checked against it. Without
# one, as for the signed list of a release, the file is downloaded every
# time. A download says "download <url>" first.
#
# antic_get_source of src/native/get-source.cmake, tools/get-sysroot.cmake,
# tools/fetch-release.cmake and tools/get-ninja.cmake include this file.
# The test downloads_once refuses a download anywhere else.
cmake_minimum_required(VERSION 3.21)

function(antic_download url file)
    set(algorithm "${ARGV2}")
    set(digest "${ARGV3}")
    if(NOT digest STREQUAL "" AND EXISTS "${file}")
        file(${algorithm} "${file}" have)
        if(have STREQUAL digest)
            return()
        endif()
    endif()
    message(STATUS "download ${url}")
    set(expected "")
    if(NOT digest STREQUAL "")
        set(expected EXPECTED_HASH "${algorithm}=${digest}")
    endif()
    get_filename_component(directory "${file}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    file(DOWNLOAD "${url}" "${file}" STATUS status ${expected})
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        file(REMOVE "${file}")
        message(FATAL_ERROR "${url}: ${text}")
    endif()
    # A digest that differs is an error of the download that leaves the
    # file on disk, and a configure goes on after it, so it is checked
    # again here.
    if(NOT digest STREQUAL "")
        file(${algorithm} "${file}" have)
        if(NOT have STREQUAL digest)
            file(REMOVE "${file}")
            message(FATAL_ERROR "${url}: ${algorithm} ${have}, expected "
                                "${digest}")
        endif()
    endif()
endfunction()
