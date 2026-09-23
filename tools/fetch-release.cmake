# Download the archive of this host that a pin of the LLVM release names,
# check it and unpack it. tools/get-llvm.cmake and tools/get-clang.cmake
# include this file and call fetch_release().
#
#   fetch_release(<pin> <dest>)
#
# <pin> is tools/llvm-pin or tools/clang-pin. The archive is checked against
# the digest of the pin, against its line of SHA256SUMS of the release, and
# SHA256SUMS.sig against tools/keys/release.pem of the checkout, which the person
# who cloned it trusts. The archive unpacks into <dest>, and <dest>/.installed
# names its digest. An archive whose digest is there already is left as it
# is, so a configure that finds it downloads nothing. ARCHIVE names an
# archive already downloaded, which is checked all the same. The function
# sets fetched_host and fetched_version in the scope of the caller.
cmake_minimum_required(VERSION 3.21)

set(fetch_release_root "${CMAKE_CURRENT_LIST_DIR}/..")

function(fetch_release pin dest)
    get_filename_component(root "${fetch_release_root}" ABSOLUTE)
    file(READ "${root}/tools/llvm-version" version)
    string(STRIP "${version}" version)
    # The host is named as the six targets of antic are, so one spelling
    # serves the pins, the archive of the runtime and the option --target.
    cmake_host_system_information(RESULT os QUERY OS_NAME)
    cmake_host_system_information(RESULT platform QUERY OS_PLATFORM)
    string(TOLOWER "${os}" os)
    string(TOLOWER "${platform}" platform)
    if(platform MATCHES "^(arm64|aarch64)$")
        set(platform arm64)
    elseif(platform MATCHES "^(x86_64|amd64|x64)$")
        set(platform x86_64)
    endif()
    set(host "${os}-${platform}")
    set(fetched_host "${host}" PARENT_SCOPE)
    set(fetched_version "${version}" PARENT_SCOPE)

    # Read the row <key> of the pin, with the tag, the version and the host
    # in place of their markers.
    set(tag "")
    foreach(key tag release file "${host}-digest")
        file(STRINGS "${pin}" line REGEX "^${key}=")
        string(REGEX REPLACE "^${key}=" "" value "${line}")
        string(REPLACE "@TAG@" "${tag}" value "${value}")
        string(REPLACE "@VERSION@" "${version}" value "${value}")
        string(REPLACE "@HOST@" "${host}" value "${value}")
        set("${key}" "${value}")
    endforeach()
    set(asset "${file}")
    set(digest "${${host}-digest}")
    get_filename_component(pin_name "${pin}" NAME)
    if(digest STREQUAL "")
        message(FATAL_ERROR
                "tools/${pin_name} names no archive for ${host}. Every host "
                "antic runs on has one, so this is a host antic does not run "
                "on or a pin that lost a row.")
    endif()

    if(EXISTS "${dest}/.installed")
        file(READ "${dest}/.installed" installed)
        if(installed STREQUAL "${digest}\n")
            message(STATUS "${asset} is installed in ${dest}")
            return()
        endif()
    endif()

    # Download <name> of the release to <file>, and stop on a failure.
    function(download name file)
        file(DOWNLOAD "${release}/${name}" "${file}" STATUS status)
        list(GET status 0 code)
        list(GET status 1 text)
        if(NOT code EQUAL 0)
            file(REMOVE "${file}")
            message(FATAL_ERROR "${release}/${name}: ${text}")
        endif()
    endfunction()

    set(downloads "${dest}/downloads")
    file(MAKE_DIRECTORY "${downloads}")
    if(DEFINED ARCHIVE)
        set(archive "${ARCHIVE}")
    else()
        set(archive "${downloads}/${asset}")
        set(actual "")
        if(EXISTS "${archive}")
            file(SHA256 "${archive}" actual)
        endif()
        if(NOT actual STREQUAL digest)
            message(STATUS "download ${asset}")
            download("${asset}" "${archive}")
        endif()
    endif()
    file(SHA256 "${archive}" actual)
    if(NOT actual STREQUAL digest)
        message(FATAL_ERROR "${archive}: SHA-256 ${actual}, the pin is ${digest}")
    endif()

    # DESIGN: the pin decides which archive is right, and the signature of
    # SHA256SUMS ties the release to the release key. Both must agree, so a
    # release that changed under its tag fails here whichever file changed.
    download(SHA256SUMS "${downloads}/SHA256SUMS")
    download(SHA256SUMS.sig "${downloads}/SHA256SUMS.sig")
    file(STRINGS "${downloads}/SHA256SUMS" listed REGEX "  ${asset}$")
    string(REGEX REPLACE " .*$" "" listed "${listed}")
    if(NOT listed STREQUAL digest)
        message(FATAL_ERROR "SHA256SUMS of ${tag} lists '${listed}' for ${asset}, "
                            "and the pin ${digest}")
    endif()
    # DESIGN: openssl checks the signature, because macOS, every Linux and
    # Git for Windows carry it. The signature is ECDSA P-256 over the
    # SHA-256 digest of SHA256SUMS, which the LibreSSL of macOS verifies with
    # pkeyutl as well.
    find_program(OPENSSL openssl HINTS "C:/Program Files/Git/clangarm64/bin"
                 "C:/Program Files/Git/mingw64/bin" "C:/Program Files/Git/usr/bin")
    if(NOT OPENSSL)
        message(FATAL_ERROR "openssl is not on the PATH. It checks SHA256SUMS.sig.")
    endif()
    execute_process(COMMAND "${OPENSSL}" dgst -sha256 -binary
                            -out "${downloads}/SHA256SUMS.sha256"
                            "${downloads}/SHA256SUMS"
                    RESULT_VARIABLE hashed)
    execute_process(COMMAND "${OPENSSL}" pkeyutl -verify -pubin
                            -inkey "${root}/tools/keys/release.pem"
                            -in "${downloads}/SHA256SUMS.sha256"
                            -sigfile "${downloads}/SHA256SUMS.sig"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err
                    RESULT_VARIABLE verified)
    if(NOT hashed EQUAL 0 OR NOT verified EQUAL 0)
        message(FATAL_ERROR "SHA256SUMS.sig of ${tag} is not a signature of the "
                            "key in tools/keys/release.pem: ${out}${err}")
    endif()
    message(STATUS "${asset}: the digest of the pin, in SHA256SUMS signed by "
                   "the key in tools/keys/release.pem")

    # Every entry of the archive replaces the one of its name in <dest>.
    set(unpacked "${dest}/unpacked")
    file(REMOVE_RECURSE "${unpacked}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${unpacked}")
    file(GLOB entries RELATIVE "${unpacked}" "${unpacked}/*")
    if(NOT "VERSION" IN_LIST entries OR NOT "bin" IN_LIST entries)
        message(FATAL_ERROR "${asset} holds ${entries}, and no VERSION or bin")
    endif()
    foreach(entry IN LISTS entries)
        file(REMOVE_RECURSE "${dest}/${entry}")
        file(RENAME "${unpacked}/${entry}" "${dest}/${entry}")
    endforeach()
    file(REMOVE_RECURSE "${unpacked}")
    file(WRITE "${dest}/.installed" "${digest}\n")
endfunction()
