# The check of chapters 1 to 11 of the developer's guide

A step of the documentation driver on 2026-10-11, on the branch `docs`.
It checks chapters 1 to 11 of `docs/site/developers-guide/` against the
code, as a reviewer who did not write them. The task text gives the step
no id, so the report takes the name `guide-check-1-11`. The step changes
no code under `src/` and no file under `tests/`.

## Method

For each feature section the step read the parts "How antic builds it"
and "What it costs" and the line "Where it lives". It then read the code
behind each claim or compiled the example of the section again. Scripts
under `build/scratch-guide/rv/` build a block of a page for any target
and mode and print the LLVM module and the disassembly of one function.
A last script read every "Where it lives" line of the eleven pages. All
452 files and names stand in the files their lines name.

## Changes

| Page | Change | Code read |
|---|---|---|
| 1 | A cache key also holds the debug mark, `strict` and `memory-checks` | `cache_key`, `src/anti/build.c:120` |
| 1 | `antic --help` lists all options but eight, which the page now names | usage text and `--no-reflect` at `src/antic/main.c:324` |
| 2 | Six `parser_*.c` files, not five | `src/antic/parser_*.c` |
| 2 | `lld` stands in `bin/` of the runtime archive | `build/host/runtime/bin/` |
| 2 | `--memory-checks` and a Windows plugin host only default to a link of objects | `lto_mode`, `src/antic/driver.c:262` and `:1810` |
| 3 | The sizes of `(i8, int, i8)` and `(int, i8, i8)` are printed by the example | `compute`, `src/antic/layout.c:224` |
| 4 | A turn of a collection loop makes three loads for its change check, not two | `walk_check`, `src/antic/lower_stmt.c:214` |
| 4 | The 64 bytes of the iterator are printed by the example | `IndexedWalk`, `src/std/anti/collection.anti:911` |
| 4 | An excerpt shows which copies of a deferred call `opt` merged | release object of `scan` |
| 5 | The `EXPR_HERE` case stands in `lower_address`, and `lower_location_value` writes the record | `src/antic/lower_expr.c:650`, `src/antic/lower_place.c:482` |
| 5 | The release module of the closure example holds `clo.main` alone, with an excerpt | release module of `clo` |
| 5 | `dup` of an `own fn` takes one atomic add as well | `src/rt/snapshot.c:17` and `:47` |
| 5 | A ranking of why a dev build is slower left the pitfalls | no program of `tests/bench` measures it |
| 6 | `anti_rt_hook` makes two atomic loads for `created` and `destroyed`, not one | `src/rt/hooks.c:22` and `:68`, `src/rt/loaded.c:46` and `:92` |
| 8 | A header writes `ANTI_ALIGNAS(N)`, a macro for `_Alignas` and `alignas` | `src/antic/header.c:611` and `:1644` |
| 9 | The cost of an object counts the atomic loads of `anti_rt_hook`, where the table said 0 | the same lines of `hooks.c` and `loaded.c` |
| 9 | The whole-program pass also skips a plugin, and `--closed` runs it for a host | `whole_program`, `src/antic/whole.c:909` |
| 9 | A claim that the hook call is the one cost a C++ object lacks is gone | no measurement carries it |
| 10 | A Windows link folds with `/OPT:SAFEICF`, and `--linker platform` folds nothing | `src/antic/linker.c:337` to `:370` |
| 11 | A teardown counts the atomic loads of `anti_rt_hook`, in two cost tables | release module of `owners` and `teardown` |
| 11 | The cost of `share`, `dup` and the teardown of a `Shared` handle names its hook calls | `src/std/anti/mem.anti:220`, `src/rt/hooks.c:92` |

Chapter 7 needed no change.

## Examples and excerpts added

- Chapter 3: one more line of the program `pairs`, with its output.
- Chapter 4: one more line of the program `loops`, and an `asm` excerpt
  of `scan` in release mode.
- Chapter 5: an `llvm` excerpt of `clo` in release mode.

## Claims that held

The step rebuilt these and found them as written.

- Chapter 2: the three file sizes of `walk` and the six section sizes.
- Chapter 3: the arm64 forms of the saturating add, the division and the
  carry, and the calls of `anti_rt_f16_to_f32` at `v1`.
- Chapter 3 and 5: the signature of a `str`, a `Point`, a `Box` and a
  `SourceLocation` parameter on all six targets.
- Chapter 8: the descriptor of 136 bytes, the field record of 48 and the
  eight multiplications of the default hash of `Point`.
- Chapter 9: the table of 19 pointers, the 18 function records of the
  class of `mirror`, and the casts of each mode.
- Chapter 10: 22 copies in the dev module of `copies` and 9 in the
  release object.
- Chapter 11: a release build keeps the test for a zero table before a
  teardown. The release module of `teardown` holds 14 such calls.

## Findings in the code

- The usage text of `antic` in `src/antic/main.c` lacks `--checks`,
  `--no-checks`, `--asserts`, `--no-asserts`, `--trace`, `--no-trace`,
  `--no-hooks` and `--no-reflect`. `main.c` parses all eight.
- The emitter writes `align 8` on the load of a `u64` field of a packed
  struct at offset 6. Chapter 8 shows the line. LLVM may assume that
  alignment, and the address has none.

## Answers from the code

- `anti_rt_reserve` of `src/rt/grow.h` doubles from a first size in one
  call of `realloc`, which is the growth of a `Builder`.
- `anti_rt_dup` allocates and copies through the table and calls no
  hook. Lowering writes `anti_rt_hook_copied` after it.
- `lto_usable` of `src/antic/driver.c:277` refuses `--lto` with
  `--memory-checks` for a macOS target alone. Every other target with
  the option only defaults to a link of objects.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on
every Mac, and `guide_examples` is among them. The docs-style checker
reports nothing on the eleven pages and on this report. `git grep` finds
no marker of unfinished text in the pages. No file under `tests/`
changed, so no sanitizer suite ran. This is the state before the commit
of this report:

```text
$ git log --oneline -3
ef7d626a Correct chapters 5 and 9 to 11 of the developer's guide against the code
d7d4dfaf Correct chapters 1 and 9 of the developer's guide against the code
f7c760fe Name the macro a C header writes for align in chapter 8 of the guide
$ git status --short
$ git rev-parse HEAD origin/docs
ef7d626a6102c1802697b485122497e3359fab36
ef7d626a6102c1802697b485122497e3359fab36
```
