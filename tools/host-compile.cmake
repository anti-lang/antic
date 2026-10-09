# The options of the pinned clang that compile a source of antic or anti
# for one host. tools/pack-anti.cmake compiles the programs of a package
# with them, and the tests host_sources_<host> compile every source with
# them and the warning set, so a warning of another host's headers or
# ABI fails the suite of any machine before it fails a release.

include("${CMAKE_CURRENT_LIST_DIR}/windows-compile.cmake")

# The triple of clang for host. A Windows host takes the gnu triple of
# tools/windows-compile.cmake, as every C of a Windows target does.
function(antic_host_triple host out)
    if(host MATCHES "^windows-")
        antic_windows_triple(triple "${host}")
        set("${out}" "${triple}" PARENT_SCOPE)
        return()
    endif()
    set(triples
        "macos-arm64=arm64-apple-macos11"
        "macos-x86_64=x86_64-apple-macos11"
        "linux-x86_64=x86_64-unknown-linux-musl"
        "linux-arm64=aarch64-unknown-linux-musl")
    foreach(row IN LISTS triples)
        if(row MATCHES "^${host}=(.*)$")
            set("${out}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "unknown host ${host}")
endfunction()

# The options of a compile for host into out: the target, the version
# antic reports, the release of the pinned clang anti names, the versions
# of musl and mimalloc that antic writes into a notice, and the headers
# of the family. root is the repository, sysroot the directory of
# tools/get-sysroot.cmake, resource the resource directory of the clang,
# macos_sdk the pinned Apple SDK, which only a macOS host reads, and
# version the version of tools/version. Each family reads its headers
# from a different place: the Apple SDK, the musl sysroot, or the
# mingw-w64 sysroot.
function(antic_host_compile_options out host root sysroot resource macos_sdk
                                    version)
    antic_host_triple("${host}" triple)
    include("${root}/tools/clang-release.cmake")
    antic_clang_release(clang_version clang_tag clang_page)
    include("${root}/tools/libc-versions.cmake")
    antic_libc_versions(musl_version mimalloc_version)
    set(options --target=${triple} -std=c11 -O2 "-ffile-prefix-map=${root}=."
                "-DANTIC_VERSION=\"${version}\""
                "-DANTI_CLANG_VERSION=\"${clang_version}\""
                "-DANTI_CLANG_TAG=\"${clang_tag}\""
                "-DANTI_CLANG_PAGE=\"${clang_page}\""
                "-DANTIC_MUSL_VERSION=\"${musl_version}\""
                "-DANTIC_MIMALLOC_VERSION=\"${mimalloc_version}\"")
    if(host MATCHES "^macos-")
        list(APPEND options -isysroot "${macos_sdk}")
    elseif(host MATCHES "^linux-")
        list(APPEND options --sysroot "${sysroot}/${host}")
    else()
        # antic is a Windows program too, compiled against the headers of
        # mingw-w64 as the programs it builds are.
        antic_windows_compile_options(windows_options "${sysroot}/${host}"
                                      "${resource}")
        list(APPEND options ${windows_options})
    endif()
    set("${out}" "${options}" PARENT_SCOPE)
endfunction()
