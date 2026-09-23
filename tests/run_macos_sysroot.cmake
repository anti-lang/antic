# tools/zig-stubs-pin names a release of Zig, whose generated stubs of
# libSystem link a program for macos-arm64 and macos-x86_64 on every host.
# tools/get-sysroot.cmake takes the stubs and the headers of the macOS C
# library from its source archive into sysroot/macos-<cpu>, with the SDK
# version of the stubs, Zig's MIT licence and the text of the APSL. A
# stand-in archive served from a file:// URL takes the place of the real
# one in a copy of the script, and the test runs on every host.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_macos_sysroot.cmake

# The pin names a release of Zig on ziglang.org and the APSL of a release
# of the SPDX licence list.
file(STRINGS "${ROOT}/tools/zig-stubs-pin" pins REGEX "^[A-Z]")
foreach(line IN LISTS pins)
    string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
    list(GET pair 0 key)
    list(GET pair 1 value)
    set("real_${key}" "${value}")
endforeach()
if(NOT real_ZIG_TAG MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "ZIG_TAG is '${real_ZIG_TAG}'")
endif()
if(NOT real_ZIG_URL STREQUAL "https://ziglang.org/download/@TAG@/zig-@TAG@.tar.xz")
    message(FATAL_ERROR "ZIG_URL is '${real_ZIG_URL}'")
endif()
if(NOT real_APSL_URL MATCHES "^https://raw\\.githubusercontent\\.com/spdx/license-list-data/v[0-9.]+/text/APSL-2\\.0\\.txt$")
    message(FATAL_ERROR "APSL_URL is '${real_APSL_URL}'")
