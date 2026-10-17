# The installers take the package and SHA256SUMS from the GitHub release
# of the tag, and SHA256SUMS.sig from anti-lang.com. The two halves of a
# release stand on two hosts, so a forged release needs both. The test
# stands a fake release area and a fake site on disk, signs the manifest
# with a key of its own and runs the installer against a copy of itself
# that carries the public half, because the private key of the release is
# not in this repository.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/runners/run_installer_github.cmake

# The lines of tools/release-base and tools/site-base, which both
# installers carry.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(READ "${ROOT}/tools/release-base" pins)
foreach(key repository download api)
    if(NOT pins MATCHES "(^|\n)${key}=([^\n]+)")
        message(FATAL_ERROR "tools/release-base names no ${key}")
    endif()
    set("pin_${key}" "${CMAKE_MATCH_2}")
endforeach()
string(REPLACE "@REPOSITORY@" "${pin_repository}" pin_download "${pin_download}")
string(REPLACE "@REPOSITORY@" "${pin_repository}" pin_api "${pin_api}")
string(REPLACE "/@TAG@" "" pin_download "${pin_download}")

file(READ "${ROOT}/tools/site-base" site_pins)
foreach(key site signature key)
    if(NOT site_pins MATCHES "(^|\n)${key}=([^\n]+)")
        message(FATAL_ERROR "tools/site-base names no ${key}")
    endif()
    set("site_${key}" "${CMAKE_MATCH_2}")
endforeach()
string(REPLACE "@SITE@" "${site_site}" site_signature "${site_signature}")
string(REPLACE "@SITE@" "${site_site}" site_key "${site_key}")
# The directory of the signature under the webroot, without the version of
# one release. The fake site of this test holds the same shape.
string(REPLACE "/@VERSION@/SHA256SUMS.sig" "" site_signature_root
       "${site_signature}")
string(REPLACE "${site_site}/" "" signature_dir "${site_signature_root}")

# DESIGN: a release is published once, to the GitHub release of its tag.
# The two installers stand on the site and cannot read a file of the
# repository, so each carries the address. This test is the seam: a move
# of the release area is a change of tools/release-base, and an installer
# that kept the old one fails here.
foreach(installer install.sh install.ps1)
    file(READ "${ROOT}/tools/${installer}" text)
    foreach(name "${pin_download}" "${pin_api}" "${site_site}"
            ANTI_GITHUB ANTI_SITE_BASE ANTI_BASE)
        string(FIND "${text}" "${name}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "tools/${installer} does not name `${name}`, "
                                "which tools/release-base holds")
        endif()
    endforeach()
    # The site serves this script, the downloads page and the public key.
    # A package it served as well would stand beside the key that checks
    # it, and one host would hold both halves.
    string(FIND "${text}" "downloads/resources" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "tools/${installer} still takes a binary from the "
                            "download area of anti-lang.com, and the binaries "
                            "of a release are assets of its GitHub release")
    endif()
endforeach()

fake_release_host()
if(NOT skip STREQUAL "")
    message("SKIP: ${skip}")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(cmake_dir "${CMAKE_COMMAND}" DIRECTORY)

# The fake release area. Its assets stand under the tag, as the assets of
# a GitHub release do, and the answer of the latest-release API stands
# beside it.
set(area "${WORK}/github/v${version}")
fake_release_package("${area}" "${WORK}/package" "${version}")
file(WRITE "${WORK}/github/latest.json"
     "{\"url\":\"https://api.github.com/repos/anti-lang/antic/releases/1\","
     "\"tag_name\":\"v${version}\",\"name\":\"Anti ${version}\"}\n")

# The key of this test, and the copy of the installer that carries its
# public half in place of the key of the release.
fake_release_key()
set(installer_script "${WORK}/install.sh")
# The same copy without the check of the signature, which takes every
# one. The case of a wrong signature must pass with it, so that case
# fails for the signature and for nothing else.
set(check "openssl pkeyutl -verify")
string(FIND "${installer}" "${check}" at)
if(at EQUAL -1)
    message(FATAL_ERROR "tools/install.sh holds no `${check}` to take out")
endif()
string(REPLACE "${check}" "true || ${check}" unchecked "${installer}")
file(WRITE "${WORK}/install-unchecked.sh" "${unchecked}")

# Sign the manifest the way step 6 of ./r signs it, and publish the
# signature on the fake site rather than beside the package. Step 9 puts
# it under <webroot>/${signature_dir}/<version>/.
set(site_area "${WORK}/site/${signature_dir}/${version}")
file(MAKE_DIRECTORY "${site_area}")
fake_release_sign("${area}/SHA256SUMS" "${site_area}/SHA256SUMS.sig")

