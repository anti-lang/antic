# The options of the pinned clang that compile a source of antic or anti
# for one host, and the link of a Linux host. tools/pack-anti.cmake
# compiles and links the programs of a package with them. The tests
# host_sources_<host> compile every source with the options and the
# warning set, so a warning of another host's headers or ABI fails the
# suite of any machine before it fails a release. The test linux_libc
# compiles and links a stand-in with both, so a change of either that
# reaches the libc of the machine fails it as well.

include("${CMAKE_CURRENT_LIST_DIR}/target-compile.cmake")

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
    antic_target_triple(triple "${host}")
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

# The command that links a Linux program of a release into out: ld.lld
# of llvm_bin, static and position independent against the musl of the
# pinned sysroot of host, with the objects that follow output between the
# start files. The clang driver looks for the start files of gcc on
# Linux, so the objects go to ld.lld with the musl ones instead. The test
# linux_link_recipe refuses this link spelled anywhere else.
function(antic_host_linux_link out host sysroot llvm_bin output)
    set(lib "${sysroot}/${host}/usr/lib")
    set("${out}" "${llvm_bin}/ld.lld" -static -pie --no-dynamic-linker
                 -o "${output}" "${lib}/rcrt1.o" "${lib}/crti.o" ${ARGN}
                 "${lib}/libc.a" "${lib}/libclang_rt.builtins.a"
                 "${lib}/crtn.o" PARENT_SCOPE)
endfunction()
