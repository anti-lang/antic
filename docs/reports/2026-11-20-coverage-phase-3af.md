# Batch 3af of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it found their defects in code whose output a reader reads
again, so this batch read a pair that no batch had read: the reflection
runtime of `src/rt/reflect.c` and `src/rt/call.c` against the records of
the descriptors that `src/antic/lower_desc.c` writes and the trampolines
of `src/antic/whole_tables.c`. `src/rt/reflect.c` missed 20 of its 130
branches. Eddie's direction is the Mac alone, so the VMs were not
touched. The batch found four defects, T346 to T349, and fixed them all.
The next free id is T350.

## Commits

- `f526ec61` lists T346 to T348, and `89b7bd3a` T349.
- `a047fb0b` fixes T349, `da9c0637` T346, `b7956460` T347 and `d9b6820b`
  T348.
- `5958ced0` tests `get` and `set` over a field of each width and an
  index below zero.
- The docs commit records the times in `CLAUDE.md`, the commits of the
  fixes in `docs/audit/tests-findings.md` and the guards of the batch in
  `docs/notes/unreached-guards.md`.

## Findings

- T346, major, fixed. `reflect.call` refused a public function whose
  parameter or result is a `c_long`, a `c_ulong`, a `c_wchar` or an enum
  with `NOT_CARRIED`. A `Value` carries each, as `reflect.get` gives a
  field of the type.
- T347, severe, fixed. The record of a public function that may fail
  held the signature of its table entry. A function with a result took
  one argument more than it declares. One without a result gave its
  error back as a `Ptr` value, so the failure was lost and the error
  leaked. The function list counted the out pointer as a parameter.
- T348, severe, fixed. `reflect.set` of an `own` pointer field and
  `reflect.call` of a function with an `own` parameter gave the object a
  second owner, and the program freed it twice.
- T349, severe, fixed. A parameter and a result of an enum over `i8` or
  `i16` were `zeroext`, since `param_ext` of `src/antic/lower.c` took the
  signedness of the enum. C extends the `int16_t` of the header by its
  sign, so `is_low(Level_Low)` of an export function gave 0. The fix of
  T346 showed it, since a trampoline extends by the sign of the integer.

## Choices

- The fix of T346 keeps the structural hash of an interface as it was.
  That hash reads the same spelling of a signature. Naming the four
  types there would change the chain of every interface with such a
  function. A host would then refuse a plugin built before the fix.
- T347 and T348 take the gap procedure. Each gets one `[provisional]`
  line under "Object model" in `docs/decisions.md`. A function that may
  fail and one with an `own` parameter have no signature, and `set` of
  an `own` field fails, each with `NOT_CARRIED`, the code and the text
  of the refusal of an `own fn` field. No new message was needed.

## Pinned outputs

- `tests/emit-identity/programs.sha256` was written again by each of the
  fixes of T346, T347 and T348. The function lists of `anti.text.Builder`
  and of `anti.lang` hold functions whose signature changed, so the
  numbers of the constants of the programs that import them moved.
- The fix of T348 changes the runtime, so the digest of
  `tests/link-identity/return42.macos-arm64.sha256` is rewritten.
- The excerpt of `docs/site/developers-guide/15-hooks.md` names the
  constant of the name of `draw` as `@traced.58`, since `check`, which
  may fail, lost its signature.

## Read without a find

- `reflect.get` and `reflect.set` over every integer width, the C types,
  a function field and an index out of range. `reflect.new` of a class
  with an `inject` field fills the field.
- The escaping of names and texts in the LLVM text and of the file names
  of `-g`. The canonical NaN of the result of an export function.
- The module path of `src/antic/modpath.c` and the keys of the caches of
  `anti build`.

## Fuzzer

`cmake -DFIRST=44401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 44401 to
44600 at the default size in 151 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.38%, 3753 missed of 56729 | 93.39%, 3752 missed of 56765 |
| Regions | 98.52%, 1266 missed | 98.52%, 1269 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1230 missed | 98.72%, 1232 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/rt/reflect.c` | 20 | 11 |
| `src/antic/lower_desc.c` | 59 | 65 |

The fixes add 36 branches. The sides of them that the suite misses stand
in the section of the batch in `docs/notes/unreached-guards.md`, beside
the guards of `src/rt/reflect.c`. `src/rt/mem.c`,
`src/rt/platform_posix.c` and `src/rt/trace.c` moved by one or two, as
the threads and the clock of the suite take other paths from run to run.
The coverage run had 7935 tests, all passed, and `./c coverage` took
1097 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed, apart
  from the two runs that `CLAUDE.md` names. There `emit_identity` and
  `guide_examples` read the outputs before they were written again.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7945. Every test passed.
- The host runs 7947 tests and each sanitizer tree 7945.
  `tests/known-failures.txt` holds the two lines of `host=windows`, the
  line of T316 and the six lines of T338 and T341 to T345. The step that
  builds Eddie's answers of 2026-11-19 takes those seven.

## Questions for Eddie

- T347: should `reflect.call` of a function that may fail forward the
  error of the function, instead of refusing it with `NOT_CARRIED`? That
  needs a form of the trampoline that passes the out pointer and reads
  the error.
- T348: should `reflect.set` of an `own` field take the value over, and
  free the object the field held, where it now refuses? A `Value` would
  then need a way to give its pointer away.
- The structural hash of an interface still reads a function that takes
  a `c_long`, a `c_ulong`, a `c_wchar` or an enum by its count of
  parameters alone. Should the hash name those types, at the price of the
  chain of every such interface?

The [provisional] entries of T347 and T348 in `docs/decisions.md` await
review. No finding of the batch overlaps the areas that the step of the
answers of 2026-11-19 takes.
