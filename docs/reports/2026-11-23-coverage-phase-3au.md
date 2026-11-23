# Batch 3au of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `07564a6f` gave the ranking. The batch read two
files that batch 3z had probed and no batch had read: the descriptors,
the tables and the chains of lowering in `src/antic/lower_desc.c`, which
missed 65 of its 600 branches, and the pages of `anti doc` in
`src/antic/docpage.c`, which missed 50 of its 364. It read both against
the specifications, `docs/decisions.md` and the DESIGN comments of the
two files, and probed each side with the antic and the anti of the host
tree. The batch found five defects, T408 to T412, and fixed four. T412
waits for Eddie. The next free id is T413. The batch before found six,
and this one found five, so the phase goes on.

## Findings

- T408, major, fixed. `class_chain` hashed the text of `signature_code`
  for every entry of an interface. For an entry that may fail, that text
  is the result `?*Error` and the out pointer, `ptr.ptr`, whatever the
  declared result. A plugin built against a version whose `read` gives
  an `f64` loaded into a host whose `read` gives an `int`, and the host
  printed `4620693217682128896`, the bits of `8.0`. The chain hashes the
  text that `reflect.call` keys on now, `!` and the declared result,
  through `entry_code`.
- T409, minor, fixed. The page of an enum wrote a value below zero of a
  signed base as the pattern of its 64 bits, `Down = 18446744073709551615`.
- T410, minor, fixed. The page of a float constant wrote six digits,
  `PI: f64 = 3.14159`, and `1e+20`, which is no float literal of Anti.
  It writes the fewest digits that give back the value now. A value that
  is not finite carries none, by a provisional line under "The doc
  command".
- T411, minor, fixed. The page of `pub singleton class Config` wrote
  `pub class Config`.
- T412, minor, open. The page of `pub fn note(to: *log.Logger)` writes
  `*Logger`. R178 of "Rules of 2026-10-13" says the page writes a type as
  the source spells it. Question 1 asks.

## Commits

- `5e33bd17` lists T408 to T412 with six tests.
- `538d0c1e`, `b2f33475`, `622b2f18` and `cc675f17` fix T408 to T411.
- `0ff649a3` tests the sides of the pages. Its page holds a name of a
  type that is no copy and a function that gives nothing and may fail.
  It holds a function of C that takes a function and the float constants
  of each form. It holds a base of `anti.lang` other than the root and
  the functions of a synchronized class.
- `1a1b573b` records the times, the counts and the fixes, and the
  section "The sides of batch 3au" of `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Fuzzer

`cmake -DFIRST=47401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 47401 to
47600 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.55%, 3737 missed of 57904 | 93.56%, 3731 missed of 57934 |
| Regions | 98.51%, 1300 missed of 87367 | 98.51%, 1303 missed of 87433 |
| Functions | 99.87%, 6 missed of 4772 | 99.87%, 6 missed of 4774 |
| Lines | 98.71%, 1260 missed of 98031 | 98.71%, 1261 missed of 98074 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/lower_desc.c` | 65 of 600 | 66 of 602 |
| `src/antic/docpage.c` | 50 of 364 | 44 of 392 |

The "before" run had 8307 tests in 1090 s. The run after the batch, at
`0ff649a3`, had 8315 in 1090 s. Every test passed in each. No input reaches a
side of `lower_desc.c` that the before run missed. The fix of T408 adds
the check of `entry_code`, which is a guard too. The file misses one
more side. Every side of the two files that the after run misses has a
row in the note. The row stands under the heading of its file or under
"The sides of batch 3au".

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,8327` before each commit of code. The parts took 280 s to
  386 s, 250 s to 281 s and 379 s to 471 s. `CLAUDE.md` holds each.
  Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8325. `asan` took 249 s, 182 s, 267 s, 163 s,
  114 s and 635 s, and `ubsan` 249 s, 182 s, 261 s, 106 s, 85 s and
  660 s. Every test passed.
- The host runs 8327 tests and each sanitizer tree 8325.
  `tests/known-failures.txt` holds the two lines of T136 and T138, three
  lines each of T385, T387, T404 and T407, and the line of T412.

## Pinned outputs

The fix of T408 moves the chain of the interface of `may_fail_table`, so
its six lines of `tests/emit-identity/programs.sha256` are written
again. `tests/link-identity/return42.macos-arm64.sha256` stands as it
was.

## Questions for Eddie

1. T412. R178 says `anti doc` writes a type as the source spells it, and
   its provisional entry takes the spelling per item for the names of
   one type, as `i64` and `int`. A class, a struct or an enum of another
   module stands without its module, `*Logger` for `*log.Logger`, while
   the base after `inherits` stands with the whole path of its module.
   The page of a library file has no import to read the spelling from.
   Does the page write such a type with the last segment of its module,
   as a plain import spells it, with the whole path, as the base, or
   with the spelling the item records, as the names of one type are?

T385, T387, T404, T407, the question on `anti fmt` and the lines of a
table, question 1 of batch 3ao on `compare_swap` of an `own` field,
question 1 of batch 3aq on the two messages of `provides`, and the two
questions of batch 3as on the grades of T398 and T399 and on a local
`Mutex` passed to an `own` parameter wait for him as before.
