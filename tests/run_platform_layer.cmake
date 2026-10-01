# Rule 22 of docs/c-guidelines.md: a `#if` on the host system stands in the
# platform layer alone. That is src/antic/platform.c and platform.h for
# antic, src/anti/platform.c and platform.h for anti, and src/rt/platform.h
# with platform_posix.c and platform_windows.c for the runtime. A host is
# named by _WIN32, __APPLE__, __linux__, _MSC_VER, _M_ARM64 or _M_X64, the
# macros that docs/audit/data/platform-conditionals.txt counts. Run with
# cmake -P and ROOT, the root of the repository.

set(layer "platform.c" "platform.h" "platform_posix.c" "platform_windows.c")
# The runtime files whose host branches steps 33 and 34 of "Fix steps" in
# docs/audit/summary.md move into the layer. A step that moves one takes
# it off this list.
set(pending_rt "fs.c" "trace.c" "cpu.c" "mem.c" "start.c" "errno.c"
    "init.c" "atomic.c")
set(failures "")

foreach(part antic anti rt)
    set(src "${ROOT}/src/${part}")
    file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
    foreach(source IN LISTS sources)
        if(source IN_LIST layer)
            continue()
        endif()
        if(part STREQUAL "rt" AND source IN_LIST pending_rt)
            continue()
        endif()
        # file(STRINGS) drops empty lines, so a finding names the line by
        # its text rather than by a number.
        file(STRINGS "${src}/${source}" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^[ ]*#[ ]*(if|ifdef|ifndef|elif)[ (]" AND
               line MATCHES "(_WIN32|__APPLE__|__linux__|_MSC_VER|_M_ARM64|_M_X64)")
                string(STRIP "${line}" shown)
                string(APPEND failures "\nsrc/${part}/${source}: ${shown}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "a host branch outside the platform layer:${failures}")
endif()
