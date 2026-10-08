# A release build links the program and the runtime as bitcode through
# full LTO by default, and `--lto none` links both as objects, the link of
# before. A build with a profile takes the default as well. A Windows
# program that hosts plugins, which LTO cannot serve without being asked,
# keeps the objects.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCE    a program that exits with 42
#   HOSTING   a program that hosts plugins
#   WORK      a directory for the output

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Run antic with ARGN, which must succeed and print nothing.
function(antic)
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${ARGN}
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic ${ARGN} gave ${status}\n${out}${err}")
    endif()
endfunction()

# Fail unless the object at path is bitcode, or with NOT unless it is
# not. Bitcode starts with `BC` 0xC0DE, or on Darwin with the magic of
# the wrapper around it, 0x0B17C0DE in little-endian order.
function(expect_bitcode path)
    cmake_parse_arguments(PARSE_ARGV 1 arg "NOT" "" "")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${path} is missing")
    endif()
    file(READ "${path}" magic LIMIT 4 HEX)
    set(bitcode FALSE)
    if(magic MATCHES "^(4243c0de|dec0170b)$")
        set(bitcode TRUE)
    endif()
    if(arg_NOT AND bitcode)
        message(FATAL_ERROR "${path} is bitcode")
    elseif(NOT arg_NOT AND NOT bitcode)
        message(FATAL_ERROR "${path} starts with ${magic}, not the magic of bitcode")
    endif()
endfunction()

# Run the program at path, which must exit with 42. The object of a
# program stands beside it under the name of the program and its suffix.
function(expect_run path)
    execute_process(COMMAND "${path}" RESULT_VARIABLE status)
    if(NOT status EQUAL 42)
        message(FATAL_ERROR "${path} exits with ${status}, expected 42")
    endif()
endfunction()

set(exe "")
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
set(object ".o")
if(CMAKE_HOST_WIN32)
    set(object ".obj")
endif()

antic(-o "${WORK}/default${exe}" "${SOURCE}")
expect_bitcode("${WORK}/default${exe}${object}")
expect_run("${WORK}/default${exe}")

antic(--lto full -o "${WORK}/full${exe}" "${SOURCE}")
expect_bitcode("${WORK}/full${exe}${object}")
expect_run("${WORK}/full${exe}")

antic(--lto none -o "${WORK}/none${exe}" "${SOURCE}")
expect_bitcode("${WORK}/none${exe}${object}" NOT)
expect_run("${WORK}/none${exe}")

antic(--profile-generate -o "${WORK}/profile${exe}" "${SOURCE}")
expect_bitcode("${WORK}/profile${exe}${object}")

antic(--profile-generate --lto none -o "${WORK}/profile-none${exe}"
      "${SOURCE}")
expect_bitcode("${WORK}/profile-none${exe}${object}" NOT)

# The .def file of a Windows host lists the COFF symbols of its object.
if(EXISTS "${RUNTIME}/sysroot/windows-x86_64")
    antic(--target windows-x86_64 -o "${WORK}/host.exe" "${HOSTING}")
    expect_bitcode("${WORK}/host.exe.obj" NOT)
endif()
