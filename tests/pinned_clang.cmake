# Put the pinned clang of the build first on PATH, for a runner whose
# cases run `anti bind --clang`. Include it with CLANG_DIR set to the
# pinned clang of the build, ANTIC_CLANG_DIR, or empty.
#
# anti bind takes the pinned clang of build/deps beside its tree, and
# otherwise the first clang on PATH. A tree configured with a pinned clang
# elsewhere, as on the test machines, has none beside it, so the runner
# puts that one first on PATH. Every program the runner starts inherits
# it, and a case that sets PATH itself replaces it.

if(CLANG_DIR)
    file(TO_NATIVE_PATH "${CLANG_DIR}/bin" clang_bin)
    if(CMAKE_HOST_WIN32)
        set(ENV{PATH} "${clang_bin};$ENV{PATH}")
    else()
        set(ENV{PATH} "${clang_bin}:$ENV{PATH}")
    endif()
endif()
