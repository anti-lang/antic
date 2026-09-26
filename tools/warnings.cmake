# The warnings of every C file of this repository: antic, anti, the
# runtime, the glue around the native libraries, the tests and what the
# packer compiles. Warnings are errors. docs/c-guidelines.md states the
# rule, and the test warning_set refuses a warning flag spelled anywhere
# else, apart from the flags of the third-party projects in
# src/native/warnings.cmake.
# DESIGN: the set once stood in a dozen places. The cross builds of
# anti_rt and the packer carried three warnings fewer than the host
# build, and a Mac builds its runtime from the cross build alone, so
# -Wconversion never read the runtime of a macOS program.

set(ANTIC_C_WARNINGS -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
    -Wstrict-prototypes)

# The set for the C++ files of the tests, which check that a generated
# header compiles as C++. -Wstrict-prototypes is C alone.
set(ANTIC_CXX_WARNINGS -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion)

# The same rule for MSVC, which a reader's build with
# -DANTIC_SYSTEM_COMPILER=ON may use on Windows.
set(ANTIC_MSVC_WARNINGS /W4 /WX)
