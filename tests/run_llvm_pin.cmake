# tools/llvm-pin names the release of anti-lang/llvm-tools that the LLVM
# tools come from: its tag, where it lies, the name of an asset and the
# digest of the asset of each of the five hosts. The version stands in
# tools/llvm-version alone and the rest in the pin alone.
#
#   cmake -DROOT=<repository> -P tests/run_llvm_pin.cmake

set(hosts linux-x86_64 linux-arm64 macos-arm64 windows-x86_64 windows-arm64)
set(pin "${ROOT}/tools/llvm-pin")
file(READ "${ROOT}/tools/llvm-version" version)
string(STRIP "${version}" version)
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "tools/llvm-version is `${version}`")
endif()

# Set <out> to the value of the row <key> of the pin, which must exist.
function(row key out)
    file(STRINGS "${pin}" line REGEX "^${key}=")
    if(line STREQUAL "")
        message(FATAL_ERROR "tools/llvm-pin has no row ${key}")
    endif()
    string(REGEX REPLACE "^${key}=" "" value "${line}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

row(tag tag)
if(NOT tag MATCHES "^@VERSION@-anti\\.[1-9][0-9]*$")
    message(FATAL_ERROR "the tag is `${tag}`, not @VERSION@-anti.<build>")
endif()
row(release release)
if(NOT release STREQUAL "https://github.com/anti-lang/llvm-tools/releases/download/@TAG@")
    message(FATAL_ERROR "the release lies at `${release}`")
endif()
row(file file)
if(NOT file MATCHES "@TAG@" OR NOT file MATCHES "@HOST@"
   OR NOT file MATCHES "\\.tar\\.xz$")
    message(FATAL_ERROR "the asset is `${file}`, which names no tag or host")
endif()
foreach(host IN LISTS hosts)
    row("${host}-digest" digest)
    string(LENGTH "${digest}" length)
    if(NOT digest MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 64)
        message(FATAL_ERROR "${host} has the digest `${digest}`")
    endif()
endforeach()
# The release carries an archive for macos-x86_64, which is no host of
# antic. No path in antic downloads it, so the pin names no digest for it.
file(STRINGS "${pin}" line REGEX "^macos-x86_64-")
if(NOT line STREQUAL "")
    message(FATAL_ERROR "tools/llvm-pin names macos-x86_64, which antic does "
                        "not support: ${line}")
endif()

# The public key that checks SHA256SUMS.sig, in PEM form, and never a
# private one.
set(key "${ROOT}/tools/llvm-tools-key.pem")
if(NOT EXISTS "${key}")
    message(FATAL_ERROR "tools/llvm-tools-key.pem, the key that checks "
                        "SHA256SUMS.sig, is missing")
endif()
file(READ "${key}" text)
if(NOT text MATCHES "^-----BEGIN PUBLIC KEY-----\n" OR text MATCHES "PRIVATE")
    message(FATAL_ERROR "tools/llvm-tools-key.pem is not a public key in PEM form")
endif()

file(READ "${pin}" text)
if(text MATCHES "${version}")
    message(FATAL_ERROR "tools/llvm-pin spells ${version}, which belongs in "
                        "tools/llvm-version alone")
endif()
foreach(script get-llvm.cmake install.sh install.ps1)
    file(READ "${ROOT}/tools/${script}" text)
    if(text MATCHES "${version}" OR text MATCHES "anti-lang/llvm-tools")
        message(FATAL_ERROR "tools/${script} spells a version or the release, "
                            "which belong in tools/llvm-version and "
                            "tools/llvm-pin")
    endif()
endforeach()

# tools/get-llvm.cmake installs into the directory that CMakeLists.txt
# reads by default, and each spells it once.
file(STRINGS "${ROOT}/CMakeLists.txt" line
     REGEX "^set\\(ANTIC_LLVM_DIR \"\\\${CMAKE_SOURCE_DIR}/[^\"]+\"")
string(REGEX REPLACE "^.*CMAKE_SOURCE_DIR}/([^\"]+)\".*$" "\\1" read "${line}")
file(STRINGS "${ROOT}/tools/get-llvm.cmake" line
     REGEX "^    set\\(DEST \"\\\${root}/[^\"]+\"\\)$")
string(REGEX REPLACE "^.*root}/([^\"]+)\".*$" "\\1" written "${line}")
if(read STREQUAL "" OR NOT read STREQUAL written)
    message(FATAL_ERROR "CMakeLists.txt reads the LLVM tools from `${read}`, "
                        "and tools/get-llvm.cmake installs them into "
                        "`${written}`")
endif()
