# A version lives in tools/<name>-version and the archive of each host in
# tools/<name>-pin, which spells the version as @VERSION@. No copy of
# either can drift, and every archive comes over HTTPS. The LLVM pin has a
# form of its own, which tests/run_release_pins.cmake checks. READERS
# lists the files under ROOT that download the archive. Each reads the pin,
# with either separator, and does not spell the version.
#
#   cmake -DROOT=<repository> -DNAME=cmake
#         "-DREADERS=tools/install.sh;tools/install.ps1" -P tests/run_pin.cmake

cmake_minimum_required(VERSION 3.21)

set(hosts macos-arm64 macos-x86_64 linux-x86_64 linux-arm64 windows-x86_64
          windows-arm64)
set(pin "${ROOT}/tools/${NAME}-pin")
file(READ "${ROOT}/tools/${NAME}-version" version)
string(STRIP "${version}" version)
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "tools/${NAME}-version is `${version}`")
endif()

foreach(host IN LISTS hosts)
    file(STRINGS "${pin}" url REGEX "^${host}-url=")
    file(STRINGS "${pin}" digest REGEX "^${host}-digest=")
    string(REGEX REPLACE "^${host}-url=" "" url "${url}")
    string(REGEX REPLACE "^${host}-digest=" "" digest "${digest}")
    if(NOT url MATCHES "@VERSION@")
        message(FATAL_ERROR "${host} has no archive with @VERSION@ in it")
    endif()
    # A user downloads over HTTPS and has no other way in.
    if(NOT url MATCHES "^https://")
        message(FATAL_ERROR "${host} is served from `${url}`, not over HTTPS")
    endif()
    string(LENGTH "${digest}" length)
    if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
        message(FATAL_ERROR "${host} has the digest `${digest}`")
    endif()
endforeach()

# The version is searched as a literal: as a regular expression its dots
# would match any byte.
file(READ "${pin}" text)
string(FIND "${text}" "${version}" at)
if(NOT at EQUAL -1)
    message(FATAL_ERROR "tools/${NAME}-pin spells ${version}, which belongs "
                        "in tools/${NAME}-version alone")
endif()
if(READERS STREQUAL "")
    message(FATAL_ERROR "READERS names no file that reads tools/${NAME}-pin")
endif()
foreach(reader IN LISTS READERS)
    if(NOT EXISTS "${ROOT}/${reader}")
        message(FATAL_ERROR "${reader} does not exist")
    endif()
    file(READ "${ROOT}/${reader}" text)
    string(FIND "${text}" "tools/${NAME}-pin" reads)
    if(reads EQUAL -1)
        string(FIND "${text}" "tools\\${NAME}-pin" reads)
    endif()
    string(FIND "${text}" "${version}" at)
    if(reads EQUAL -1)
        message(FATAL_ERROR "${reader} does not read tools/${NAME}-pin")
    endif()
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "${reader} spells ${version}, which belongs in "
                            "tools/${NAME}-version alone")
    endif()
endforeach()
