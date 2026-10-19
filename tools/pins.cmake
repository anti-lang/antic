# The one reader of the pins of tools/, the files <name>-pin and
# sysroot-pins. A pin holds comments and KEY=value rows. Every script that
# needs a value of a pin includes this file, and the test pin_readers
# refuses a pin read anywhere else.
#
# antic_read_pins(<file>) sets every row of <file> whose key starts with
# a capital letter as a variable of the caller.
#
# antic_pin_value(<out> <file> <key> [OPTIONAL]) sets <out> to the value of
# the row <key>. A row that is missing or empty stops the run, unless
# OPTIONAL is given, and then <out> is empty.
#
# antic_pin_url(<out> <name>) sets <out> to the URL of the source package
# that tools/<name>-pin names as <NAME>_URL, with @VERSION@ replaced by
# <NAME>_VERSION, @YEAR@ by <NAME>_YEAR and @NUMBER@ by the version in
# the form of sqlite.org. The caller has read the pin with
# antic_read_pins, so each pin is read once. antic_get_source of
# src/native/get-source.cmake and the record of
# tools/upstream-sources.cmake take the URL from here, so each pin holds
# it once.

function(antic_read_pins file)
    file(STRINGS "${file}" pins REGEX "^[A-Z]")
    foreach(line IN LISTS pins)
        string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
        list(GET pair 0 key)
        list(GET pair 1 value)
        set("${key}" "${value}" PARENT_SCOPE)
    endforeach()
endfunction()

function(antic_pin_value out file key)
    file(STRINGS "${file}" rows REGEX "^${key}=")
    set(value "")
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^${key}=" "" value "${row}")
    endforeach()
    if(value STREQUAL "" AND NOT "${ARGN}" STREQUAL "OPTIONAL")
        get_filename_component(name "${file}" NAME)
        message(FATAL_ERROR "tools/${name} has no row ${key}")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(antic_pin_url out name)
    string(TOUPPER "${name}" key)
    foreach(field VERSION URL)
        if(NOT DEFINED "${key}_${field}" OR "${${key}_${field}}" STREQUAL "")
            message(FATAL_ERROR "tools/${name}-pin has no ${key}_${field}")
        endif()
    endforeach()
    set(url "${${key}_URL}")
    string(REPLACE "@VERSION@" "${${key}_VERSION}" url "${url}")
    if(url MATCHES "@YEAR@")
        if(NOT DEFINED "${key}_YEAR")
            message(FATAL_ERROR "tools/${name}-pin has no ${key}_YEAR")
        endif()
        string(REPLACE "@YEAR@" "${${key}_YEAR}" url "${url}")
    endif()
    if(url MATCHES "@NUMBER@")
        # X.Y.Z is written as X, then Y and Z in two digits each, then 00,
        # so 3.8.2 would be 3080200.
        if(NOT "${${key}_VERSION}" MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            message(FATAL_ERROR "${key}_VERSION is `${${key}_VERSION}`, "
                                "expected X.Y.Z")
        endif()
        set(number "${CMAKE_MATCH_1}")
        foreach(part "${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
            if(part LESS 10)
                string(APPEND number "0")
            endif()
            string(APPEND number "${part}")
        endforeach()
        string(APPEND number "00")
        string(REPLACE "@NUMBER@" "${number}" url "${url}")
    endif()
    if(url MATCHES "@[A-Z]+@")
        message(FATAL_ERROR "${key}_URL holds a name the pin does not fill: "
                            "${url}")
    endif()
    set(${out} "${url}" PARENT_SCOPE)
endfunction()
