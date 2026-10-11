# Chapters 19 to 21 of the developer's guide

The step of the documentation driver that follows `guide-library`, on
2026-10-11, on the branch `docs`. It writes chapters 19, 20 and 21 of
`docs/site/developers-guide/` in full, which completes the guide. It
changes no code under `src/`. The task text gives the step no id, so the
report takes the name `guide-final`.

## What the step wrote

- `19-tests.md` holds the `tests` and `fixtures` blocks in the five parts
  of a feature section. It holds `anti test` with the output of a run that
  passes, of a failed assertion, of a trap and of an error that reaches
  `fatal`. The forms that state an expected output and `--memory-checks`
  in a test run follow.
- `20-targets.md` follows the form of chapters 1, 2 and 18. It holds the
  six targets, `--target` and `--cpu`, the six processor levels with the
  check at start, cross-building, the state of script mode, `-g` with an
  lldb session, the build id and the LLVM pipeline of each kind of build.
- `21-performance.md` holds one table of 89 rows, one per feature section
  of chapters 3 to 17, 19 and 20, with the counts of its cost part and a
  link to it. `tests/bench` and its table follow, then the profile
  options, tracing and the checks of a release build.

## The examples it added

The three pages hold 7 `anti` blocks that compile, 2 of them programs that
run against their output. The compiler refuses 7 more with the error each
names, and the pages hold 9 excerpts.

- Chapter 19: `purse`, with the code of a test from a build with
  `--tests`, and `labels`. Four more modules stand without a name.
- Chapter 20: `levels`, with `blend`, `tick` and `widen` at `v1` and `v3`
  and at `armv8.0` and `armv8.2`, and the nodes of `-g`.
- Chapter 21: `kept`, with `pick` and `total` in a release build and in
  one with `--checks`.

A script compared the 110 links of chapter 21 with the headings of the
pages. Each resolves, by the heading ids Goldmark writes by default.

## Changes under tests/

- `tests/run_document_examples.cmake` takes three more options of an
  excerpt: `option=tests` is `--tests`, `option=debug` is `-g` and
  `option=cpu-<level>` is `--cpu <level>`. Six copies of the two pages,
  each with a wrong line or without its option, fail the runner.
- `tests/run_bench.cmake` takes `RELEASE_OPTIONS`, options for the release
  build alone. Dev mode refuses `--profile-use`, so a build with a profile
  could not go through the runner before.

## The measured table

`tests/run_bench.cmake` ran seven times at commit `a9e8ffd0` on the
development Mac, an Apple M4 Pro. The Anti times in milliseconds:

| Program | Plain | `--lto none` | `--checks --asserts` | `--no-hooks` | `--trace objects` | `--profile-use` |
|---|---:|---:|---:|---:|---:|---:|
| `scalar_loop` | 284.6 | 286.3 | 286.4 | 287.8 | 288.9 | 288.6 |
| `objects` | 104.0 | 102.8 | 120.4 | 102.6 | 483.3 | 79.0 |
| `builder` | 56.0 | 57.6 | 56.4 | 59.2 | 58.1 | 52.5 |
| `simd_loop` | 111.3 | 111.5 | 111.5 | 112.3 | 113.7 | 114.5 |
| `map_work` | 136.2 | 137.0 | 165.4 | 132.2 | 139.3 | 144.7 |

The C twins took 283.1, 101.2, 42.9, 111.1 and 125.4 ms in the first run.

## What was wrong in antic

None of these is changed. The guide states each as it is.

- A dev run of `anti test` over a module that declares `main` fails at
  the link, with no line of totals. antic links the object of that module
  as a program without the objects of its imports. `--release` tests it.
- `anti test` reports the end of a runner as
  `antic: <runner> ended by signal 6`, and counts `3 tests in 1 modules`.
- A trap of a dev check inside a test names no test. A failed assertion
  alone prints `FAIL <module>.<test>`.
- A `setup` with a parameter compiles. The error then names a line of the
  runner.
- `-I src` gives `./src/com/example/purse.anti` the module path `purse`,
  since the root is compared with the path as written.
- A dev run prints each warning of a module twice, once for its library
  file and once for its object.
- `--checks` in a release build does not bring back the test for a zero
  table before a dispatch.
- With `--checks`, the overflow check of `t += items[i]` keeps LLVM from
  vectorizing the loop.
- With a profile `map_work` ran slower, 144.7 ms against 136.2.

## What was wrong in older documents

- `docs/tooling.md` gives `anti test [--release] [--all-modes]`. It
  describes a run of every module and of the programs under `test/`
  against expected outputs. The tool takes files and `-I` roots, has no
  `--all-modes` and reads no `test/`.
- "Debug information" of `docs/decisions.md` describes `.file` and `.loc`
  directives and a compile unit of language C99 that antic writes as
  bytes. `src/antic/llvm_debug.c` writes metadata nodes, with
  `DW_LANG_C11`, and llc writes the tables.
- "Build ids" of `docs/decisions.md` names the assembly as the digested
  text and `src/antic/debug.c` as the file of the ranges. The digest takes
  the LLVM text, and the spans come from `src/antic/llvm_debug.c`.
- Script mode stands in `docs/anti-language-additions.md` as a rule. The
  lexer refuses `#`, and `anti file.anti` prints the usage.

## Answers from the code

- A build without `--tests` holds nothing of the two blocks. The assembly
  of both modes and the library file are equal byte for byte with the
  blocks and without them.
- The runner is Anti source under `anti/test/`. The runtime keeps the name
  of the running test in two words.
- `antic -c` writes the same library file for every level and for every
  target, for the module of chapter 20.
- At `v3`, `a + (b - a) * t` is `vsubsd`, `vmulsd`, `vaddsd`. antic writes
  no flag that lets LLVM fuse the two.
- A program of macos-x86_64 at the default `v3` stops under Rosetta with
  status 70 and the message of AVX2.
- The build id is the same with `-g`, in another directory and under
  `--lto none`, and differs per target.
- `--profile-generate` refuses `-S`, so no excerpt shows an instrumented
  module.
- The traced `objects` makes 245.76 million traced calls and ran in
  483.3 ms against 104.0, with no handler installed.

## Open points for Eddie

- The links of chapter 21 are `relref` to a sibling page with a heading
  id. The guide stands beside `index.md`, which Hugo reads as a leaf
  bundle, the open point of `guide-plan`.
- `tests/bench` holds no program that installs a trace handler and none
  for a static field, so the guide states no time for either.
- `guide_examples` took 236 s in the suite.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 350 s with `ctest --preset host -j14`. The sanitizer suites did not
run: the step changed two CMake scripts under `tests/`, no C file and not
`tests/CMakeLists.txt`. The docs-style checker reports nothing on the three
pages and on this report. `git grep` finds no marker of unfinished text in
the three pages. This is the state before the commit of this report:

```text
$ git log --oneline -3
5f8fe30e State what anti test takes from the manifest's layout
42cf0b59 Write chapter 21 of the developer's guide, performance
a9e8ffd0 Take options for the release build alone in the bench runner
$ git status --short
$ git rev-parse HEAD origin/docs
5f8fe30e0b4aeaa8f5a15e0f81fe4d70b234ea67
5f8fe30e0b4aeaa8f5a15e0f81fe4d70b234ea67
```
