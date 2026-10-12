#!/bin/sh
# Build antic, the anti tool and the runtime in one of five configurations.
#
#   ./c                  Debug build in build/host, the default
#   ./c release          Release build in build/release
#   ./c asan             Debug build under AddressSanitizer in build/asan
#   ./c ubsan            Debug build under both sanitizers in build/ubsan
#   ./c <config> test    build, then run the suite of that tree
#   ./c <config> fresh   configure the tree again before the build
#
#   ./c coverage             build in build/coverage, run the suite and
#                            write the coverage report
#   ./c coverage part 1/3    the same with the suite in parts, one call per
#                            part. The first part removes the profiles of
#                            the run before, and the last writes the report
#                            from the profiles of every part
#   ./c coverage report      write the report again from the profiles on
#                            disk, and run no test
#
# The coverage build instruments antic, anti and the unit tests. Its suite
# leaves raw profiles in build/coverage/profiles, and the report is
# build/coverage/summary.txt, one row per file of src/antic, src/anti and
# src/rt, with the annotated sources in build/coverage/html. The status
# is that of the suite, and the report is written after a failing test as
# well. tools/coverage.cmake holds the flags and the directory.
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
part=
report=0
after=
for arg in "$@"; do
    if [ "$after" = part ]; then
        part=$arg
        after=
        continue
    fi
    case "$arg" in
        debug|host) preset=host ;;
        release|asan|ubsan|coverage) preset=$arg ;;
        test) test=1 ;;
        fresh) fresh=1 ;;
        part) after=part ;;
        report) report=1 ;;
        -h|--help) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "c: unknown argument $arg" >&2; exit 64 ;;
    esac
done
if [ "$preset" != coverage ] && { [ -n "$part$after" ] || [ "$report" = 1 ]; }; then
    echo "c: part and report belong to ./c coverage" >&2
    exit 64
fi
number=1
parts=1
if [ -n "$part$after" ]; then
    number=${part%%/*}
    parts=${part#*/}
    case "$part" in
        [1-9]/[1-9]|[1-9]/[1-9][0-9]|[1-9][0-9]/[1-9][0-9]) ;;
        *) number=0 ;;
    esac
    if [ "$number" -lt 1 ] || [ "$number" -gt "$parts" ]; then
        echo "c: part takes <number>/<parts>, as in ./c coverage part 1/3" >&2
        exit 64
    fi
fi

jobs=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
tree="build/$preset"

if [ "$fresh" = 1 ] || [ ! -f "$tree/CMakeCache.txt" ]; then
    cmake --preset "$preset"
fi
if [ "$preset" = coverage ]; then
    status=0
    if [ "$report" = 0 ]; then
        cmake --build --preset coverage -j "$jobs"
        # The profiles of an earlier run count for programs that have
        # changed since, so a run starts without them.
        if [ "$number" = 1 ]; then
            cmake --build --preset coverage --target coverage_clear
        fi
        if [ "$parts" = 1 ]; then
            ctest --preset coverage -j "$jobs" || status=$?
        else
            total=$(ctest --preset coverage -N | sed -n 's/^Total Tests: //p')
            first=$(( (number - 1) * total / parts + 1 ))
            last=$(( number * total / parts ))
            echo "c: part $number of $parts, tests $first to $last of $total"
            ctest --preset coverage -j "$jobs" -I "$first,$last" || status=$?
        fi
    fi
    if [ "$number" = "$parts" ]; then
        cmake --build --preset coverage --target coverage_report
    fi
    echo "c: coverage build in $tree"
    exit "$status"
fi
cmake --build --preset "$preset" -j "$jobs"
if [ "$test" = 1 ]; then
    ctest --preset "$preset" -j "$jobs"
fi
echo "c: $preset build in $tree"
