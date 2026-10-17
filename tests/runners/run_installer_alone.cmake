# The installers download the package alone. Each takes the package of the
# host and SHA256SUMS from the release, SHA256SUMS.sig from the site,
# checks the signature and the digest, unpacks, runs antic --version and
# anti --version of the package as the check of the install and puts the
# two on the path. Neither runs CMake, lays out a sysroot or reads a
# script or a pin of the package, which carries none. Eddie decided this on
# 2026-10-08, in decision 7 of docs/work-order-distribution.md.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/runners/run_installer_alone.cmake

# Both installers are read for the words of the steps that left them, and
# for the check of the install. Then install.sh runs against a package of
# the two programs and nothing else, with a cmake and an xcode-select on
# the PATH that fail when they are called.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

foreach(installer install.sh install.ps1)
    file(READ "${ROOT}/tools/${installer}" text)
    string(TOLOWER "${text}" folded)
    foreach(word cmake get-sysroot package-api xcode-select commandlinetools
            anti_microsoft)
        string(FIND "${folded}" "${word}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR "tools/${installer} names `${word}`, and an "
                                "installer downloads the package and runs "
                                "nothing of it")
        endif()
    endforeach()
    string(FIND "${text}" "--version" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "tools/${installer} runs no --version of the "
                            "package, which is the check of an install")
    endif()
endforeach()

fake_release_host()
if(NOT skip STREQUAL "")
    message("SKIP: ${skip}")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Write the staging area of a package whose two programs print <printed>
# as their version, in the layout the packer writes, with a signed
# manifest.
function(stage name printed)
    set(area "${WORK}/${name}/base/anti/${version}")
    fake_release_package("${area}" "${WORK}/${name}/package" "${printed}")
    fake_release_sign("${area}/SHA256SUMS" "${area}/SHA256SUMS.sig")
endfunction()

# The key of this test, and the copy of the installer that carries its
# public half in place of the key of the release.
fake_release_key()

# DESIGN: a cmake and an xcode-select stand first on the PATH. Each
# leaves a file with its name and fails, so an installer that calls either
# fails here and names the call.
file(MAKE_DIRECTORY "${WORK}/bin")
foreach(tool cmake xcode-select)
    file(WRITE "${WORK}/bin/${tool}"
         "#!/bin/sh\necho called > '${WORK}/${tool}.called'\n"
         "echo \"${tool}: an installer called me with $*\" >&2\nexit 1\n")
    file(CHMOD "${WORK}/bin/${tool}"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
                     GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach()

# Run the installer into a directory of its own, against the staging area
# <name>, and answer every question with no.
function(install_run name out_variable log_variable)
    set(home "${WORK}/home-${name}")
    file(REMOVE_RECURSE "${home}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "PATH=${WORK}/bin:$ENV{PATH}"
                "ANTI_VERSION=${version}"
                "ANTI_BASE=file://${WORK}/${name}/base"
                "ANTI_HOME=${home}"
                ANTI_REPLACE=yes ANTI_PATH=no
                sh "${WORK}/install.sh"
        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set("${out_variable}" "${failed}" PARENT_SCOPE)
    set("${log_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()

# A package of the two programs alone installs, and the installer runs
# both as the check of the install.
stage(whole "${version}")
install_run(whole failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "the installer refused a package of the two programs "
                        "alone\n${log}")
endif()
foreach(tool cmake xcode-select)
    if(EXISTS "${WORK}/${tool}.called")
        message(FATAL_ERROR "the installer called ${tool}, and nothing a user "
                            "runs needs it\n${log}")
    endif()
endforeach()
foreach(line "antic ${version}" "anti ${version}")
    string(FIND "${log}" "${line}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "the installer did not print `${line}`, which the "
                            "program of the package prints as the check of the "
                            "install\n${log}")
    endif()
endforeach()
if(NOT EXISTS "${WORK}/home-whole/bin/antic" OR
   NOT EXISTS "${WORK}/home-whole/.anti-install")
    message(FATAL_ERROR "the installer unpacked no package or wrote no "
                        "marker\n${log}")
endif()

# A package whose programs print another version is the wrong package,
# and the install stops after naming both versions.
stage(other "98.0.0")
install_run(other failed log)
if(failed EQUAL 0)
    message(FATAL_ERROR "the installer took a package whose antic prints "
                        "98.0.0 as ${version}\n${log}")
endif()
string(FIND "${log}" "98.0.0" found_printed)
string(FIND "${log}" "${version}" found_wanted)
if(found_printed EQUAL -1 OR found_wanted EQUAL -1)
    message(FATAL_ERROR "the installer stopped without naming the version "
                        "the package prints and the one it was to be\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")
