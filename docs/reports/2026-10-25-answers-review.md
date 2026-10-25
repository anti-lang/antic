# Step review: the answers of 2026-10-24

The step checks each of the twenty answers of "Answers of 2026-10-24" in
`docs/decisions.md` against the repository: its entry, the overview, the
object model, the tests that hold it and the code. For Q17 to Q20 it writes
programs of its own that try the edges of each rule. The probes stand in
`build/answers-review/q17/` to `q20/`, their runs in
`build/answers-review/q17*.log` to `q20*.log`, and the logs of the gates in
`build/drive/logs/answers-review/`.

## The four answers of the language

Each probe ran in dev mode, in release mode and with `--memory-checks`.

- Q17, `take(x.f)`, 19 programs. They behave as the answer says: a function
  `take` in scope and a method `b.take()` keep their names, and an element
  of an array, a field in parentheses, a narrowed `?*Holder`, the
  `destruct` of the owner, a base field holding a derived object and an
  interface field all work. So do a tuple element, `for h in &hs`, a
  synchronized and a concurrent class under `sync`, a generic function, a
  copy of a nested generic and `h.head = take(h.head)`. The copy of a `for`
  walk, a guarded field outside `sync`, a private field and a field of a
  singleton are refused with the words of `=`. One finding: T203.
- Q18, simd structs and the five constraints, 24 programs. `add`, `sub`,
  `mul`, `div` and `neg` hold on `i32`, `u32`, `i8`, `f16`, `f32` and
  `f64` lanes, above the native width and above the vector cap, through a
  named constraint, a constraint passed on by another generic and a
  function of a class. `neg` on unsigned lanes, `lt`, `eq` and a
  `HashSet<Vec4>` are refused. `+=` on a value of a type parameter is
  refused, by Q16. Dev mode stops a lane divided by zero and the least
  value divided by -1, in direct and in generic code alike, and lets the
  other operators wrap. The `[provisional]` entry on R031 records that.
  No finding.
- Q19, inference through the bases, 16 programs. One and two levels up, a
  generic base two generics deep, a base that swaps its parameters, two
  parameters that agree, a closure typed from the inferred `T`, an `own`
  and a `lent` parameter, `self` of a derived class and a constraint on
  the inferred `T` all work. Explicit arguments that disagree, two
  parameters that disagree, a class value for a parameter of a value and a
  slice of derived pointers are refused, each as without inference. No
  finding.
- Q20, the bare name of a class constant, 18 programs. A default of a
  parameter called from outside, an array type of a parameter, a `switch`
  arm, a local declared after a use, a block, a `for` variable, a closure
  parameter, a tuple and a `let ... else` binding, `construct`,
  `destruct`, an `operator fn` of the body, a `concrete fn`, a nested
  struct and class, a static function of a generic class and a function
  with a type parameter of its own all work. The bare name is refused in
  an operator at module level, a constant at module level and the class
  around a nested class. Findings: T204, and T205 beside it.

## The sixteen other answers

- Q1. `CLAUDE.md` names both answers under the feature freeze, item 14
  follows Q3, the presets follow Q8 and Q11, and the tests list
  `tests/asm/` and `tests/clib/headers/` by Q9.
- Q2. The overview, the object model and the additions hold Q17 to Q20.
  The ten answers of 2026-10-18 stand in the overview and in at least one
  specification. One line was stale: the keyword section of the
  overview still listed `take` as not built. `11d820fa` fixes it.
- Q3 and Q4. The entry on `anti.log` reads the file at the first line
  written. `set_name`, `set_level` and `set_format` take the lock, and
  `std_log_setters` holds them.
- Q5, Q7, Q14 and Q15 have their message tests:
  `anti_build_message_repositories_older_then_second`, the three
  `*_message_work_in_the_layout`, the two of the version of clang and
  `anti_build_message_repository_of_a_drive_letter`.
