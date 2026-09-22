# The answers on the header of an export class

The two answers to `docs/reports/2026-09-22-qualified-answers.md` are done. The
development Mac passes 623 of 623 tests, and the ASan and UBSan builds pass 622 each,
without `no_paths`. The commit that changed code ran the full suite first, and the push
ran both sanitizer suites. The VMs did not run.

## What was done

1. Answer 1, `91ab472`. The entry that gives an abstract entry no prototype and keeps its
   wrapper through the table lost its tag. `grep -c '\[provisional\]' docs/decisions.md`
   gives 2.
2. Answer 2, `fc46346`. The header declares each function once, in the section of the
   class that declares it. The sections of `Square`, `Circle` and `Tile` no longer repeat
   `Shape_move`, and their wrappers such as `anti_Square_move` still call through their
   own table. The entry in the decisions says so. `tests/dump/canvas.h` pins the header,
   and `clib_classes` calls the inherited move of `Tile` from C, compiled as C11 and as
   C++17 and linked.

## Found and not done

Nothing.

## Questions

None.
