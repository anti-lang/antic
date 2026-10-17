# Check the directory of a release: the files that its signed manifest
# names and nothing else.
#
#   cmake -DDIR=<directory> [-DKEY=<public key>] -P tools/check-release.cmake
#
# DIR holds the packages and the symbols archives of one version, the
# SHA256SUMS that names each of them and the signature SHA256SUMS.sig.
# KEY names the public key that the signature is read against,
# tools/keys/release.pem of this repository by default. Step 6 of ./r
# runs the checks over build/dist/packages, which step 7 uploads to the
# GitHub release.
#
# DESIGN: the directory holds the files that its manifest names and
# nothing else. A file beside them is either a leftover of a debugging
# session, which a user may take for part of the release, or a file the
# manifest forgot, which no pin can reach. The check refuses both before
# anything is uploaded.
cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED DIR)
    message(FATAL_ERROR "usage: cmake -DDIR=<directory> [-DKEY=<key>] "
                        "-P tools/check-release.cmake")
endif()
if(NOT DEFINED KEY)
    set(KEY "${CMAKE_CURRENT_LIST_DIR}/keys/release.pem")
endif()

set(manifest "${DIR}/SHA256SUMS")
if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "${DIR} holds no SHA256SUMS")
endif()

# DESIGN: every installer reads SHA256SUMS.sig before it trusts a line of
# SHA256SUMS, so a release without it installs nowhere. The signature is
# read here with the two openssl commands of the installers, against the
# key they carry, and a manifest changed after it was signed stops the
# release rather than the user.
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
# The digest stands beside the directory and never in it, which holds
# the files of the manifest alone.
get_filename_component(hashed "${DIR}" ABSOLUTE)
string(APPEND hashed ".sha256")
execute_process(COMMAND "${OPENSSL}" dgst -sha256 -binary -out "${hashed}"
                        "${manifest}"
                RESULT_VARIABLE failed OUTPUT_QUIET ERROR_QUIET)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "openssl gave no digest of ${manifest}")
endif()
execute_process(COMMAND "${OPENSSL}" pkeyutl -verify -pubin -inkey "${KEY}"
                        -in "${hashed}" -sigfile "${signature}"
                RESULT_VARIABLE wrong OUTPUT_QUIET ERROR_QUIET)
file(REMOVE "${hashed}")
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
                "A release holds the files of the manifest alone.")
    endif()
endforeach()

list(LENGTH listed count)
message(STATUS "${DIR}: ${count} files, each named and each digest right")
