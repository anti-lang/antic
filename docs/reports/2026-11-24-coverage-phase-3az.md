# Batch 3az of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `1ca93c0c` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the places, the
addresses and the constants of lowering in `src/antic/lower_place.c`,
which missed 23 of its 262 branches, and the run-time side of
`anti.regex` in `src/rt/patterns.c`, which missed 23 of its 233.
`src/antic/ast_dump.c` missed 23 as well, and the batch left the dump of
the tree for a later one. It read the first against "Aliasing of views"
in `docs/anti-language-additions.md` and the DESIGN comments of the file,
and the second against "Regular expressions" and "Bytes" of the same
document, the entries of patterns under "Regular expressions" in
`docs/decisions.md` and the DESIGN comments of the file. It probed each
side with the antic of the host tree and read the counts of each with
llvm-cov. The batch found T430 to T432 and fixed T430 and T431. T432
stays open for a choice of Eddie. The next free id is T433. The batch
before found one, and this one found three, so the phase goes on.

## Findings

- T430, severe, fixed. A `Match` field that `Object.deserialize` leaves
  without a pattern, from a text that gives it as `null`, stopped the
  program with SIGSEGV at `h.m.group(1)` and `h.m.group("year")`.
  `anti_rt_regex_group` and `anti_rt_regex_group_named` handed the
  missing handle on to PCRE2. Both now check it first, as every other
  function of `src/rt/patterns.c` that takes a pattern from Anti does,
  and the program stops with the message of a search of a `Regex`
  without a pattern. A `[provisional]` entry under "Regular expressions"
  gives the rule for a match.
- T431, major, fixed. `data.patch(re"(a)", b"x", "")` compiled and wrote
  into group 1, while `m.group("")` is refused with
  ``` the pattern has no group `` ```. `patch_literal_group` of
  `src/antic/sema_pattern.c` took a name of no bytes for no name. The
  check of the arguments now records that the call gave a name, and an
  empty one gets that message.
- T432, severe, open. `patch` with a compiled pattern into a group that
  an empty name computed at run time names writes into the one group of
  the pattern and gives 1, where a name the pattern lacks fails with
  `MissingGroup`. The checker passes the empty text for a call without a
  name, and the DESIGN of `anti_rt_regex_patch` reads a name of no bytes
  as no name. The fix changes that DESIGN, so it waits for question 1.

## Commits

- `482add6a` lists T430 to T432.
- `1c4d389e` fixes T430, and `39188d91` fixes T431.
- `f37d5820` tests the sides of `src/rt/patterns.c`.
- `f447625d` records the times, the counts, the hashes of the fixes, the
  section "The sides of batch 3az" of `docs/notes/unreached-guards.md`
  and `docs/reports/2026-11-24-state.md`, which the session wrote when
  its context was compacted.
- The docs commit of this report.

## Choices

- No side of `lower_place.c` that the suite misses can be reached. A
  static field is `atomic` and never a place, the checker writes `super`
  before a field of a base, `here` is no constant expression, and a `?T`
  and a `SourceLocation` have their fields alone. Every side has a row
  in the section of the batch. The shared data of `lower_const_address`
  matches a mutable global of its module only by an equal tree, and
  every mutable global of a module holds a scalar, so no constant reads
  one.
- The side of `failure` in `src/rt/patterns.c` that stops a program with
  the message of PCRE2 is reached by a `str` that is no valid UTF-8, and
  `text.slice("é", 0, 1)` gives one. The batch wrote no test of it, by
  question 2.

## Fuzzer

`cmake -DFIRST=48401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 48401 to
48600 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.64%, 3711 missed of 58324 | 93.66%, 3698 missed of 58324 |
| Regions | 98.50%, 1318 missed of 88015 | 98.50%, 1316 missed of 88017 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.70%, 1278 missed of 98630 | 98.71%, 1275 missed of 98635 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/lower_place.c` | 23 of 262 | 23 of 262 |
| `src/rt/patterns.c` | 23 of 233 | 12 of 233 |
| `src/antic/sema_pattern.c` | 13 of 442 | 12 of 442 |

The "before" run had 8463 tests in 1193 s. The run after the batch had
8471 in 1127 s. Every test passed in each. Every side of the two files
that the after run misses has a row in the section of the batch, apart
from the side of `failure` that question 2 names.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds each
  time. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8481. `asan` took 255 s, 191 s, 286 s, 170 s, 115 s
  and 647 s, and `ubsan` 259 s, 182 s, 275 s, 170 s, 116 s and 704 s.
  Every test passed.
- The host runs 8483 tests and each sanitizer tree 8481.
  `tests/known-failures.txt` holds the lines of T136, T138 and T432.

## Pinned outputs

The fixes move no output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. T432. `patch` passes no name as the empty text, so a name of no bytes
   computed at run time writes into the one group of a pattern, or into
   the whole match of one without groups. Should an empty name fail with
   `MissingGroup`, as `m.group("")` stops the program, and may the fix
   change the DESIGN of `anti_rt_regex_patch` and the parameters of
   `patch`, `patch_literal` and `patch_fixed` of `anti.regex` to tell a
   name from no name?
2. `text.slice` cuts a `str` at any byte, so `text.slice("é", 0, 1)` is a
   `str` that holds no valid UTF-8, and a search of it stops the program
   with `UTF-8 error: 1 byte missing at end`. The entry on UTF-8 in
   `docs/decisions.md` says a `str` is valid UTF-8. Should `text.slice`
   keep to the bytes as its comment gives it, or move a cut inside a
   character, and if so, to which side?
