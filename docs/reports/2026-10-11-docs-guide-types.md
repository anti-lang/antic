# Chapters 8 to 10 of the developer's guide

The step `guide-types` of the documentation driver, on 2026-10-11, on the
branch `docs`. It writes chapters 8 to 10 of `docs/site/developers-guide/`
in full. It changes no code under `src/`. Under `tests/` it changes one
line of `tests/run_document_examples.cmake`.

## What the step wrote

- `08-structs-enums.md` has seven sections: structs, `packed` and `align`,
  unions and bitfields, simd structs, enums, variants and pattern matching.
- `09-classes.md` has seven: the object in memory, `construct` and
  `destruct`, fields and methods, interfaces and dispatch, static fields and
  singletons, operators, and reflection.
- `10-generics.md` has four: generic functions and classes, constraints,
  the copies per use, and the other generic forms.

Every section has the five parts of the task. The layout of a struct, a
variant and an object stands as `size_of` in a program and as the type of
the IR. The LLVM module shows it as the slot `[N x i8]`. A call through an
interface stands as IR, as the thunk and as the two instructions of the
release code. The generics chapter counts the copies of one program: 22
functions in the dev object and 9 in the release object.

## The examples it added

The three pages hold 19 `anti` blocks that compile, 18 of them programs that
run against their output. The compiler refuses 97 more blocks with the error
each names, and the pages hold 52 excerpts.

- Chapter 8: `rects`, `wire`, `bits`, `lanes`, `kinds`, `shapes`, `match`.
- Chapter 9: `objects`, `build`, `calls`, `faces`, `statics`, `ops`,
  `mirror`.
- Chapter 10: `gen`, `cons`, `copies`, `forms`.

No time stands in the three chapters, so `tests/bench` gained no program.

The runner read a line that starts with `operator`, `synchronized` or
`concurrent` as a statement. The three words now open an item, as `fn` and
`class` do.

## Answers from the code

- The LLVM module holds no struct type. A struct, a variant and an object
  are a slot `[N x i8]` with an alignment, and a field is a byte offset.
  The named type stands in the IR of antic, with `offset_of` as a symbol.
- The default `==` and hash of a struct are written in place and call
  nothing. The hash runs four multiplications per field and reads no seed.
- A write to a bitfield keeps the low bits in both modes. No check exists.
- The unit of a bitfield is the smallest integer that holds its bits, one
  byte for `mid: i32 : 5` at bit 3. Windows gives `Wide` 6 bytes and Linux
  4, which two excerpts show.
- The tag of a variant is a `u8` up to 255 cases. Its load carries a range,
  and no arm tests for a tag outside the cases.
- Every table starts with 17 entries: the descriptor, the seven functions
  of the root and the nine hooks. The first `pub` function has slot 17.
- A class value in the frame, and the result of an operator that returns
  one, call `anti_rt_hook` for `created` in both modes.
- `delete` runs the `destroyed` hook, then `destruct` down to the base, then
  the `own` fields and the inline fields. A failed `construct` runs no
  `destruct`, not even that of a base whose `construct` completed.
- A release thunk has the body of its function inlined, so an interface
  call is two loads and one indirect jump.
- `Config.get()` of a singleton does not lock. A thread that loses the race
  frees its object after `construct` ran, without a `destruct`.
- The default `equals` of a class calls `anti_rt_descriptor` twice, to
  compare the classes, before it compares the fields.
- `reflect.call` finds its trampoline by `strcmp` over the signature texts.
- An operator constraint compiles to an instruction or a direct call. An
  interface constraint compiles to a dispatch through the interface table.
- `merge_copies` merges by the IR of antic. `Slot<*Person>.get` and
  `Slot<int>.get` are the same machine code and stay two functions.

## What was wrong in older documents and in antic

- The load of a field of a packed struct carries the alignment of its type:
  `load i64, ptr, align 8` at offset 6 of a struct of alignment 1. The
  provisional entry on alignments in `docs/decisions.md` allows it. LLVM may
  assume the alignment.
- `docs/anti-object-model.md` gives an enum `<`, `<=`, `>` and `>=`. The
  checker refuses them. It also calls `as` to an enum unchecked, and a dev
  build checks it.
- The overview and the object model say that compound assignments derive
  from an operator. `a += b` with `operator fn add` is refused for a struct
  and for a class, with "`+=` does not apply".
- The object model refuses six things that compile: a class with an
  `abstract fn` and no `abstract`, a direct call `c.destruct()`, `mutable`
  on a field of a plain class, `static atomic` on an `f64`, `==` on pointers
  of two unrelated classes, and `equals` replaced without `hash`, which
  gives no warning. The guide states none of the six.
- `operator fn eq` that returns an `int` compiles.
- An `align(N)` below the alignment of the fields ends the build in the back
  end, with no line number and without the word `error:`. A `refused` block
  cannot name it, so the chapter shows the message as text.
- A program that uses a generic class with a nested type from a library file
  is refused at line 10 of a file that has 8 lines. The position belongs to
  the source of the library.
- The object model gives a descriptor to a struct that a class holds or that
  is exported. Lowering writes one for every struct of a module.

## Open points for Eddie

- `guide_examples` took 139 s in the suite, against 29 s before the step.
  The host suite still ran in 353 s.
- `check-web-content` was not run. The part headings and the line "Where it
  lives:" are fixed by the task, as the report of the step before says.
- Chapter 10 names `--icf=safe` of lld as the place where identical machine
  code is folded. No excerpt of the guide shows a link.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 353 s with `ctest --preset host -j14`. The sanitizer suites did not
run, since the step changed no C file under `tests/` and not
`tests/CMakeLists.txt`. The docs-style checker reports nothing on the three
pages and on this report. `git grep` finds no marker of unfinished text in
the three pages. This is the state before the commit of this report:

```text
$ git log --oneline -3
e8fbaac3 Correct the count of multiplications of the default hash in chapter 8
fc308cd9 Write chapter 10 of the developer's guide, generics
1a55ce7c Read synchronized and concurrent as the start of an item in the document runner
$ git status --short
?? docs/reports/2026-10-11-docs-guide-types.md
$ git rev-parse HEAD origin/docs
e8fbaac39339a51da733b533ed7b40848760180e
e8fbaac39339a51da733b533ed7b40848760180e
```
