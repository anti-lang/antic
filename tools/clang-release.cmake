# The pinned clang that anti bind --clang takes, read from tools/llvm-version
# and tools/clang-pin: its version, the tag of its release for the host of
# anti and the page of that release. anti names them when the clang it
# finds is missing or of another major version. CMakeLists.txt compiles
# them into anti, the bind tests expect them, and tools/pack-anti.cmake
# compiles them into the anti of a package.
include("${CMAKE_CURRENT_LIST_DIR}/pins.cmake")

function(antic_clang_release version_out tag_out page_out host)
    file(STRINGS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/llvm-version" version
         LIMIT_COUNT 1)
    antic_release_pin(clang "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-pin"
                      "${host}")
    if(clang_TAG STREQUAL "")
        message(FATAL_ERROR "tools/clang-pin names no release for ${host}")
    endif()
    set(${version_out} "${version}" PARENT_SCOPE)
    set(${tag_out} "${clang_TAG}" PARENT_SCOPE)
    set(${page_out} "${clang_PAGE}" PARENT_SCOPE)
endfunction()
