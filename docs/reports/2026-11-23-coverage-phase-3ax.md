# Batch 3ax of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `67d0c20e` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the writer of the
library file in `src/antic/antl_write.c`, which missed 28 of its 530
branches, and the checker's part of hashing and of the default `==` in
`src/antic/sema_hash.c`, which missed 27 of its 366. It read them against
"Library file" in `docs/tooling.md`, "Hashing and order" in
`docs/anti-language-additions.md`, the entries of the default `==` and
hash under "Generics and collections" in `docs/decisions.md` and the
DESIGN comments of both files. It probed each side with the antic of the
host tree and of the coverage tree. The batch found T421 to T427. It fixed
T421, T422, T423, T425 and T426, closed T424 as no defect and left T427
open. The next free id is T428. The batch before found two, and this one
found six, so the phase goes on.

## Findings

- T421, minor, fixed. A `*Tally` of a concurrent class without a hash of
  its own met the constraint `hash`, and the copy was refused at
  `x.hash()` in the body of the generic. `meets_hook` of
  `src/antic/sema_generic.c` now asks of what the pointer points at, as
  `sema_hash_call` does, and the use is refused where it stands.
- T422, minor, fixed. The library file stored the `///` text of a private
  function of a generic class and of the functions and constants of a
  private type. "Library file" in `docs/tooling.md` says such text is
  never stored. `put_type` leaves it out now.
- T423, major, fixed. `walk` of `sema_hash.c` walked the elements of a
  slice part, which the default hash takes as its view. A generic struct
  with a field of `[]T` then asked `T` for `hash`. A dropped hash of a
  struct with a slice of hashed structs got no warning `unused-result`.
- T424, no defect. `==` on two `?[]int` is refused by the provisional
  entry on the parts of a tuple and the value of a `?T`, which
  `errors/equality.anti` holds. The first fix met that test and was
  dropped. The program of two `?[]int` left the suite, and the program of
  a struct with a `?[]int` field became its refusal.
- T425, major, fixed. A class with an `own` slice of unions, or of structs
  that hold one, got the default `==`, which passes over every union.
  `sema_class_gap` now reads the element of an `own` slice.
- T426, major, fixed. A field default that names a function of another
  module through a direct import was taken, while `text.equal` is refused.
  The library file carried no default for it. It now gets the same
  refusal, `a function is not a constant expression`.
- T427, major, open. A field default that names an `extern fn` of the
  module or a function of the body of the class reaches no client.
  `default_fn` of `antl_write.c` takes a name of a function of the module
  alone. Question 1 asks how to close it.

## Commits

- `01239961` lists T421 to T426, and `778877b9` lists T427.
- `5dea5f6b`, `32f46956`, `aeb2d07a`, `5d6c790d` and `39f06229` fix
  T421, T422, T423, T425 and T426. `97847a44` closes T424.
- `9199855f` tests the sides of the two files.
- `b7d64400` records the times, the counts, the hashes of the fixes and
  the section "The sides of batch 3ax" of `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Choices

- T422 keeps the text of a private field, of a `construct` and a
  `destruct` whatever their level and of each value of an enum. The C
  header that `anti bind --header` writes from a library file prints
  them, as the header of `--lib` prints them from the source.
  `anti_bind_header` failed on a first fix that dropped the text of a
  `construct`. A provisional entry under "Compiler behaviour" records it.
- T426 refuses the default with the message that `text.equal` gets, so
  the fix adds no message. A provisional entry beside that of T115
  records it.
- The test of the type argument of T421 names no generic function. A
  refused type argument gives a message again at each use of its copy,
  for every constraint, as question 4 says.

## Fuzzer

`cmake -DFIRST=48001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 48001 to
48200 at the default size in 158 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.59%, 3731 missed of 58232 | 93.61%, 3722 missed of 58254 |
| Regions | 98.50%, 1314 missed of 87872 | 98.50%, 1316 missed of 87900 |
| Functions | 99.87%, 6 missed of 4796 | 99.87%, 6 missed of 4797 |
| Lines | 98.71%, 1274 missed of 98509 | 98.71%, 1274 missed of 98526 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/antl_write.c` | 28 of 530 | 27 of 542 |
| `src/antic/sema_hash.c` | 27 of 366 | 18 of 370 |

The "before" run had 8395 tests in 1200 s. The run after the batch had
8424 in 1135 s. Every test passed in each. Every side of the two files
that the after run misses has a row in `docs/notes/unreached-guards.md`,
apart from two. The library of T422 holds the constant of a private class
in `put_type`, line 677, since the commit of the sides. The body of a
private `construct` in `carried_member`, line 67, is missing for
`abstract fn construct(self);`, which antic takes, as question 3 says.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds each
  time. Before the fix of T422, `anti_bind_header` failed on the text of
  a `construct`, which the fix then kept. Before the first fix of T424,
  `error_equality` failed on the refusal of `?[]int`, which closed T424.
  Every other test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8434. `asan` took 258 s, 186 s, 281 s, 171 s, 113 s
  and 707 s, and `ubsan` 261 s, 182 s, 278 s, 169 s, 121 s and 723 s.
  Every test passed.
- The host runs 8436 tests and each sanitizer tree 8434.
  `tests/known-failures.txt` holds the two lines of T136 and T138 and the
  four lines of T427.

## Pinned outputs

No fix moves an output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. T427. A field default that names an `extern fn` of the module or a
   function of the body of its class reaches no client. Carrying them
   needs a new form in the defaults of a library file, the C name of an
   `extern fn` and the class of a function of a body. The DESIGN of
   `put_defaults` in `src/antic/antl_write.c` names a function of the
   module alone. The other way is to refuse both, as T426 refuses a
   function of another module. Which do you want?
2. T422. "Library file" in `docs/tooling.md` says no `///` text of a
   private item is stored. The file still stores that of a private field
   of a class, since both C headers print it. Should both headers and the
   file drop it?
3. `abstract fn construct(self);` and `abstract fn destruct(self);` in an
   abstract class pass the checker, and no rule names them. Should they be
   refused?
4. A refused type argument gives a message again at each use of its copy,
   at `Keyed<P>` and then at `first(k)`, as `P` lacks `lt` for both. Is
   that one mistake?

The step "Answers of 2026-11-23" asked how a page of `anti doc` spells
the base after `inherits`. Eddie answered on the same day: the base
follows the spelling of the source, as every other type does. The step
after this batch builds it.
