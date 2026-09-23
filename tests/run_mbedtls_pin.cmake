# The Mbed TLS version and digest live in tools/mbedtls-pin alone. The
# download script reads them from there, so no second copy can drift.
#
#   cmake -DROOT=<repository> -P tests/run_mbedtls_pin.cmake

set(pin "${ROOT}/tools/mbedtls-pin")
set(script "${ROOT}/src/native/get-mbedtls.cmake")
set(recipe "${ROOT}/src/native/mbedtls.cmake")
foreach(file "${pin}" "${script}" "${recipe}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

file(STRINGS "${pin}" version REGEX "^MBEDTLS_VERSION=")
file(STRINGS "${pin}" digest REGEX "^MBEDTLS_DIGEST=")
string(REGEX REPLACE "^MBEDTLS_VERSION=" "" version "${version}")
string(REGEX REPLACE "^MBEDTLS_DIGEST=" "" digest "${digest}")
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "MBEDTLS_VERSION is `${version}`, expected a version")
endif()
string(LENGTH "${digest}" length)
if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
    message(FATAL_ERROR "MBEDTLS_DIGEST is `${digest}`, expected 64 hex digits")
endif()

# The source comes over HTTPS and the digest is checked on every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    if(NOT line MATCHES "https://")
        message(FATAL_ERROR "src/native/get-mbedtls.cmake reads `${line}`, "
                            "which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
if(NOT text MATCHES "EXPECTED_HASH \"SHA256=\\\${MBEDTLS_DIGEST}\"")
    message(FATAL_ERROR "src/native/get-mbedtls.cmake does not check the "
                        "download against MBEDTLS_DIGEST")
endif()

foreach(file "${script}" "${recipe}")
    file(READ "${file}" text)
    foreach(literal "${version}" "${digest}")
        string(REPLACE "." "\\." pattern "${literal}")
        if(text MATCHES "${pattern}")
            message(FATAL_ERROR "${file} spells `${literal}`, "
                                "which belongs in tools/mbedtls-pin alone")
        endif()
    endforeach()
endforeach()