endif()
foreach(key ZIG_DIGEST APSL_DIGEST)
    string(LENGTH "${real_${key}}" length)
    if(NOT real_${key} MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
        message(FATAL_ERROR "${key} is '${real_${key}}'")
    endif()
endforeach()

# A stand-in source archive of Zig 9.9.9 with the files the script takes.
file(REMOVE_RECURSE "${WORK}")
set(zig "${WORK}/source/zig-9.9.9")
file(WRITE "${zig}/lib/libc/darwin/libSystem.tbd" "stub\n")
file(WRITE "${zig}/lib/libc/darwin/SDKSettings.json" "{\"MinimalDisplayName\":\"26.4\"}\n")
file(WRITE "${zig}/lib/libc/include/any-darwin-any/stdio.h" "/* stdio */\n")
file(WRITE "${zig}/LICENSE" "The licence of Zig\n")
file(WRITE "${zig}/lib/std/std.zig" "// not taken\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cJf "${WORK}/zig-9.9.9.tar.xz"
                        zig-9.9.9
                WORKING_DIRECTORY "${WORK}/source" COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${WORK}/apsl.txt" "The text of the APSL\n")

# A copy of the script beside the real sysroot pins and a Zig pin that
# names the stand-ins.
file(MAKE_DIRECTORY "${WORK}/tools")
file(COPY_FILE "${ROOT}/tools/get-sysroot.cmake" "${WORK}/tools/get-sysroot.cmake")
file(COPY_FILE "${ROOT}/tools/deps-dir.cmake" "${WORK}/tools/deps-dir.cmake")
file(COPY_FILE "${ROOT}/tools/sysroot-pins" "${WORK}/tools/sysroot-pins")
function(write_pin zig_digest)
    file(SHA256 "${WORK}/apsl.txt" apsl_digest)
    file(WRITE "${WORK}/tools/zig-stubs-pin"
"ZIG_TAG=9.9.9
ZIG_URL=file://${WORK}/zig-@TAG@.tar.xz
ZIG_DIGEST=${zig_digest}
APSL_URL=file://${WORK}/apsl.txt
APSL_DIGEST=${apsl_digest}
")
endfunction()
file(SHA256 "${WORK}/zig-9.9.9.tar.xz" zig_digest)
write_pin("${zig_digest}")

function(get_sysroot result)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                            -DLLVM_BIN=bin "-DTARGETS=macos-arm64;macos-x86_64"
                            -P "${WORK}/tools/get-sysroot.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                    ENCODING NONE)
    set(${result} "${status}" PARENT_SCOPE)
    set(output "${out}${err}" PARENT_SCOPE)
endfunction()

# Fail unless <file> holds <text>.
function(expect_text file text)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing:\n${output}")
    endif()
    file(READ "${file}" actual)
    if(NOT actual STREQUAL text)
        message(FATAL_ERROR "${file} holds '${actual}', not '${text}'")
    endif()
endfunction()

get_sysroot(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed:\n${output}")
endif()
foreach(target macos-arm64 macos-x86_64)
    set(root "${WORK}/sysroot/${target}")
    expect_text("${root}/usr/lib/libSystem.tbd" "stub\n")
    expect_text("${root}/usr/include/stdio.h" "/* stdio */\n")
    expect_text("${root}/sdk-version" "26.4\n")
endforeach()
expect_text("${WORK}/sysroot/licenses/zig.txt" "The licence of Zig\n")
expect_text("${WORK}/sysroot/licenses/apsl.txt" "The text of the APSL\n")
if(EXISTS "${WORK}/sysroot/macos-arm64/usr/include/std.zig" OR
   EXISTS "${WORK}/sysroot/macos-arm64/lib")
    message(FATAL_ERROR "the macOS sysroot took files beside the stubs and headers")
endif()

# The SDK that APPLE_SDK or anti sdk import put into sdk/ stays through
# a run that does not name it.
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/sdk-version" "26.5\n")
get_sysroot(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the second run failed:\n${output}")
endif()
expect_text("${WORK}/sysroot/macos-arm64/sdk/sdk-version" "26.5\n")

# An archive of another digest is refused.
write_pin(0000000000000000000000000000000000000000000000000000000000000000)
file(REMOVE_RECURSE "${WORK}/sysroot/.download")
get_sysroot(status)
if(status EQUAL 0 OR NOT output MATCHES "expected")
    message(FATAL_ERROR "tools/get-sysroot.cmake took an archive of another "
                        "digest:\n${output}")
endif()

# APPLE_SDK names a MacOSX.sdk. Its .tbd stubs under usr/lib and
# System/Library/Frameworks go to sdk/ of each macOS sysroot as regular
# files, with the version of the SDK and the digest of what was copied. A
# symbolic link to a stub becomes a copy of it, as the top-level stub of a
# framework is, and a linked directory such as Versions/Current stays out,
# since lld reads no path through it. Headers and dylibs stay out too.
write_pin("${zig_digest}")
set(sdk "${WORK}/MacOSX9.9.sdk")
file(WRITE "${sdk}/SDKSettings.json" "{\"Version\":\"9.9\",\"MinimalDisplayName\":\"9.9\"}\n")
file(WRITE "${sdk}/usr/lib/libSystem.tbd" "libSystem\n")
file(WRITE "${sdk}/usr/lib/system/libsystem_c.tbd" "libsystem_c\n")
file(WRITE "${sdk}/usr/lib/libz.dylib" "not a stub\n")
file(WRITE "${sdk}/usr/include/stdio.h" "/* header */\n")
set(framework "${sdk}/System/Library/Frameworks/Foo.framework")
file(WRITE "${framework}/Versions/A/Foo.tbd" "Foo\n")
file(WRITE "${framework}/Versions/A/Headers/Foo.h" "/* header */\n")
# A copy of an SDK on Windows holds no symbolic link, so the stand-in
# there copies instead.
set(linked 1)
if(NOT CMAKE_HOST_WIN32)
    file(CREATE_LINK A "${framework}/Versions/Current" SYMBOLIC RESULT linked)
endif()
if(linked STREQUAL "0")
    file(CREATE_LINK Versions/Current/Foo.tbd "${framework}/Foo.tbd" SYMBOLIC)
else()
    # A host without the privilege of a symbolic link copies, as the copy
    # of an SDK on such a host holds.
    file(COPY "${framework}/Versions/A/" DESTINATION "${framework}/Versions/Current")
    file(COPY_FILE "${framework}/Versions/A/Foo.tbd" "${framework}/Foo.tbd")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                        -DLLVM_BIN=bin "-DTARGETS=macos-arm64;macos-x86_64"
                        "-DAPPLE_SDK=${sdk}" -P "${WORK}/tools/get-sysroot.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
set(output "${out}${err}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed with APPLE_SDK:\n${output}")
endif()
foreach(target macos-arm64 macos-x86_64)
    set(copy "${WORK}/sysroot/${target}/sdk")
    expect_text("${copy}/usr/lib/libSystem.tbd" "libSystem\n")
    expect_text("${copy}/usr/lib/system/libsystem_c.tbd" "libsystem_c\n")
    set(stubs System/Library/Frameworks/Foo.framework/Foo.tbd
              System/Library/Frameworks/Foo.framework/Versions/A/Foo.tbd)
    if(NOT linked STREQUAL "0")
        list(APPEND stubs
             System/Library/Frameworks/Foo.framework/Versions/Current/Foo.tbd)
    elseif(EXISTS "${copy}/System/Library/Frameworks/Foo.framework/Versions/Current")
        message(FATAL_ERROR "${copy} took Versions/Current, a linked directory")
    endif()
    list(APPEND stubs usr/lib/libSystem.tbd usr/lib/system/libsystem_c.tbd)
    list(SORT stubs)
    set(lines "")
    foreach(stub IN LISTS stubs)
        if(IS_SYMLINK "${copy}/${stub}")
            message(FATAL_ERROR "${copy}/${stub} is a symbolic link")
        endif()
        file(SHA256 "${copy}/${stub}" one)
        string(APPEND lines "${one}  ./${stub}\n")
    endforeach()
    expect_text("${copy}/System/Library/Frameworks/Foo.framework/Foo.tbd" "Foo\n")
    foreach(absent usr/lib/libz.dylib usr/include/stdio.h
                   System/Library/Frameworks/Foo.framework/Versions/A/Headers/Foo.h)
        if(EXISTS "${copy}/${absent}")
            message(FATAL_ERROR "${copy} holds ${absent}, which is no stub")
        endif()
    endforeach()
    expect_text("${copy}/sdk-version" "9.9\n")
    string(SHA256 digest "${lines}")
    expect_text("${copy}/digest" "${digest}\n")
endforeach()

# A directory that is no SDK is refused.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                        -DLLVM_BIN=bin -DTARGETS=macos-arm64
                        "-DAPPLE_SDK=${WORK}/source" -P "${WORK}/tools/get-sysroot.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(status EQUAL 0 OR NOT "${out}${err}" MATCHES "SDKSettings.json")
    message(FATAL_ERROR "tools/get-sysroot.cmake took a directory that is no "
                        "SDK:\n${out}${err}")
endif()
