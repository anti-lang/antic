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
#   ./c test changed [<base>]
#                        build in build/host and run the tests that the
#                        changes since <base>, HEAD by default, select:
#                        the tracked changes, staged and not, and the
#                        untracked files outside build/. A base such as
#                        origin/main takes in every commit since it
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
# A run of the suite writes build/coverage/commit.txt beside the report:
# the commit it measured, then clean, or modified when a file outside
# docs/, CLAUDE.md, README.md and CHANGELOG.md differed from that commit
# at either end of the run. A batch of coverage takes a clean run as its
# own before run when none of those files changed since, by answer 3 of
# "Answers of 2026-11-25" in docs/decisions.md.
#
# The coverage build instruments antic, anti, the unit tests and the
# runtime its programs link. Its suite leaves raw profiles in
# build/coverage/profiles, and the report is build/coverage/summary.txt,
# one row per file of src/antic, src/anti and src/rt, with the annotated
# sources in build/coverage/html. The status is that of the suite, and
# the report is written after a failing test as well.
# tools/coverage.cmake holds the flags and the directory.
#
# Each configuration is a preset of CMakePresets.json and builds in
# build/<preset>. host is the Debug build that CMakeLists.txt gives a tree
# without a build type. The first build of a tree configures it, which
# installs the pinned clang, the LLVM tools, the sysroots and raylib under
# build/deps. A later build configures again with fresh alone. The job
# count is the number of processors.
#
# tests/test-areas.txt maps each path to the areas of the tests a change
# of it selects. changed prints the areas and the number of tests it
# picked and runs them, the unit tests and the guards of the repository
# among them, by answer 2 of "Answers of 2026-11-25" in docs/decisions.md.
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
changed=0
base=HEAD
for arg in "$@"; do
    if [ "$after" = part ]; then
        part=$arg
        after=
        continue
    fi
    if [ "$after" = changed ]; then
        base=$arg
        after=
        continue
    fi
    case "$arg" in
        debug|host) preset=host ;;
        release|asan|ubsan|coverage) preset=$arg ;;
        test) test=1 ;;
        changed) changed=1; test=1; after=changed ;;
        fresh) fresh=1 ;;
        part) after=part ;;
        report) report=1 ;;
        -h|--help) sed -n '2,52p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "c: unknown argument $arg" >&2; exit 64 ;;
    esac
done
if [ "$after" = changed ]; then
    after=
fi
if [ "$changed" = 1 ] && [ "$preset" != host ]; then
    echo "c: test changed runs the tests of build/host" >&2
    exit 64
fi
if [ "$changed" = 1 ] && ! git rev-parse --verify --quiet "$base^{commit}" >/dev/null; then
    echo "c: test changed takes a commit, not $base" >&2
    exit 64
fi
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

# The map of the areas, which tests/test_areas.cmake names as well and
# the test test_areas holds to this spelling.
map=tests/test-areas.txt

# Print the number of / in $1.
slashes() {
    printf '%s' "$1" | tr -cd / | wc -c | tr -d ' '
}

# Print the words of the first line of the map that names the path $1,
# and all when no line names it. A * of a file line stands for any
# characters but /, so the line and the path hold as many / each.
map_words() {
    depth=$(slashes "$1")
    while read -r pattern words; do
        case "$pattern" in
            ''|'#'*|areas:) continue ;;
            */) case "$1" in "$pattern"*) echo "$words"; return ;; esac ;;
            *)
                if [ "$(slashes "$pattern")" = "$depth" ]; then
                    case "$1" in $pattern) echo "$words"; return ;; esac
                fi ;;
        esac
    done < "$map"
    echo all
}

# Print the files of the tests of the tree, by the subdirs() line of each.
test_files() {
    dirs=$tree
    while [ -n "$dirs" ]; do
        next=
        for dir in $dirs; do
            echo "$dir/CTestTestfile.cmake"
            for sub in $(sed -n 's/^subdirs("\(.*\)")$/\1/p' "$dir/CTestTestfile.cmake"); do
                next="$next $dir/$sub"
            done
        done
        dirs=$next
    done
}

