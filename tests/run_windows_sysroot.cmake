# tools/get-sysroot.cmake lays out the Windows sysroot of a target from the
# source release of mingw-w64 that tools/sysroot-pins names: the headers of
# mingw-w64-headers into include/, with _mingw.h written from _mingw.h.in,
# and the import libraries that llvm-dlltool writes from the .def files of
# mingw-w64-crt into lib/, each .def.in preprocessed for the processor of
# the target, beside clang_rt.builtins.lib of the pinned clang. A stand-in
# source release served from a file:// URL takes the place of the real one
# in a copy of the script. The pinned clang preprocesses the .def files
# and llvm-ar, as llvm-dlltool, writes the libraries, as in a real run.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCLANG_DIR=<dir>
#         -DLLVM_BIN=<dir> -DOBJDUMP=<llvm-objdump>
#         -P tests/run_windows_sysroot.cmake

cmake_minimum_required(VERSION 3.21)

# The pin names a release by its version, an https URL with the version
# to fill in, and the digest of the archive.
file(STRINGS "${ROOT}/tools/sysroot-pins" pins REGEX "^MINGW_")
foreach(line IN LISTS pins)
    string(REGEX REPLACE "^([^=]+)=(.*)$" "\\1;\\2" pair "${line}")
    list(GET pair 0 key)
    list(GET pair 1 value)
    set("${key}" "${value}")
endforeach()
if(NOT MINGW_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "MINGW_VERSION is '${MINGW_VERSION}'")
endif()
if(NOT MINGW_URL MATCHES "^https://.*@VERSION@")
    message(FATAL_ERROR "MINGW_URL is '${MINGW_URL}', not an https URL with "
                        "@VERSION@")
endif()
if(NOT MINGW_DIGEST MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "MINGW_DIGEST is '${MINGW_DIGEST}'")
endif()
string(LENGTH "${MINGW_DIGEST}" length)
if(NOT length EQUAL 64)
    message(FATAL_ERROR "MINGW_DIGEST is '${MINGW_DIGEST}'")
endif()

# The stand-in release, with the layout of the real one: the headers of
# the C runtime under crt/, those of the API under include/ and those of
# the driver kit under ddk/include/, each beside files that are no
# headers, the macros of the processors in def-include/, a .def.in per
# DLL of one group and a plain .def per DLL of the other, and the licence.
file(REMOVE_RECURSE "${WORK}")
set(version 9.9.9)
set(release "mingw-w64-v${version}")
set(stage "${WORK}/stage/${release}")
set(headers "${stage}/mingw-w64-headers")
file(WRITE "${headers}/crt/_mingw.h.in"
     "#ifndef __MSVCRT_VERSION__\n"
     "#  define __MSVCRT_VERSION__ @DEFAULT_MSVCRT_VERSION@\n"
     "#endif\n"
     "#ifndef _WIN32_WINNT\n"
     "#define _WIN32_WINNT @DEFAULT_WIN32_WINNT@\n"
     "#endif\n")
set(copied crt/stdio.h crt/sys/types.h crt/sec_api/stdio_s.h
    crt/sec_api/sys/wchar_s.h crt/swprintf.inl include/windows.h
    include/GL/gl.h include/psdk_inc/intrin-impl.h include/winres.rh
    include/wrl/wrappers/corewrappers.h ddk/include/ddk/ntddk.h)
set(left crt/Makefile.am crt/ChangeLog include/sample.idl include/ChangeLog
    include/Makefile.in)
foreach(path IN LISTS copied left)
    file(WRITE "${headers}/${path}" "${path}\n")
endforeach()
set(crt "${stage}/mingw-w64-crt")
file(WRITE "${crt}/def-include/func.def.in"
     "#if defined(__x86_64__)\n"
     "#define F_X64(x) x\n"
     "#define F_ARM64(x)\n"
     "#elif defined(__aarch64__)\n"
     "#define F_X64(x)\n"
     "#define F_ARM64(x) x\n"
     "#else\n"
     "#error Unrecognized architecture\n"
     "#endif\n")
set(preprocessed ucrtbase ntdll kernel32 user32 ws2_32)
set(plain gdi32 shell32 winmm dbghelp bcrypt)
foreach(dll IN LISTS preprocessed)
    file(WRITE "${crt}/lib-common/${dll}.def.in"
         "LIBRARY \"${dll}.dll\"\n"
         "EXPORTS\n"
         "#include \"func.def.in\"\n"
         "; the comment of a .def file\n"
         "${dll}_common\n"
         "F_X64(${dll}_x64)\n"
         "F_ARM64(${dll}_arm64)\n"
         "${dll}_decorated@8\n"
         "${dll}_data DATA\n")
