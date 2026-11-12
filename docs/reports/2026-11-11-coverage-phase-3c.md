# Batch 3c of the third phase of coverage

This step sorts the regions that `./c coverage` at the end of batch 3b showed
unreached in the safety check `escaping-local` and the pass over the
whole program: `src/antic/sema_escape.c`, `src/antic/whole.c` and
`src/antic/whole_tables.c`. It follows the method of
`docs/reports/2026-11-11-coverage-phase-3b.md`. A library file written
by hand is an input a user can hand to antic, as that report holds, so
an IR that such a file gives the pass counts as reachable, and
`unit_whole` changes the IR in its place. Eddie's direction is the Mac
alone, so the VMs were not touched. The batch adds no executable. One
refusal of the matrix and one of `tests/errors/` join the suite, and
`unit_whole` with its new cases takes 0.03 s.

## Commits

- `78da7f4e` lists T237, `5a74f36d` fixes it.
- `63067f87` tests `sema_escape.c`.
- `215ffe5d` tests `whole.c`.
- `e6cca7eb` tests `whole_tables.c`.
- `ad3e0095` names the guards in `docs/notes/unreached-guards.md`.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fix in `docs/audit/tests-findings.md`.

## Findings

- T237, minor, fixed. A closure captured a local reference and went to
  a `concurrent` parameter of a function value, a parameter or a field
  of function type. It was refused with "the concurrent parameter ``",
  an empty name in backticks. `check_concurrent` takes the name from the
  item of the function called, and a function value has none. The
  message now names no parameter.

## sema_escape.c

`error_escape_paths` holds 22 functions of one path each. 15 are refused
with one message each. They return a tuple field of a local struct and
the deeper of two references of a tuple, and store into an element of
an array behind a parameter. A `dispatch` is never joined, a bare
`return`, a handler, a loop and a guard return before the join, and a
`yield` comes before it. A join stands on the right of `&&`, `||` and
`??` and in the arguments of a call after `?.`. Closures go to a
`concurrent` parameter of a class and of a field. The others compile:
a constant pointer, a text, a guard and nested handlers between a
`dispatch` and its join, a closure that returns the address of a local
of its function, and a collection reached through a call and through a
local. Five rows of guards stand in the note.

## whole.c

`unit_whole` takes three cases. A worker writes a field of a struct,
and the struct is a `mutable` field of a singleton in another module.
The singleton check reports it. Two copies of a generic whose length is
a product of symbolic values merge. A global that holds the address of
the second then names the first. A program is built with `--closed`,
and its provider alone reads whether backtraces are on. It gets the
default of the backtraces from the second reach of T104. The program
of T104, `matrix_classes_injected_logger_never_called`, no longer takes
that path. A program that injects can host a plugin, so its first reach
takes every function. Six rows of guards stand in the note.

## whole_tables.c

The `--no-reflect` case of `reflect_calls` now lowers with
`LOWER_NO_REFLECT`, as antic does, so the descriptors hold no function
list. A class has a function that takes a struct and one of 64
parameters. A hand-written file damages the signatures of two more.
None of the four gets a trampoline. A slot that the IR lacks is added
and filled, and a copy of a generic is a provider. A plugin reads an
interface whose descriptor carries no record of versions or no version
text. Its chain is empty or has a length of a float, and a class
provides it without an init. Three rows of guards stand in the note.

## Branches left

The sides of a condition that a guard of the note holds were not chased,
nor in `sema_escape.c` a `fallthrough` and a `_` binding between a
`dispatch` and its join, and in `whole.c` the sides of `same_function`
and `same_global` for copies that differ in one part alone, and a
collision in the table of the class records.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.16%, 1539 missed | 98.22%, 1489 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.39%, 1525 missed | 98.45%, 1471 missed |
| Branches | 91.32%, 4815 missed | 91.42%, 4761 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `sema_escape.c` | 96.27%, 34 missed | 99.02%, 9 missed | 87.20% | 91.74% |
| `whole.c` | 95.73%, 28 missed | 96.65%, 22 missed | 86.97% | 87.55% |
| `whole_tables.c` | 97.44%, 29 missed | 99.47%, 6 missed | 87.29% | 91.31% |

Every line that stays unreached in the three files is a guard of the
note. The coverage suite ran 7195 tests, all passed, and `./c coverage`
took 977 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- The run before the listing of T237 also failed `fmt_canonical` and
  `errors_registered`, for two programs of the next commits that stood
  unregistered in the tree. Both passed once those files were set aside,
  and the committed tree held neither.
- The indexer of macOS and XProtect took cores of the Mac during the
  first run, so its times are a few seconds slow.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7205. Every test passed. `asan` took 247 s,
  187 s, 266 s, 189 s, 451 s and 442 s. `ubsan` took 251 s, 186 s,
  258 s, 196 s, 472 s and 470 s.

## Provisional lines

None. The fix of T237 changes the words of one message and no rule.

## Questions for Eddie

None. Every guard stays by "Answers of 2026-11-10, second set", and the
hand-written library file counts as input by the method of batch 3b.
