# Chapters 1 to 3 of the developer's guide

The step `guide-foundations` of the documentation driver, on 2026-10-11, on
the branch `docs`. It writes chapters 1 to 3 of
`docs/site/developers-guide/` in full and changes no code under `src/`.

## What the step wrote

- `01-getting-started.md` holds the six packages and their contents and
  the install with one command and by hand. It lists the options of antic
  and the commands of `anti`. It builds a first program both ways and names
  the files `anti build` writes, the two modes and the directories of an
  install.
- `02-how-antic-works.md` lists the thirteen steps from source to
  executable with the file and the function of each. It takes one program
  through all of them. It states dev against release, the whole-program
  pass and the LTO link, and the runtime archive with the allocator. It
  gives the parts of a linked executable by size, the marks of an example
  and the options that print each excerpt.
- `03-values.md`: seven feature sections, each with its five parts. They are
  types and literals, variables and constants, integers, floats and `f16`,
  text and bytes, tuples and type inference.
- `tests/run_document_examples.cmake` takes five more kinds of excerpt,
  `tokens`, `ast`, `types`, `ir` and `opt`, each read from `antic
  --dump-<kind>`. Chapter 2 shows a dump of each, and without the marks the
  suite would not check them. A `const` that starts a line is now an item
  of the module, so the functions of an example reach it.

## The examples it added

The three pages hold 21 `anti` blocks that compile, and 13 of them are
programs that run against their output. The compiler refuses 42 more blocks
with the error each names. The pages hold 32 excerpts.

- Chapter 1: `first`, and `grow`, which wraps in release mode, with the
  text of its dev check as an excerpt.
- Chapter 2: `walk` with an excerpt of the tokens, both trees and the IR
  before and after the optimizer. It also has the LLVM module of each mode
  and the machine code for x86_64 and arm64. `hello` of the skeleton stays
  as the page of marks.
- Chapter 3: `lits`, `sizes`, `consts`, `ints`, `wide`, `floats`, `texts`,
  `pairs` and `infer`. The sizes come from `size_of` in programs that run.

No time stands in the three chapters, so `tests/bench` gained no program.

## The proof of the new marks

Five copies of chapter 2 with one line changed each went through the
runner. The logs are `build/drive/logs/g13-page-neg_*.log`.

| Copy | What the runner says |
|---|---|
| `neg_ir`, `neg_tokens`, `neg_types`, `neg_ast_order` | the line does not stand where the excerpt puts it |
| `neg_header` | the excerpt is not written as `ast excerpt=<id> target=<target> mode=<dev\|release>` |

## Answers from the code

- A release excerpt shows a function when the example marks it `export
  fn`. `linkage` of `src/antic/llvm_emit.c` writes it `dso_local` and never
  `internal`, so opt keeps it under its plain name.
- The IR after lowering is the same text in both modes. `back_end` of
  `src/antic/driver.c` drops the checks later, with
  `optimize_drop_failures`.
- An unsigned `+`, `-` and `*` has no check. Neither has a float operation,
  the least value divided by -1 or unary `-` of the least value. A dev
  build of a program that runs each case showed it.
- A `char` converts to `u32` and back, and to no other type.
- A generic call fixes its type arguments from the left, so `is_less(4,
  small)` is refused where `is_less(small, 4)` compiles.
- An `f"..."` takes one block of 32 bytes that doubles, by `reserve` of
  `src/rt/text.c` and `anti_rt_reserve` of `src/rt/grow.h`. A `Builder` is
  32 bytes in the frame.
- A `str` parameter is two `i64` on the System V targets, one `[2 x i64]`
  on the three arm64 targets and a pointer on windows-x86_64.
- A function keeps a frame record only when it calls another, by the
  attribute `"frame-pointer"="non-leaf"`.
- `anti` links the driver of antic and starts no antic process, by the
  calls of `driver_run` in `src/anti/build.c`.

## What was wrong in older documents

- The usage text of `src/antic/main.c` lists none of `--checks`,
  `--no-checks`, `--asserts`, `--no-asserts`, `--no-reflect`, `--no-hooks`,
  `--trace` and `--no-trace`, which `run` of the same file reads.
- The usage text says `--dump-opt` prints the IR after optimization. In
  release mode a build drops the checks before the optimizer and the dump
  does not, so the dump shows checks that the program lacks.
- `docs/anti-syntax-overview.md` says a `str` is NUL-terminated outside its
  length. That holds for a literal and for the text of an `f"..."`. A `str`
  of `text.slice` or `text.from_bytes` shares memory and has no NUL of its
  own.
- `docs/notes/lowering.md` still describes lowering by the chapters of the
  book, with structs and strings as work to come.
- `anti run` reports a program that a signal ended with the prefix
  `antic:`, as in `antic: ./dist/macos-arm64/dev/ov ended by signal 6`.

## Open points for Eddie

- `check-web-content` reports 40 errors on the three pages. The part
  headings and the line "Where it lives:" account for 17 of them, which the
  task fixes. The rest are sentences that the checker counts as long
  because it counts each inline code token as a word.
- The dev messages of chapter 3 stand in a table that no test runs, since
  `guide_examples` runs a program in release mode alone. A dev excerpt pins
  the text of four of them. A mark that runs a block in dev mode and reads
  its standard error would pin all of them.
- The sizes of the executable in chapter 2 are of commit `ca620e14` and no
  test checks them.
- `export fn` in an example of chapter 3 comes before chapter 17 explains
  it. One sentence at the top of chapter 3 says why.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 369 s with `ctest --preset host -j14`. The sanitizer suites did not
run, since the step changed no C file and not `tests/CMakeLists.txt`. The
docs-style checker reports nothing on the three pages and on this report.
`git grep` finds no marker of unfinished text in the three pages. This is
the state before the commit of this report:

```text
$ git log --oneline -3
c5c3f071 Shorten sentences of chapters 1 to 3 of the developer's guide
01529eac Correct the frame record and the surviving function in chapter 2
8d6eecec Write the text, tuple and inference sections of chapter 3
$ git status --short
?? docs/reports/2026-10-11-docs-guide-foundations.md
$ git rev-parse HEAD origin/docs
c5c3f0712fc1d63ee7fa8af7d9384a6760bc465a
c5c3f0712fc1d63ee7fa8af7d9384a6760bc465a
```
