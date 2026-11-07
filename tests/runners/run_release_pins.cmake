# tools/llvm-pin and tools/clang-pin name the release of anti-lang/llvm-tools
# that the LLVM tools and the pinned clang come from: where it lies, the
# name of an asset, and the tag and the digest of the asset of each of the
# six hosts. Both pins name one tag for each host. The version stands in
# tools/llvm-version alone and the rest in the pins alone, and no
# installer names a pin or the version. antic_release_pin of
# tools/pins.cmake alone reads the rows of a host. tools/get-llvm.cmake and
# tools/get-clang.cmake install into the directories that
# tools/pinned-compiler.cmake reads, each spelled once.
#
#   cmake -DROOT=<repository> -P tests/runners/run_release_pins.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/pins.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels.cmake")
antic_read_cpu_levels("${ROOT}/tools/cpu-levels")
set(hosts ${ANTIC_TARGETS})
file(READ "${ROOT}/tools/llvm-version" version)
string(STRIP "${version}" version)
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "tools/llvm-version is `${version}`")
endif()


foreach(name llvm clang)
    file(READ "${ROOT}/tools/${name}-pin" text)
    if(text MATCHES "(^|\n)tag=")
        message(FATAL_ERROR "tools/${name}-pin names one tag for every host, "
                            "and each host names its own")
    endif()
    antic_pin_value(release "${ROOT}/tools/${name}-pin" release)
    if(NOT release STREQUAL "https://github.com/anti-lang/llvm-tools/releases/download/@TAG@")
        message(FATAL_ERROR "tools/${name}-pin lies at `${release}`")
    endif()
    antic_pin_value(file "${ROOT}/tools/${name}-pin" file)
    set(prefix llvm-tools)
    if(name STREQUAL "clang")
        set(prefix clang)
    endif()
    if(NOT file STREQUAL "${prefix}-@TAG@-@HOST@.tar.xz")
        message(FATAL_ERROR "the asset of tools/${name}-pin is `${file}`")
    endif()
    foreach(host IN LISTS hosts)
        antic_release_pin(pinned "${ROOT}/tools/${name}-pin" "${host}")
        if(NOT pinned_TAG MATCHES "^${version}-anti\\.[1-9][0-9]*$")
            message(FATAL_ERROR "${host} in tools/${name}-pin has the tag "
                                "`${pinned_TAG}`, not @VERSION@-anti.<build>")
        endif()
        set(${name}_${host}_tag "${pinned_TAG}")
        string(LENGTH "${pinned_DIGEST}" length)
        if(NOT pinned_DIGEST MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
            message(FATAL_ERROR "${host} in tools/${name}-pin has the digest "
                                "`${pinned_DIGEST}`")
        endif()
        set(asset "${prefix}-${pinned_TAG}-${host}.tar.xz")
        if(NOT pinned_ASSET STREQUAL asset OR NOT pinned_RELEASE STREQUAL
           "https://github.com/anti-lang/llvm-tools/releases/download/${pinned_TAG}")
            message(FATAL_ERROR "${host} in tools/${name}-pin gives the asset "
                                "${pinned_RELEASE}/${pinned_ASSET}")
        endif()
    endforeach()
    file(READ "${ROOT}/tools/${name}-pin" text)
    if(text MATCHES "${version}")
        message(FATAL_ERROR "tools/${name}-pin spells ${version}, which belongs "
                            "in tools/llvm-version alone")
    endif()
endforeach()
foreach(host IN LISTS hosts)
    if(NOT llvm_${host}_tag STREQUAL clang_${host}_tag)
        message(FATAL_ERROR "tools/llvm-pin names ${llvm_${host}_tag} for "
                            "${host} and tools/clang-pin "
                            "${clang_${host}_tag}. The tools and clang of a "
                            "host come from one release.")
    endif()
endforeach()

# The rows of a host are read through antic_release_pin alone, so the
# format of the two pins stands in tools/pins.cmake. A line of another
# script that spells the row of a host, or reads a row of either pin
# itself, is refused. Comments are prose and stay out of the search.
file(GLOB scripts RELATIVE "${ROOT}" "${ROOT}/tools/*.cmake"
     "${ROOT}/tools/*.sh" "${ROOT}/tests/*.cmake" "${ROOT}/tests/runners/*.cmake"
     "${ROOT}/src/native/*.cmake")
list(APPEND scripts CMakeLists.txt tests/CMakeLists.txt
     src/native/CMakeLists.txt)
list(REMOVE_ITEM scripts tools/pins.cmake tests/runners/run_release_pins.cmake)
set(found "")
foreach(script IN LISTS scripts)
    file(STRINGS "${ROOT}/${script}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*#")
            continue()
        endif()
        if(line MATCHES "(\\\${[A-Za-z_]+}|(linux|macos|windows)-[a-z0-9_]+)-(tag|digest)" OR
           line MATCHES "antic_(pin_value|read_pins)\\([^)]*(llvm|clang)-pin")
            string(APPEND found "\n  ${script}: ${line}")
        endif()
    endforeach()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these lines read the release pins outside "
                        "antic_release_pin of tools/pins.cmake:${found}")
endif()

foreach(script get-llvm.cmake get-clang.cmake fetch-release.cmake install.sh
        install.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    if(text MATCHES "${version}" OR text MATCHES "anti-lang/llvm-tools")
        message(FATAL_ERROR "tools/${script} spells a version or the release, "
                            "which belong in tools/llvm-version and the pins")
    endif()
endforeach()

# DESIGN: the LLVM tools travel in bin/ of every package, which Eddie
# decided on 2026-10-08 in docs/work-order-distribution.md, so no
# installer reads the pin or the version to download them. A package
# carries neither file in tools/, and the test package_contents checks
# that.
foreach(script install.sh install.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    foreach(name llvm-pin llvm-version)
        if(text MATCHES "${name}")
            message(FATAL_ERROR "tools/${script} names ${name}, and the LLVM "
                                "tools travel in bin/ of the package")
        endif()
    endforeach()
endforeach()

# tools/get-llvm.cmake and tools/get-clang.cmake install into the
# directories that tools/pinned-compiler.cmake reads by default, and each
# spells its directory once, under ANTIC_DEPS_DIR of tools/deps-dir.cmake.
foreach(pair "llvm=LLVM" "clang=CLANG")
    string(REPLACE "=" ";" pair "${pair}")
    list(GET pair 0 name)
    list(GET pair 1 upper)
    file(STRINGS "${ROOT}/tools/pinned-compiler.cmake" line
         REGEX "^set\\(ANTIC_${upper}_DIR \"\\\${ANTIC_DEPS_DIR}/[^\"]+\"")
    string(REGEX REPLACE "^.*ANTIC_DEPS_DIR}/([^\"]+)\".*$" "\\1" read "${line}")
    file(STRINGS "${ROOT}/tools/get-${name}.cmake" line
         REGEX "^    set\\(DEST \"\\\${ANTIC_DEPS_DIR}/[^\"]+\"\\)$")
    string(REGEX REPLACE "^.*ANTIC_DEPS_DIR}/([^\"]+)\".*$" "\\1" written "${line}")
    if(read STREQUAL "" OR NOT read STREQUAL written)
        message(FATAL_ERROR "tools/pinned-compiler.cmake reads ${name} from "
                            "`${read}`, and tools/get-${name}.cmake installs it "
                            "into `${written}`")
    endif()
endforeach()
