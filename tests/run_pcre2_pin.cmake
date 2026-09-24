# The PCRE2 version and digest live in tools/pcre2-pin alone. The download
# script reads them from there, so no second copy can drift.
#
#   cmake -DROOT=<repository> -P tests/run_pcre2_pin.cmake

set(pin "${ROOT}/tools/pcre2-pin")
set(script "${ROOT}/src/native/get-pcre2.cmake")
set(recipe "${ROOT}/src/native/pcre2.cmake")
set(source "${ROOT}/src/native/pcre2-source.cmake")
set(files "${ROOT}/src/native/pcre2-files.cmake")
foreach(file "${pin}" "${script}" "${recipe}" "${source}" "${files}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

file(STRINGS "${pin}" version REGEX "^PCRE2_VERSION=")
file(STRINGS "${pin}" digest REGEX "^PCRE2_DIGEST=")
string(REGEX REPLACE "^PCRE2_VERSION=" "" version "${version}")
string(REGEX REPLACE "^PCRE2_DIGEST=" "" digest "${digest}")
if(NOT version MATCHES "^[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "PCRE2_VERSION is `${version}`, expected a version")
endif()
string(LENGTH "${digest}" length)
if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
    message(FATAL_ERROR "PCRE2_DIGEST is `${digest}`, expected 64 hex digits")
endif()

# The source comes over HTTPS and the digest is checked on every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    if(NOT line MATCHES "https://")
        message(FATAL_ERROR "src/native/get-pcre2.cmake reads `${line}`, "
                            "which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
if(NOT text MATCHES "EXPECTED_HASH \"SHA256=\\\${PCRE2_DIGEST}\"")
    message(FATAL_ERROR "src/native/get-pcre2.cmake does not check the "
                        "download against PCRE2_DIGEST")
endif()

foreach(file "${script}" "${recipe}" "${source}" "${files}")
    file(READ "${file}" text)
    foreach(literal "${version}" "${digest}")
        if(text MATCHES "${literal}")
            message(FATAL_ERROR "${file} spells `${literal}`, "
                                "which belongs in tools/pcre2-pin alone")
        endif()
    endforeach()
endforeach()

# JIT stays off: the recipe never defines SUPPORT_JIT.
file(READ "${recipe}" text)
if(text MATCHES "-DSUPPORT_JIT")
    message(FATAL_ERROR "src/native/pcre2.cmake turns JIT on")
endif()
