# Batch 3t of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `ef9cd1aa` gave the ranking. The batch took
`src/antic/pattern.c` with 67 missed branches and `sema_operator.c` with
65, which no batch had chosen by their branches. Each side ran first
through the antic of the coverage tree alone, with a profile of its own,
as 3s did. The sections of both files in `docs/notes/unreached-guards.md`
were read first. Eddie's direction is the Mac alone, so the VMs were not
touched. The probes found two defects, T290 and T291. The next free id is
T292. The batch before found none, and this one found two, so the phase
goes on.

## Commits

- `e26d69fe` tests the forms of a pattern literal that the reader of
  `pattern.c` missed, in `error_pattern_exponential_reader`, and the fit
  rule of `patch` past a branch reset and past a repeat of an empty
  group. `da720fb0` adds the classes the reader missed.
- `c4576a1b` tests the comparisons of `sema_operator.c` that the suite
  missed: function values of other parameters, results and forms, a
  pointer beside an `int` and a pointer to a class, an `int` beside a
  type parameter, a scalar times a simd struct, and a pointer to a
  derived class on the left of its base.
- `9e1ca064` lists T290, and `08295c8e` fixes it.
- `37600f6b` lists T291, and `0fd731b5` fixes it.
- `f2bfaa1d` records the rules of both fixes and their commits.
- `78dafac4` names the guards. One commit records the times and the
  counts in `CLAUDE.md` with this report.

## Findings

- T290, minor. A local that a named function initialises holds the plain
  form of `fn(int) -> int`, and a parameter the form of two words. `==`
  between the two was refused with `` the operands of `==` have the types
  `fn(int) -> int` and `fn(int) -> int` ``, one name for two types. The
  refusal of two function types of one signature that differ in their
  form alone now adds `the plain form on the left and the form of two
  words on the right`, as T177 named the owned form.
- T291, major. PCRE2 repeats the last character of `\Q...\E` with the
  quantifier after it, and the reader handed the quantifier the whole
  quoted run. `(\Qxa\E+)+` failed the check `exponential-pattern`, where
  `(xa+)+` passes. The characters before the last now stand in the
  sequence.

One `[provisional]` entry was added, beside the rules of the form of two
words: `==` between the two forms stays refused, and the message names
them. The specification makes each form a type of its own and does not
say whether the two compare. The entry of T291 has no tag, since the
check reads a pattern as the engine does.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 42000.
`cmake -DFIRST=42001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 42001 to
42200 at the default size in 360 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.74%, 4085 missed | 92.79%, 4057 missed |
| Regions | 98.52%, 1262 missed | 98.51%, 1264 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1225 missed | 98.72%, 1225 missed |

| File | Branches missed before | after |
|---|---|---|
| `pattern.c` | 67 | 43 |
| `sema_operator.c` | 65 | 56 |

The two fixes add regions and branches of their own. `sync.c`,
`platform_posix.c`, `plugin.c`, `text.c` and `hooks.c` of the runtime
moved by one to three sides between the runs, which the order of their
threads decides. The first coverage run had 7591 tests, all passed, and
took 1089 s. The run after had 7611, all passed, and took 1005 s. The 43
sides left in `pattern.c` are guards of the note: a pattern that PCRE2
refuses before the reader runs, a byte the callers never pass and a group
without its child. Most of the 56 left in `sema_operator.c` take a symbol
without a type or an error type after a message, as the last section of
the note counts them.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` three times: once before the two test commits and the
  list of T290, once before the fix of T290, the test of the classes and
  the list of T291, and once before the fix of T291. The times stand in
  `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7621. Every test passed.
- The host runs 7623 tests and each sanitizer tree 7621.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`antl_verify.c` with 65 missed branches, `whole.c` with 65,
`sema_const.c` and `sema_copies.c` with 80 each and `sema_stmt.c` with
68 got no probe here. The sides of `antl_verify.c` read the records of
a damaged library file, which need cases of `unit_modules` written by
hand, as batch 3s wrote for `antl_tree.c`.

## Questions for Eddie

- T290 keeps `==` between the plain form and the form of two words
  refused. A plain function value converts to the form of two words at a
  call. The pointer rule compares `*T` with `?*T`, since one converts to
  the other. Should the two forms compare by their code
  instead?

Four stay open, and nothing here acts on them: `take` of an `own ?fn`
field, the handler of T278, the reach of T281 across modules and the body
of a `do ... while` in a `construct`, from batch 3q.
