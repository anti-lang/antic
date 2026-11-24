# Batch 3bc of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `ace70289` gave the ranking. The batch read two
files that no batch chosen by branches had read: the functions lowering
writes for a class in `src/antic/lower_class.c`, which missed 19 of its
136 branches, and the error channel in lowering in
`src/antic/lower_error.c`, which missed 20 of its 102. It read them
against "Static fields and singletons", "Errors" and "The C view" in
`docs/anti-object-model.md`, "Hooks and tracing" and "Generics and
collections" in `docs/decisions.md`, `docs/notes/hooks.md` and the
DESIGN comments of both files. It probed each side with the antic of
the host tree and of the coverage tree. The batch found T440 and T441
and fixed both. The next free id is T442. The batch before found six,
and this one found two, so the phase goes on.

## Findings

- T440, severe, fixed. The `get` of a copy of a generic singleton of a
  library module called another function of the module:
  `registry.Reg<int>.get()` crashed in release mode and called `bump`
  in dev mode. The reader of a library file built the `get` of a
  singleton as a plain function of the library, so the copy that
  `member_copy` makes had no body and no mark, and lowering declared no
  function for it. A singleton declares no `get` of its own by R112, so
  the reader now marks the `get` of every singleton. The test holds a
  copy that the library file holds, a copy that the program and a
  second library make each, which gives one instance, and a function of
  the class without `self`.
- T441, severe, fixed. `anti_<Class>_construct`, the C counterpart of
  `Class(args)`, called the init of the class, which runs the `created`
  hook, before the `construct` with the arguments, and the hook ran
  when that `construct` failed as well. The function now prepares the
  object as the init does and runs the hook once `construct` succeeded,
  as `Class(args)` does. The DESIGN of `lower_class_construct` stands as
  it was, since the function still prepares the object as the init
  does.

## Commits

- `0b443f0a` lists T440 and T441.
- `b0b3782a` fixes T440 and `518efac3` fixes T441.
- `ad236d2a` tests the sides of both files, with
  `clib_created_no_hooks` and the second library of
  `modules_generic_singleton`.
- `b12ca8d2` tests `alloc(T, n)` converted to an interface, the side of
  `lower_converts_in_place` that the run of coverage after the batch
  named, and adds its program to the manifest of `emit_identity`.
- `b28e4251` tests a function without `self` of a generic singleton,
  the side of the fix of T440 that the same run named.
- `7bc65dab` records the times, the counts, the hashes of the fixes
  and the section "The sides of batch 3bc" of
  `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Choices

- The fix of T440 marks the `get` in the reader of a library file,
  where the mark was lost. The item of a library file then says what
  the item of the source says, and the copy takes the mark from it.
- The `created` hook of `anti_<Class>_construct` carries the mark of an
  object on the heap, as the hook of the init carried it before the
  fix. The batch moved the hook and left what the runtime counts as it
  was.
- `yield` in the handler of a call that stands as an argument of a
  `*Shape` is checked against the result of the call, `*Square`, by the
  DESIGN of `check_handled` in `src/antic/sema_call.c` and D of
  "Answers of 2026-11-19", which name a `let` of a `?*Base`. The batch
  left it so.
- The lost race of `get` of a singleton frees the object it built
  without its teardown, after its `created` hook ran, by the DESIGN of
  `singleton_instance`. No test can make the race happen on demand, so
  the batch wrote none and left the code as it stands.
- Every side of the two files that the suite misses after the batch has
  a row in "The sides of batch 3bc" of `docs/notes/unreached-guards.md`.

## Fuzzer

`cmake -DFIRST=49001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 49001 to
49200 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.75%, 3648 missed of 58354 | 93.75%, 3648 missed of 58362 |
| Regions | 98.51%, 1308 missed of 88062 | 98.51%, 1308 missed of 88069 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.72%, 1267 missed of 98666 | 98.72%, 1265 missed of 98687 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/lower_class.c` | 19 of 136 | 15 of 138 |
| `src/antic/lower_error.c` | 20 of 102 | 16 of 102 |

The "before" run had 8543 tests in 1204 s. One failed, `fmt_canonical`,
on the library file of `clib_created`, which the batch wrote while the
run read the tree. It passed alone once the file stood in its canonical
form. The run after the batch, at `ad236d2a`, had 8562 tests in 1052 s,
and every test passed. The fix of T441 added two branches to
`lower_class.c`, and the fix of T440 six to `src/antic/antl_read.c`.
`b12ca8d2` takes one more side of `lower_error.c`, and `b28e4251` the
side of the fix of T440 in `antl_read.c` that the run missed. The
antic of the coverage tree measured both alone, so `lower_error.c`
misses 15 of 102 with them. Each side that stays has a row in the
section of the batch. The two files miss 8 sides fewer, while the total
stands. `antl_read.c` misses one more, the side that `b28e4251`
takes, and `src/anti/fmt.c` one more, which the failed `fmt_canonical`
of the run before reached. The other six lie in files that the batch
did not change. The summary of the run before was not kept, so this
report cannot name them.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds
  each time. Every test passed in each. In the run before `b12ca8d2`
  `emit_identity` failed on the new program, and it passed alone once
  the manifest held the texts of the program.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8574. `asan` took 260 s, 186 s, 293 s, 170 s,
  112 s and 705 s, and `ubsan` 270 s, 188 s, 284 s, 174 s, 121 s and
  767 s. Every test passed.
- The host runs 8576 tests and each sanitizer tree 8574.
  `tests/known-failures.txt` holds the lines of T136, T138 and T432.

## Pinned outputs

`b12ca8d2` adds the six texts of `alloc_count_to_an_interface` to
`tests/emit-identity/programs.sha256`, and no line of the manifest
changed. No fix moves an output, so the manifest and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were
apart from those six lines.

## Questions for Eddie

1. The handler of a call that stands as an argument of a `*Shape`
   parameter yields the result type of the call, `*Square`, and refuses
   another class of the interface, `` expected `*Square`, found `*Line` ``.
   D of "Answers of 2026-11-19" lets the handler of a `let` of a
   `?*Base` yield any `?*Base`. Should the handler of a call at a
   parameter, or at a `let` of a `*Base` that may not be `none`, yield
   any value of that type as well?
2. A thread that loses the race of the first `get` of a singleton frees
   the object it built with `free` alone. Its `created` hook ran, its
   `destruct` does not run, and what its `construct` allocated stays.
   Should the loser tear the object down as `delete` does?
