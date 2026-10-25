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
# ANTIC_DEPS_DIR is the directory this file arrives at, which the build
# reads, and ANTI_DEPS_DIR the option a person gives.
#
# Two finer ways move one download each, and both follow ANTIC_DEPS_DIR by
# default: ANTI_DEPS_DIR holds the cache paths, by Q8 of "Answers of
# 2026-10-24" in docs/decisions.md, and the test deps_dir reads every one.
# ANTIC_CLANG_DIR and ANTIC_LLVM_DIR of tools/pinned-compiler.cmake are
# cache paths, which the release script and docs/vm-setup.md give for the
# VMs. The sysroots, raylib and the native sources are antic_shared_path of
# CMakeLists.txt, which also takes a path the cache of build/host recorded,
# so a sanitizer tree finds them there.
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