- Q6. `native_frames` and `native_hosts` hold the flags.
- Q8 and Q11. `presets` reads the `-g`, `ASAN_OPTIONS` and `ANTIC_FAULTS`
  of every preset, `tools/faults.cmake` refuses `ANTIC_FAULTS` in a Release
  build, `deps_dir` reads the cache paths and `unit_link` the line of a
  Windows library.
- Q9. `tests/dump/` holds dumps alone, and every file of `tests/unit/` is
  a test or serves one. The two programs of the aggregates replace the
  one.
- Q10, Q12, Q13 and Q16 stand as the answers give them: no `patch_bytes`
  in `anti.text`, the DESIGN comment in `tools/coverage.cmake`, the two
  refusals of `<<%` in a copy, and the object model and the overview on
  compound assignments.

## A point for Eddie

The step `points` built Q9 with a new directory, `tests/asm/`, and added
it to the list of `tests/runners/run_repo_layout.cmake` in `10fdfa4f`.
"Repository layout" in `CLAUDE.md` gives a new directory under `tests/`
and an addition to that list to Eddie's decision. Q9 names the two
directories and no new name, and the `[provisional]` entry of the step
records the choice. This review changes nothing there.

## Findings

None closed. Three recorded, each with its tests in
`tests/known-failures.txt`:

- T203, major, `5c4cf3a3`. A statement that compares the value of
  `take(x.f)` with `none`, or passes it to a parameter that is not `own`,
  drops the one owner of the object, and antic accepts it. The object
  leaks. `sema_check_take` refuses `take` as a whole statement alone. Two
  refusals of the matrix. `alloc` passed to such a parameter leaks the
  same way and has no rule that refuses it.
- T204, major, `d199fede`. The value of an enum nested in a class cannot
  name a constant of the class by its bare name. `sema_check` declares the
  values of the enums before the constants of the class bodies.
- T205, severe, `92df8ba7`. An enum value written as `Table.ROWS` reads 0,
  and the value after it 1, with no message. Same order of the passes: the
  constant is not folded yet, and `declare_enum_values` keeps the number
  it counted. One fix of the order would close T204 and T205.

All three are left for the steps of their areas, ownership and classes.
`tests/known-failures.txt` held 7 entries before and holds 15 after.

The probes met two things that are no finding. The refusal of a
constraint repeats one message at each use of the generic in a
statement, three times for `HashSet<Vec4>.new()` and `s.add(...)`. A
constant and a field of one name, `const KEY` beside `pub KEY: int`, are
accepted, and `Vault.KEY` and `v.KEY` name each.

## `[provisional]` entries

None added.

## Gates

- Zero warnings of our own code in the `host`, `asan` and `ubsan`
  builds: `build2.log`, `asan-build.log` and `ubsan-build.log`.
- Host, `ctest --preset host -j14` in three parts, `host-1.log` to
  `host-3.log`: 6752 tests. All pass apart from the 2 skips of
  `mimalloc_environment`, and the 13 listed tests fail as expected: the
  three of T201, the two of T202 and the eight of T203 to T205. The parts
  took 370 s, 259 s and 258 s.
- ASan, six parts, `asan-*.log`: 6750 tests, the same 2 skips and the same
  13 expected failures. 241 s, 189 s, 250 s, 185 s, 440 s and 352 s.
- UBSan, six parts, `ubsan-*.log`: 6750 tests, the same. 249 s, 190 s,
  248 s, 194 s, 459 s and 355 s.
- `emit_identity` and `link_identity_<target>` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## Not run on the target

The step changes no code. The eight new tests would show the same
defects on Linux and Windows, where the checker is the same.

## State before the commit of the report

```text
$ git log --oneline -3
e6b46af4 Count the suite of the step review of the answers of 2026-10-24
92df8ba7 Record T205, an enum value of a class constant that reads 0
d199fede Record T204, a class constant by its bare name in a nested enum
$ git status --short
?? docs/reports/2026-10-25-answers-review.md
$ git rev-parse HEAD origin/main
e6b46af45647adf1acb79a84bdc16e25bcc79ba5
e6b46af45647adf1acb79a84bdc16e25bcc79ba5
```

The commit of this report follows, then its push.
