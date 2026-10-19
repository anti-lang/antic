# The pinned clang that anti bind --clang takes, read from tools/llvm-version
# and tools/clang-pin: its version, the tag of its release and the page of
# that release. anti names them when the clang it finds is missing or of
# another major version. CMakeLists.txt compiles them into anti, the bind
# tests expect them, and tools/pack-anti.cmake compiles them into the anti
# of a package.
include("${CMAKE_CURRENT_LIST_DIR}/pins.cmake")

function(antic_clang_release version_out tag_out page_out)
    file(STRINGS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/llvm-version" version
         LIMIT_COUNT 1)
    antic_pin_value(tag "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-pin" tag)
    antic_pin_value(page "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/clang-pin" release)
    string(REPLACE "@VERSION@" "${version}" tag "${tag}")
    string(REPLACE "@TAG@" "${tag}" page "${page}")
    string(REPLACE "/releases/download/" "/releases/tag/" page "${page}")
    set(${version_out} "${version}" PARENT_SCOPE)
    set(${tag_out} "${tag}" PARENT_SCOPE)
    set(${page_out} "${page}" PARENT_SCOPE)
endfunction()
