# Chapters 4 to 7 of the developer's guide

The step `guide-basics` of the documentation driver, on 2026-10-11, on the
branch `docs`. It writes chapters 4 to 7 of `docs/site/developers-guide/`
in full and changes no code under `src/` and no file under `tests/`.

## What the step wrote

- `04-flow.md` has five sections: `if` and `while`, `for` over ranges and
  collections, `switch`, `break` and `continue`, and `defer` and `undo`.
- `05-functions.md` has seven: declaration and parameters, defaults and
  source locations, the calling convention, inlining, closures, `snapshot
  fn` and `own fn`, and function values. The calling convention shows the
  signatures of one program for System V, AAPCS64 and Microsoft x64.
- `06-errors.md` has five: failing functions, `try` and the handlers, error
  origins and stack traces, `never`, and `assert` with the checks of dev
  mode. The first two follow one error from its `fail` through two `try`
  expressions to its handler, in the IR and in the machine code. The third
  counts what an origin and a capture cost in dev, in release and in release
  with `--anti.backtrace`.
- `07-optionals.md` has three: nullable pointers, optional values, and the
  forms that read an optional.

Every section has the five parts of the task.

## The examples it added

The four pages hold 26 `anti` blocks that compile, and 21 of them are
programs that run against their output. The compiler refuses 85 more blocks
with the error each names. The pages hold 53 excerpts of the IR, the LLVM
module and the machine code.

- Chapter 4: `branch`, `loops`, `arms`, `jumps`, `scan`, `exits`, `results`
  and `paths`.
- Chapter 5: `decl`, `dflt`, `conv`, `inl`, `clo`, `snap` and `fnval`.
- Chapter 6: `errs`, `travel`, `hnd`, `thr`, `origin`, `nev` and `asr`.
- Chapter 7: `nodes`, `opts`, `rd` and `reads`.

No time stands in the four chapters, so `tests/bench` gained no program.
Every cost is a count that an excerpt or a `size_of` in a program shows.

## Answers from the code

- antic writes no `switch` instruction of LLVM. `lower_switch` writes a
  chain of compares in the order of the arms, and `opt` of a release build
  turns it into a table. The plan of chapter 4 expected the instruction.
- A deferred statement is copied to every exit of its block. On a failing
  exit `lower_run_defers` writes every `undo` of the block before its
  `defer` statements.
- `return v;` of a scalar computes `v` before the deferred statements run.
  A struct or a `str` that names a local is read after them, by the DESIGN
  comment in `lower_run_defers`, so a `defer` that changes the local changes
  the result. The chapter shows both in a program.
- A range loop tests the distance to its end before it moves the counter,
  so it needs no overflow check. A loop over a slice has no bounds check.
- A default value is filled at the call as a constant. `here` is the address
  of a constant record of 64 bytes, which System V passes `byval`.
- A named function passed where a closure may stand gets a context of 0,
  and the callee passes that context as one more argument.
- An `own fn` is one `malloc` through `anti_rt_snapshot_new` with one atomic
  add, and it holds the bytes of each captured `str` behind its record.
- Making an error calls `anti_rt_hook` for the `created` hook, and deleting
  it runs the `destroyed` hook, so an error costs more than its `malloc`.
- `?.` reaches a pointer field or a pointer result alone. `p?.count` of an
  `int` field is refused.
- A `?int` is 16 bytes and travels in two registers, the value and the flag.

## What was wrong in older documents and in antic

- A `return`, a `break`, a `continue`, a `fail` or a `try` inside a `defer`
  or an `undo` ends antic with signal 10. No document states a rule for it.
  `fn f() -> int { defer { return 1; } return 0; }` shows it.
- `let a = half(3) catch e { };` compiles, and `a` then holds an undefined
  value in a release build. The checker accepts a handler of a value that
  reaches its end without `yield`. Chapter 6 names it as a pitfall.
- The report of the step before says that the IR after lowering is the same
  text in both modes. Under `--dev` lowering adds a test of the table
  pointer before a call through a table, by `lower_load_table`.
- `docs/anti-syntax-overview.md` says a `snapshot fn` is accepted at an
  `own` field, a `keep own` parameter and a `concurrent` parameter. It is
  also accepted at a plain parameter of function type, where it lives in
  the frame.
- The overview shows `Error` without its module. A file names the class
  `lang.Error`, and `?*Error` as a type of a variable is refused.
- A `text output` block shows line numbers of the module the test builds,
  which drops the blank lines between items. Two examples print such
  numbers and say so.

## Open points for Eddie

- `check-web-content` still reports errors on the four pages. Most come
  from the part headings and the line "Where it lives:", which the task
  fixes. The rest are sentences it counts as long because of inline code.
- The dev output of a stack trace in chapter 6 stands in a `text` block that
  no test runs, since `guide_examples` runs a program in release mode alone.
- An excerpt of the release module names attribute groups such as `#3` and
  metadata such as `!tbaa !18`, which the runner does not make equal. A
  change of the standard library may move those numbers.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 357 s with `ctest --preset host -j14`. `guide_examples` passed in
29 s. The sanitizer suites did not run, since the step changed no file under
`tests/`. The docs-style checker reports nothing on the four pages and on
this report. `git grep` finds no marker of unfinished text in the four
pages. This is the state before the commit of this report:

```text
$ git log --oneline -3
b4ee20b9 Correct the cost of an error object and tighten sentences in chapters 4 to 7
8085fa1e Write chapter 7 of the developer's guide, optionals
b4fd9796 Write chapter 6 of the developer's guide, errors
$ git status --short
?? docs/reports/2026-10-11-docs-guide-basics.md
$ git rev-parse HEAD origin/docs
b4ee20b991714dbeda3c092045f8bc6d4e820b4f
b4ee20b991714dbeda3c092045f8bc6d4e820b4f
```
