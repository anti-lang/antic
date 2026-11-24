# Batch 3ba of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `ed318919` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the dump of the
tree in `src/antic/ast_dump.c`, which missed 23 of its 558 branches, and
the sites of the hooks in `src/antic/lower_hook.c`, which missed 21 of
its 82. `src/anti/bindwrite.c` missed 22, and the batch left the writer of
`anti bind` for a later one. It read the first against the dump of the
tree in `docs/notes/parser.md` and `docs/notes/sema.md`, and the second
against "Hooks and tracing" and "Other features with generics" in
`docs/anti-language-additions.md`, the entries under "Hooks and tracing"
in `docs/decisions.md`, `docs/notes/hooks.md` and the DESIGN comments of
both files. It probed each side with the antic of the host tree and of
the coverage tree. The batch found T433 and fixed it. The next free id is
T434. The batch before found three, and this one found one, so the phase
goes on.

## Findings

- T433, major, fixed. A `--trace` pattern that names a generic class,
  `--trace Box` for `class Box<T>`, instrumented no copy of it, while
  `trace class Box<T>` marks every copy and the module path reaches them.
  `lower_traced_class` of `lower_hook.c` compared the pattern with the
  name of the copy, `Box<int>`. It now asks the generic of a copy as
  well, so the pattern of the generic names each copy and the pattern of
  one copy still names that copy. A copy of a generic of a library file
  follows, since the program lowers it.

## Commits

- `c7122b7e` lists T433.
- `f6abaa3e` fixes T433.
- `9e8d4d91` tests the sides of `lower_hook.c` and `ast_dump.c`.
- `92a29a5d` tests a pattern that names no copy of a generic, a side that
  the fix added and the run after the batch showed missed.
- `6ec5c832` records the times, the counts, the hash of the fix, the
  section "The sides of batch 3ba" of `docs/notes/unreached-guards.md`
  and the two provisional entries.
- The docs commit of this report.

## Choices

- Two gaps of "Hooks and tracing" got the smallest option and a
  `[provisional]` entry there, and each waits for a question below. A
  write to an element of an array field or to a field of a struct field
  calls no `changed`. A pattern that writes the module path before the
  name of a class names no class. Neither changes code.
- `--dump-types` writes no type beside a field of a class. It writes one
  beside a field of a struct, a union and a case of a variant.
  `dump_params` takes the type of a field from a struct alone, and
  `tests/dump/generics.types` pins the field of a class without one. The
  batch left it as it stands.
- Every side of the two files that the suite misses after the batch has
  a row in "The sides of batch 3ba" of `docs/notes/unreached-guards.md`.

## Fuzzer

`cmake -DFIRST=48601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 48601 to
48800 at the default size in 162 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.65%, 3701 missed of 58324 | 93.68%, 3686 missed of 58328 |
| Regions | 98.50%, 1318 missed of 88017 | 98.50%, 1317 missed of 88023 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.71%, 1275 missed of 98635 | 98.71%, 1275 missed of 98637 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/ast_dump.c` | 23 of 558 | 14 of 558 |
| `src/antic/lower_hook.c` | 21 of 82 | 16 of 86 |

The "before" run had 8471 tests in 1116 s. The run after the batch, at
`9e8d4d91`, had 8493 in 1172 s. Every test passed in each. The fix added
four branches to `lower_hook.c`, and one of them was missed after the
batch. `92a29a5d` takes it, which the antic of the coverage tree measured
alone, so the file misses 15 of 86 with it. Each of the 15 has a row in
the section of the batch.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds each
  time. Before the list of T433 `fmt_canonical` failed on the comment of
  the new program, which passed alone once it stood in its canonical
  form. Every other test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8504. `asan` took 258 s, 184 s, 272 s, 167 s, 107 s
  and 725 s, and `ubsan` 263 s, 187 s, 275 s, 168 s, 117 s and 710 s.
  Every test passed.
- The host runs 8506 tests and each sanitizer tree 8504.
  `tests/known-failures.txt` holds the lines of T136, T138 and T432.

## Pinned outputs

The fix moves no output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. `--trace writes` calls `changed` after a write whose target is a field
   of an instrumented class. `self.arr[1] = v` writes an element of the
   array field `arr`, and `self.pair.a = v` a field of the struct field
   `pair`, and neither calls the hook. The rule says "after every write
   to a field of an instrumented class, wherever the write stands". Should
   both call `changed` with the record of `arr` and of `pair`, and should
   a write through a pointer field, `self.p.x = v`, stay without one?
2. A `--trace` pattern names a class by the name its module gives it,
   `Store` or `Outer.Inner`, and `--trace app.Store` names no class,
   although the hooks name the function `app.Store.put`. Two modules that
   each declare a `Store` are told apart only by the module path, which
   reaches every class of the module. Should a pattern of the module path
   and the name of a class name that class alone?
