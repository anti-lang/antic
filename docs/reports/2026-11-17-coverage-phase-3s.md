# Batch 3s of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `d2069413` gave the ranking. The batch took
`src/antic/antl_tree.c` with 75 missed branches, which batches 3q and 3r
named and did not test. Each side ran first through the antic or the unit
tests of the coverage tree alone, with a profile of its own, as 3r did.
The sections of `antl_tree.c` in `docs/notes/unreached-guards.md` were
read first. Eddie's direction is the Mac alone, so the VMs were not
touched. The probes found no defect. The next free id stays T290. The
batch before found T289, so the phase goes on.

## Commits

- `e4bddc2e` tests a generic class of a library that inherits a class of
  the library, as `modules_generic_base` in release and dev mode.
- `4e8ac2ad` tests two later files of `counter` that give a function of
  a class without `self` and a generic function an `f64` in place of an
  `int`. The read of each user is refused, by the third answer of
  "Answers of 2026-11-12". The later file holds the alias `Twin`, which
  the search for a function of a class passes over.
- `1c237c8a` tests damaged externs, declarations and trees in
  `unit_modules`. An extern has a contract, a visibility or a kind past
  the last. A declaration has marks past its four or more names than
  parameters, or calls a struct a class. A tree has a kind past 1 or a
  function its generic lacks, and a shuffle lacks its simd struct.
- `09c84809` names the guards in the note. One commit records the times
  and the counts in `CLAUDE.md` with this report.

## Findings

None. Every damaged file is refused, and each later file is refused with
the message that names the function and both signatures. The generic
class with a base of its library builds, runs and replaces the function
of its base in both modes.

No `[provisional]` entry was added.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 41800.
`cmake -DFIRST=41801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 41801 to
42000 at the default size in 377 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.70%, 4103 missed | 92.74%, 4081 missed |
| Regions | 98.51%, 1267 missed | 98.52%, 1261 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.72%, 1225 missed |

| File | Branches missed before | after |
|---|---|---|
| `antl_tree.c` | 75 | 64 |
| `antl_verify.c` | 67 | 65 |
| `antl_read.c` | 47 | 46 |
| `antl_read_ir.c` | 83 | 82 |

The base of a generic class is no side of `antl_tree.c`: every class
inherits `Object` at the least, so the test of `e4bddc2e` holds a case
that no test held and reaches no new side there. `sync.c`,
`platform_posix.c` and `trace.c` moved by two or three sides between the
runs, which the order of their threads decides. The first coverage run
had 7585 tests, all passed, and took 1094 s. The run after had 7591, all
passed, and took 978 s. The 64 sides left are guards of the note, or the
switches over the three modes of a walk and the values of a damaged file
that its last section counts.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,7603` before the commits. One run covered the first two test
  commits, and one the commit of `unit_modules`. The times stand in
  `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7601. Every test passed.
- The host runs 7603 tests and each sanitizer tree 7601.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`sema_copies.c` with 80 missed branches and `sema_const.c` with 80 got no
probe here. Neither did `sema_stmt.c` with 68, `pattern.c` and
`antl_verify.c` with 67 and `whole.c` and `sema_operator.c` with 65,
which no batch of the third phase chose by its branches. A reading of
`sema_stmt.c` found its sides to be guards of the note or NULL operands
after a message: `sema_closure.c` gives the message of an anonymous
function that reaches its end, and `visit_call` meets no `free` or
`take`. Most sides
left in `sema_call.c`, `sema_generic.c` and `sema_expr.c` take a NULL
operand or an error type after a message.

## Questions for Eddie

None new. Four stay open, and nothing here acts on them: `take` of an
`own ?fn` field, the handler of T278, the reach of T281 across modules
and the body of a `do ... while` in a `construct`, from batch 3q.
