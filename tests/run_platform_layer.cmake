# Rule 22 of docs/c-guidelines.md for antic: a `#if` on the host system
# stands in src/antic/platform.c and platform.h alone. A host is named by
# _WIN32, __APPLE__, __linux__, _MSC_VER, _M_ARM64 or _M_X64, the macros
# that docs/audit/data/platform-conditionals.txt counts. Run with cmake
# -P and ROOT, the root of the repository.

set(src "${ROOT}/src/antic")
set(layer "platform.c" "platform.h")
set(failures "")

file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
foreach(source IN LISTS sources)
    if(source IN_LIST layer)
        continue()
    endif()
    # file(STRINGS) drops empty lines, so a finding names the line by its
    # text rather than by a number.
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ ]*#[ ]*(if|ifdef|ifndef|elif)[ (]" AND
           line MATCHES "(_WIN32|__APPLE__|__linux__|_MSC_VER|_M_ARM64|_M_X64)")
            string(STRIP "${line}" shown)
            string(APPEND failures "\n${source}: ${shown}")
        endif()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "a host branch outside the platform layer:${failures}")
endif()
