# The versions of the two C libraries that every program of musl links,
# which the notice of such a program names: musl of the pinned sysroot,
# from tools/sysroot-pins, and mimalloc, from tools/mimalloc-pin.
# CMakeLists.txt compiles them into antic, src/native/mimalloc.cmake
# names the source directory of mimalloc after its version, the test
# license_notice expects both, and tools/pack-anti.cmake compiles them
# into the antic of a package.
function(antic_libc_versions musl_out mimalloc_out)
    file(STRINGS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sysroot-pins" musl
         REGEX "^MUSL_VERSION=")
    file(STRINGS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mimalloc-pin" mimalloc
         REGEX "^MIMALLOC_VERSION=")
    string(REGEX REPLACE "^MUSL_VERSION=" "" musl "${musl}")
    string(REGEX REPLACE "^MIMALLOC_VERSION=" "" mimalloc "${mimalloc}")
    if(musl STREQUAL "" OR mimalloc STREQUAL "")
        message(FATAL_ERROR "tools/sysroot-pins or tools/mimalloc-pin names "
                            "no version")
    endif()
    set(${musl_out} "${musl}" PARENT_SCOPE)
    set(${mimalloc_out} "${mimalloc}" PARENT_SCOPE)
endfunction()
