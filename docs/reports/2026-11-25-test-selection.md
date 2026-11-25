# Test selection

This step builds the three answers of "Answers of 2026-11-25" in `docs/decisions.md`.
It starts at `3bea1f95`.

## Commits

- `0ed312b0` records the three answers and the three provisional choices of the step.
- `a2178a6a` gives every test its areas, adds the map `tests/test-areas.txt`, the guard
  `test_areas` with its self test `test_areas_runner`, and the word `./c test changed`.
  `CLAUDE.md` states answer 1 and names the word under "Building".
- `340d9f20` makes `./c coverage` write `build/coverage/commit.txt`, and records the
  fourth provisional choice.

## Areas

The line `areas:` of `tests/test-areas.txt` names seven areas. "Choosing the tests of
a change" in `docs/notes/hosts-and-harness.md` gives the rule of each.

| Area | Tests | Rule |
|---|---|---|
| `compiler` | 8486 | runs antic, or reads `src/antic/` |
| `runtime` | 8502 | runs antic or anti, names the runtime archive, or reads `src/rt/` or `src/native/` |
| `std` | 8502 | the same, or reads `src/std/` |
| `tool` | 1116 | runs anti, or reads `src/anti/` |
| `packaging` | 147 | the pins, the toolchain, the packages, the installers, `./r`, `deps_dir` |
| `repo` | 61 | a guard over the whole tree, always selected |
| `unit` | 67 | a C unit test or a program of the runtime of the unit tests, always selected |

`tests/test_areas.cmake` reads the map, and `antic_test_area(<area>...)` opens a
section: 22 calls in `tests/CMakeLists.txt` and 2 in `src/native/CMakeLists.txt`.
`test_areas` refuses a test without an area and a label outside the set. It also reads
the command of each test, so one that names antic, anti, the runtime archive or a
directory of `src/` carries the areas of what it names. It refuses a line of the map
that names nothing, and a `tools/compile.sh` that does not spell the path of the map.
It failed for all 8579 tests before the sections existed and passes in 6.5 s.

## The map and the word

The first line of the map that names a path decides. `docs/`, `CLAUDE.md`, `README.md`
and `CHANGELOG.md` select nothing. Every `CMakeLists.txt`, `CMakePresets.json`,
`tests/runners/`, the helpers `tests/*.cmake`, `tools/*.cmake` and the pins select every
test. A file of `tests/` selects the tests that name it, a file of its stem or a
directory of `tests/` that holds it, and every test when none does.

`./c test changed [<base>]` reads `git diff --name-only --no-renames <base>`, `HEAD` by
default, and the untracked files outside `build/`. It adds `unit` and `repo`, prints the
number of paths, the areas and the number of tests, and runs `ctest -j14 -L` on the
areas, then `--tests-from-file` on the named tests outside them.

## Measurements

Each change added one empty line to the file and was reverted after the run.

| Change | Areas | Tests | Wall time | CPU time of the tests |
|---|---|---|---|---|
| `src/antic/lexer.c` | compiler, repo, unit | 8553 | 1056 s | 14,531 s |
| `src/anti/build.c` | tool, repo, unit | 1182 | 263 s | 1,158 s |
| `docs/notes/llvm.md` | none | 0 | 0.1 s | 0 s |

The full host suite in three parts took 15,244 s of test time. A selection without a
run gave 2447 tests for a program of `tests/matrix/` and 452 for
`tests/anti-build/messages.cmake`. The 2292 refusals of the matrix run in
`tests/matrix/` and take 28 s of CPU time between them.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and `-I 4301,8581`: 391 s,
285 s and 434 s before `a2178a6a`, 390 s, 284 s and 433 s before `340d9f20`. Each
sanitizer suite ran once in the six parts of `asan`, the last up to test 8579: `asan`
250 s, 185 s, 275 s, 170 s, 110 s and 684 s, `ubsan` 254 s, 182 s, 275 s, 167 s, 116 s
and 726 s. Every test passed. The host tree holds 8581 tests and each sanitizer tree
8579, two more than before, both of `test_areas`.

## Coverage

`./c coverage` removes `build/coverage/commit.txt` at its first part and writes it after
the report of its last part: the commit of `HEAD`, then `clean` or `modified`. A run is
`clean` when `HEAD` and the tree outside the four texts stood the same at both ends. A
run with stand-in tools showed both lines. No real coverage run was part of this step.

## Questions for Eddie

1. A change of `src/antic/` selects 8553 of the 8581 tests, since every test that runs
   antic carries `compiler`. The fast set of answer 1 is then the suite in one run. It
   saves time for a change of `src/anti/`, of `tools/`, of a test file or of texts
   alone. Is that the selection you want, or should a change of the compiler skip some
   family, such as the matrix in dev mode?
2. Tests of `packaging` that name antic or anti, `package_pack` among them at 300 s,
   carry `compiler` as well, so a change of the compiler runs them. Your note said they
   matter only when the packaging or the tools change. Should `test_areas` let a test
   of `packaging` name antic without `compiler`?
3. Answer 3 counts `CLAUDE.md`, `README.md` and `CHANGELOG.md` with `docs/`, since every
   batch writes its times to `CLAUDE.md`. Read as written, no batch could take the run
   of the batch before. Is the reading right?
