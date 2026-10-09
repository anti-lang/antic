# The version, the digest and the URL of a third-party library live in
# tools/<NAME>-pin alone. Its download script reads them from there, and
# the record of the upstream sources of the runtime archive reads the URL
# there too, so no second copy can drift. Run with cmake -P and these
# values:
#   ROOT    the repository
#   NAME    the library, as in tools/<NAME>-pin. The keys of the pin are
#           <NAME>_VERSION, <NAME>_DIGEST and <NAME>_URL in capitals
#   SCRIPT  the download script, relative to ROOT
#   FILES   optional files relative to ROOT, separated by commas, that
#           spell neither the version nor the digest either
#   HASH    the algorithm of EXPECTED_HASH, SHA256 or SHA3_256
#   PARTS   the number of parts of the version, 2 or 3
#   YEAR    optional, ON when the pin also holds <NAME>_YEAR
#   ABSENT  optional text that the file ABSENT_IN, relative to ROOT, does
#           not hold
#
# DESIGN: every search for a literal is string(FIND). A version used as a
# regular expression lets `.` match any byte, so a text one dot short of
# the version was refused as a copy of it.

cmake_minimum_required(VERSION 3.21)

string(TOUPPER "${NAME}" key)
set(pin "${ROOT}/tools/${NAME}-pin")
set(script "${ROOT}/${SCRIPT}")
string(REPLACE "," ";" others "${FILES}")
list(TRANSFORM others PREPEND "${ROOT}/")
foreach(file "${pin}" "${script}" ${others})
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

# Set <out> to the value of <key>_<field> in the pin.
function(read_pin out field)
    file(STRINGS "${pin}" line REGEX "^${key}_${field}=")
    string(REGEX REPLACE "^${key}_${field}=" "" value "${line}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

read_pin(version VERSION)
read_pin(digest DIGEST)
set(form "[0-9]+")
foreach(part RANGE 2 ${PARTS})
    string(APPEND form "\\.[0-9]+")
endforeach()
if(NOT version MATCHES "^${form}$")
    message(FATAL_ERROR "${key}_VERSION is `${version}`, expected a version "
                        "of ${PARTS} parts")
endif()
string(LENGTH "${digest}" length)
if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
    message(FATAL_ERROR "${key}_DIGEST is `${digest}`, expected 64 hex digits")
endif()
read_pin(url URL)
if(NOT url MATCHES "^https://[^ ]+$")
    message(FATAL_ERROR "${key}_URL is `${url}`, which is not HTTPS")
endif()
if(YEAR)
    read_pin(year YEAR)
    if(NOT year MATCHES "^20[0-9][0-9]$")
        message(FATAL_ERROR "${key}_YEAR is `${year}`, expected a year")
    endif()
endif()

# The source comes over HTTPS, from the URL of the pin or one the script
# spells, and the digest is checked on every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    string(FIND "${line}" "https://" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${SCRIPT} reads `${line}`, which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
string(FIND "${text}" "EXPECTED_HASH \"${HASH}=\${${key}_DIGEST}\"" at)
if(at EQUAL -1)
    message(FATAL_ERROR "${SCRIPT} does not check the download against "
                        "${key}_DIGEST with ${HASH}")
endif()

foreach(file "${script}" ${others})
    file(READ "${file}" text)
    foreach(literal "${version}" "${digest}")
        string(FIND "${text}" "${literal}" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${file} spells `${literal}`, which belongs "
                                "in tools/${NAME}-pin alone")
        endif()
    endforeach()
endforeach()

if(DEFINED ABSENT)
    file(READ "${ROOT}/${ABSENT_IN}" text)
    string(FIND "${text}" "${ABSENT}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "${ABSENT_IN} holds `${ABSENT}`")
    endif()
endif()
