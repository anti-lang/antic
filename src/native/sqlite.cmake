# SQLite, a library of the runtime archive that no module binds yet, built
# for every target of the runtime tree as lib/<target>/libsqlite3.a, or
# sqlite3.lib on Windows. anti.db of docs/anti-language-additions.md is to
# open it, and is not built.
#
# The source is the amalgamation of the release that tools/sqlite-pin
# names, which antic_get_source of get-source.cmake downloads into
# build/deps/sqlite and checks against the pinned digest. sqlite.org
# publishes the SHA3-256 of the archive, and the directory carries the
# version in the form of its file names, 3XXYYZZ.

antic_shared_path(ANTIC_SQLITE_DIR "${ANTIC_DEPS_DIR}/sqlite"
    "the SQLite amalgamation from tools/sqlite-pin")
antic_get_source(ANTIC_SQLITE_SOURCE sqlite "${ANTIC_SQLITE_DIR}" SHA3_256
    sqlite3.c)
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
# The text is written with LF on every host, as LICENSES/sqlite.txt holds
# it. file(WRITE) writes CRLF on Windows.
file(CONFIGURE OUTPUT "${ANTIC_SQLITE_WORK}/licence.txt"
     CONTENT "SQLite is in the public domain.\n\n${antic_sqlite_blessing}\n"
     @ONLY NEWLINE_STYLE UNIX)
antic_native_license(sqlite "${ANTIC_SQLITE_WORK}/licence.txt")
# The two headers of the amalgamation, which its build installs.
antic_native_headers(sqlite3 "${ANTIC_SQLITE_SOURCE}" sqlite3.h sqlite3ext.h)

# DESIGN: sqlite3.c of the amalgamation with no compile-time option of our
# own, the defaults of the release: serialized threading, and extensions
# loaded at run time. It compiles with the warnings of SQLite's own build,
# ANTIC_SQLITE_WARNINGS of src/native/warnings.cmake.

set(antic_sqlite_probe "${PROJECT_SOURCE_DIR}/tests/abi/sqlite_probe.c")
foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    antic_native_target(triple flags "${target}")
    antic_static_library(name sqlite3 "${target}")
    set(work "${ANTIC_SQLITE_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2
        ${flags}
        "-ffile-prefix-map=${ANTIC_SQLITE_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(object "${work}/sqlite3.o")
    add_custom_command(OUTPUT "${object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_SQLITE_WARNINGS}
            -c "${ANTIC_SQLITE_SOURCE}/sqlite3.c"
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
    # that target would compile C against the library, with the headers of
    # include/sqlite3/ of the runtime tree.
    set(probe "${work}/sqlite_probe.o")
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_C_WARNINGS}
            -isystem "${ANTIC_RUNTIME_DIR}/include/sqlite3"
            -c "${antic_sqlite_probe}" -o "${probe}"
        DEPENDS "${antic_sqlite_probe}"
            "${ANTIC_RUNTIME_DIR}/include/sqlite3/sqlite3.h"
        VERBATIM)
    add_custom_target(sqlite_${target} ALL DEPENDS "${library}" "${probe}")

    # The library links for every target, and on the host the program runs.
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/sqlite_link.anti")
    antic_native_tests(sqlite_run sqlite_link "${target}"
        SOURCE "${source}"
        OBJECTS "${probe}" "${library}"
        WORK "${ANTIC_SQLITE_WORK}")
endforeach()

add_test(NAME sqlite_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -DNAME=sqlite
            -DSCRIPT=src/native/get-source.cmake
            -DCALLER=src/native/sqlite.cmake
            -DHASH=SHA3_256 -DPARTS=3
            -DYEAR=ON
            -P "${ANTIC_RUNNERS}/run_native_pin.cmake")
