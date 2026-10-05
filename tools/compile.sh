#!/bin/sh
# Build antic, the anti tool and the runtime in one of four configurations.
#
#   ./c                  Debug build in build/host, the default
#   ./c release          Release build in build/release
#   ./c asan             Debug build under AddressSanitizer in build/asan
#   ./c ubsan            Debug build under both sanitizers in build/ubsan
#   ./c <config> test    build, then run the suite of that tree
#   ./c <config> fresh   configure the tree again before the build
#
# Each configuration is a preset of CMakePresets.json and builds in
# build/<preset>. host is the Debug build that CMakeLists.txt gives a tree
# without a build type. The first build of a tree configures it, which
# installs the pinned clang, the LLVM tools, the sysroots and raylib under
# build/deps. A later build configures again with fresh alone. The job
# count is the number of processors.
set -eu

root=$(cd "$(dirname "$0")" && pwd)
[ -f "$root/tools/version" ] || root=$(dirname "$root")
cd "$root"

preset=host
test=0
fresh=0
for arg in "$@"; do
    case "$arg" in
        debug|host) preset=host ;;
        release|asan|ubsan) preset=$arg ;;
        test) test=1 ;;
        fresh) fresh=1 ;;
        -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "c: unknown argument $arg" >&2; exit 64 ;;
    esac
done

jobs=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
tree="build/$preset"

if [ "$fresh" = 1 ] || [ ! -f "$tree/CMakeCache.txt" ]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset" -j "$jobs"
if [ "$test" = 1 ]; then
    ctest --preset "$preset" -j "$jobs"
fi
echo "c: $preset build in $tree"
