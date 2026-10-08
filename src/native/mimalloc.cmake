# mimalloc, the C allocator of every program of musl, built for
# linux-x86_64 and linux-arm64 at every processor level as
# lib/<target>/<level>/libmimalloc.a, beside the runtime of that level.
#
# The source comes from the release that tools/mimalloc-pin names, which
# get-mimalloc.cmake downloads into build/deps/mimalloc and checks against
# the pinned digest.
#
# DESIGN: musl's allocator maps and unmaps every large block, and cost
# tests/bench/map_work.anti about 30 percent on linux-arm64 against its C
# twin linked with glibc. Eddie decided on 2026-10-08 to measure mimalloc
# and adopt it if it won. src/antic/linker.c links it before libc.a in
# every program of musl, so malloc, free, calloc, realloc and
# aligned_alloc of the program, the runtime and musl resolve to it. See
# the entry on the allocator of musl under "Libraries and runtime" in
# docs/decisions.md for the measurement.
#
# DESIGN: the library stands at every level of its target, as the runtime
# does, and not at the default level alone as the other native libraries.
# Every program of musl links it, so a program built with --cpu below the
# default would otherwise run code of the default level before its
# processor check. The level replaces the -march= that mimalloc's own
# build picks, armv8.1-a on ARM64.

antic_shared_path(ANTIC_MIMALLOC_DIR "${ANTIC_DEPS_DIR}/mimalloc"
    "the mimalloc source from src/native/get-mimalloc.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_MIMALLOC_DIR}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-mimalloc.cmake"
                RESULT_VARIABLE antic_mimalloc_fetched)
if(NOT antic_mimalloc_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-mimalloc.cmake failed, so the build "
                        "has no mimalloc source")
endif()
file(STRINGS "${PROJECT_SOURCE_DIR}/tools/mimalloc-pin"
     antic_mimalloc_version REGEX "^MIMALLOC_VERSION=")
string(REGEX REPLACE "^MIMALLOC_VERSION=" "" antic_mimalloc_version
       "${antic_mimalloc_version}")
set(ANTIC_MIMALLOC_SOURCE
    "${ANTIC_MIMALLOC_DIR}/mimalloc-${antic_mimalloc_version}")
set(ANTIC_MIMALLOC_WORK "${CMAKE_BINARY_DIR}/native/mimalloc")
antic_native_license(mimalloc "${ANTIC_MIMALLOC_SOURCE}/LICENSE")

# DESIGN: src/static.c, the one object that mimalloc's own build writes as
# mimalloc.o for a static override, with the definitions and flags that
# its CMakeLists.txt gives that object in a Release build for clang on
# Linux with MI_LIBC_MUSL on and every other option at its default. One
# object holds every function of the C allocator, so the link takes all
# of them or none. The flags that are no warnings stand here, and the
# warnings in ANTIC_MIMALLOC_WARNINGS of src/native/warnings.cmake.
#
# [provisional] DESIGN: one option differs from its default. MI_ALLOW_THP
# is OFF, so MI_DEFAULT_ALLOW_THP is 0 where the default FULL gives 2, and
# mimalloc asks for no transparent huge pages. With FULL,
# tests/bench/builder.anti ran 115 ms on anti-linux against 50 ms with
# musl's allocator, and with OFF it ran 50 ms. map_work ran 150 ms with
# FULL, 157 with OFF and 185 with musl's allocator, mixed_work 178, 177
# and 200, and the other three programs were alike in all three. A run
# may still ask for them with MIMALLOC_ALLOW_THP. The rule of "Decisions"
# in docs/work-order-llvm-optimization.md picks OFF, the faster program on
# every one, measured 2026-10-08.
set(ANTIC_MIMALLOC_DEFINES -DMI_MALLOC_OVERRIDE -DMI_LIBC_MUSL=1
    -DMI_DEFAULT_ALLOW_THP=0 -DNDEBUG=1 -DMI_GUARDED=0 -DMI_STATS=1
    -DMI_PROFILE=1)
set(ANTIC_MIMALLOC_FLAGS -std=gnu11 -O3 -fvisibility=hidden -mno-outline
    -ftls-model=local-dynamic -fno-builtin-malloc)

foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    if(NOT target MATCHES "^linux-(x86_64|arm64)$")
        continue()
    endif()
    antic_native_target(triple flags "${target}")
    list(FILTER flags EXCLUDE REGEX "^-march=")
    antic_runtime_sections(sections "${target}")
    antic_cpu_levels(levels "${target}")
    set(libraries "")
    foreach(level IN LISTS levels)
        antic_cpu_march(march "${level}")
        set(work "${ANTIC_MIMALLOC_WORK}/${target}-${level}")
        set(object "${work}/mimalloc.o")
        set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}/libmimalloc.a")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND "${CMAKE_C_COMPILER}" --target=${triple}
                ${ANTIC_MIMALLOC_FLAGS} ${flags} "${march}" ${sections}
                ${ANTIC_MIMALLOC_WARNINGS} ${ANTIC_MIMALLOC_DEFINES}
                "-ffile-prefix-map=${ANTIC_MIMALLOC_SOURCE}=."
                "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
                "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=."
                -I "${ANTIC_MIMALLOC_SOURCE}/include"
                -c "${ANTIC_MIMALLOC_SOURCE}/src/static.c" -o "${object}"
            DEPENDS "${ANTIC_MIMALLOC_SOURCE}/src/static.c"
            VERBATIM)
        add_custom_command(OUTPUT "${library}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${ANTIC_RUNTIME_DIR}/lib/${target}/${level}"
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
            COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" "${object}"
            DEPENDS "${object}"
            VERBATIM)
        list(APPEND libraries "${library}")
    endforeach()
    add_custom_target(mimalloc_${target} ALL DEPENDS ${libraries})
endforeach()

add_test(NAME mimalloc_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -DNAME=mimalloc
            -DSCRIPT=src/native/get-mimalloc.cmake
            "-DFILES=src/native/mimalloc.cmake"
            -DHASH=SHA256 -DPARTS=3
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_pin.cmake")
