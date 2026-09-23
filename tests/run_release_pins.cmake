# tools/llvm-pin and tools/clang-pin name the release of anti-lang/llvm-tools
# that the LLVM tools and the pinned clang come from: its tag, where it
# lies, the name of an asset and the digest of the asset of each of the six
# hosts. Both pins name one release. The version stands in
# tools/llvm-version alone and the rest in the pins alone.
#
#   cmake -DROOT=<repository> -P tests/run_release_pins.cmake

set(hosts linux-x86_64 linux-arm64 macos-arm64 macos-x86_64 windows-x86_64
          windows-arm64)
file(READ "${ROOT}/tools/llvm-version" version)
string(STRIP "${version}" version)
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "tools/llvm-version is `${version}`")
endif()

# Set <out> to the value of the row <key> of the pin <name>, which must
# exist.
function(row name key out)
    file(STRINGS "${ROOT}/tools/${name}-pin" line REGEX "^${key}=")
    if(line STREQUAL "")
        message(FATAL_ERROR "tools/${name}-pin has no row ${key}")
    endif()
    string(REGEX REPLACE "^${key}=" "" value "${line}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

foreach(name llvm clang)
    row(${name} tag tag)
    if(NOT tag MATCHES "^@VERSION@-anti\\.[1-9][0-9]*$")
        message(FATAL_ERROR "the tag of tools/${name}-pin is `${tag}`, not "
                            "@VERSION@-anti.<build>")
    endif()
    set(${name}_tag "${tag}")
    row(${name} release release)
    if(NOT release STREQUAL "https://github.com/anti-lang/llvm-tools/releases/download/@TAG@")
        message(FATAL_ERROR "tools/${name}-pin lies at `${release}`")
    endif()
    row(${name} file file)
    set(prefix llvm-tools)
    if(name STREQUAL "clang")
        set(prefix clang)
    endif()
    if(NOT file STREQUAL "${prefix}-@TAG@-@HOST@.tar.xz")
        message(FATAL_ERROR "the asset of tools/${name}-pin is `${file}`")
    endif()
    foreach(host IN LISTS hosts)
        row(${name} "${host}-digest" digest)
        string(LENGTH "${digest}" length)
        if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
            message(FATAL_ERROR "${host} in tools/${name}-pin has the digest "
                                "`${digest}`")
        endif()
    endforeach()
    file(READ "${ROOT}/tools/${name}-pin" text)
    if(text MATCHES "${version}")
        message(FATAL_ERROR "tools/${name}-pin spells ${version}, which belongs "
                            "in tools/llvm-version alone")
    endif()
endforeach()
if(NOT llvm_tag STREQUAL clang_tag)
    message(FATAL_ERROR "tools/llvm-pin names ${llvm_tag} and tools/clang-pin "
                        "${clang_tag}. Every archive comes from one release.")
endif()

foreach(script get-llvm.cmake get-clang.cmake fetch-release.cmake install.sh
        install.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    if(text MATCHES "${version}" OR text MATCHES "anti-lang/llvm-tools")
        message(FATAL_ERROR "tools/${script} spells a version or the release, "
                            "which belong in tools/llvm-version and the pins")
    endif()
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
