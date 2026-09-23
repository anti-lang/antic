# The directory of the pinned downloads: clang, the LLVM tools, the
# sysroots, raylib and the sources of the native libraries.
#
# DESIGN: the downloads lie in build/deps of the checkout, one copy that the
# host tree and both sanitizer trees read. This file is the one place the
# build names that directory. The configure step and the get-*.cmake
# scripts include it.
get_filename_component(ANTIC_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/../build/deps"
                       ABSOLUTE)
