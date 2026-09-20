# The installers take the package, SHA256SUMS and SHA256SUMS.sig from the
# GitHub release of the tag, and anti-lang.com serves no binary. The test
# stands a fake release area on disk, signs its manifest with a key of its
# own and runs the installer against a copy of itself that carries the
# public half, because the private key of the release is not in this
# repository.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_installer_github.cmake

# The three lines of tools/release-base, which both installers carry.
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

# DESIGN: a release is published once, to the GitHub release of its tag.
# The two installers stand on the site and cannot read a file of the
# repository, so each carries the address. This test is the seam: a move
# of the release area is a change of tools/release-base, and an installer
# that kept the old one fails here.
foreach(installer install.sh install.ps1)
    file(READ "${ROOT}/tools/${installer}" text)
    foreach(name "${pin_download}" "${pin_api}" ANTI_GITHUB ANTI_BASE)
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

if(CMAKE_HOST_WIN32)
    message("SKIP: install.sh needs a shell")
    return()
endif()
find_program(OPENSSL openssl)
if(NOT OPENSSL)
    message("SKIP: openssl is missing")
    return()
endif()
find_program(CURL curl)
if(NOT CURL)
    message("SKIP: curl is missing")
    return()
endif()

set(version "99.0.0")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(os macos)
else()
    set(os linux)
endif()
# A script of cmake -P carries no processor, so it comes from uname, as
# it does in the installer.
execute_process(COMMAND uname -m OUTPUT_VARIABLE arch
                OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING NONE)
if(arch STREQUAL "aarch64")
    set(arch arm64)
endif()
if(NOT arch MATCHES "^(arm64|x86_64)$")
    message("SKIP: no package for the processor ${arch}")
    return()
endif()
set(host "${os}-${arch}")
set(asset "anti-${version}-${host}.tar.xz")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(cmake_dir "${CMAKE_COMMAND}" DIRECTORY)

# Run <command> and fail with <message> unless it succeeds.
function(run message)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE failed
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT failed EQUAL 0)
        message(FATAL_ERROR "${message}\n${out}${err}")
    endif()
endfunction()

# The package the installer unpacks. It carries what install.sh reads and
# no program: the version of the installer interface, and the stubs of
# libSystem that tell it this package links for macOS on its own. Without
# tools/llvm-pin it downloads no toolchain, so the test reaches no server.
set(tree "${WORK}/package")
file(MAKE_DIRECTORY "${tree}/anti/bin")
file(WRITE "${tree}/anti/tools/package-api" "1\n")
file(WRITE "${tree}/anti/bin/antic" "#!/bin/sh\necho \"antic ${version}\"\n")
file(WRITE "${tree}/anti/bin/anti" "#!/bin/sh\necho \"anti ${version}\"\n")
foreach(target macos-arm64 macos-x86_64)
    file(WRITE "${tree}/anti/sysroot/${target}/usr/lib/libSystem.tbd" "stub\n")
endforeach()

# The fake release area. Its assets stand under the tag, as the assets of
# a GitHub release do, and the answer of the latest-release API stands
# beside it.
set(area "${WORK}/github/v${version}")
file(MAKE_DIRECTORY "${area}")
run("the package did not pack" "${CMAKE_COMMAND}" -E chdir "${tree}"
    "${CMAKE_COMMAND}" -E tar cJf "${area}/${asset}" anti)
file(SHA256 "${area}/${asset}" digest)
file(WRITE "${area}/SHA256SUMS" "${digest}  ${asset}\n")
file(WRITE "${WORK}/github/latest.json"
     "{\"url\":\"https://api.github.com/repos/anti-lang/antic/releases/1\","
     "\"tag_name\":\"v${version}\",\"name\":\"Anti ${version}\"}\n")

# The key of this test, and the copy of the installer that carries its
# public half in place of the key of the release.
run("openssl wrote no private key" "${OPENSSL}" ecparam -name prime256v1
    -genkey -noout -out "${WORK}/private.pem")
run("openssl wrote no public key" "${OPENSSL}" ec -in "${WORK}/private.pem"
    -pubout -out "${WORK}/public.pem")
file(READ "${WORK}/public.pem" public)
string(STRIP "${public}" public)
file(READ "${ROOT}/tools/install.sh" installer)
set(block "-----BEGIN PUBLIC KEY-----[A-Za-z0-9+/=\n]+-----END PUBLIC KEY-----")
if(NOT installer MATCHES "${block}")
    message(FATAL_ERROR "tools/install.sh holds no public key to replace")
endif()
string(REGEX REPLACE "${block}" "${public}" installer "${installer}")
file(WRITE "${WORK}/install.sh" "${installer}")

# Sign the manifest the way step 6 of ./r signs it.
run("openssl hashed nothing" "${OPENSSL}" dgst -sha256 -binary
    -out "${WORK}/SHA256SUMS.sha256" "${area}/SHA256SUMS")
run("openssl signed nothing" "${OPENSSL}" pkeyutl -sign
    -inkey "${WORK}/private.pem" -in "${WORK}/SHA256SUMS.sha256"
    -out "${area}/SHA256SUMS.sig")

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
                "ANTI_HOME=${home}"
                ANTI_REPLACE=yes ANTI_PATH=no ANTI_MICROSOFT=no
                ${ARGN}
                sh "${WORK}/install.sh"
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
# as on the staging one.
file(READ "${area}/SHA256SUMS" manifest)
file(WRITE "${area}/SHA256SUMS"
     "0000000000000000000000000000000000000000000000000000000000000000  ${asset}\n")
install_run(wrong failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "signature")
    message(FATAL_ERROR "the installer took a manifest the signature does not "
                        "cover\n${log}")
endif()
file(WRITE "${area}/SHA256SUMS" "${manifest}")

# ANTI_BASE stays the override of a release, which checks its packages
# before they are published. It names a directory in the layout the
# packer writes, and the version with it.
set(staging "${WORK}/staging/anti/${version}")
file(MAKE_DIRECTORY "${staging}")
foreach(name "${asset}" SHA256SUMS SHA256SUMS.sig)
    file(COPY "${area}/${name}" DESTINATION "${staging}")
endforeach()
install_run(staging failed log "ANTI_BASE=file://${WORK}/staging"
            "ANTI_VERSION=${version}")
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
