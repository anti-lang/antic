# Batch 3as of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `d3c5005d` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that batch 3z had probed and no batch had read:
the values and the moves of the checker in `src/antic/sema_value.c`,
which missed 58 of its 530 branches, and the shared parts of lowering in
`src/antic/lower.c`, which missed 59 of its 654. It read both against the
specifications, `docs/decisions.md` and the DESIGN comments of the two
files, and probed each side with the antic of the host tree. The batch
found six defects, T396 to T401, and fixed each. The next free id is
T402. The batch before found none, and this one found six, so the phase
goes on.

## Findings

- T396, severe. `fits_bound` of `lower.c` compared the IR type alone of
  each parameter and of the result, so a bound function took the
  signature of another: a struct of four `f64` passed as a struct of one
  word, an `i8` as a `zeroext` `u8`, and a result of four words was read
  as one, a bus error. It now compares as `has_signature` does.
- T397, severe. `a ?? b` and `p?.f` that give a value a local or an `own`
  field holds passed as fresh values, and `let`, `=`, `return`, an `own`
  argument, a tuple and an `own` field gave the value a second owner. The
  new `sema_copies_existing` of `sema_value.c` names them, and the copy
  refusals, the move into an `own` parameter and the copy of a `dispatch`
  read it. `sync` keeps `sema_reads_existing`, since a `??` stands in no
  place.
- T398, severe. The fresh value that `??` takes was never torn down where
  the statement dropped it. `lower_is_fresh` names a `??` of two fresh
  sides, and `coalesce_value` keeps the slot of a fresh side beside a
  held one to the end of the statement, on that path alone, through the
  new `lower_keep_value`.
- T399, severe. `dup(x)` of a value, an array literal and `as` of a fresh
  value to its own type were never torn down where a statement dropped
  them. `lower_is_fresh` names them.
- T400, major. `sema_spell` spelled no path through the field the checker
  writes to reach an inherited field, so `b.one = b.one` of an inherited
  `own` field was refused, and `b.two = b.one` named "the value". It now
  passes over that field.
- T401, minor. The refusal of a copy of a class whose teardown is the
  `destroyed` it replaces said `has `own` fields`. It says
  `replaces `destroyed`` now. The phrase is new, since none named the
  hook, and a `[provisional]` line records it.

`std_sync_map` and `std_concurrent_map` wrote `m.get(key) ?? missing` of
a local, which T397 refuses, and copy it with `dup` now.

## Commits

- `4e0685c2` lists T396 to T401 with 35 tests of the matrix.
- `b5d8abac`, `2de3cb65`, `340e199f`, `4af427b0`, `747abf44` and
  `15541871` fix T396, T397, T399, T398, T400 and T401. The fix of T397
  adds a test of a `dispatch` of `a ?? b`.
- `028eae55` tests twelve sides of the two files.
- `ea57c927` records the times, the counts, the fixes, the phrase and the
  guards of the batch in `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Fuzzer

`cmake -DFIRST=47001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 47001 to
47200 at the default size in 161 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.52%, 3748 missed of 57822 | 93.51%, 3755 missed of 57896 |
| Regions | 98.51%, 1301 missed | 98.51%, 1302 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1259 missed | 98.71%, 1260 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_value.c` | 58 of 530 | 57 of 558 |
| `src/antic/lower.c` | 59 of 654 | 55 of 650 |

The fixes add 74 branches, so the totals move little. The "before" run
had 8221 tests in 1103 s, and the run after the batch 8285 in 1131 s.
Every test passed in each.

Sides that the batch reached by a probe and did not test, for the next
batch: `sync m as Mutex { }`, the one cast `sema_reads_existing` still
reads, an `extern fn` of a name of `allocating_function` with a result
that is no pointer, and a signature of no parameters met by `fits_bound`.
The tests of a quiet check in `sema_note_move` and `sema_move_local`, and
the sides of `lower_find_function`, `lower_find_global`,
`lower_add_param`, `find_signature`, `same_c_signature` and
`declare_function`, are not sorted.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, from 346 s to 389 s, 212 s
  to 301 s and 367 s to 415 s. `CLAUDE.md` holds each. Every test
  passed, after the two tests of `anti.collection` above.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8295. `asan` took 252 s, 181 s, 266 s, 165 s,
  130 s and 691 s, and `ubsan` 264 s, 187 s, 269 s, 165 s, 144 s and
  732 s. Every test passed.
- The host runs 8297 tests and each sanitizer tree 8295.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

No fix moved `tests/emit-identity/programs.sha256` or
`tests/link-identity/return42.macos-arm64.sha256`, and both stand as
they were.

## Questions for Eddie

1. T398 and T399 are graded `severe`, a `destruct` that never runs, by
   the sentence on the grades. T387, a fresh argument never torn down,
   stands as `minor`. Which grade do the three take?
2. A local `Mutex` passed to an `own` parameter is refused with
   `` a `Mutex` cannot be copied ``, while the DESIGN of `moves_at_call`
   in `src/antic/sema_value.c` says a Mutex moves there. A fresh
   `Mutex.new()` moves. Does a local Mutex move into an `own`
   parameter?

T385, T387, the question on `anti fmt` and the lines of a table, question
1 of batch 3ao on `compare_swap` of an `own` field and question 1 of
batch 3aq on the two messages of `provides` wait for him as before.