endforeach()
foreach(dll IN LISTS plain)
    file(WRITE "${crt}/lib-common/${dll}.def"
         "LIBRARY \"${dll}.dll\"\n"
         "EXPORTS\n"
         "${dll}_plain\n")
endforeach()
# A .def of the directory of a processor comes before the one of
# lib-common/, as in the makefile of mingw-w64-crt.
foreach(pair "lib64;x64" "libarm64;arm64")
    list(GET pair 0 dir)
    list(GET pair 1 cpu)
    file(WRITE "${crt}/${dir}/bcrypt.def"
         "LIBRARY \"bcrypt.dll\"\n"
         "EXPORTS\n"
         "bcrypt_${cpu}_own\n")
endforeach()
file(WRITE "${stage}/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt"
     "the licence of the runtime of mingw-w64\n")
file(MAKE_DIRECTORY "${WORK}/packages")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cjf
                        "${WORK}/packages/${release}.tar.bz2" "${release}"
                WORKING_DIRECTORY "${WORK}/stage" COMMAND_ERROR_IS_FATAL ANY)
file(SHA256 "${WORK}/packages/${release}.tar.bz2" digest)

# A copy of the script beside pins that name the stand-in.
file(MAKE_DIRECTORY "${WORK}/tools")
file(COPY_FILE "${ROOT}/tools/get-sysroot.cmake" "${WORK}/tools/get-sysroot.cmake")
file(COPY_FILE "${ROOT}/tools/deps-dir.cmake" "${WORK}/tools/deps-dir.cmake")
file(COPY_FILE "${ROOT}/tools/zig-stubs-pin" "${WORK}/tools/zig-stubs-pin")
file(STRINGS "${ROOT}/tools/sysroot-pins" kept REGEX "^[^M]|^M[^I]")
list(JOIN kept "\n" text)
string(APPEND text "\nMINGW_VERSION=${version}\n"
       "MINGW_URL=file://${WORK}/packages/mingw-w64-v@VERSION@.tar.bz2\n"
       "MINGW_DIGEST=${digest}\n")
file(WRITE "${WORK}/tools/sysroot-pins" "${text}")

function(get_sysroot result)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/sysroot"
                            "-DLLVM_BIN=${LLVM_BIN}" "-DCLANG_DIR=${CLANG_DIR}"
                            "-DTARGETS=windows-x86_64;windows-arm64"
                            -P "${WORK}/tools/get-sysroot.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                    ENCODING NONE)
    set(${result} "${status}" PARENT_SCOPE)
    set(output "${out}${err}" PARENT_SCOPE)
endfunction()

