# Batch 3e of the third phase of coverage

This step sorts the regions that `./c coverage` at `5efbfb7f` showed
unreached in three files of the checker: `src/antic/sema_copies.c`,
`src/antic/sema_index.c` and `src/antic/sema_lock.c`. It also takes the
three paths of `src/anti/build.c` that batch 3d left. It follows the
method of `docs/reports/2026-11-12-coverage-phase-3d.md`. A library file
written by hand is an input, so `unit_modules` writes the lock facts of
a library in their place. Eddie's direction is the Mac alone, so the VMs
were not touched. The batch adds no executable: one matrix program takes
21 functions more, and the other cases are five refusals, one library
file, one unit case and three cases of the table of `anti build`.

## Commits

- `0d916352` tests `sema_index.c`.
- `e475f58e` tests `sema_lock.c`.
- `4f1758b0` tests the three paths of `build.c`.
- `b6861121` names the guards in `docs/notes/unreached-guards.md`.
- This report, with the times and the counts in `CLAUDE.md`.

## Findings

None. Every new case gave what the rule and the DESIGN comments say, so
no T-row was written, and T252 stays the next free id.

## sema_index.c

Every missed line of the file stood in a row of the note already. The
cases take the sides of the conditions that a program reaches.
`values_index_out_of_range_followed` takes constants and sums past the
bound the check follows, the length of a slice as the index of an
array, and loops whose bounds name another loop variable or a parameter.
It takes parts of a slice whose bounds the check cannot follow,
products of loop variables, of parameters and of an unknown, and a field
named `len`. It also takes loop bodies that a `try` block, `catch fatal`,
a call that never returns, `fail`, `while`, `do`, `fallthrough` or
`yield` may leave, the complement `~`, the least step and an array past
the bound. `values_index_out_of_range_of_an_array_by_the_length_of_a_slice`
is refused: `xs[s.len + 3]` on an array of three elements.

## sema_lock.c

Four refusals of the matrix take two functions that call each other
before a `send`, two copies of one generic that warn once, edges that
stand at one place and a walk of the graph that reaches one lock two
ways. The tree `lock_sides` takes the generic `ordered` and the library
`top`, which imports both sides of a cycle without `main` and takes its
locks in a closure alone. `modules_lock_order_of_a_copy_of_a_generic`
refuses the edge of the program against that of the copy, and holds a
struct of its own of the same name. `error_lock_order` takes a Mutex of a
tuple, which is no lock of the graph by the provisional entry on the
names of the locks of C4. `unit_modules` gives a client a library edge
from a lock to itself. Five rows of guards stand in the note.

## sema_copies.c

Every missed line is a guard that the checker stands before. Three rows
of the note take the lines that no row named yet, and `xf` gets a row
of its own. No case was added.

## build.c

`profile_use_unread` names a missing profile, and `runtime_level_unread`
builds against a stand-in archive whose directory of the level is a
file. It stands on every host but Windows, which makes no symbolic link
without the right to. `fault_path_dependency_named_with_a_slash` makes
both calls of `realpath` fail for a path dependency named `../one/`, the
fourteenth and the fifteenth stat. The count was found on the Mac with
the coverage tree, which reaches the line in the work directory of the
case.

## Branches left

The sides of a condition that a guard of the note holds were not
chased. Two stay in `sema_lock.c`. One is a lock held three times at
one site, which `twice` would find on its second test. The other is a
path of the graph from a lock to itself, which `find_cycles` never asks
for.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.26%, 1468 missed | 98.29%, 1440 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.49%, 1438 missed | 98.51%, 1419 missed |
| Branches | 91.53%, 4733 missed | 91.66%, 4659 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `sema_copies.c` | 96.62%, 47 missed | 96.62%, 47 missed | 88.95% | 88.95% |
| `sema_index.c` | 96.70%, 24 missed | 96.70%, 24 missed | 81.67% | 90.19% |
| `sema_lock.c` | 97.56%, 22 missed | 98.89%, 10 missed | 86.46% | 90.83% |
| `build.c` | 98.97%, 14 missed | 99.49%, 7 missed | 97.57% | 98.13% |

The lines that stay unreached in the four files are guards of the note.
The coverage suite ran 7276 tests before and 7286 after, all passed.
`./c coverage` took 977 s before and 980 s after.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7296. Every test passed. `asan` took 270 s,
  184 s, 267 s, 185 s, 454 s and 462 s. `ubsan` took 297 s, 187 s,
  263 s, 197 s, 463 s and 482 s. The new unit case passed in both trees
  before its commit as well.

## Provisional lines

None. No rule changed.

## Questions for Eddie

- A lock is named by its type and its field, `Pair.a`, by the
  provisional entry on the names of the locks of C4. In
  `modules_lock_order_of_a_copy_of_a_generic` the program declares a
  `Pair` of its own beside `pairs.Pair`, and the message of the cycle
  says `Pair.a` for the lock of the library. The graph keeps the two
  apart. Should a message name a lock of another module with its module,
  `pairs.Pair.a`?
