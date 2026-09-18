# Install llvm-mc, lld, llvm-ar, llvm-objdump and llvm-readobj for this
# host into <dir>/bin. llvm-mc assembles, lld links under the names ld.lld,
# ld64.lld and lld-link, and llvm-ar writes static libraries. llvm-objdump
# reads the format and architecture of an output, and llvm-readobj decodes
# the Windows unwind data for the tests.
#
#   cmake [-DDEST=<dir>] [-DARCHIVE=<file>] -P tools/get-llvm.cmake
#
# The tools come from the release that tools/llvm-pin names, whose recipe
# builds them from the LLVM source of tools/llvm-version. The script
# downloads the asset of this host and checks its digest against the pin.
# It checks SHA256SUMS of the release against the pin, and SHA256SUMS.sig
# with gpgv against the key of tools/llvm-tools-key.gpg. DEST defaults to
# build/llvm of the repository.
# ARCHIVE names an asset already downloaded, which is checked all the same.
cmake_minimum_required(VERSION 3.20)

get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED DEST)
    set(DEST "${root}/build/llvm")
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/llvm-version" version)
string(STRIP "${version}" version)
# The host is named as the six targets of antic are, so one spelling
# serves the pin, the archive of the runtime and the option --target.
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

# Set <out> to the row <key> of the pin, with the tag, the version and the
# host in place of their markers.
function(pin key out)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/llvm-pin" line REGEX "^${key}=")
    string(REGEX REPLACE "^${key}=" "" value "${line}")
    string(REPLACE "@TAG@" "${tag}" value "${value}")
    string(REPLACE "@VERSION@" "${version}" value "${value}")
    string(REPLACE "@HOST@" "${host}" value "${value}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()
set(tag "")
pin(tag tag)
pin(release release)
pin(file asset)
pin(key key)
pin("${host}-digest" digest)
if(digest STREQUAL "")
    message(FATAL_ERROR
            "tools/llvm-pin has no LLVM tools for ${host}. Every host antic "
            "runs on has them, so this is a host antic does not run on or a "
            "pin that lost a row.")
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

set(downloads "${DEST}/downloads")
file(MAKE_DIRECTORY "${downloads}")
if(NOT DEFINED ARCHIVE)
    set(ARCHIVE "${downloads}/${asset}")
    set(actual "")
    if(EXISTS "${ARCHIVE}")
        file(SHA256 "${ARCHIVE}" actual)
    endif()
    if(NOT actual STREQUAL digest)
        message(STATUS "download ${asset}")
        download("${asset}" "${ARCHIVE}")
    endif()
endif()
file(SHA256 "${ARCHIVE}" actual)
if(NOT actual STREQUAL digest)
    message(FATAL_ERROR "${ARCHIVE}: SHA-256 ${actual}, the pin is ${digest}")
endif()

# DESIGN: the pin decides which asset is right, and the signature of
# SHA256SUMS ties the release to the key of anti-lang. Both must agree, so
# a release that changed under its tag fails here whichever file changed.
download(SHA256SUMS "${downloads}/SHA256SUMS")
download(SHA256SUMS.sig "${downloads}/SHA256SUMS.sig")
file(STRINGS "${downloads}/SHA256SUMS" listed REGEX "  ${asset}$")
string(REGEX REPLACE " .*$" "" listed "${listed}")
if(NOT listed STREQUAL digest)
    message(FATAL_ERROR "SHA256SUMS of ${tag} lists '${listed}' for ${asset}, "
                        "and the pin ${digest}")
endif()
find_program(GPGV gpgv HINTS "C:/Program Files/Git/usr/bin")
if(NOT GPGV)
    message(FATAL_ERROR "gpgv is not on the PATH. It checks SHA256SUMS.sig.")
endif()
# The gpgv of Git for Windows reads C: in a keyring path as the scheme of a
# URL, so the keyring is named from its own directory.
execute_process(COMMAND "${GPGV}" --status-fd 1
                        --keyring ./llvm-tools-key.gpg
                        "${downloads}/SHA256SUMS.sig" "${downloads}/SHA256SUMS"
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
                OUTPUT_VARIABLE status_lines ERROR_VARIABLE err
                RESULT_VARIABLE verified)
if(NOT verified EQUAL 0 OR NOT status_lines MATCHES "VALIDSIG ${key} ")
    message(FATAL_ERROR "SHA256SUMS.sig of ${tag} is not a signature of the "
                        "key ${key}: ${err}")
endif()
message(STATUS "${asset}: the digest of the pin, in SHA256SUMS signed by ${key}")

# The asset holds bin/ with the five tools, licenses/ and VERSION.
set(unpacked "${DEST}/unpacked")
file(REMOVE_RECURSE "${unpacked}" "${DEST}/bin" "${DEST}/licenses"
     "${DEST}/VERSION")
file(ARCHIVE_EXTRACT INPUT "${ARCHIVE}" DESTINATION "${unpacked}")
foreach(name bin licenses VERSION)
    if(NOT EXISTS "${unpacked}/${name}")
        message(FATAL_ERROR "${asset} holds no ${name}")
    endif()
    file(RENAME "${unpacked}/${name}" "${DEST}/${name}")
endforeach()
file(REMOVE_RECURSE "${unpacked}")

# lld answers to its four names through argv[0]. The asset carries one
# copy, because Windows has no symbolic link without a privilege.
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
foreach(name ld.lld ld64.lld lld-link)
    file(COPY_FILE "${DEST}/bin/lld${exe}" "${DEST}/bin/${name}${exe}")
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${DEST}/bin"
                        -P "${CMAKE_CURRENT_LIST_DIR}/check-llvm.cmake"
                RESULT_VARIABLE checked)
if(NOT checked EQUAL 0)
    message(FATAL_ERROR "the tools in ${DEST}/bin are not LLVM ${version}")
endif()
