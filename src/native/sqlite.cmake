# SQLite, the reference driver of anti.db, built for every target of the
# runtime tree as lib/<target>/libsqlite3.a, or sqlite3.lib on Windows.
#
# The source is the amalgamation of the release that tools/sqlite-pin
# names, which get-sqlite.cmake downloads into build/deps/sqlite and checks
# against the pinned digest.

antic_shared_path(ANTIC_SQLITE_DIR "${ANTIC_DEPS_DIR}/sqlite"
    "the SQLite amalgamation from src/native/get-sqlite.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_SQLITE_DIR}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-sqlite.cmake"
                OUTPUT_VARIABLE antic_sqlite_source
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE antic_sqlite_fetched)
if(NOT antic_sqlite_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-sqlite.cmake failed, so the build "
                        "has no SQLite source")
endif()
# The script names the directory it unpacked on its last line, since the
# name holds the version in the form of sqlite.org and not as the pin
# writes it.
string(REGEX MATCH "[^\n]*$" ANTIC_SQLITE_SOURCE "${antic_sqlite_source}")
string(REGEX REPLACE "^-- " "" ANTIC_SQLITE_SOURCE "${ANTIC_SQLITE_SOURCE}")
if(NOT EXISTS "${ANTIC_SQLITE_SOURCE}/sqlite3.c")
    message(FATAL_ERROR "src/native/get-sqlite.cmake named "
                        "`${ANTIC_SQLITE_SOURCE}`, which holds no sqlite3.c")
endif()
set(ANTIC_SQLITE_WORK "${CMAKE_BINARY_DIR}/native/sqlite")

# SQLite is in the public domain and ships no licence file. The comment at
# the top of sqlite3.h holds the dedication, which becomes the file of the
# licence directory.
file(READ "${ANTIC_SQLITE_SOURCE}/sqlite3.h" antic_sqlite_header LIMIT 1024)
string(REGEX MATCH "The author disclaims.*than you give\\." antic_sqlite_blessing
       "${antic_sqlite_header}")
string(REGEX REPLACE "\n\\*\\*[ ]*" "\n" antic_sqlite_blessing
       "${antic_sqlite_blessing}")
string(STRIP "${antic_sqlite_blessing}" antic_sqlite_blessing)
if(antic_sqlite_blessing STREQUAL "")
    message(FATAL_ERROR "sqlite3.h holds no dedication to the public domain")
endif()
file(WRITE "${ANTIC_SQLITE_WORK}/licence.txt"
     "SQLite is in the public domain.\n\n${antic_sqlite_blessing}\n")
antic_native_license(sqlite "${ANTIC_SQLITE_WORK}/licence.txt")

# DESIGN: sqlite3.c of the amalgamation with no compile-time option of our
# own, the defaults of the release: serialized threading, and extensions
# loaded at run time. It compiles under the warnings of anti_rt.
set(ANTIC_SQLITE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# DESIGN: four warnings are off on Windows alone, where SQLite compiles
# the code it writes for _MSC_VER, which clang for MSVC defines.
# -Wlanguage-extension-token of -Wpedantic reports __int64 in sqlite3.h,
# so the probe needs it off as well, and the __try blocks of SQLITE_USE_SEH.
# -Wsign-compare, -Wunused-variable and -Wunused-function report the
# exception filter and the lock check of those blocks. The filter compares
# a DWORD with an int, and the lock check is a function for an assert that
# the release compiles out. No warning is raised on the other four targets.
set(ANTIC_SQLITE_WARNINGS_windows -Wno-language-extension-token
    -Wno-sign-compare -Wno-unused-variable -Wno-unused-function)

set(antic_sqlite_probe "${PROJECT_SOURCE_DIR}/tests/abi/sqlite_probe.c")
foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    antic_native_target(triple flags "${target}")
    antic_native_library(name "${target}" sqlite3)
    set(work "${ANTIC_SQLITE_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(warnings ${ANTIC_SQLITE_WARNINGS})
    if(target MATCHES "^windows-")
        list(APPEND warnings ${ANTIC_SQLITE_WARNINGS_windows})
    endif()
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2
        ${warnings} ${flags}
        "-ffile-prefix-map=${ANTIC_SQLITE_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(object "${work}/sqlite3.o")
    add_custom_command(OUTPUT "${object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} -c "${ANTIC_SQLITE_SOURCE}/sqlite3.c"
            -o "${object}"
        DEPENDS "${ANTIC_SQLITE_SOURCE}/sqlite3.c"
            "${ANTIC_SQLITE_SOURCE}/sqlite3.h"
        VERBATIM)
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${ANTIC_RUNTIME_DIR}/lib/${target}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" "${object}"
        DEPENDS "${object}"
        VERBATIM)

    # The C half of the test sqlite_link_<target>, compiled as a program of
    # that target would compile C against the library.
    set(probe "${work}/sqlite_probe.o")
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} -Wshadow -Wconversion -Wstrict-prototypes
            -I "${ANTIC_SQLITE_SOURCE}"
            -c "${antic_sqlite_probe}" -o "${probe}"
        DEPENDS "${antic_sqlite_probe}" "${ANTIC_SQLITE_SOURCE}/sqlite3.h"
        VERBATIM)
    add_custom_target(sqlite_${target} ALL DEPENDS "${library}" "${probe}")

    # The library links for every target, and on the host the program runs.
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/sqlite_link.anti")
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME sqlite_run
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOBJECTS=${probe},${library}"
                "-DWORK=${ANTIC_SQLITE_WORK}/run"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME sqlite_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOBJECTS=${probe},${library}"
            "-DWORK=${ANTIC_SQLITE_WORK}/link"
            "-DTARGET=${target}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

add_test(NAME sqlite_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_sqlite_pin.cmake")