# Run the installer into a directory of its own, against the fake release
# area, and answer every question with no. ANTI_VERSION stays unset, so
# the run resolves the version through the latest-release API.
function(install_run name out_variable log_variable)
    set(home "${WORK}/home-${name}")
    file(REMOVE_RECURSE "${home}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "PATH=${cmake_dir}:$ENV{PATH}"
                "ANTI_GITHUB=file://${WORK}/github"
                "ANTI_GITHUB_API=file://${WORK}/github/latest.json"
                "ANTI_SITE_BASE=file://${WORK}/site"
                "ANTI_HOME=${home}"
                ANTI_REPLACE=yes ANTI_PATH=no
                ${ARGN}
                sh "${installer_script}"
        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set("${out_variable}" "${failed}" PARENT_SCOPE)
    set("${log_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()

# The newest version comes from the API, and the package from the release
# of that tag.
install_run(latest failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer took nothing from the release\n${log}")
endif()
if(NOT EXISTS "${WORK}/home-latest/bin/antic")
    message(FATAL_ERROR "the installer unpacked no package\n${log}")
endif()
if(NOT "${log}" MATCHES "${asset}")
    message(FATAL_ERROR "the installer names no package of ${version}, so the "
                        "latest-release API decided nothing\n${log}")
endif()
if(NOT "${log}" MATCHES "signature")
    message(FATAL_ERROR "the installer says nothing of the signature\n${log}")
endif()
if(NOT "${log}" MATCHES "${WORK}/site")
    message(FATAL_ERROR "the installer did not name the site it read the "
                        "signature from\n${log}")
endif()

# DESIGN: the signature never comes from the host that serves the
# binaries. A release whose signature stands on GitHub alone is refused,
# even when that signature is the right one over the right manifest.
file(RENAME "${site_area}/SHA256SUMS.sig" "${area}/SHA256SUMS.sig")
install_run(github_signature failed log)
if(failed EQUAL 0)
    message(FATAL_ERROR "the installer took a signature from the release that "
                        "holds the binaries\n${log}")
endif()
if(NOT "${log}" MATCHES "SHA256SUMS.sig")
    message(FATAL_ERROR "the installer stopped for another reason than the "
                        "missing signature\n${log}")
endif()
file(RENAME "${area}/SHA256SUMS.sig" "${site_area}/SHA256SUMS.sig")

# A version the caller names reaches the same release and asks the API
# nothing. The answer of the API is moved away, so a run that reads it
# fails here.
file(RENAME "${WORK}/github/latest.json" "${WORK}/latest.json")
install_run(named failed log "ANTI_VERSION=${version}")
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer asked the API for a version it was "
                        "given\n${log}")
endif()
file(RENAME "${WORK}/latest.json" "${WORK}/github/latest.json")

# The signature covers the manifest that names the package, on this route
# as on the staging one. A signature of another manifest stands on the
# site, and the manifest and the package stay right, so only the
# signature is wrong.
file(RENAME "${site_area}/SHA256SUMS.sig" "${WORK}/SHA256SUMS.sig")
file(WRITE "${WORK}/other/SHA256SUMS"
     "0000000000000000000000000000000000000000000000000000000000000000  ${asset}\n")
fake_release_sign("${WORK}/other/SHA256SUMS" "${site_area}/SHA256SUMS.sig")
install_run(wrong failed log)
if(failed EQUAL 0 OR
   NOT "${log}" MATCHES "carries no signature of the key of Anti")
    message(FATAL_ERROR "the installer took a manifest the signature does not "
                        "cover\n${log}")
endif()
set(installer_script "${WORK}/install-unchecked.sh")
install_run(wrong_unchecked failed log)
set(installer_script "${WORK}/install.sh")
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "an installer that takes every signature refused the "
                        "case of a wrong one, so the case tells nothing\n${log}")
endif()
file(RENAME "${WORK}/SHA256SUMS.sig" "${site_area}/SHA256SUMS.sig")

# ANTI_BASE stays the override of a release, which checks its packages
# before they are published. It names a directory in the layout the
# packer writes, and the version with it.
set(staging "${WORK}/staging/anti/${version}")
file(MAKE_DIRECTORY "${staging}")
foreach(name "${asset}" SHA256SUMS)
    file(COPY "${area}/${name}" DESTINATION "${staging}")
endforeach()
file(COPY "${site_area}/SHA256SUMS.sig" DESTINATION "${staging}")
# The site publishes nothing of a release that step 6 has not signed yet,
# so a staging area carries its own signature and this run reads no site.
install_run(staging failed log "ANTI_BASE=file://${WORK}/staging"
            "ANTI_VERSION=${version}"
            "ANTI_SITE_BASE=file://${WORK}/no-site")
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer refused the staging area of a "
                        "release\n${log}")
endif()

# A staging area holds one version and no answer of an API, so a run
# without a version stops rather than reaching GitHub for it.
install_run(unversioned failed log "ANTI_BASE=file://${WORK}/staging")
if(failed EQUAL 0 OR NOT "${log}" MATCHES "ANTI_VERSION")
    message(FATAL_ERROR "the installer read a version of the API for a "
                        "staging area\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")
