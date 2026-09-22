# The root's copy, the qualified bodies and two settled entries

The two answers to `docs/reports/2026-09-22-concrete-signature.md` are done, and so is the
later request to settle two entries. The development Mac passes 616 of 616 tests, and the
ASan and UBSan builds pass 615 each, without `no_paths`. Each commit that changed code ran
the full suite first, and the push ran both sanitizer suites. The VMs did not run.

## What was done

1. Answer 1, `3a930da`. The specification, the syntax overview and the decisions give the
   root `copy(self, to: *Object)`. It fills `to`, which `dup` has already allocated, and a
   replacement fills fields and never allocates. No code changed.
2. The request to settle, `2d21d3d`. The entries on a `may fail` function value and on the
   flag byte of the library file lost their tags. They stood at lines 493 and 494, one
   below the lines given, because answer 1 added a line above them.
   `grep -c '\[provisional\]' docs/decisions.md` gave 2 then. It gives 4 now, with the two
   entries below.
3. Answer 2, `972635b`. Two qualified bodies of one name fill two interfaces, each its own
   table. An unqualified body beside a qualified one fills every table of its name that no
   qualified body fills. Two bodies are still one name twice when both are plain, or when
   their qualifiers reach one table. A use of the name on the class skips the bodies
   qualified by an interface. When only those exist, a call, a bound function and `T.f`
   are refused with ``` `Hand` fills `draw` only for `Drawable`, `Card` and `Deck`, so the
   call is ambiguous, reach it through an interface ```. `types_body_table` in `src/types.c`
   says which tables a body fills. The checker, lowering and the header read the tables
   through it, where each had its own copy of the rule before.

Tests: `programs/qualified_bodies.anti` calls through every interface pointer and
sub-object, in release and in dev mode. `errors/qualified_bodies.anti` and
`errors/qualified_calls.anti` hold the refusals and the ambiguity message.
`modules/shared` crosses a module boundary with both forms, and a class of the program
inherits them there. `clib_classes` pins the header of an export class with both.

## Found and fixed

- `e0b17a2`. A call on a sub-object, `c.ser.f()`, called the function at index 0 of the
  module, `printf` in the test. An abstract function has no function in the IR. A
  name a sub-object promotes took the same path. This is the form the specification gives
  for reaching a qualified body. The call now reads the table of the sub-object.
- A body qualified by a base filled no table, and a call through the base trapped on the
  zero entry. It fills the primary table now.
- The check that every abstract function is filled took a body for one interface as
  filling the primary table and every other interface. `errors/concrete_signature.err`
  gains ``` `Disc` lacks `concrete fn radius` ```, whose entry was zero.
- The library file carried no qualifier. A module that imported a class with one qualified
  body built a table with an entry the class lacks. A call of a later function crashed.
  The format is version 40.
- The header gave an export class a table entry for a body of an interface.

## Found and not done

- A direct call of an abstract function, `self.super.area()` or a call through
  `Shape.area`, still reaches the function at index 0. It is the root of the sub-object
  defect, reached another way. The checker should refuse it, which needs a message.
- The header declares an inherited entry under the name of the class it shows,
  `Square_move`, while the symbol is `Shape_move`. That predates this session.

## Provisional decisions for review

- A body qualified by another class has the symbol `T.Q.f`, and `T_Q_f` in C.
- A plain body beside one qualified by a base holds no entry of the primary table. `c.f()`
  on the class calls it directly, through a pointer too.

## Questions

- A single qualified body is now ambiguous as well, by the words "only qualified bodies",
  and the message names one interface. Before, the call went straight to that body and
  passed over a replacement in a class below. Keep the refusal, or reach the one body
  through the table of its sub-object?
- An interface table takes a qualified body from any level of the chain before a nearer
  plain one, as it did before this session. A plain replacement in a class below therefore
  does not reach a table that a base filled with a qualified body. Is that intended?