get_sysroot(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed:\n${output}")
endif()

# The symbols of an import library, as llvm-objdump lists them.
function(library_symbols out library)
    execute_process(COMMAND "${OBJDUMP}" --syms "${library}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE table
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}\n${err}")
    endif()
    set(${out} "${table}" PARENT_SCOPE)
endfunction()

foreach(pair "windows-x86_64;x86_64;x64;arm64;coff-x86-64"
             "windows-arm64;aarch64;arm64;x64;coff-arm64")
    list(GET pair 0 target)
    list(GET pair 1 arch)
    list(GET pair 2 own)
    list(GET pair 3 other)
    list(GET pair 4 format)
    set(tree "${WORK}/sysroot/${target}")
    # Every header lands under include/ by its path below crt/, include/
    # or ddk/include/, and no other file of the release does.
    foreach(path IN LISTS copied)
        string(REGEX REPLACE "^(crt|include|ddk/include)/" "" below "${path}")
        if(NOT EXISTS "${tree}/include/${below}")
            message(FATAL_ERROR "${tree} lacks include/${below}")
        endif()
        file(READ "${tree}/include/${below}" text)
        if(NOT text STREQUAL "${path}\n")
            message(FATAL_ERROR "${tree}/include/${below} holds '${text}'")
        endif()
    endforeach()
    foreach(path IN LISTS left)
        string(REGEX REPLACE "^(crt|include|ddk/include)/" "" below "${path}")
        if(EXISTS "${tree}/include/${below}")
            message(FATAL_ERROR "${tree} holds include/${below}, which is no "
                                "header")
        endif()
    endforeach()
    foreach(path include/_mingw.h.in crt sdk)
        if(EXISTS "${tree}/${path}")
            message(FATAL_ERROR "${tree} holds ${path}")
        endif()
    endforeach()
    # _mingw.h names the UCRT and Windows 10, the values the configure of
    # mingw-w64 fills in.
    file(READ "${tree}/include/_mingw.h" text)
    if(NOT text MATCHES "#  define __MSVCRT_VERSION__ 0xE00\n" OR
       NOT text MATCHES "#define _WIN32_WINNT 0xa00\n" OR text MATCHES "@")
        message(FATAL_ERROR "${tree}/include/_mingw.h holds\n${text}")
    endif()
    # One import library per DLL, of the processor of the target. A
    # .def.in gives the names of every processor and of this one, with
    # the stdcall decoration of a name stripped, and a plain .def its
    # names as they are.
    foreach(dll IN LISTS preprocessed plain)
        if(NOT EXISTS "${tree}/lib/${dll}.lib")
            message(FATAL_ERROR "${tree} lacks lib/${dll}.lib")
        endif()
        library_symbols(table "${tree}/lib/${dll}.lib")
        if(NOT table MATCHES "file format ${format}")
            message(FATAL_ERROR "${tree}/lib/${dll}.lib is not ${format}:\n${table}")
        endif()
        if(dll STREQUAL "bcrypt")
            set(wanted "__imp_bcrypt_${own}_own")
            set(unwanted "__imp_bcrypt_plain" "__imp_bcrypt_${other}_own")
        elseif(dll IN_LIST plain)
            set(wanted "__imp_${dll}_plain")
            set(unwanted "")
        else()
            set(wanted "__imp_${dll}_common" "__imp_${dll}_${own}"
                "__imp_${dll}_decorated" "__imp_${dll}_data")
            set(unwanted "__imp_${dll}_${other}" "__imp_${dll}_decorated@8")
        endif()
        foreach(name IN LISTS wanted)
            if(NOT table MATCHES " ${name}\n")
                message(FATAL_ERROR "${tree}/lib/${dll}.lib lacks ${name}:\n${table}")
            endif()
        endforeach()
        foreach(name IN LISTS unwanted)
            if(table MATCHES " ${name}\n")
                message(FATAL_ERROR "${tree}/lib/${dll}.lib holds ${name}:\n${table}")
            endif()
        endforeach()
    endforeach()
    # The builtins of the pinned clang for the target, as they are.
    file(GLOB builtins
         "${CLANG_DIR}/lib/clang/*/lib/${arch}-pc-windows-msvc/clang_rt.builtins.lib")
    list(LENGTH builtins count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${CLANG_DIR} holds ${count} builtins of ${arch}")
    endif()
    file(SHA256 "${builtins}" pinned)
    if(NOT EXISTS "${tree}/lib/clang_rt.builtins.lib")
        message(FATAL_ERROR "${tree} lacks lib/clang_rt.builtins.lib")
    endif()
    file(SHA256 "${tree}/lib/clang_rt.builtins.lib" copied_digest)
    if(NOT copied_digest STREQUAL pinned)
        message(FATAL_ERROR "${tree}/lib/clang_rt.builtins.lib is not the "
                            "one of the pinned clang")
    endif()
endforeach()

# The licence of mingw-w64 and the one of the builtins.
file(READ "${WORK}/sysroot/licenses/mingw-w64.txt" text)
if(NOT text STREQUAL "the licence of the runtime of mingw-w64\n")
    message(FATAL_ERROR "licenses/mingw-w64.txt holds '${text}'")
endif()
if(NOT EXISTS "${WORK}/sysroot/licenses/compiler-rt.txt")
    message(FATAL_ERROR "licenses/compiler-rt.txt is missing")
endif()

# A second run lays the sysroot out again from the download it kept.
file(REMOVE "${WORK}/packages/${release}.tar.bz2")
get_sysroot(status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake failed on a second run:\n${output}")
endif()
if(NOT EXISTS "${WORK}/sysroot/windows-arm64/lib/ucrtbase.lib")
    message(FATAL_ERROR "the second run lost lib/ucrtbase.lib")
endif()
message(STATUS "the Windows sysroots of mingw-w64 are laid out")
