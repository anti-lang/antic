# tests/run_macos_sdk.cmake reports itself skipped for a machine that has
# no SDK of the pinned version, and fails for an SDK of that name whose
# libSystem.tbd changed. Run with cmake -P and these values:
#   RUNNER  tests/run_macos_sdk.cmake
#   ROOT    the repository
#   WORK    a directory for the files
#
# SDKS of the runner names the directory it looks in for the SDK.

cmake_minimum_required(VERSION 3.21)

if(NOT APPLE)
    message("SKIP: the Apple SDK is pinned on the Mac that packs a release")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/none")
file(STRINGS "${ROOT}/tools/macos-sdk-pin" row REGEX "^MACOS_SDK_NAME=")
string(REPLACE "MACOS_SDK_NAME=" "" name "${row}")
file(MAKE_DIRECTORY "${WORK}/changed/${name}/usr/lib")
file(WRITE "${WORK}/changed/${name}/usr/lib/libSystem.tbd" "# changed\n")

set(failures "")
# sdk_case(<name> <directory> skip|fail)
function(sdk_case name sdks verdict)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${ROOT}"
                            "-DSDKS=${sdks}" "-DWORK=${WORK}/run-${name}"
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(said "${out}${err}")
    if(verdict STREQUAL "skip")
        if(NOT status EQUAL 0 OR NOT said MATCHES "SKIP: ")
            set(failures "${failures}${name}: not skipped\n${said}\n"
                PARENT_SCOPE)
        endif()
    elseif(status EQUAL 0 OR said MATCHES "SKIP: " OR
           NOT said MATCHES "digest")
        set(failures "${failures}${name}: not failed on the digest\n${said}\n"
            PARENT_SCOPE)
    endif()
endfunction()

sdk_case(none "${WORK}/none" skip)
sdk_case(changed "${WORK}/changed" fail)

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_macos_sdk.cmake:\n${failures}")
endif()
file(REMOVE_RECURSE "${WORK}")
