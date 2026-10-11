# Chapters 11 and 12 of the developer's guide

The step `guide-ownership` of the documentation driver, on 2026-10-11, on
the branch `docs`. It writes chapters 11 and 12 of
`docs/site/developers-guide/` in full. It changes no code under `src/`.
Under `tests/` it adds one field to the excerpt header of
`tests/run_document_examples.cmake`.

## What the step wrote

- `11-ownership.md` has five sections: `own` and `lent`, `dup` and moves,
  teardown and temporaries, the rules at a call, and `Shared<T>`.
- `12-memory.md` has five: pointers, slices and views, the bounds checks,
  `anti.mem` and the allocator, and `--memory-checks`.

Every section has the five parts of the task. A move stands as the copy
and the one store that lowering writes, and a teardown as the code at each
exit of a block. The count of a `Shared<T>` stands as `atomicrmw` in the
module, as `lock incq` and `lock decq` on x86-64 and as the `ldaxr` loop of
linux-arm64. The bounds check stands in dev mode, in release mode and in a
release build with `--checks`.

## The examples it added

The two pages hold 14 `anti` blocks that compile, 11 of them programs that
run against their output. The compiler refuses 65 more blocks with the
error each names, and the pages hold 39 excerpts.

- Chapter 11: `owners`, `moves`, `teardown`, `calls`, `shared`, `cycle`.
- Chapter 12: `ptrs`, `slices`, `bounds`, `pools`, `checked`.

Chapter 12 holds the one time table of the two chapters. It gives the five
programs of `tests/bench` with and without `--memory-checks`, from
`tests/run_bench.cmake`, with the machine, the commit, the mode and both
commands. `tests/bench` gained no program.

## The runner

An `llvm` or `asm` excerpt may end its header with `option=checks`,
`option=no-checks` or `option=memory-checks`. The run builds the block with
that option of antic. Four changed copies of chapter 12 prove the field,
each in `build/scratch-guide/proof-option/`: a wrong line under
`option=checks`, the `__asan_loadN` excerpt without its option, an option
on an `ir` dump and an unknown option. All four fail with the message of
their case.

## Answers from the code

- A move is `memcopy` into a slot of the frame and a store of zero into
  the table pointer of the local. The teardown of a local that moves
  somewhere tests the table first. A struct or a tuple clears the table of
  each owning part.
- A local that never moves is torn down behind a test that traps on a zero
  table, `anti_rt_table_unset`. The test stays in a release build.
- An argument that is a temporary is torn down right after its call. A
  value read through a field, or used as the object of a call, lives to
  the end of the statement. A slot of the frame holds its address.
- A plain class value parameter gets a copy of the bytes: `byval` on
  x86-64, a `memcpy` before the call on arm64. A write to its field does
  not reach the caller.
- An `own` pointer parameter is `noalias` and is torn down by no one. The
  function writes `delete(p)`.
- A `str` is copied into an `own` parameter and does not move. A slice
  and a struct with a pointer field move.
- `Shared<T>.new` is one `malloc` of the cell, with the count and the value
  in one block. `share()` is one atomic add and no allocation.
- `alloc(T, n)` is `malloc(n * size)`, not zeroed, unless `T` is a class or
  owns something, where it is `calloc`.
- A pointer has no bounds check in any mode, and neither has `s[lo..hi]`.
- `alloc` and `free` of `Allocator` are the table entries 17 and 18.
- `--memory-checks` puts a call of `__asan_loadN` or `__asan_storeN` before
  each load and store, and none before an access of the frame or a global.
  A release build with the option links without LTO.

## What was wrong in older documents and in antic

- `return s.slot` of a field that owns something compiles. It copies the
  bytes, and the caller and the field then free one block twice. A program
  of 19 lines ends by signal 5 on macos-arm64 in dev and in release. `let`
  and `=` refuse the same copy. The guide states no refusal there.
- `docs/anti-language-additions.md` says `--memory-checks` makes a program
  run two to three times slower. The five programs of `tests/bench` ran
  8.1 to 53.5 times slower on the Mac, by the table of chapter 12.
- `CLAUDE.md` and the skeleton of chapter 12 name the table entries 8 and
  9 for `alloc` and `free`. `src/rt/object.h` gives 17 and 18, after the
  nine hooks.
- `xs[lo..hi]` is not checked against the length in dev mode, and its
  length `hi - lo` is a plain subtraction. The guide says so.
- A constant index past the end, `a[4]` of a `[4]int`, compiles.
- A pointer to a local, a slice of a local array and a pointer read after
  `free` all compile. The object model calls ownership a tree that is
  stated and not checked, and the guide lists the three as pitfalls.
- The first form of the `owners` example added to bytes of `alloc(byte, n)`
  that nothing had written. It printed the right sums by chance, and the
  example now stores a zero first.
- A program that writes `catch fatal` on a `?*T` needs `import anti.lang`.
  The runner adds the import to every block, so a reader who copies an
  example into a file of their own must add the line.

## Open points for Eddie

- `check-web-content` reports the part headings and "Where it lives:", as
  in every chapter. It also counts a sentence that starts with a code span
  as longer than the docs-style checker does. The step fixed the contrast
  frames it named and left the rest.
- The time table was measured while the audit ran in the main checkout.
  The C twins took the same time in both runs within 4 percent.
- `guide_examples` took 136 s in the suite.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 354 s with `ctest --preset host -j14`. The sanitizer suites did not
run, since the step changed no C file under `tests/` and not
`tests/CMakeLists.txt`. The docs-style checker reports nothing on the two
pages and on this report. `git grep` finds no marker of unfinished text in
the two pages. This is the state before the commit of this report:

```text
$ git log --oneline -3
6b9d6c7a Write the pixels of the ownership example before it reads them
032311b3 Write chapter 12 of the developer's guide, memory
ff0386bd Take one option of antic in an excerpt of the document runner
$ git status --short
?? docs/reports/2026-10-11-docs-guide-ownership.md
$ git rev-parse HEAD origin/docs
6b9d6c7a547e2fb9fcfdfcb6ff58a5dc1a78d2e9
6b9d6c7a547e2fb9fcfdfcb6ff58a5dc1a78d2e9
```
