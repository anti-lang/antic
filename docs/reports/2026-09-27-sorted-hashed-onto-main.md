# The sorted and hashed lanes of round five onto main

The step brings `add5/sorted` and then `add5/hashed` onto `main`. For each
lane it picks the commits after `d8865cd`, the commit that
`build/drive/add5-status/base-<lane>` names, in order. It then adds the test
that waited for the hashed lane. Both branches and the worktree of the hashed
lane stay. Status: DONE.

## What was done

- `cfbf011..0bb15bd`, the seven commits of `add5/sorted`. There were conflicts
  in `tests/run_anti_check.cmake` and `tests/run_checks.cmake`. The dev checks
  of `List<T>` and `Grid<T>` from the seq lane stand beside the check of
  `sorted_changed`. The front end and the formatting of `src/std/` count 31
  files: the 30 of `main` and `sorted.anti`.
- `4119775..01b8204`, the ten commits of `add5/hashed`. There were conflicts in
  `tests/CMakeLists.txt`, where the dev tests of the sorted collections stand
  before those of the maps and the sets, and in the file count, which reads 33
  with `map.anti` and `set.anti`.
- `14e3749`: `std/map_tuple.anti` takes `Map<(int, int), Owned>` through set,
  add, get, contains, remove, update, `dup`, `==` and a churn of twenty
  thousand sets and removals with `remove_all`. Every case ends with no block
  alive and no room held. The 287 entries left after the churn agree with a
  model of the same sequence in Python. The test runs in release and in dev
  mode.

No generated or pinned file was in conflict. `programs.sha256` and the pinned
library files needed no update, since every new test lies in `tests/std/` or
`tests/checks/`. `docs/decisions-sorted.md` and `docs/decisions-hashed.md` stay
as the lanes wrote them, for the reconcile. The lanes' own reports stand in
`docs/reports/` beside this one.

## Proof

- Host: 1195 of 1195 passed at `14e3749`.
- ASan through `ctest --preset asan`, with leak detection: 1194 of 1194.
- UBSan through `ctest --preset ubsan`: 1194 of 1194.
- The docs-style checker reports nothing on every `.md` file of both lanes and
  on this report.
- `add5/sorted` still points at `e450fc7` and `add5/hashed` at `bea661e`.

## Questions

- `CLAUDE.md` and `docs/anti-syntax-overview.md` do not yet name the sorted
  and hashed collections as built. That was left for the reconcile, with the
  decision files.
