# Rule 22 of docs/c-guidelines.md: a `#if` on the host system stands in the
# platform layer alone. That is src/antic/platform.c and platform.h for
# antic, src/anti/platform.c and platform.h for anti, and src/rt/platform.h
# with platform_posix.c and platform_windows.c for the runtime. A host is
# named by _WIN32, __APPLE__, __linux__, _MSC_VER, _M_ARM64 or _M_X64, the
# macros that docs/audit/data/platform-conditionals.txt counts. A lock or
# a once of the system stands in the runtime's layer alone as well, and the
# rest of src/rt takes a name of enum anti_rt_lock. Run with cmake -P and
# ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

set(layer "platform.c" "platform.h" "platform_posix.c" "platform_windows.c")
set(system_locks "pthread_mutex_t|pthread_cond_t|pthread_once|SRWLOCK|CONDITION_VARIABLE|CRITICAL_SECTION|INIT_ONCE|os_unfair_lock")
set(failures "")

foreach(part antic anti rt)
    set(src "${ROOT}/src/${part}")
    file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
    foreach(source IN LISTS sources)
        if(source IN_LIST layer)
            continue()
        endif()
        # file(STRINGS) drops empty lines, so a finding names the line by
        # its text rather than by a number.
        file(STRINGS "${src}/${source}" lines)
        foreach(line IN LISTS lines)
            if(part STREQUAL "rt" AND line MATCHES "(${system_locks})")
                string(STRIP "${line}" shown)
                string(APPEND failures
                    "\nsrc/${part}/${source}: a lock of its own: ${shown}")
            endif()
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
