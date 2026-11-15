# Batch 3j of the third phase of coverage

This step sorts the regions that `./c coverage` showed unreached in the
candidates that the report of batch 3i left: `src/antic/layout.c`,
`src/rt/platform_posix.c`, `src/rt/fault.c`, `src/anti/check.c`,
`src/antic/lower_simd.c`, `src/antic/lower_hash.c`,
`src/antic/sema_thread.c`, `src/antic/lower_owning.c`, `src/anti/cache.c`
and `src/antic/sema.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each region first ran
through the antic or the anti of the coverage tree alone, with a profile
of its own, so a probe showed the count of each line it reached. Eddie's
direction is the Mac alone, so the VMs were not touched. The batch found
no defect. T260 is still the next free id.

## Commits

- `a61b2a24` tests `layout.c`.
- `52a44ef0` tests `check.c` and `cache.c`.
- `7b06268d` tests `fault.c` and `platform_posix.c`.
- `5de0f50f` tests `sema_thread.c` and `lower_simd.c`.
- The commit of the guards in `docs/notes/unreached-guards.md`, the times
  and the counts in `CLAUDE.md`, and this report.

## layout.c

A constant array and a constant struct hold NaN and its negation, which
the back end writes as the canonical bits. A constant struct holds a
bitfield of all 64 bits of its unit. A size expression that divides by
zero on linux-x86_64 stands as an argument of a call and as the right
operand of an operation, two cases of `tests/errors/`. `test_layout.c`
lays out a struct whose array fails before its `align(2)` fails too,
which keeps the first message, and a size expression of `fadd`, which a
library file can carry. The four lines left are the refusal of a struct
that holds itself, a guard.

## check.c and cache.c

`anti check` compiles the block of a fence that stands after spaces,
holds no block for a fence of `text` or of no tag, and finds a file of
the length `anti fmt` writes with other bytes, a list of direct imports
out of order. `anti build` builds a program when the environment names
no cache of the user, when that cache is a file, when the file of the
digests holds lines of another form and when a directory stands where
its new copy goes. The faults of the seam are held while the key is
made, so these cases use the file system. The lines left in `check.c`
are guards of classes that the front-end class runs first.

## fault.c and platform_posix.c

A count of `ANTI_FAULT` without digits and one with a byte below the
digits are refused, and an empty variable names no fault. An empty
`ANTI_CONF` names no file. The lines left run on Windows, in a race of
two threads or on an older processor.

## sema_thread.c and lower_simd.c

Five refusals of the matrix get one message each: `close(c) catch`, a
`dispatch` of a local that holds an integer and of an element of an
array of functions, `compare_swap` on a bitfield that `unchecked` marks
and an atomic field of an unknown type. A `join_all` whose slice has a
low bound of the run, or stands on an element of an array, runs, since
the checker follows neither. `simd.any` and `simd.all` of a mask of 512
lanes fold its bytes in a loop.

## Sorted without a test

`lower_hash.c` misses no line that an input reaches: a lock part is
passed over before `hash_at`, and the rest follows a `return`. The
`TYPEX_CONST` arm of `sema.c` stays a guard, since the parser and the
check of a generic argument refuse a constant where a type stands. In
`lower_owning.c` a moved `keep own` function did not reach the clear of
an `own fn`, which stays unsorted.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.41%, 1349 missed | 98.43%, 1329 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.60%, 1335 missed | 98.63%, 1309 missed |
| Branches | 91.88%, 4547 missed | 91.94%, 4511 missed |

| File | Lines before | Lines after | Branches missed before | after |
|---|---|---|---|---|
| `layout.c` | 13 missed | 4 missed | 25 | 17 |
| `platform_posix.c` | 13 missed | 13 missed | 13 | 16 |
| `fault.c` | 13 missed | 13 missed | 13 | 10 |
| `check.c` | 12 missed | 12 missed | 9 | 5 |
| `lower_simd.c` | 12 missed | 2 missed | 7 | 4 |
| `lower_hash.c` | 12 missed | 12 missed | 6 | 6 |
| `sema_thread.c` | 12 missed | 12 missed | 52 | 45 |
| `cache.c` | 12 missed | 5 missed | 26 | 15 |

`platform_posix.c` gained the empty value of `anti_rt_getenv` and lost
four sides of the walk of the stack and one of `dladdr`, which the run of
3i reached. Which frames a trace meets depends on the run, so those sides
belong to the measurement. The coverage suite ran 7441 tests after, all
passed. `./c coverage` took 992 s.

## Suites

- The host suite ran before each of the four code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,<last>`. The times stand in
  `CLAUDE.md`. Every test passed apart from the two skips of
  `mimalloc_environment`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7451. Every test passed. The last part took 484 s
  under ASan and 532 s under UBSan.
- The host runs 7453 tests and each sanitizer tree 7451.
  `tests/known-failures.txt` holds the two lines of `host=windows` alone.

## Next candidates

Past the files of this batch and `atomic.c`, the note has no section
yet for `src/antic/lower_owning.c`, which misses 12 lines. It has none
for `src/antic/sema.c`, `src/rt/toml.c`, `src/antic/antl_write.c` and
`src/antic/ast_dump.c` either, which miss 11 each. `sema_thread.c`
misses 45 branches and `sema.c` 53.

## Questions for Eddie

1. The guard of `compute` in `layout.c` refuses a struct that holds
   itself and returns with a size of 1. The layout of the struct then
   calls `layout_agg` on itself again, without end. A unit test that built such a struct by
   hand ended with a stack overflow, so the batch left it out. The
   checker and the reader of a library file refuse such a struct before
   the back end, so no input reaches it. Should the guard be made to end,
   or stay as it is?
