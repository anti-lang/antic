# The Apple SDK a release links its macOS binaries against is the one
# tools/macos-sdk-pin names, and an SDK that changed under that name is
# refused. The packer then links both macOS targets against it.
#
#   cmake -DROOT=<repository> -DCLANG=<clang> -DLLVM_BIN=<dir> -DWORK=<dir>
#         -P tests/run_macos_sdk.cmake

if(NOT APPLE)
    message("SKIP: the Apple SDK is pinned on the Mac that packs a release")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(script "${ROOT}/tools/macos-sdk.cmake")
set(pin "${ROOT}/tools/macos-sdk-pin")

# Print the path of the pinned SDK, or fail with what it said.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DPIN=${pin}" -P "${script}"
                OUTPUT_VARIABLE sdk ERROR_VARIABLE err
                RESULT_VARIABLE failed
                OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING NONE)
file(STRINGS "${pin}" row REGEX "^MACOS_SDK_VERSION=")
string(REPLACE "MACOS_SDK_VERSION=" "" version "${row}")
if(NOT failed EQUAL 0)
    message("SKIP: macOS SDK ${version} is not installed here\n${err}")
    return()
endif()
if(NOT IS_DIRECTORY "${sdk}")
    message(FATAL_ERROR "the pinned SDK is `${sdk}`, which is no directory")
endif()
# The path names the pinned version and not another one.
file(STRINGS "${pin}" row REGEX "^MACOS_SDK_NAME=")
string(REPLACE "MACOS_SDK_NAME=" "" name "${row}")
get_filename_component(found "${sdk}" NAME)
if(NOT found STREQUAL "${name}")
    message(FATAL_ERROR "the pinned SDK is ${found} and ${pin} names ${name}")
endif()

# DESIGN: the digest is what catches an SDK that changed under its own
# name, which is the case a version alone does not see. A copy of the
# tree with one byte added to libSystem.tbd stands in for it.
set(fake "${WORK}/SDKs/${name}")
file(MAKE_DIRECTORY "${fake}/usr/lib")
file(READ "${sdk}/usr/lib/libSystem.tbd" tbd)
file(WRITE "${fake}/usr/lib/libSystem.tbd" "${tbd}\n# changed\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DPIN=${pin}"
                        "-DSDKS=${WORK}/SDKs" -P "${script}"
                OUTPUT_VARIABLE out ERROR_VARIABLE err
                RESULT_VARIABLE refused ENCODING NONE)
if(refused EQUAL 0)
    message(FATAL_ERROR "a libSystem.tbd that is not the pinned one passed")
endif()
if(NOT "${out}${err}" MATCHES "digest")
    message(FATAL_ERROR "the refusal does not name the digest\n${out}${err}")
endif()

# An SDK of the pinned version that is nowhere fails naming the version,
# so a machine without it reads what to install.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DPIN=${pin}"
                        "-DSDKS=${WORK}/none" -P "${script}"
                OUTPUT_VARIABLE out ERROR_VARIABLE err
                RESULT_VARIABLE refused ENCODING NONE)
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "${version}")
    message(FATAL_ERROR "a missing SDK does not name the pinned version\n"
                        "${out}${err}")
endif()

# The packer links both macOS targets against that SDK. One small
# program stands for antic here, because what is read is the SDK of the
# link and not the compiler.
if(NOT CLANG OR NOT EXISTS "${CLANG}")
    message("SKIP: the pinned clang is not built")
    return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")
file(WRITE "${WORK}/probe.c" "int main(void) { return 0; }\n")
foreach(pair "macos-arm64=arm64-apple-macos11" "macos-x86_64=x86_64-apple-macos11")
    string(REPLACE "=" ";" parts "${pair}")
    list(GET parts 0 host)
    list(GET parts 1 triple)
    execute_process(
        COMMAND "${CLANG}" ${ANTIC_C_WARNINGS} "--target=${triple}"
                -isysroot "${sdk}"
                "--ld-path=${LLVM_BIN}/ld64.lld" -o "${WORK}/probe-${host}"
                "${WORK}/probe.c"
        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT failed EQUAL 0)
        message(FATAL_ERROR "${host} does not link against the pinned SDK "
                            "${sdk}\n${out}${err}")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
