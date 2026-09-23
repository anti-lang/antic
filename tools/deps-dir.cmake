# The directory of the pinned downloads: clang, the LLVM tools, the
# sysroots, raylib and the sources of the native libraries.
#
# DESIGN: the downloads lie in build/deps of the checkout, one copy that the
# host tree and both sanitizer trees read. tools/deps-dir.cmake is the one
# place the build names that directory. The configure step and the get-*.cmake
# scripts include it.
#
# DESIGN: ANTI_DEPS_DIR names another directory of downloads, so a worktree
# reads those of the main checkout instead of fetching its own. The
# configure step takes it as a cache variable, and a script run with -P as
# the variable -DANTI_DEPS_DIR gives it. Unset or empty, it is build/deps.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(ANTI_DEPS_DIR "" CACHE PATH
        "the directory of the pinned downloads, empty for build/deps")
endif()
if(DEFINED ANTI_DEPS_DIR AND NOT ANTI_DEPS_DIR STREQUAL "")
    get_filename_component(ANTIC_DEPS_DIR "${ANTI_DEPS_DIR}" ABSOLUTE)
else()
    get_filename_component(ANTIC_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/../build/deps"
                           ABSOLUTE)
endif()
