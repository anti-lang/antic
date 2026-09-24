# The miniaudio version and digest live in tools/miniaudio-pin alone. The download
# script reads them from there, so no second copy can drift.
#
#   cmake -DROOT=<repository> -P tests/run_miniaudio_pin.cmake
#
# The test raylib_miniaudio_pin compares the version with the one raylib
# bundles.

set(pin "${ROOT}/tools/miniaudio-pin")
set(script "${ROOT}/src/native/get-miniaudio.cmake")
set(recipe "${ROOT}/src/native/miniaudio.cmake")
foreach(file "${pin}" "${script}" "${recipe}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

file(STRINGS "${pin}" version REGEX "^MINIAUDIO_VERSION=")
file(STRINGS "${pin}" digest REGEX "^MINIAUDIO_DIGEST=")
string(REGEX REPLACE "^MINIAUDIO_VERSION=" "" version "${version}")
string(REGEX REPLACE "^MINIAUDIO_DIGEST=" "" digest "${digest}")
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "MINIAUDIO_VERSION is `${version}`, expected a version")
endif()
string(LENGTH "${digest}" length)
if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
    message(FATAL_ERROR "MINIAUDIO_DIGEST is `${digest}`, expected 64 hex digits")
endif()

# The source comes over HTTPS and the digest is checked on every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    if(NOT line MATCHES "https://")
        message(FATAL_ERROR "src/native/get-miniaudio.cmake reads `${line}`, "
                            "which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
if(NOT text MATCHES "EXPECTED_HASH \"SHA256=\\\${MINIAUDIO_DIGEST}\"")
    message(FATAL_ERROR "src/native/get-miniaudio.cmake does not check the "
                        "download against MINIAUDIO_DIGEST")
endif()

foreach(file "${script}" "${recipe}")
    file(READ "${file}" text)
    foreach(literal "${version}" "${digest}")
        if(text MATCHES "${literal}")
            message(FATAL_ERROR "${file} spells `${literal}`, "
                                "which belongs in tools/miniaudio-pin alone")
        endif()
    endforeach()
endforeach()
