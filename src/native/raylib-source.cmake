# The source of raylib, which the tests read beside raylib.cmake.
# CMakeLists.txt at the top includes this file before it adds tests/ and
# src/native/, so both find the source in place.
#
# The source is the release that tools/raylib-pin names, which the
# bindings use as well. antic_get_source of get-source.cmake
# downloads it into build/deps/raylib and checks the pinned digest, and
# ANTIC_RAYLIB_SOURCE names the unpacked release.

antic_shared_path(ANTIC_RAYLIB_DIR "${ANTIC_DEPS_DIR}/raylib"
    "the raylib source from tools/raylib-pin")
antic_get_source(ANTIC_RAYLIB_SOURCE raylib "${ANTIC_RAYLIB_DIR}" SHA256
    src/rcore.c)
