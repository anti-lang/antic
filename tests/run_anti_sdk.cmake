# anti sdk export packs the .tbd stubs and the version of a MacOSX.sdk into
# apple-sdk-<version>.tar.xz, and runs on a Mac alone. anti sdk import
# unpacks such a bundle into sdk/ of both macOS sysroots on any host,
# records the digest of the bundle and fetches nothing. A stand-in SDK
# takes the place of Apple's.
#
#   cmake -DANTI=<anti> -DWORK=<dir> -P tests/run_anti_sdk.cmake

file(REMOVE_RECURSE "${WORK}")

# A stand-in SDK: stubs in usr/lib and in a framework whose top-level stub
# and Versions/Current are symbolic links, as Apple lays them out, beside
# a header and a dylib that are no stubs.
set(sdk "${WORK}/MacOSX9.9.sdk")
file(WRITE "${sdk}/SDKSettings.json" "{\"CanonicalName\":\"macosx9.9\",\"Version\":\"9.9\"}\n")
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
    file(COPY_FILE "${framework}/Versions/A/Foo.tbd" "${framework}/Foo.tbd")
endif()

set(stubs usr/lib/libSystem.tbd usr/lib/system/libsystem_c.tbd
          System/Library/Frameworks/Foo.framework/Foo.tbd
          System/Library/Frameworks/Foo.framework/Versions/A/Foo.tbd)
set(bundle "${WORK}/out/apple-sdk-9.9.tar.xz")
file(MAKE_DIRECTORY "${WORK}/out")
execute_process(COMMAND "${ANTI}" sdk export --sdk "${sdk}" -o "${WORK}/out"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(CMAKE_HOST_APPLE)
    if(NOT status EQUAL 0 OR NOT EXISTS "${bundle}")
        message(FATAL_ERROR "anti sdk export wrote no ${bundle}:\n${out}${err}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${bundle}"
                    OUTPUT_VARIABLE listing COMMAND_ERROR_IS_FATAL ANY ENCODING NONE)
    string(REGEX REPLACE "(^|\n)\\./" "\\1" listing "${listing}")
    foreach(entry sdk-version ${stubs})
        if(NOT listing MATCHES "(^|\n)${entry}\n")
            message(FATAL_ERROR "${bundle} lacks ${entry}:\n${listing}")
        endif()
    endforeach()
    foreach(absent Versions/Current libz.dylib stdio.h Foo.h SDKSettings)
        if(listing MATCHES "${absent}")
            message(FATAL_ERROR "${bundle} holds ${absent}:\n${listing}")
        endif()
    endforeach()
else()
    if(status EQUAL 0 OR NOT "${out}${err}" MATCHES "Mac")
        message(FATAL_ERROR "anti sdk export ran on a host that is no Mac:\n"
                            "${out}${err}")
    endif()
    # The bundle that export writes on a Mac, made here by CMake.
    set(stage "${WORK}/stage")
    file(WRITE "${stage}/sdk-version" "9.9\n")
    foreach(stub IN LISTS stubs)
        file(READ "${sdk}/${stub}" text)
        file(WRITE "${stage}/${stub}" "${text}")
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cJf "${bundle}" .
                    WORKING_DIRECTORY "${stage}" COMMAND_ERROR_IS_FATAL ANY)
endif()

# Fail unless <file> holds <text>.
function(expect_text file text)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "${file} is missing")
    endif()
    file(READ "${file}" actual)
    if(NOT actual STREQUAL text)
        message(FATAL_ERROR "${file} holds '${actual}', not '${text}'")
    endif()
endfunction()

function(import file result)
    execute_process(COMMAND "${ANTI}" sdk import "${file}"
                            --sysroot "${WORK}/sysroot"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                    ENCODING NONE)
    set(${result} "${status}" PARENT_SCOPE)
    set(output "${out}${err}" PARENT_SCOPE)
endfunction()

# An SDK imported earlier is replaced whole.
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/usr/lib/stale.tbd" "stale\n")
import("${bundle}" status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti sdk import failed:\n${output}")
endif()
file(SHA256 "${bundle}" digest)
foreach(target macos-arm64 macos-x86_64)
    set(copy "${WORK}/sysroot/${target}/sdk")
    expect_text("${copy}/usr/lib/libSystem.tbd" "libSystem\n")
    expect_text("${copy}/System/Library/Frameworks/Foo.framework/Foo.tbd" "Foo\n")
    expect_text("${copy}/sdk-version" "9.9\n")
    expect_text("${copy}/digest" "${digest}\n")
    if(EXISTS "${copy}/usr/lib/stale.tbd")
        message(FATAL_ERROR "anti sdk import kept a stub of the SDK before")
    endif()
endforeach()

# A file that is no bundle is refused, and the SDK before stays.
file(WRITE "${WORK}/empty/readme.txt" "no SDK\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cJf "${WORK}/empty.tar.xz" .
                WORKING_DIRECTORY "${WORK}/empty" COMMAND_ERROR_IS_FATAL ANY)
import("${WORK}/empty.tar.xz" status)
if(status EQUAL 0 OR NOT output MATCHES "sdk-version")
    message(FATAL_ERROR "anti sdk import took a file that is no bundle:\n${output}")
endif()
expect_text("${WORK}/sysroot/macos-arm64/sdk/sdk-version" "9.9\n")
import("${WORK}/missing.tar.xz" status)
if(status EQUAL 0)
    message(FATAL_ERROR "anti sdk import took a file that does not exist")
endif()
