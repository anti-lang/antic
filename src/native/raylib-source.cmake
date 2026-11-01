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

# The configuration of raylib's build: the desktop back end over GLFW with
# OpenGL 3.3, the default of raylib's own Makefile, with raylib's config.h
# unchanged. raylib.cmake compiles the library with these definitions,
# and anti.raylib is bound under them.
set(ANTIC_RAYLIB_CONFIGURATION PLATFORM_DESKTOP_GLFW GRAPHICS_API_OPENGL_33)

# DESIGN: anti.raylib binds the four headers of raylib_public_headers in
# one module, by Q4 of "Answers of 2026-11-01" in docs/decisions.md. The
# order is the one of rcore.c, raylib.h first, which defines
# RL_VECTOR2_TYPE, RL_MATRIX_TYPE and the others, so the other three
# declare no type of their own. raylib's configuration holds, and
# RAYMATH_IMPLEMENTATION with it, under which rcore.c compiles raymath:
# its functions are then extern inline, libraylib defines each on every
# target, and anti bind writes no shim. The build and the tests of
# anti.raylib bind from these two lists.
set(ANTIC_RAYLIB_BIND_HEADERS raylib.h rlgl.h raymath.h rcamera.h)
set(ANTIC_RAYLIB_BIND_DEFINES ${ANTIC_RAYLIB_CONFIGURATION}
    RAYMATH_IMPLEMENTATION)
