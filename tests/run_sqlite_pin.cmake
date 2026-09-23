# The SQLite version, year and digest live in tools/sqlite-pin alone. The
# download script reads them from there, so no second copy can drift.
#
#   cmake -DROOT=<repository> -P tests/run_sqlite_pin.cmake

set(pin "${ROOT}/tools/sqlite-pin")
set(script "${ROOT}/src/native/get-sqlite.cmake")
set(recipe "${ROOT}/src/native/sqlite.cmake")
foreach(file "${pin}" "${script}" "${recipe}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

file(STRINGS "${pin}" version REGEX "^SQLITE_VERSION=")
file(STRINGS "${pin}" digest REGEX "^SQLITE_DIGEST=")
string(REGEX REPLACE "^SQLITE_VERSION=" "" version "${version}")
string(REGEX REPLACE "^SQLITE_DIGEST=" "" digest "${digest}")
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "SQLITE_VERSION is `${version}`, expected a version")
endif()
file(STRINGS "${pin}" year REGEX "^SQLITE_YEAR=")
string(REGEX REPLACE "^SQLITE_YEAR=" "" year "${year}")
if(NOT year MATCHES "^20[0-9][0-9]$")
    message(FATAL_ERROR "SQLITE_YEAR is `${year}`, expected a year")
endif()
string(LENGTH "${digest}" length)
if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
    message(FATAL_ERROR "SQLITE_DIGEST is `${digest}`, expected 64 hex digits")
endif()

# The digest is the SHA3-256 that sqlite.org publishes, so it has 64 hex
# digits as a SHA-256 does. The source comes over HTTPS and the digest is checked on every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    if(NOT line MATCHES "https://")
        message(FATAL_ERROR "src/native/get-sqlite.cmake reads `${line}`, "
                            "which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
if(NOT text MATCHES "EXPECTED_HASH \"SHA3_256=\\\${SQLITE_DIGEST}\"")
    message(FATAL_ERROR "src/native/get-sqlite.cmake does not check the "
                        "download against SQLITE_DIGEST")
endif()

foreach(file "${script}" "${recipe}")
    file(READ "${file}" text)
    foreach(literal "${version}" "${digest}")
        string(REPLACE "." "\\." pattern "${literal}")
        if(text MATCHES "${pattern}")
            message(FATAL_ERROR "${file} spells `${literal}`, "
                                "which belongs in tools/sqlite-pin alone")
        endif()
    endforeach()
endforeach()
