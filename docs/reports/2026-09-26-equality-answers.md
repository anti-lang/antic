# Equality by kind, for every value type, and shared operator names

The step builds Eddie's three answers to the questions of
`docs/reports/2026-09-26-round-five-needs.md`. Each answer is recorded in
`docs/decisions.md` and `docs/anti-language-additions.md`. Status: BLOCKED on
one test, "a Map with a tuple key". `Map` is in `add5/hashed` and not yet in
main.

## What was done

- `3e0cae1`: the default `==` and hash take each field by its kind. A `str`
  goes by its bytes, an `own []T` element by element, and a plain slice or a
  pointer by address. The runtime's `equals` and `hash` of `Object` follow the
  same rule. They passed over every `str` and slice field before, so two
  class values that differed in a `str` alone were equal. `own` stands on a
  class field alone, so the `own` rule lives in the runtime. Test:
  `programs/eq_kinds.anti`.
- `7db7ba9`: tuples, variants and `?T` get a default `==` when every part has
  one. The default hash of each existed already. Tests:
  `programs/eq_parts.anti`, which also takes tuples through
  `K: eq + hash`, then `errors/equality.anti` and a tuple generic in
  `tests/modules/hashing/`.
- `db83453`: module-level `operator fn` items may share a name when their
  first types differ. Each one's symbol becomes `eq:Point`, which the IR, the
  library file and generic copies carry, so the format stays at 72. A module
  with one such function keeps the plain name, and no existing symbol
  changed. Tests: `programs/operator_overloads.anti`, `errors/overloads.anti`,
  and three `operator fn eq` of a library reached from another module in both
  modes.

## What failed and how it was fixed

- A struct with a plain slice field now has `==`, so `errors/equality.anti`
  refuses a struct with an array field instead.
- The runtime change moves the digest of `return42` for macos-arm64, which
  `link_identity_macos-arm64` pins.
- `==` on two `?T` values was refused as a read without a test.
  `binary_operands` now lets `==` and `!=` read two of them whole.
  `errors/optional_values.anti` and a unit test of variants had pinned the
  old refusals, and both now expect the new rule.
- The docs-style checker joins the trailing comments of neighbouring fields
  into one sentence, so the comment of `overloaded` in `ast.h` sits below
  `trace`.

## Blocked

The test of a `Map` with a tuple key needs `Map`, which only `add5/hashed`
has. Tuple keys pass through `K: eq + hash` in main, the two hooks a map
asks of its key. Whether `Map<(int, int), V>` itself runs is untested. The test
belongs to that lane after its rebase, or to main once the lane is merged.

## Proof

- Host: 1138 of 1138 passed at `db83453`, in `build/scratch/suite6.log`.
- ASan: 1137 of 1137 passed at `db83453`, in `build/scratch/asan-suite.log`.
- UBSan: 1137 of 1137 passed at `db83453`, in `build/scratch/ubsan-suite.log`.
  Both run without `no_paths`, as always.
- The docs-style checker reports nothing on every file the step touched,
  `tests/CMakeLists.txt` aside, whose findings stood before.

## Questions for Eddie

- A shared `operator fn` is reached by its operator and as a method. A bare
  call `eq(a, b)` reports `` unknown name `eq` ``. Should it pick by the type
  of the first argument instead?
- The runtime still passes over an inline struct, class, array or union
  field of a class, as before. The compiler's default of a struct compares
  those parts. Should `Object.equals` walk them as well?
