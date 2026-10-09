# antic takes its tools, its libraries and its sysroots from the runtime
# archive alone, which is the package once it is installed. A tool that
# bin/ of the archive lacks is refused by its path and never taken from
# the search path, even when the search path holds it. A Windows link
# without the sysroot of its target is refused and never reads the
# library directories of LIB, which an MSVC environment sets, and a
# Windows link with the sysroot reads LIB no more, nor the Visual Studio
# and the Windows SDK a Windows host installed: a library the sysroot
# lacks stays missing. No message of antic or anti names
# tools/get-sysroot.cmake, which a user has no use for. Each stand-in
# archive here is a directory of links into the real one, with a bin/ or
# a sysroot/ of its own.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DRUNTIME=<dir> -DOPT=<opt>
#         -DLLC=<llc> -DWORK=<dir> -P tests/run_own_tools.cmake

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(source "${ROOT}/tests/programs/return42.anti")
set(path_sep ":")
if(CMAKE_HOST_WIN32)
    set(path_sep ";")
endif()

# A stand-in archive at dir: lib/, std/ and licenses/ of the real one, and
# sysroot/ or bin/ unless the caller lays out its own.
function(stand_in dir)
    file(MAKE_DIRECTORY "${dir}")
    foreach(part lib std licenses ${ARGN})
        file(CREATE_LINK "${RUNTIME}/${part}" "${dir}/${part}" SYMBOLIC)
    endforeach()
endfunction()

# Run antic with the search path holding bin/ of the real archive, so a
# fallback on it would succeed, and with the given variables set.
function(run_antic what expected_status pattern)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "" "ENV;COMMAND")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${RUNTIME}/bin" ${arg_ENV}
                "${ANTIC}" ${arg_COMMAND}
        WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE status
        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL expected_status)
        message(FATAL_ERROR "${what}: antic ended with ${status}, not "
                            "${expected_status}\n${out}${err}")
    endif()
    if(NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: antic wrote\n${err}\nand not ${pattern}")
    endif()
endfunction()

# An archive whose bin/ is empty: opt is missing, then llc and the lld of
# the target, and llvm-ar for a static library.
set(no_tools "${WORK}/no-tools")
stand_in("${no_tools}" sysroot)
file(MAKE_DIRECTORY "${no_tools}/bin")
run_antic("opt missing" 1 "antic: [^\n]*[/\\\\]bin[/\\\\]opt(\\.exe)? is missing"
          COMMAND --runtime "${no_tools}" --target linux-x86_64
                  -o "${WORK}/no-opt" "${source}")
run_antic("ld.lld missing" 1
          "antic: [^\n]*[/\\\\]bin[/\\\\]ld\\.lld(\\.exe)? is missing"
          COMMAND --runtime "${no_tools}" --target linux-x86_64
                  --opt "${OPT}" --llc "${LLC}" -o "${WORK}/no-lld" "${source}")
run_antic("lld-link missing" 1
          "antic: [^\n]*[/\\\\]bin[/\\\\]lld-link(\\.exe)? is missing"
          COMMAND --runtime "${no_tools}" --target windows-x86_64
                  --opt "${OPT}" --llc "${LLC}" -o "${WORK}/no-lld-link.exe"
                  "${source}")
run_antic("llvm-ar missing" 1
          "antic: [^\n]*[/\\\\]bin[/\\\\]llvm-ar(\\.exe)? is missing"
          COMMAND --runtime "${no_tools}" --target linux-x86_64 --lib static
                  --opt "${OPT}" --llc "${LLC}" -I "${ROOT}/tests/clib"
                  -o "${WORK}/no-ar.a"
                  "${ROOT}/tests/clib/com/example/doubling.anti")
foreach(name no-opt no-lld no-lld-link.exe no-ar.a)
    if(EXISTS "${WORK}/${name}")
        message(FATAL_ERROR "antic wrote ${name} with a tool of the search "
                            "path")
    endif()
endforeach()

# The two Windows checks need the Windows sysroot of the archive.
set(windows "${RUNTIME}/sysroot/windows-x86_64")
if(NOT EXISTS "${windows}/crt/lib/x86_64/msvcrt.lib")
    message("SKIP the Windows checks: ${windows} is not here")
else()
    # An archive without the Windows sysroot, and LIB naming the real one:
    # the link is refused, and names the package.
    set(no_windows "${WORK}/no-windows")
    stand_in("${no_windows}" bin)
    file(MAKE_DIRECTORY "${no_windows}/sysroot")
    set(lib "${windows}/crt/lib/x86_64;${windows}/sdk/lib/um/x86_64")
    set(lib "${lib};${windows}/sdk/lib/ucrt/x86_64")
    run_antic("no Windows sysroot" 1
              "antic: linking for windows-x86_64 needs the sysroot [^\n]*windows-x86_64[^\n]* of the package"
              ENV "LIB=${lib}"
              COMMAND --runtime "${no_windows}" --target windows-x86_64
                      -o "${WORK}/no-sysroot.exe" "${source}")
    if(EXISTS "${WORK}/no-sysroot.exe")
        message(FATAL_ERROR "antic linked a Windows program from LIB")
    endif()
    # The sysroot without its ucrt/ directory, and LIB naming the real one:
    # lld-link reads LIB no more, so ucrt.lib stays missing. On the Windows
    # VM lld-link once found it in the Windows Kits of the machine instead,
    # which /vctoolsdir and /winsdkdir stop.
    set(partial "${WORK}/partial-windows")
    stand_in("${partial}" bin)
    set(partial_sysroot "${partial}/sysroot/windows-x86_64")
    file(MAKE_DIRECTORY "${partial_sysroot}/sdk/lib/ucrt/x86_64")
    file(CREATE_LINK "${windows}/crt" "${partial_sysroot}/crt" SYMBOLIC)
    file(CREATE_LINK "${windows}/sdk/lib/um" "${partial_sysroot}/sdk/lib/um"
         SYMBOLIC)
    run_antic("ucrt.lib from LIB" 1 "ucrt\\.lib"
              ENV "LIB=${windows}/sdk/lib/ucrt/x86_64"
              COMMAND --runtime "${partial}" --target windows-x86_64
                      -o "${WORK}/partial.exe" "${source}")
    if(EXISTS "${WORK}/partial.exe")
        message(FATAL_ERROR "lld-link took ucrt.lib from LIB")
    endif()
endif()

# No message names tools/get-sysroot.cmake. A comment may, and a message
# is a line with a string literal.
file(GLOB sources "${ROOT}/src/antic/*.c" "${ROOT}/src/anti/*.c")
foreach(file IN LISTS sources)
    file(STRINGS "${file}" lines REGEX "get-sysroot\\.cmake")
    foreach(line IN LISTS lines)
        if(line MATCHES "\"")
            message(FATAL_ERROR "${file} names tools/get-sysroot.cmake in a "
                                "message: ${line}")
        endif()
    endforeach()
endforeach()

# The links go first, so nothing of the real archive is removed with them.
file(GLOB links "${WORK}/*/lib" "${WORK}/*/std" "${WORK}/*/sysroot"
     "${WORK}/*/bin" "${WORK}/*/licenses" "${WORK}/*/sysroot/*/crt"
     "${WORK}/*/sysroot/*/sdk/lib/um")
foreach(link IN LISTS links)
    if(IS_SYMLINK "${link}")
        file(REMOVE "${link}")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
