# Batch 3bd of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `56f7beda` gave the ranking. The batch read two
files that no batch chosen by branches had read: the parser of a spelling
of a C type in `src/anti/bindtype.c`, which missed 15 of its 292
branches, and the module path of a source in `src/antic/modpath.c`,
which missed 14 of its 96. It read them against "Binding generator" in
`docs/tooling-addendum.md`, "The bind command" and R147 of "Rules of
2026-10-13" in `docs/decisions.md`, "Left out" in `docs/notes/bind.md`
and the DESIGN comments of both files. The batch found T442 to T444 and
fixed all three. The next free id is T445. The batch before found two,
and this one found three, so the phase goes on.

## Findings

- T442, major, fixed. `anti bind` left out a function that takes a
  callback of 65 parameters, with `` the type `void (*)(int, ...)` does
  not parse ``. `anti bind --clang` of a header left it out as well.
  `function_of` collected the parameters in an array of 64 and refused
  the list past it. The list now moves to `arena_alloc` memory of the
  module when it outgrows the array.
- T443, major, fixed. A source under the search root `/` took the file
  name alone as its module path. `under` drops the trailing separators of
  a root, so `/` kept no byte and was taken for a root that holds
  nothing. A root of separators alone now holds every absolute path, as
  R147 says, and an empty root still holds nothing.
- T444, major, fixed. A length of an array in binary, `int[0b101]`, read
  as 0, so `anti bind` of a description left the struct out for a field
  without a form. `strtoll` of base 0 takes no `0b`. A length with the
  prefix `0b` or `0B` is now read in base 2, as clang reads it for
  `anti bind --clang`.

## Commits

- `5bfb36ac` lists T442 to T444. It adds `anti_bind_api_spellings`, which
  binds the spellings of `bindtype.c` that no description held, and the
  cases of `module_paths` in `tests/unit/test_modules.c` that reach the
  sides of `modpath.c`.
- `e638d3b6` fixes T442, `94b8fb5f` T443 and `b5ae245a` T444.
- `68a3d740` tests a relative source under the root `/`, the side of
  the fix of T443 that the run of coverage after the batch named.
- `60db709d` records the times, the counts, the hashes of the fixes, the
  section "The sides of batch 3bd" of `docs/notes/unreached-guards.md`
  and the state file.
- The docs commit of this report.

## Choices

- The fix of T442 keeps the array of 64 on the stack. It copies the list
  to `arena_alloc` memory when it outgrows the array, so a list of 64 or
  fewer allocates nothing more than before.
- The fix of T444 reads the prefix `0b` alone. The parser still reads a
  length with `strtoll`, which stops at a suffix of `u` or `l`.
- With the fix of T443, `antic -I / -c <path>` on this Mac refuses the
  source with `` `Volumes` in <path> is not a lowercase identifier ``,
  since R147 takes the whole absolute path. A relative source whose
  directory cannot be resolved is compared as written, and the root `/`
  holds no relative path.
- Every side of the two files that the suite misses after the batch has
  a row in "The sides of batch 3bd" of `docs/notes/unreached-guards.md`.

## Fuzzer

`cmake -DFIRST=49201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 49201 to
49400 at the default size in 155 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.76%, 3640 missed of 58362 | 93.80%, 3621 missed of 58374 |
| Regions | 98.52%, 1307 missed of 88069 | 98.52%, 1305 missed of 88085 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.72%, 1265 missed of 98687 | 98.72%, 1264 missed of 98705 |

| File | Branches missed before | after |
|---|---|---|
| `src/anti/bindtype.c` | 15 of 292 | 3 of 298 |
| `src/antic/modpath.c` | 14 of 96 | 3 of 102 |

The run before had 8564 tests in 1187 s, and the run after, at
`b5ae245a`, 8567 in 1172 s. Every test passed in both. The fixes of T442
and T444 added six branches to `bindtype.c`, and the fix of T443 six to
`modpath.c`. `68a3d740` takes the one side of the fix of T443 that the
run missed, which the unit tests of the coverage tree measured alone, so
`modpath.c` misses 2 of 102 with it. The two files miss 23 sides fewer.
`src/rt/hooks.c` misses one fewer, and `src/rt/mem.c` one and
`src/rt/platform_posix.c` four more. The batch changed none of the
three.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds
  each time. Every test passed in each.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8577. `asan` took 254 s, 186 s, 280 s, 167 s,
  116 s and 790 s, and `ubsan` 267 s, 187 s, 273 s, 164 s, 118 s and
  737 s. Every test passed.
- The host runs 8579 tests and each sanitizer tree 8577.
  `tests/known-failures.txt` holds the lines of T136, T138 and T432.

## Pinned outputs

No fix moves an output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None. Each fix follows a rule that the specifications and
`docs/decisions.md` state.
