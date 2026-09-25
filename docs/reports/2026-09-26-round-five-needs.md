# What the stopped lanes of round five need

The step puts into main the four things the lanes `add5/sorted`,
`add5/hashed`, `add5/seq` and `add5/memcheck` stopped at. Items 1 and 2 are
built. Items 3 and 4 are recorded as decisions for the memcheck lane, as
Eddie chose, since `--memory-checks` exists on that lane alone.

## What was done

- `dfb1ce1`: `union` names a function after `fn` and after the `::` of a
  qualified `concrete fn`, and a member after `.` and `?.`, as `alloc` and
  `free` do. `expect_function_name` of `parser.c` takes it. An `inject` field
  keeps to `alloc` and `free`. `anti fmt` read `union` as the start of an
  item: it wrote `fn union (` and put the brace of `if a.union(b) == 15 {` on a
  line of its own. `name_unions` of `fmt.c` now reads it as a name in the same
  positions. Test: `programs/union_name.anti`, which `fmt_canonical` also
  checks.
- `1566a5e`: a struct and a class get a default `==`, and neither gets a
  default `<`. A struct compares its fields in order, each with its own `==`,
  and leaves at the first that differs. A class value compares through the
  `equals` of its chain. `lt` refusals on a struct or a class end with "A
  struct or a class has no default order and declares `operator fn lt`". The
  library format is version 72. Tests: `programs/eq_default.anti`,
  `errors/equality.anti`, and two generics of `tests/modules/hashing/` in
  both modes.
- `7bfb55b`: the two memcheck decisions, each tagged `[built by the memcheck
  lane of round five]`. "Memory checks" of the additions and the overview say
  the same.

## What failed and how it was fixed

- A module-level `operator fn eq` of one struct was taken as the `==` of every
  struct and class of its module, which gave `` expected `Name`, found
  `Point` ``. `operator_symbol` now tests the first parameter, as `sema_hook`
  did. A module still declares one free function of a name, so it still
  gives one struct its own `operator fn eq`.
- The first host suite, run while the test of item 2 was unfinished in the
  tree, failed `fmt_canonical` and `emit_identity` on that file alone.
  Neither was a defect of item 1.
- Format 72 changed four pins: `pick.antl.hex`, `scale.antl.hex`, the copy in
  `test_modules.c` and the refusal of a newer version. Only the version byte
  and the new node fields differ. `generics.err`, `constraints.err`,
  `hashing.err` and `handles.err` take the new `lt` refusal. `hashing.err`
  now reports `lt` where it reported `eq`, since `Point` has `==` now.
- `eq_default` runs in release alone. Its `str` case builds a text with
  `f"..."`, and a dev link of it needs the objects of `anti.text` and
  `anti.mem`. The default runs in dev through `hashing_modules_dev`.

## Proof

- Host: 1131 of 1131 passed at `1566a5e`, in `build/scratch/suite3.log`.
- ASan: 1130 of 1130 passed at `7bfb55b`, in `build/scratch/asan-suite.log`.
- UBSan: 1130 of 1130 passed at `7bfb55b`, in `build/scratch/ubsan-suite.log`.
  Both run without `no_paths`, as always.
- `emit_identity` gained the lines of the two new programs. No other
  program's assembly changed.
- The docs-style checker reports nothing on every file the step touched,
  `tests/CMakeLists.txt` aside, whose findings stood before the step.

## Questions for Eddie

- The `equals` of `Object` passes over `str` and slice fields, and its
  `hash` does the same. Two class values that differ only in a `str` are
  therefore `==`. The object model says `equals` compares the contents of
  `own` fields. Should the root compare the bytes of a `str` field, and hash
  them?
- The default `==` names the struct and the class alone. A tuple, a
  variant and a `?T` still have none. Should they get one?
