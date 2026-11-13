# Batch 3f of the third phase of coverage

This step answers the question of batch 3e on the names of the locks of
C4, as T252. It then sorts the regions that `./c coverage` after batch
3e showed unreached in four files: `src/antic/lower_stmt.c`,
`src/antic/lower_expr.c`, `src/antic/lower_desc.c` and
`src/antic/platform.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each candidate
first ran through the antic of the coverage tree alone. Eddie's
direction is the Mac alone, so the VMs were not touched, and the paths
of `platform.c` for the other hosts are compiled out of the
measurement. The batch adds two matrix programs, for T253 and T254. The
other cases extend four matrix programs, one modules test and two unit
groups, and three refusals take T252.

## Commits

- `4a7096d1` lists T252, `6004cb18` fixes it.
- `0bc0b7ce` tests `lower_stmt.c`.
- `a7d684f0` tests `lower_expr.c`.
- `8505d3df` lists T253 and T254.
- `d4a2fd82` tests `platform.c`.
- `7741a381` names the guards in `docs/notes/unreached-guards.md`.
- `1295a902` keeps the module of a nested type in the name of a lock,
  which the measurement found unreached in the fix of T252.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fix of T252 in `docs/audit/tests-findings.md`.

## Findings

- T252, minor, fixed. R136 of "Rules of 2026-10-13" settled the
  question of batch 3e: a message names a type as the source spells it.
  The message of `lock-order` named the lock of `pairs.Pair` as `Pair.a`
  beside a `Pair` of the program. `lock_text` of `sema_lock.c` now
  writes the module as the module that compiles names it: the alias of
  its import, or the last segment of its path. A type that a direct
  import lists stays bare, and a type of a module that the program does
  not import takes the whole path. A lock of the module stays bare. A
  provisional line beside the entries of C4 records the reading, and no
  entry of C4 or DESIGN comment said otherwise. The five tests of a
  cycle through a library changed, and three refusals take the alias,
  the direct import and the module not imported. The last message runs
  past the 160 bytes of a diagnostic and is cut there, as every message
  is. C5 names no lock, and its message keeps the form of its entry.
- T253, severe, open. `ops.f(key(1))` through a function value in a
  field, to a parameter that is not `own`, never tears the call result
  down, and a field it owns leaks. The provisional entry on fresh
  arguments leaves the arguments of a call through a function value
  alone. Eddie's decision of 2026-09-28 has the caller tear down a call
  result passed to a parameter that is not `own`.
- T254, severe, open. After `let call = take;`, where `take` has an
  `own` parameter, `call(key(2))` tears the result down twice. The
  caller reads the marks of the local, which has none, and the callee
  tears its `own` parameter down. A value that owns a field stopped a
  build of `--memory-checks` with a SEGV. `own` belongs to the function
  and not to its type, so a function value carries no mark.

The tests of T253 and T254 print one teardown per key. "Ownership at a
call" keeps one owner for a value that needs a teardown. Both stay in
the known failures, since the fix needs the choice below.

## lower_stmt.c

`errors_derived_construct_fails_after_its_base` takes a `Drone` whose
`construct` may fail and calls the `construct` of a base that cannot
fail. Lowering then marks the base built after a call without a
handler, and the failure after it gives back what the base took.
`ownership_received_value_in_each_place` destroys a local of another
type beside a value that `recv` gave, and the value stays to its block.
The other missed lines are guards: a `yield` the checker refuses, an
`else` block that cannot fall through, a place that `lower_place` always
gives, and statements that always hold their expression.

## lower_expr.c

`classes_constant_by_its_bare_name` reads `Limits.ORIGIN`, a constant of
a struct, outside its class. `values_add_with_carry_over_two_words`
adds the carry after an operand of another operator, `hi * 2 + f.carry`.
The client of `items` takes `items.abs` and `it.abs`, an `extern fn` of
the library, as a value. The probes found T253 and T254. A unary `-` of
an unsigned integer and `??` with a right side that returns `never` are
refused, so the regions behind them are guards.

## lower_desc.c

Every missed line is a guard. `lower_class_descriptor` builds each part
of a descriptor once, after it found no descriptor, so the lookup of a
part that the module holds already finds nothing. The rows of the note
name them with the empty table and the entry of an interface that no
accepted class leaves empty.

## platform.c

`unit_link` takes a path relative to itself and the root `//host` alone.
The group of the tool platform takes a name that is a suffix alone, a
file asked for as a directory, a link that replaces a link and a run
whose environment holds a variable whose name starts with the name it
sets. The run uses `/bin/sh`, as the cases of a report in `test_syms.c`
do. The child of `platform_run_in` between `fork` and `exec` writes no
profile, as `work_not_waited` writes none, and the note names both.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.29%, 1440 missed | 98.30%, 1434 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.51%, 1419 missed | 98.52%, 1412 missed |
| Branches | 91.66%, 4659 missed | 91.67%, 4653 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `lower_stmt.c` | 98.41%, 24 missed | 98.74%, 19 missed | 91.52% | 91.90% |
| `lower_expr.c` | 98.70%, 22 missed | 98.70%, 22 missed | 91.38% | 91.68% |
| `lower_desc.c` | 98.35%, 22 missed | 98.35%, 22 missed | 88.95% | 88.95% |
| `platform.c` | 93.72%, 26 missed | 94.20%, 24 missed | 87.65% | 91.76% |
| `sema_lock.c` | 98.89%, 10 missed | 98.93%, 10 missed | 90.83% | 90.80% |

The lines that stay unreached in the four files are guards of the note,
or paths that write no counts, and the sides of their conditions were
not chased. The value `items.abs` passes its arm in
`lower_expr_value` by another path, so that side stays unreached. The
first measurement after the batch found three lines of the fix of T252
unreached, a type nested in a class that a direct import lists, and
`1295a902` removed them. The coverage suite ran 7286 tests before and
7295 after, all passed. `./c coverage` took 980 s before and 697 s
after.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7305. Every test passed. `asan` took 231 s,
  119 s, 241 s, 173 s, 461 s and 437 s. `ubsan` took 288 s, 186 s,
  259 s, 181 s, 482 s and 467 s. An earlier run before `1295a902` passed
  as well. The new unit cases passed in both trees before their commit.

## Provisional lines

- The names of the locks of another module in the message of C4, beside
  the entries of C4 in `docs/decisions.md`.

## Questions for Eddie

- T253 and T254. A function value carries no `own` marks, by the entry
  on `own cause` under "Object model", so the caller of a call through
  one cannot know whether the callee takes a fresh argument over. Today
  a call through a field leaves the argument alone and one through a
  local tears it down, so one leaks and the other tears down twice.
  Which rule holds? One reading refuses a function with an `own`
  parameter that needs a teardown wherever a function value is
  expected. The caller then tears down every fresh argument of a call
  through a function value, which replaces the provisional entry that
  leaves them alone. The other makes `own` a mark of the function type,
  `fn(own Key) -> int`, a new form under the feature freeze.