# Print the texts that name the path $1 of tests/ in a test file: the
# file, a file of its stem beside it and each directory that holds it,
# up to tests/, each before the characters that may end a word there.
# The test files spell the source directory as the tree's cache names it.
named_texts() {
    dir=${1%/*}
    file=${1##*/}
    stem=${file%%.*}
    for end in '"' ',' ';' ']'; do
        echo "$source/$1$end"
    done
    if [ -n "$stem" ]; then
        echo "$source/$dir/$stem."
    fi
    while :; do
        for end in '"' ',' ';' ']' '/"'; do
            echo "$source/$dir$end"
        done
        case "$dir" in */*) dir=${dir%/*} ;; *) break ;; esac
    done
}

if [ "$changed" = 1 ]; then
    mkdir -p "$tree"
    paths="$tree/changed-paths.txt"
    {
        git -c core.quotepath=false diff --name-only --no-renames "$base"
        git -c core.quotepath=false ls-files --others --exclude-standard
    } | grep -v '^build/' | sort -u > "$paths"
    areas=
    named="$tree/changed-named.txt"
    : > "$named"
    everything=0
    while IFS= read -r path; do
        words=$(map_words "$path")
        case "$words" in
            all) everything=1 ;;
            none) ;;
            named) echo "$path" >> "$named" ;;
            *) areas="$areas $words" ;;
        esac
    done < "$paths"
    echo "c: $(wc -l < "$paths" | tr -d ' ') changed paths since $base"
    if [ "$everything" = 0 ] && [ -z "$areas" ] && [ ! -s "$named" ]; then
        echo "c: the changes select no test"
        exit 0
    fi
fi

if [ "$fresh" = 1 ] || [ ! -f "$tree/CMakeCache.txt" ]; then
    cmake --preset "$preset"
fi
# Print the commit of HEAD, then clean, or modified when a file outside
# docs/, CLAUDE.md, README.md and CHANGELOG.md differs from it.
measured_state() {
    git rev-parse HEAD
    if [ -n "$(git status --porcelain -- . ':!docs/' ':!CLAUDE.md' \
                   ':!README.md' ':!CHANGELOG.md')" ]; then
        echo modified
    else
        echo clean
    fi
}

if [ "$preset" = coverage ]; then
    status=0
    measured="$tree/commit.txt"
    if [ "$report" = 0 ]; then
        cmake --build --preset coverage -j "$jobs"
        # The profiles of an earlier run count for programs that have
        # changed since, so a run starts without them.
        if [ "$number" = 1 ]; then
            cmake --build --preset coverage --target coverage_clear
            rm -f "$measured"
            measured_state > "$measured.start"
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
        # The run measured one commit when the tree stood at it at the
        # start of the first part and at the end of the last.
        if [ "$report" = 0 ]; then
            measured_state > "$measured.end"
            if [ -f "$measured.start" ] &&
               cmp -s "$measured.start" "$measured.end"; then
                mv "$measured.end" "$measured"
            else
                { git rev-parse HEAD; echo modified; } > "$measured"
                rm -f "$measured.end"
            fi
            rm -f "$measured.start"
            echo "c: $(tr '\n' ' ' < "$measured")in $measured"
        fi
    fi
    echo "c: coverage build in $tree"
    exit "$status"
fi
cmake --build --preset "$preset" -j "$jobs"
if [ "$changed" = 1 ]; then
    status=0
    if [ "$everything" = 1 ]; then
        total=$(ctest --preset host -N | sed -n 's/^Total Tests: //p')
        echo "c: areas: all, $total tests"
        ctest --preset host -j "$jobs" || status=$?
        echo "c: $preset build in $tree"
        exit "$status"
    fi
    # A change that selects a test selects the unit tests and the guards
    # of the repository as well.
    areas=$(echo unit repo $areas | tr ' ' '\n' | sort -u | tr '\n' ' ')
    areas=${areas% }
    expression="^($(echo "$areas" | tr ' ' '|'))\$"
    listed="$tree/changed-listed.txt"
    ctest --preset host -N -L "$expression" |
        sed -n 's/^ *Test *#[0-9]*: //p' > "$listed"
    # The tests that name a changed file of tests/ and carry none of the
    # areas, by the rule `named` of tests/test-areas.txt. A file that no
    # test names selects every test.
    extra="$tree/changed-tests.txt"
    : > "$extra"
    texts="$tree/changed-texts.txt"
    files=$(test_files)
    source=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$tree/CMakeCache.txt")
    while IFS= read -r path; do
        named_texts "$path" > "$texts"
        names=$(grep -h -F -f "$texts" $files |
            sed -n -e 's/^add_test("\([^"]*\)".*/\1/p' \
                   -e 's/^set_tests_properties("\([^"]*\)".*/\1/p' |
            sort -u)
        if [ -z "$names" ]; then
            echo "c: no test names $path, so it selects every test"
            everything=1
            break
        fi
        echo "$names" >> "$extra"
    done < "$named"
    if [ "$everything" = 1 ]; then
        total=$(ctest --preset host -N | sed -n 's/^Total Tests: //p')
        echo "c: areas: all, $total tests"
        ctest --preset host -j "$jobs" || status=$?
        echo "c: $preset build in $tree"
        exit "$status"
    fi
    sort -u "$extra" | grep -v -x -F -f "$listed" > "$extra.new" || true
    mv "$extra.new" "$extra"
    count=$(wc -l < "$listed" | tr -d ' ')
    more=$(wc -l < "$extra" | tr -d ' ')
    echo "c: areas: $areas, $count tests"
    echo "c: tests that name a changed file of tests/: $more more"
    ctest --preset host -j "$jobs" -L "$expression" || status=$?
    if [ "$more" != 0 ]; then
        ctest --preset host -j "$jobs" --tests-from-file "$extra" || status=$?
    fi
    echo "c: $preset build in $tree"
    exit "$status"
fi
if [ "$test" = 1 ]; then
    ctest --preset "$preset" -j "$jobs"
fi
echo "c: $preset build in $tree"
