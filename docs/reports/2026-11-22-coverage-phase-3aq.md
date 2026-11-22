# Batch 3aq of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `ea511a22` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the owning values
of lowering in `src/antic/lower_owning.c`, which missed 37 of its 346
branches, and the declarations of the checker in
`src/antic/sema_declare.c`, which missed 36 of its 790. It read them
against the specifications, `docs/decisions.md` and the DESIGN comments
of the two files. Each side ran first through the antic of the coverage
tree alone, with a profile of its own. The batch found three defects,
T393, T394 and T395, and fixed all three. The next free id is T396.

## Commits

- `7defe961` tests sixteen sides of the two files.
- `12528a81` lists T393 and T394.
- `6183ccad` fixes T393.
- `7393a41c` lists T395, which takes the two tests of an `int` default
  from T394.
- `d4fada62` fixes T394.
- `a618470a` fixes T395.
- `6e17b0eb` tests eight more sides, which the run of coverage after the
  fixes showed.
- `9e3eaf2e` records the times and the counts in `CLAUDE.md`, the commits
  of the fixes in `docs/audit/tests-findings.md` and the sides of the
  batch in `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Findings

- T393, minor, fixed. `class Thing inherits Alias { }` for
  `type Alias = Nope;` got `` `Alias` is not a class `` after
  `` unknown type `Nope` ``, a second message of one mistake.
  `resolve_base` of `src/antic/sema_declare.c` returned on a base of
  another module of the error type alone. It now returns on an alias of
  the error type as well.
- T394, major, fixed. The name of a function as the value of an enum,
  the width of a bitfield, the N of `align(N)` or the value of a
  constant that an enum value names got no message, and the program
  compiled. `B = helper` gave `B` the number after `A`. The checker reads
  these places before the functions have their types, where the check
  of a function name gives the error type without a message.
  `sema_refuse_value_parts` of `src/antic/sema_const.c` refuses such a
  name there with `a function is not a constant expression`, the message
  of the evaluator, as the walk of `sema_refuse_runtime_parts` refuses a
  call.
- T395, severe, fixed. The default of a field that names a function was
  never checked against the type of the field. `wrong`, a
  `fn(str) -> int`, as the default of a `fn(int) -> int` field compiled,
  and the program called it with an `int`. `helper` as the default of an
  `int` field failed the verification of the IR, and `Maker.make` there
  crashed antic in `lower_address`. `check_field_defaults` leaves such a
  default to `check_function_defaults`, which `sema_declare_all_members`
  runs once the functions and the members have their types.

## Choices

- No fix adds a message or an option. No DESIGN comment changed.
- T394 and T395 have one cause in two kinds of place. A place of a
  number takes no function, and the message of the evaluator names it
  there. A default of a field takes a function where the field has a
  function type, so it is checked once the functions have their types.
  An `int` field then gets `` expected `int`, found `fn() -> int` ``, the
  message of `const N: int = helper;`.
- The first fix of T394 refused a function in the defaults of fields as
  well. 45 tests failed on defaults of fields of function type, so that
  fix was not committed, and T395 took the defaults.
- The fix of T395 makes `Maker.make` of its test public. The message is
  then the one of the type, and not that of a private member.
- The tests of the sides are four programs and twenty refusals of the
  matrix, beside the eight refusals of the three findings.
  `modules_provides_a_class_with_an_open_function` holds both messages it
  gets, as question 1 asks.

## Read without a find

- A library file whose import has no library file is refused on load
  with `` a.antl needs module `b` ``, so the checker never meets it.
- No width, `align(N)` or enum value was found that the check takes and
  the evaluation refuses. The check folds every operator and refuses a
  division by zero and a shift out of range first.
- `take` through a view, and a `construct` that fails in the arguments
  of the call of its base three levels deep, give the right values. The
  `destruct` of a level that takes arguments and was never built does
  not run.

## Fuzzer

`cmake -DFIRST=46601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 46601 to
46800 at the default size in 161 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.45%, 3780 missed of 57722 | 93.49%, 3765 missed of 57822 |
| Regions | 98.51%, 1301 missed | 98.51%, 1303 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1257 missed | 98.71%, 1259 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/lower_owning.c` | 37 of 346 | 31 of 346 |
| `src/antic/sema_declare.c` | 36 of 790 | 27 of 816 |

The fix of T394 adds 74 branches to `src/antic/sema_const.c`, which
missed 80 of 1086 before and 83 of 1160 after. The three new sides that
no input reaches are a part that is `NULL`, a place read after the
functions have their types and the side of `fail_const` that never
comes. Every side the batch left in the two files is a guard.
`docs/notes/unreached-guards.md` names each, in the section of the batch
or in those of the two files. The "before" run had 8162 tests in
1089 s, and the run after the fixes 8194 in 1108 s. The run after the
last tests had 8202 in 1112 s. Every test passed in each.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. `CLAUDE.md` holds the
  times. The first fix of T394 failed 45 tests and was not committed.
  Before the fix of T395, `emit_identity` failed on the twelve texts the
  fix moves, and it passed alone once the manifest stood written. Every
  other run passed every test.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8212. Every test passed.
- The host runs 8214 tests and each sanitizer tree 8212.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

The fix of T395 gives the `own fn` default of the programs
`effects_edges` and `snapshots` its conversion, so each builds the pair
in a slot and copies it into the field, with the same value as before.
The twelve digests of the two programs in
`tests/emit-identity/programs.sha256` move with it, and no other line.
`tests/link-identity/return42.macos-arm64.sha256` stands as it was.

## Questions for Eddie

1. `provides Shape as Half;` for a `class Half` not marked `abstract`
   that declares an open function gets two messages:
   `` `Half` has the open function `more` and must be `abstract` `` and
   `` `Half` is abstract, and a `provides` line names a complete class ``.
   Is the second one a second message of one mistake? The recommendation
   is to keep both, since the class is not complete whatever its mark
   says, and `sema_check_provides` tests the open functions on purpose.

T385, T387, the question on `anti fmt` and the lines of a table, and
question 1 of batch 3ao on `compare_swap` of an `own` field wait for him
as before.
