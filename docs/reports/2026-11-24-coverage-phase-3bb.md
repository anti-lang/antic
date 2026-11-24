# Batch 3bb of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `9435be8d` gave the ranking. The batch read two
files that no batch chosen by branches had read: the checks of the
operators in `src/antic/sema_operator.c`, which missed 59 of its 628
branches, and the writer of `anti bind` in `src/anti/bindwrite.c`, which
missed 22 of its 354 and which batch 3ba left for a later batch. It read
the first against "Operators" and "Pointers and conversions" in
`docs/anti-object-model.md`, the entries on operators, `??`, `in` and
the forms of a function type in `docs/decisions.md` and the DESIGN
comments of the file. It read the second against "Bitfields" and the
probe in `docs/tooling-addendum.md`, "The bind command" in
`docs/decisions.md`, `docs/notes/bind.md` and its DESIGN comments. It
probed each side with the antic and the anti of the host tree and of the
coverage tree. The batch found T434 to T439 and fixed each. The next free
id is T440. The batch before found one, and this one found six, so the
phase goes on.

## Findings

- T434, minor, fixed. An `operator fn` whose signature names a type that
  is not there gave a second message at every use, at module level and
  in a class body: `p < q`, `p + p`, `p * 3`, `-p`, `p in p..p`, a
  constraint `T: add` and a class that replaces `equals` beside such an
  `operator fn hash`. `sema_operator_symbol` and `sema_module_operator`
  now find a refused operator of the module as `sema_hook` finds a
  refused hook by T233, and the uses stop on its signature without a
  message.
- T435, major, fixed. `a + 5` with `operator fn add<T: Number, U:
  Number>(a: Key<T>, b: U)` was refused with `` expected `Key<int>` ``,
  while `add(a, 5)` compiled. A literal of a parameter that the first
  operand leaves open now takes its own type, as the call gives it.
- T436, minor, fixed. `p + 1` beside an `operator fn add` of one
  parameter was refused for the type of the literal. It now gets
  `` `operator fn add` takes one operand beside its own ``, as `p + p`
  does.
- T437, minor, fixed. `none == none` gave one message per side. The left
  side holds the one message now.
- T438, minor, fixed. The C probe of `anti bind --probe` wrote
  `v.anon0.x = 1` for a field of an anonymous member of C11, which clang
  refuses. The probe of C now leaves the name of the member out of the
  path, and the probe of Anti keeps it.
- T439, major, fixed. `anti bind` wrote an unnamed bitfield of C that is
  not zero-width as `_: c_int : 3`, which antic refuses, since `_` is the
  zero-width bitfield of Anti. Such a field binds as `pad<n>` now, by a
  provisional entry under "The bind command".

## Commits

- `3d4823cb` lists T434 to T438.
- `c8909337`, `685dfe57`, `e2d24602`, `de9c0ecb` and `152c29be` fix T434
  to T438, one each.
- `59e72053` lists T439, which the case `write_sides` showed when its
  module did not compile, and `5e47d7af` fixes it.
- `115421ab` tests the sides of both files.
- `1a90b582` tests the sides that the run of coverage after the batch
  named.
- `df24bac1` records the times, the counts, the hashes of the
  fixes and the section "The sides of batch 3bb" of
  `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Choices

- T439 met a gap: the specification names no form for an unnamed
  bitfield of C that is not zero-width. The batch took the smallest form
  that keeps the layout of C, a field `pad<n>` of the type and width of
  the bitfield, numbered in each record as `anon<n>` is, and wrote one
  `[provisional]` line under "The bind command" in `docs/decisions.md`.
  The probes give the field no value, since C names it not.
- An `operator fn` of one parameter, as `operator fn eq(a: Point)`, is
  taken at its declaration and refused at a use. The batch left the
  declaration as it stands, and T436 makes every use give the one
  message of the arity.
- A struct of a description that holds itself by value binds, and its
  module does not compile. The DESIGN of the walk of the probes in
  `bindwrite.c` says that the reader does not check it, and the batch
  left it so.
- Every side of the two files that the suite misses after the batch has
  a row in "The sides of batch 3bb" of `docs/notes/unreached-guards.md`.

## Fuzzer

`cmake -DFIRST=48801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 48801 to
49000 at the default size in 165 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.68%, 3688 missed of 58328 | 93.73%, 3661 missed of 58354 |
| Regions | 98.50%, 1318 missed of 88023 | 98.51%, 1312 missed of 88062 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.71%, 1275 missed of 98637 | 98.72%, 1267 missed of 98666 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_operator.c` | 59 of 628 | 43 of 642 |
| `src/anti/bindwrite.c` | 22 of 354 | 15 of 364 |

The "before" run had 8494 tests in 1155 s, and the run after the batch,
at `115421ab`, 8530 in 1201 s. Every test passed in each. The fixes added
14 branches to `sema_operator.c` and 10 to `bindwrite.c`. `1a90b582`
takes nine more of the first file and two more of the second, which the
antic and the anti of the coverage tree measured alone, so the files miss
34 of 642 and 13 of 364 with it. Each of them has a row in the section
of the batch.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds each
  time. Every test passed in each. Two runs before `1a90b582` were
  stopped in their first part to add tests the coverage tree had shown
  reachable, and the run that followed holds the time.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8553. `asan` took 261 s, 189 s, 282 s, 172 s, 112 s
  and 832 s, and `ubsan` 266 s, 185 s, 277 s, 167 s, 118 s and 732 s.
  Every test passed.
- The host runs 8555 tests and each sanitizer tree 8553.
  `tests/known-failures.txt` holds the lines of T136, T138 and T432.

## Pinned outputs

No fix moves an output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. T439 binds an unnamed bitfield of C that is not zero-width as a field
   `pad<n>`, by a provisional entry. `pad<n>` can meet a field of the
   same name in the record, as `anon<n>` can. Should the binding keep
   that name or take another? Or should it leave such a struct out with
   a warning, as it does for a struct whose field has no form?
2. An `operator fn` of one parameter at module level, as
   `operator fn eq(a: Point) -> bool`, is taken at its declaration and
   refused at every use with `` `operator fn eq` takes one operand
   beside its own ``. `operator fn hash` with the wrong parameters is
   refused at its declaration. Should every operator of the table be
   refused at its declaration when it does not take two operands?
