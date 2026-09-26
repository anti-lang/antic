# The warning flags of each third-party library, as its own project
# builds it. The sources are theirs, so a warning in them is reported by
# their rules, printed and never patched. The glue and the probes around
# them are ours and take ANTIC_C_WARNINGS of tools/warnings.cmake.
# docs/c-guidelines.md states the rule, and the test warning_set refuses a
# warning flag spelled anywhere but here and in tools/warnings.cmake.
# DESIGN: where a project has a Makefile and a CMake build, the flags
# are those of the Makefile, the build a static library of the release
# comes from. Each set is the warnings of that build for clang, nothing
# added and nothing taken away. See "Libraries and runtime" in
# docs/decisions.md.

# PCRE2 10.48. Neither configure nor CMakeLists.txt adds a warning flag
# unless asked to. --enable-Werror is off by default.
set(ANTIC_PCRE2_WARNINGS "")

# SQLite 3.53.4. The amalgamation is compiled as sqlite.org documents it,
# with no warning flag.
set(ANTIC_SQLITE_WARNINGS "")

# Mbed TLS 3.6.7. WARNING_CFLAGS of library/Makefile.
set(ANTIC_MBEDTLS_WARNINGS -Wall -Wextra -Wformat=2 -Wno-format-nonliteral)

# miniaudio 0.11.24. It ships one CMakeLists.txt, which adds these for
# clang.
set(ANTIC_MINIAUDIO_WARNINGS -Wall -Wextra -Wpedantic)

# raylib 6.0. CFLAGS of src/Makefile for PLATFORM_DESKTOP_GLFW.
set(ANTIC_RAYLIB_WARNINGS -Wall -Wno-missing-braces -Werror=pointer-arith
    -Werror=implicit-function-declaration)
# Two definitions of raylib's CMake build turn off the deprecation
# warnings of a system. On Windows the library compiles against the C
# runtime of MSVC, for which that build defines _CRT_SECURE_NO_WARNINGS;
# the Makefile builds with MinGW, whose C runtime deprecates nothing. On
# macOS it defines GL_SILENCE_DEPRECATION, since Apple deprecates OpenGL.
set(ANTIC_RAYLIB_WARNINGS_windows -D_CRT_SECURE_NO_WARNINGS)
set(ANTIC_RAYLIB_WARNINGS_macos -DGL_SILENCE_DEPRECATION)
