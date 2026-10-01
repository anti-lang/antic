# A shared library takes the link facts of a program of its module. A
# macOS library of `--framework CoreFoundation` names the framework, a
# Linux library of `--linux-lib X11` links in the glibc mode and loads
# libX11 and libc, and a library of `--memory-checks` names the runtime of
# AddressSanitizer on macOS and on windows-x86_64. Each link fails, or
# names nothing, when the library takes the inputs of a plain program.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   READOBJ   llvm-readobj of the pinned release
#   OBJDUMP   llvm-objdump of the pinned release
#   RUNTIME   the runtime archive
#   WORK      a directory this run writes into

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Run antic on source for target with the options, which must succeed.
function(shared_library target source library)
    execute_process(COMMAND "${ANTIC}" --target "${target}" --lib shared
                            --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                            ${ARGN} -o "${library}" "${source}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --lib shared for ${target} failed with "
                            "${status}: ${ARGN}\n${out}${err}")
    endif()
endfunction()

# Check that the text a tool printed for library names each pattern.
function(names library text)
    foreach(pattern IN LISTS ARGN)
        if(NOT text MATCHES "${pattern}")
            message(FATAL_ERROR "${library} lacks ${pattern}\n${text}")
        endif()
    endforeach()
endfunction()

file(WRITE "${WORK}/cf.anti"
     "extern fn CFAbsoluteTimeGetCurrent() -> f64;\n\n"
     "export fn now() -> f64\n{\n\treturn CFAbsoluteTimeGetCurrent();\n}\n")
file(WRITE "${WORK}/xl.anti"
     "extern fn XOpenDisplay(name: ?*byte) -> ?*byte;\n\n"
     "export fn displays() -> int\n{\n"
     "\tif XOpenDisplay(none) == none {\n\t\treturn 0;\n\t}\n"
     "\treturn 1;\n}\n")
file(WRITE "${WORK}/mc.anti"
     "export fn read(p: *int) -> int\n{\n\treturn *p;\n}\n")

# The frameworks reach Apple's SDK, which a Mac holds and another host
# holds once anti sdk import put it in the sysroot.
if(CMAKE_HOST_APPLE OR EXISTS "${RUNTIME}/sysroot/macos-arm64/sdk/sdk-version")
    set(library "${WORK}/libcf.dylib")
    shared_library(macos-arm64 "${WORK}/cf.anti" "${library}"
                   --framework CoreFoundation)
    execute_process(COMMAND "${OBJDUMP}" --macho --dylibs-used "${library}"
                    OUTPUT_VARIABLE text ERROR_VARIABLE err ENCODING NONE)
    names("${library}" "${text}${err}"
          "CoreFoundation\\.framework/Versions/A/CoreFoundation")
endif()

foreach(target linux-arm64 linux-x86_64)
    set(library "${WORK}/libxl-${target}.so")
    shared_library(${target} "${WORK}/xl.anti" "${library}" --linux-lib X11)
    execute_process(COMMAND "${READOBJ}" --needed-libs "${library}"
                    OUTPUT_VARIABLE text ERROR_VARIABLE err ENCODING NONE)
    names("${library}" "${text}${err}" "libX11\\.so\\.6" "libm\\.so\\.6"
          "libc\\.so\\.6")
endforeach()

set(library "${WORK}/libmc.dylib")
shared_library(macos-arm64 "${WORK}/mc.anti" "${library}" --memory-checks)
execute_process(COMMAND "${OBJDUMP}" --macho --dylibs-used "${library}"
                OUTPUT_VARIABLE text ERROR_VARIABLE err ENCODING NONE)
names("${library}" "${text}${err}" "libclang_rt\\.asan_osx_dynamic\\.dylib")

set(library "${WORK}/mc.dll")
shared_library(windows-x86_64 "${WORK}/mc.anti" "${library}" --memory-checks)
execute_process(COMMAND "${READOBJ}" --coff-imports "${library}"
                OUTPUT_VARIABLE text ERROR_VARIABLE err ENCODING NONE)
names("${library}" "${text}${err}" "clang_rt\\.asan_dynamic\\.dll")
