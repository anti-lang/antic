# Batch 3aw of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `f2085136` gave the ranking. The batch read two
files that no batch chosen by branches had read: the compiled copies of
generics in `src/antic/sema_copies.c`, which missed 80 of its 726
branches, and the lexer in `src/antic/lexer.c`, which missed 32 of its
705. It read both against "Generics" in
`docs/anti-language-additions.md`, "Tables and dispatch" in
`docs/anti-object-model.md`, the entries on comments and literals in
`docs/decisions.md` and the DESIGN comments of the two files, and probed
each side with the antic of the host tree. The probes of the members of a
copy led to two defects in `src/antic/sema_call.c`. The batch found T419
and T420 and fixed both. The next free id is T421. The batch before found
six, and this one found two, so the phase goes on.

## Findings

- T419, major, fixed. A function of a copy of a generic class named as a
  value through its class, `let f = Wrap<i16>.plain;`, kept the type of
  the generic. Its call was refused with `` expected `*Wrap`, found
  `*Wrap<i16>` ``, and a parameter of `T` took no `i16`. The same held
  for a body qualified by an interface, for a function passed as an
  argument and inside a generic function. `check_type_member` gives the
  function the type of the copy now, as it gave a constant and a static
  field already.
- T420, major, fixed. A function of a class with type parameters of its
  own, named through its class, on a value or through a copy and not
  called, was taken, and the local held `fn(*Plain, U) -> U` with `U`
  open. Its call was then refused with `` expected `U`, found an integer
  literal ``. antic refuses it now with `` `map` is generic, and names a
  function where it is called ``, the message `check_name` gives a
  generic function named as a value, so the fix adds no message.

## Commits

- `168c7b99` lists T419 and T420 with four programs of `tests/matrix/`.
- `a8c242bb` and `89d6a7fb` fix T419 and T420.
- `98c44fea` tests the sides of the two files. A program holds local
  constants of `size_of(T)` in copies. Another calls a function with type
  parameters of its own through its class and through a copy.
  `unit_lexer` takes nine cases more.
- `8dda19c8` records the times, the counts, the hashes of the fixes and
  the section "The sides of batch 3aw" of
  `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Choices

- The fix of T420 reuses the message of `check_name` for the same rule,
  so the feature freeze holds: the fix adds no message.
- The program of local constants reaches no side the suite missed. The
  side of line 269 of `sema_copies.c` that the run missed is a
  substitution that gives no value, which has a row in the note. The
  program stays, since it shows that the constant of `i16` folds and that
  of `c_long` stays a size.
- Most sides of `sema_copies.c` that the suite misses are guards behind
  the checker. Those that the rows under its own heading did not name
  have rows under "The sides of batch 3aw".

## Fuzzer

`cmake -DFIRST=47801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 47801 to
48000 at the default size in 160 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.58%, 3739 missed of 58214 | 93.59%, 3732 missed of 58232 |
| Regions | 98.51%, 1313 missed of 87846 | 98.50%, 1315 missed of 87872 |
| Functions | 99.87%, 6 missed of 4795 | 99.87%, 6 missed of 4796 |
| Lines | 98.71%, 1274 missed of 98492 | 98.71%, 1274 missed of 98509 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_copies.c` | 80 of 726 | 78 of 726 |
| `src/antic/lexer.c` | 32 of 705 | 21 of 705 |
| `src/antic/sema_call.c` | 149 of 1598 | 152 of 1616 |

The "before" run had 8383 tests in 1203 s. The run after the batch, at
`98c44fea`, had 8395 in 1223 s. Every test passed in each. The two fixes
add 18 branches to `sema_call.c`, and 3 of them stay unreached: a member
after the test of a constant is a function with its type, and a bound
function is never called where it is checked. Every side of
`sema_copies.c` and `lexer.c` that the after run misses, and those 3 of
`sema_call.c`, has a row in the note, under the heading of its file or
under "The sides of batch 3aw".

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. The parts took 414 s to
  418 s, 298 s to 308 s and 446 s to 467 s. `CLAUDE.md` holds each.
  `fmt_canonical` failed before the list commit on four new programs,
  which passed alone once they stood in their canonical form. Every
  other test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8405. `asan` took 269 s, 191 s, 283 s, 173 s, 112 s
  and 711 s, and `ubsan` 269 s, 192 s, 284 s, 175 s, 123 s and 764 s. In
  the sixth part of `ubsan`, `matrix_values_text_literals_dev` reached
  its limit of 60 s while `fseventsd` of the Mac ran beside it. It took
  15 s alone, and the sixth part ran again in 754 s with every test
  passing. Every other test passed.
- The host runs 8407 tests and each sanitizer tree 8405.
  `tests/known-failures.txt` holds the two lines of T136 and T138.

## Pinned outputs

No fix moves an output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None new. The question of the step "Answers of 2026-11-23" on the
spelling of the base after `inherits` on a page of `anti doc` waits for
him as before.
