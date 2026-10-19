# The version, the digest and the URL of a third-party library live in
# tools/<NAME>-pin alone. Its download script reads them from there, and
# the record of the upstream sources of the runtime archive reads the URL
# there too, so no second copy can drift. Run with cmake -P and these
# values:
#   ROOT    the repository
#   NAME    the library, as in tools/<NAME>-pin. The keys of the pin are
#           <NAME>_VERSION, <NAME>_DIGEST and <NAME>_URL in capitals
#   SCRIPT  the script of antic_get_source, relative to ROOT, which
#           hands antic_download the digest of the pin. The test
#           downloads_once holds the check of the digest in antic_download
#   CALLER  the file, relative to ROOT, whose call of antic_get_source
#           fetches the library. It spells neither the version nor the
#           digest
#   FILES   optional files relative to ROOT, separated by commas, that
#           spell neither the version nor the digest either
#   HASH    the algorithm of the digest, SHA256 or SHA3_256, which the
#           call passes on to EXPECTED_HASH
#   PARTS   the number of parts of the version, 2 or 3
#   YEAR    optional, ON when the pin also holds <NAME>_YEAR
#   ABSENT  optional text that the file ABSENT_IN, relative to ROOT, does
#           not hold
#
# DESIGN: every search for a literal is string(FIND). A version used as a
# regular expression lets `.` match any byte, so a text one dot short of
# the version was refused as a copy of it.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/pins.cmake")

string(TOUPPER "${NAME}" key)
set(pin "${ROOT}/tools/${NAME}-pin")
set(script "${ROOT}/${SCRIPT}")
set(caller "${ROOT}/${CALLER}")
string(REPLACE "," ";" others "${FILES}")
list(TRANSFORM others PREPEND "${ROOT}/")
foreach(file "${pin}" "${script}" "${caller}" ${others})
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
endforeach()

# A row that is missing reads as empty, and the refusals below name the
# value they expected.
antic_pin_value(version "${pin}" "${key}_VERSION" OPTIONAL)
antic_pin_value(digest "${pin}" "${key}_DIGEST" OPTIONAL)
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
antic_pin_value(url "${pin}" "${key}_URL" OPTIONAL)
if(NOT url MATCHES "^https://[^ ]+$")
    message(FATAL_ERROR "${key}_URL is `${url}`, which is not HTTPS")
endif()
if(YEAR)
    antic_pin_value(year "${pin}" "${key}_YEAR" OPTIONAL)
    if(NOT year MATCHES "^20[0-9][0-9]$")
        message(FATAL_ERROR "${key}_YEAR is `${year}`, expected a year")
    endif()
endif()

# The source comes over HTTPS, from the URL of the pin or one the script
# spells, and the digest of the pin goes with every download.
file(STRINGS "${script}" urls REGEX "://")
foreach(line IN LISTS urls)
    string(FIND "${line}" "https://" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${SCRIPT} reads `${line}`, which is not HTTPS")
    endif()
endforeach()
file(READ "${script}" text)
string(FIND "${text}" "\"\${algorithm}\" \"\${\${key}_DIGEST}\"" at)
if(at EQUAL -1)
    message(FATAL_ERROR "${SCRIPT} does not hand antic_download the digest "
                        "of the pin")
endif()

# The call names the library and the algorithm of its digest, as its
# second and its fourth argument.
file(READ "${caller}" text)
string(REGEX MATCHALL "antic_get_source\\([^)]*\\)" calls "${text}")
set(asked "")
foreach(call IN LISTS calls)
    string(REGEX REPLACE "^antic_get_source\\((.*)\\)$" "\\1" arguments
           "${call}")
    separate_arguments(arguments UNIX_COMMAND "${arguments}")
    list(LENGTH arguments count)
    if(count EQUAL 5)
        list(GET arguments 1 name)
        list(GET arguments 3 algorithm)
        if(name STREQUAL NAME)
            set(asked "${algorithm}")
        endif()
    endif()
endforeach()
if(asked STREQUAL "")
    message(FATAL_ERROR "${CALLER} does not fetch ${NAME} with antic_get_source")
endif()
if(NOT asked STREQUAL HASH)
    message(FATAL_ERROR "${CALLER} checks ${NAME} with ${asked}, and the pin "
                        "holds a digest of ${HASH}")
endif()

foreach(file "${script}" "${caller}" ${others})
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
