# Batch 3at of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `efd4f9a6` gave the ranking. The batch read two
files that no batch had read: the C side of the checker in
`src/antic/sema_export.c`, which missed 33 of its 418 branches, and the
check `uninitialized-read` in `src/antic/sema_uninit.c`, which missed 32
of its 488. It read both against the specifications, `docs/decisions.md`
and the DESIGN comments of the two files, and probed each side with the
antic of the host tree. The batch found six defects, T402 to T407, and
fixed four. T404 and T407 wait for Eddie. The next free id is T408. The
batch before found six, and this one found six, so the phase goes on.

## Findings

- T402, major, fixed. `sema_check_export` checked the result of the
  function type of an `export fn` that may fail, which is `?*Error`, and
  never the out pointer that carries the result. `-> str may fail` and a
  tuple of a `str` crossed to C. The check reads the written result now.
- T403, minor, fixed. The check of a public function of an export class
  read the out pointer of the `may fail` form as a parameter, so the
  refusal of a `str` result named `the parameter 1` with type `*str`. It
  names the result now.
- T404, major, open. An export class, struct or enum whose C name an
  imported library exports as a class or a struct is not refused. opt
  stops on the helpers of two classes, and the header of the others
  defines a tag twice. `check_c_name_once` compares a struct, a union and
  a constant alone, which its DESIGN and item 5 of "Answers of
  2026-11-19" name. Question 1 asks.
- T405, major, fixed. C1 took a `switch` without `else` on an enum or a
  variant as one that may run no arm, so an element that every arm
  writes was refused after it. The test that `sema_class.c` kept for T288
  moves to `sema_stmt.c` as `sema_switch_runs_an_arm`, and C1 reads it.
- T406, minor, fixed. E2 had the same flaw, so the message of a use
  after a `switch` whose every arm deletes the node named one of the
  paths. It reads the same test now.
- T407, major, open. C1 keys a write at `p[i]` on the local `i`, and only
  `=` drops the key, so `*q = 3` through `&i`, a call that takes `&i` and
  a closure that captures `i` let a read of an element no path wrote
  pass. Question 2 asks.

## Commits

- `49edc37b` lists T402 to T407 with fourteen tests.
- `997cc282`, `67464b83`, `01e5ceef` and `a19e673b` fix T402, T403, T405
  and T406.
- `07d13024` tests five sides of the two files. `725147dd` tests two
  more, which the sorting of the sides found a program reaches. One is a
  public function of an export class of an unknown type. The other is a
  loop after a write at a computed index.
- `93856a4a` records the times, the counts and the fixes, and the
  section "The sides of batch 3at" of `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Fuzzer

`cmake -DFIRST=47201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 47201 to
47400 at the default size in 159 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.52%, 3750 missed of 57896 | 93.54%, 3740 missed of 57904 |
| Regions | 98.51%, 1301 missed | 98.51%, 1301 missed of 87367 |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed of 4772 |
| Lines | 98.71%, 1260 missed | 98.71%, 1260 missed of 98031 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_export.c` | 33 of 418 | 26 of 426 |
| `src/antic/sema_uninit.c` | 32 of 488 | 27 of 488 |

The "before" run had 8285 tests in 1115 s, and the run after the batch,
at `725147dd`, 8307 in 1135 s. Every test passed in each. Every side of
the two files that the after run misses has a row in the note. The row
stands under the heading of its file or under "The sides of batch 3at". The side of
`before` in `sema_export.c` is reached by a source with two items on one
line, which the canonical form of every test excludes.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. The parts took 371 s to
  386 s, 280 s to 285 s and 392 s to 403 s. `CLAUDE.md` holds each. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8317. `asan` took 251 s, 181 s, 271 s, 165 s,
  114 s and 629 s, and `ubsan` 253 s, 163 s, 169 s, 106 s, 85 s and
  668 s. Every test passed.
- The host runs 8319 tests and each sanitizer tree 8317.
  `tests/known-failures.txt` holds the two lines of T136 and T138. It
  holds three lines each of T385, T387, T404 and T407.

## Pinned outputs

No fix moved `tests/emit-identity/programs.sha256` or
`tests/link-identity/return42.macos-arm64.sha256`, and both stand as
they were.

## Questions for Eddie

1. T404. `check_c_name_once` of `src/antic/sema_export.c` refuses an
   export struct, union or constant whose C name another module exports
   as one of the same three kinds. Its DESIGN and item 5 of "Answers of
   2026-11-19" say so. An export class, struct or enum named as an export
   class or struct of an imported library passes. opt or the C header
   then fails. Does the refusal take every export kind whose C
   name is a tag or a helper, the classes and the enums among them?
2. T407. The DESIGN at the head of `src/antic/sema_uninit.c` says a write
   at the index a local names marks the local "until `=` changes it",
   and that "every case it cannot follow counts as written". A pointer or
   a closure changes the local without `=`. One fix keys no write on a
   local that `&` took or a closure captures, so a read at it needs the
   whole range, with a `[provisional]` line. That changes what the DESIGN
   says, and it refuses `let q = &i; p[i] = 1; let x = p[i];` when
   nothing writes through `q`. Does C1 take that fix, or does it count a
   local that a pointer or a closure may change as one it cannot follow,
   and let the read pass?

T385, T387, the question on `anti fmt` and the lines of a table, question
1 of batch 3ao on `compare_swap` of an `own` field, question 1 of batch
3aq on the two messages of `provides`, and the two questions of batch 3as
on the grades of T398 and T399 and on a local `Mutex` passed to an `own`
parameter wait for him as before.
