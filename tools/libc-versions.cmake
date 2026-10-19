# The versions of the two C libraries that every program of musl links,
# which the notice of such a program names: musl of the pinned sysroot,
# from tools/sysroot-pins, and mimalloc, from tools/mimalloc-pin.
# CMakeLists.txt compiles them into antic, src/native/mimalloc.cmake
# names the source directory of mimalloc after its version, the test
# license_notice expects both, and tools/pack-anti.cmake compiles them
# into the antic of a package.
include("${CMAKE_CURRENT_LIST_DIR}/pins.cmake")

function(antic_libc_versions musl_out mimalloc_out)
    antic_pin_value(musl "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sysroot-pins"
                    MUSL_VERSION)
    antic_pin_value(mimalloc "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mimalloc-pin"
                    MIMALLOC_VERSION)
    set(${musl_out} "${musl}" PARENT_SCOPE)
    set(${mimalloc_out} "${mimalloc}" PARENT_SCOPE)
endfunction()
