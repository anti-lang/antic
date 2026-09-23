# Upload a version directory of the download area and check what arrives.
#
#   cmake -DDIR=<directory> -DREMOTE=<user@host:/path> -P tools/publish.cmake
#
# DIR holds the files of one version, a SHA256SUMS that names each of them
# and the signature SHA256SUMS.sig. REMOTE is the directory of that
# version on the server, which scp and ssh understand. Set CHECK_ONLY to
# run the checks and upload nothing. KEY names the public key that the
# signature is read against, tools/keys/release.pem of this repository by
# default.
#
# DESIGN: the download area holds the files that its manifest names and
# nothing else. A file beside them is either a leftover of a debugging
# session, which a user may take for part of the release, or a file the
# manifest forgot, which no pin can reach. The check refuses both before
# anything is uploaded.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DIR)
    message(FATAL_ERROR "usage: cmake -DDIR=<directory> -DREMOTE=<dest> "
                        "-P tools/publish.cmake")
endif()
if(NOT DEFINED KEY)
    set(KEY "${CMAKE_CURRENT_LIST_DIR}/keys/release.pem")
endif()

set(manifest "${DIR}/SHA256SUMS")
if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "${DIR} holds no SHA256SUMS")
endif()

# DESIGN: every installer reads SHA256SUMS.sig before it trusts a line of
# SHA256SUMS, so a version directory without it installs nowhere. The
# signature is read here with the openssl of the installers, against the
# key they carry, and a manifest changed after it was signed stops the
# upload rather than the user.
set(signature "${DIR}/SHA256SUMS.sig")
if(NOT EXISTS "${signature}")
    message(FATAL_ERROR "${DIR} holds no SHA256SUMS.sig, and an installer "
                        "takes no manifest without one")
endif()
find_program(OPENSSL openssl)
if(NOT OPENSSL)
    message(FATAL_ERROR "openssl is missing, and without it SHA256SUMS.sig is "
                        "no signature of anything")
endif()
execute_process(COMMAND "${OPENSSL}" dgst -sha256 -verify "${KEY}"
                        -signature "${signature}" "${manifest}"
                RESULT_VARIABLE wrong OUTPUT_QUIET ERROR_QUIET)
if(NOT wrong EQUAL 0)
    message(FATAL_ERROR "SHA256SUMS.sig is no signature of SHA256SUMS by ${KEY}")
endif()

# The names the manifest lists, and the digest of each.
file(STRINGS "${manifest}" rows)
set(listed "")
foreach(row IN LISTS rows)
    if(NOT row MATCHES "^([0-9a-f]+)  (.+)$")
        message(FATAL_ERROR "SHA256SUMS holds the line `${row}`")
    endif()
    set(digest "${CMAKE_MATCH_1}")
    set(name "${CMAKE_MATCH_2}")
    if(NOT EXISTS "${DIR}/${name}")
        message(FATAL_ERROR "SHA256SUMS names ${name}, which ${DIR} lacks")
    endif()
    file(SHA256 "${DIR}/${name}" actual)
    if(NOT actual STREQUAL digest)
        message(FATAL_ERROR "${name}: SHA-256 ${actual}, expected ${digest}")
    endif()
    list(APPEND listed "${name}")
endforeach()

# Everything in the directory is either the manifest or a file it names.
file(GLOB found RELATIVE "${DIR}" "${DIR}/*")
foreach(name IN LISTS found)
    if(name STREQUAL "SHA256SUMS" OR name STREQUAL "SHA256SUMS.sig")
        continue()
    endif()
    if(NOT name IN_LIST listed)
        message(FATAL_ERROR
                "${name} stands in ${DIR} and SHA256SUMS does not name it. "
                "The download area holds the files of the manifest alone.")
    endif()
endforeach()

list(LENGTH listed count)
message(STATUS "${DIR}: ${count} files, each named and each digest right")
if(DEFINED CHECK_ONLY)
    return()
endif()
if(NOT DEFINED REMOTE)
    message(FATAL_ERROR "pass -DREMOTE=<user@host:/path> to upload")
endif()

foreach(name IN LISTS listed)
    execute_process(COMMAND scp -q "${DIR}/${name}" "${REMOTE}/"
                    RESULT_VARIABLE sent)
    if(NOT sent EQUAL 0)
        message(FATAL_ERROR "scp of ${name} failed")
    endif()
    message(STATUS "sent ${name}")
endforeach()
# The signature goes before the manifest, because an installer reads the
# manifest first and asks for the signature next.
execute_process(COMMAND scp -q "${signature}" "${REMOTE}/" RESULT_VARIABLE sent)
if(NOT sent EQUAL 0)
    message(FATAL_ERROR "scp of SHA256SUMS.sig failed")
endif()
execute_process(COMMAND scp -q "${manifest}" "${REMOTE}/" RESULT_VARIABLE sent)
if(NOT sent EQUAL 0)
    message(FATAL_ERROR "scp of SHA256SUMS failed")
endif()

# The manifest goes last, so a reader never sees it before its files.
string(REGEX REPLACE "^(.*):(.*)$" "\\1" host "${REMOTE}")
string(REGEX REPLACE "^(.*):(.*)$" "\\2" path "${REMOTE}")
execute_process(COMMAND ssh "${host}"
                        "cd ${path} && sha256sum -c SHA256SUMS"
                RESULT_VARIABLE checked)
if(NOT checked EQUAL 0)
    message(FATAL_ERROR "the files on ${host} do not match their digests")
endif()
message(STATUS "${REMOTE} holds what the manifest names")
