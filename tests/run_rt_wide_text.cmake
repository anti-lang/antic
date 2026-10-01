# Every text the runtime takes from Windows or gives to it is UTF-16, which
# platform_windows.c converts to and from the UTF-8 of a `str`. An ANSI
# entry point of Windows, a name that ends in A, reads and writes the code
# page of the user's language instead, which is no UTF-8 on a French or a
# German Windows. M32 of the second audit: FormatMessageA gave the message
# of `SystemError.from_win32` in it. Run with cmake -P and ROOT, the root
# of the repository.

set(src "${ROOT}/src/rt")
file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
set(failures "")
foreach(source IN LISTS sources)
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "(^|[^A-Za-z0-9_])[A-Z][A-Za-z0-9]*[a-z0-9]A[ ]*\\(")
            string(STRIP "${line}" shown)
            string(APPEND failures "\nsrc/rt/${source}: ${shown}")
        endif()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "an ANSI entry point of Windows in the runtime:${failures}")
endif()
