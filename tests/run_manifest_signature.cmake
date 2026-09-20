# The installer checks SHA256SUMS.sig against the key it carries before it
# trusts a line of SHA256SUMS, and stops when the signature is missing or
# wrong. The test signs a manifest with a key of its own and runs the
# installer against a copy of itself that holds the public half, because
# the private key of the release is not in this repository.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_manifest_signature.cmake

# The rule holds on every host, and only the shell half of it runs here.
# install.ps1 is read for the same three names: the signature it fetches,
# the refusal of a manifest without one and the staging area of a release
# that ./r installs from before step 6 signs. The Windows VM of step 5
# runs the refusal for real.
foreach(installer install.sh install.ps1)
    file(READ "${ROOT}/tools/${installer}" text)
    foreach(name SHA256SUMS.sig ANTI_STAGING "no SHA256SUMS.sig")
        string(FIND "${text}" "${name}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "tools/${installer} knows nothing of `${name}`, "
                                "and every installer checks the signature of "
                                "the manifest")
        endif()
    endforeach()
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
set(area "${WORK}/base/anti/${version}")
file(MAKE_DIRECTORY "${area}")
run("the package did not pack" "${CMAKE_COMMAND}" -E chdir "${tree}"
    "${CMAKE_COMMAND}" -E tar cJf "${area}/${asset}" anti)
file(SHA256 "${area}/${asset}" digest)
file(WRITE "${area}/SHA256SUMS" "${digest}  ${asset}\n")

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

# Run the installer into a directory of its own and answer every question
# with no, so that it downloads nothing and writes nothing outside WORK.
function(install_run name staging out_variable log_variable)
    set(home "${WORK}/home-${name}")
    file(REMOVE_RECURSE "${home}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "PATH=${cmake_dir}:$ENV{PATH}"
                "ANTI_VERSION=${version}"
                "ANTI_BASE=file://${WORK}/base"
                "ANTI_HOME=${home}"
                "ANTI_STAGING=${staging}"
                ANTI_REPLACE=yes ANTI_PATH=no ANTI_MICROSOFT=no
                sh "${WORK}/install.sh"
        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set("${out_variable}" "${failed}" PARENT_SCOPE)
    set("${log_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()

# A signature of the key the installer carries, and the install goes on.
install_run(signed no failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer refused a signed manifest\n${log}")
endif()
if(NOT EXISTS "${WORK}/home-signed/bin/antic")
    message(FATAL_ERROR "the installer unpacked no package\n${log}")
endif()
if(NOT "${log}" MATCHES "signature")
    message(FATAL_ERROR "the installer says nothing of the signature\n${log}")
endif()

# A signature of another manifest, which is what a changed line of
# SHA256SUMS leaves behind.
file(READ "${area}/SHA256SUMS" manifest)
file(WRITE "${area}/SHA256SUMS" "0000000000000000000000000000000000000000000000000000000000000000  ${asset}\n")
install_run(wrong no failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "signature")
    message(FATAL_ERROR "the installer took a manifest the signature does not "
                        "cover\n${log}")
endif()
file(WRITE "${area}/SHA256SUMS" "${manifest}")

# No signature at all. The installer stops, and names the file it wants.
file(RENAME "${area}/SHA256SUMS.sig" "${WORK}/SHA256SUMS.sig")
install_run(missing no failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "SHA256SUMS\\.sig")
    message(FATAL_ERROR "the installer took an unsigned manifest\n${log}")
endif()

# The staging area of a release, whose manifest is signed in step 6 and
# read by the VM checks of step 5 before that. It warns and goes on.
install_run(staging yes failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer refused the staging area of a release\n${log}")
endif()
if(NOT "${log}" MATCHES "warning")
    message(FATAL_ERROR "the installer takes an unsigned manifest without a "
                        "word\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")
