# The check of chapters 12 to 21 of the developer's guide

A step of the documentation driver on 2026-10-11, on the branch `docs`.
It checks chapters 12 to 21 of `docs/site/developers-guide/` against the
code, as a reviewer who did not write them. The task text gives the step
no id, so the report takes the name `guide-check-12-21`, after the report
of chapters 1 to 11. The step changes no code under `src/` and no file
under `tests/`.

## Method

For each feature section the step read the parts "How antic builds it"
and "What it costs" and the line "Where it lives". It then read the code
behind each claim. Chapters 15 and 17 to 20 state the output of commands,
so the step ran those commands again. Scripts under
`build/scratch-guide/rv2/` rebuild each project, library and plugin of
the pages. A script read every "Where it lives" paragraph of the ten
pages. Of 402 files and names, five did not stand where the page put them.

## Changes

| Page | Change | Code read |
|---|---|---|
| 12 | A slice parameter is a pointer to a copy on windows-x86_64 and one `[2 x i64]` on arm64 | dev module of `slices` on five targets |
| 12 | The path of `ArenaAllocator.alloc` that fits has eight compares, not seven. Dev makes two calls and the overflow checks | `src/std/anti/mem.anti:107`, `:179`, `:185` |
| 12 | `--memory-checks` links without LTO by default alone, and Windows gets no options hook | `lto_mode`, `src/antic/driver.c:262`, `src/antic/memcheck.c:64` |
| 13 | A `dispatch` and a `join` each make one atomic load in their hook call | `src/rt/hooks.c:21` and `:68`, `src/rt/threads.c:403` |
| 13 | The count of threads follows the key `threads`. Every channel operation tests a null handle | `src/rt/threads.c:90`, `channel_of` of `src/rt/sync.c` |
| 14 | An operation of a `ConcurrentMap` hashes its key two to four times, not once | `concurrent.anti:154` to `:210` and `:634`, `map.anti:1037` |
| 14 | An element of a `SyncList` is a tuple `(T, int)` | `synchronized.anti:57` |
| 14 | `lower_class_equals` stands in `lower_eq.c` | `src/antic/lower_eq.c:625` |
| 15 | The tables of `Profiler`, `ChangeJournal` and `ErrorMonitor` are plain fields | `src/std/anti/trace.anti:138`, `:383`, `:561` |
| 15 | Every program reads the key `plugins` and walks the injectable interfaces at start | `fill_injections`, `src/rt/conf.c:781` and `:915` |
| 15 | The sizes of the plugin host name the `main` they belong to | rebuilt: 255,344, 134,912 and 57,856 bytes |
| 15 | Three names of "Where it lives" point at their files | `lower_function.c:21`, `lower_hook.c:53`, `whole.c:829`, `driver_index.c:202` |
| 16 | `List<int>` is 64 functions in the dev module and 44 in the release module | dev and release module of `growth` |
| 16 | A `set` or an `add` of a new key probes a second time | `put` and `free_slot`, `map.anti:433` and `:591` |
| 17 | The table of the Linux link modes names its two programs, with their sizes | rebuilt: 26,184 and 227,760 bytes |
| 17 | The plugin host has 834 named dynamic symbols. 835 counted the empty first entry | `llvm-readobj --dyn-symbols` of the host |
| 18 | The size of a dev program for macOS changes with the directory of the project | `nm -ap` shows the object paths |
| 19 | The library file of `purse` is 17,903 bytes, and the leak report names lines 5 and 16 | rebuilt, and the run of `squares` |
| 20 | `llvm-objdump -d` prints 9,996 lines, not 9912 | the two programs of the session |
| 21 | Ten rows of the cost table take the atomic loads of the hook calls their sections count | `src/rt/hooks.c:68` and `:92`, `src/rt/loaded.c:46` |
| 21 | Six more rows follow the corrections of chapters 12, 13 and 16 | the rows above |
| 21 | The column `--no-hooks` is measured two more times | `tests/run_bench.cmake` with `OPTIONS=--no-hooks` |

## Examples, excerpts and measurements added

- Chapter 12: an `llvm` excerpt of `anti.mem.ArenaAllocator.alloc` in
  release mode, with its eight compares.
- Chapter 21: a table of two more runs of `--no-hooks` for `builder` and
  `map_work`, with its conditions. The page called 59.2 ms against 56.0 ms
  noise, outside the 4 percent of the twins. The two runs gave 57.8 and
  55.5 ms. `map_work` repeated its gain at 132.0 and 132.6 ms.

## Claims that held

The step rebuilt these and found them as written.

- Chapter 16: `clear` of a `List` of `n` elements moves `n (n - 1) / 2`
  elements, and the constants of every layout.
- Chapter 17: the three libraries of `geometry` at 48,480, 362,292 and
  85,352 bytes, the 7 and 40 dynamic symbols, and the C program.
- Chapter 17: the plugin of `tests/plugin` at 15,152 bytes with 2 and 20
  names, the host at 284,472 and the closed host at 352,528.
- Chapter 18: the digest of `units.antl`, both lock files, the file
  lists, the key lines, and every line `anti check`, `anti doc`,
  `anti fmt` and `anti license` print.
- Chapter 19: every run of `anti test`, with its files and its messages.
- Chapter 20: both build ids, the six cross-built sizes, the stop under
  Rosetta with status 70 and the refusals.
- Chapter 21: the plain bench of the hour lies within the noise of the
  table, and the six refusals and both sizes of a profile build.

The table of `--memory-checks` times in chapter 12 and the other five
columns of the options table in chapter 21 were not measured again.

## Findings in the code

- `Collection.clear` of `src/std/anti/collection.anti:272` removes from
  the front. A `List` therefore moves `n (n - 1) / 2` elements.
- The doc comment of `src/std/anti/trace.anti` says a handler counts
  with atomic fields. The tables of `Profiler` are plain fields. Two
  threads that enter traced functions write them without a lock.
- A dev build with `--no-checks` keeps the zero table test of a dispatch.
  `lower_load_table` of `src/antic/lower_expr.c:1202` writes it under
  `l->dev` alone, as a plain branch. The dev module of `pools` holds six
  such tests with the option and without it. Chapter 21 now says so.

## Answers from the code

- The example runner adds `import anti.lang;` to a block it extracts.
  A size or a digest of a module is therefore taken from the block as the
  page shows it. The file in the work directory of the runner gives another.
- A dev build compiles the standard library in dev mode, one object per
  module. The cost of a library function in dev mode holds its checks.
- No file under `src/`, `tests/bench` or `tests/run_bench.cmake` changed
  since `a9e8ffd0`, so the times of chapter 21 stand for this commit.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, and `guide_examples` is among them. The docs-style checker reports
nothing on the ten pages and on this report. `git grep` finds no marker of
unfinished text in the pages. No file under `tests/` changed, so no
sanitizer suite ran. The suite ran a second time on the last commit of a page, with the same count. This is the state before the commit of this report:

```text
$ git log --oneline -3
b63c8c6a Say that --no-checks leaves the zero table test in a dev build
5c1c2711 Correct the cost table of chapter 21 of the developer's guide
2d850efa Correct the count of disassembly lines in chapter 20 of the guide
$ git status --short
?? docs/reports/2026-10-11-docs-guide-check-12-21.md
$ git rev-parse HEAD origin/docs
b63c8c6aec576e3333a8311083cc2936d58b0ad4
b63c8c6aec576e3333a8311083cc2936d58b0ad4
```
