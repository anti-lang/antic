# Every source of antic and anti compiles for HOST under the warning set,
# with the options a package compiles them with. A build compiles them for
# its own host alone, so a warning of another host's headers or ABI showed
# first on that host or in a release: enums are int under the Microsoft
# ABI, and ast_dump.c passed one to an unsigned parameter. The macOS hosts
# read the pinned Apple SDK, which the build of a Mac compiles against
# and no other machine holds, so the tests cover Linux and Windows.
#
#   cmake -DROOT=<repo> -DHOST=<host> -DCLANG=<clang> -DSYSROOT=<dir>
#         -DPCRE2_INCLUDE=<dir> -DVERSION=<version>
#         -P tests/run_host_sources.cmake

include("${ROOT}/tools/sources.cmake")
include("${ROOT}/tools/warnings.cmake")
include("${ROOT}/tools/host-compile.cmake")

execute_process(COMMAND "${CLANG}" -print-resource-dir
                OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE)
antic_host_compile_options(options "${HOST}" "${ROOT}" "${SYSROOT}"
                           "${resource}" "" "${VERSION}")

set(core_includes -I "${PCRE2_INCLUDE}")
foreach(dir IN LISTS ANTIC_CORE_INCLUDE_DIRS)
    list(APPEND core_includes -I "${ROOT}/${dir}")
endforeach()
set(anti_includes ${core_includes})
foreach(dir IN LISTS ANTI_INCLUDE_DIRS)
    list(APPEND anti_includes -I "${ROOT}/${dir}")
endforeach()

set(sources ${ANTIC_CORE_SOURCES} ${ANTIC_MAIN_SOURCES} ${ANTI_SOURCES})
list(REMOVE_DUPLICATES sources)
set(failures "")
foreach(source IN LISTS sources)
    set(includes ${core_includes})
    if(source IN_LIST ANTI_SOURCES)
        set(includes ${anti_includes})
    endif()
    execute_process(COMMAND "${CLANG}" ${options} ${ANTIC_C_WARNINGS}
                            ${includes} -fsyntax-only "${ROOT}/${source}"
                    RESULT_VARIABLE failed ERROR_VARIABLE err)
    if(failed)
        string(APPEND failures "${err}")
    endif()
endforeach()

if(failures)
    message(FATAL_ERROR "the sources of antic and anti do not compile for "
                        "${HOST}:\n${failures}")
endif()
list(LENGTH sources count)
message(STATUS "${count} sources compile for ${HOST}")
