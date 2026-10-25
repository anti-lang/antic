# Step language: Q18, Q19 and Q20 of the answers of 2026-10-24

The step builds the three answers of the language that follow `take`,
each with its tests first. Logs stand in `build/drive/logs/language-*.log`.

## What changed

- `95baa4de`, Q18. A simd struct meets `add`, `sub`, `mul` and `div` on
  lanes of numbers and `neg` on signed or float lanes, through
  `sema_simd_meets` of `sema_simd.c`, which `meets_hook` asks first. A
  mask of `bool` lanes meets none. `lt`, `eq` and with them `Number` stay
  unmet, and the refusal now says that a comparison of a simd struct gives
  a mask, in place of advising an `operator fn lt` that a simd struct
  cannot declare. Tests: `matrix_types_simd_struct_meets_the_arithmetic_constraints`
  in the three modes, and the refusals of `lt`, `Number`, `eq`, `neg` on
  `u32` lanes and `add` on `bool` lanes.
- `a10751ea`, Q19. `unify` of `sema_generic.c` takes the first class up
  the `inherits` chain of a class argument that is a copy of the generic
  the parameter names. Tests: `matrix_types_generic_inferred_through_the_bases_of_an_argument`
  in the three modes, one and two levels up, a generic base, a tuple, a
  `?*Source<T>` parameter, a function with a type parameter of its own
  and `self` in a generic body. The refusals: a chain that holds no copy,
  and a class that names the generic in an `implements` line.
  `modules_generics` takes the inference and a simd struct under `add`
  across a module boundary.
- `d487b324`, Q20. `nested_find` of `sema.c` reads the constants of each
  class from `within` outward, before the module scope. Tests:
  `matrix_classes_constant_by_its_bare_name` in the three modes, in a
  constant, a field type and default, a nested struct and class, every
  kind of function and a generic class whose constant reads `size_of(T)`,
  with a local, a parameter and a module constant of the same name. The
  refusals: the bare name outside the class and in a derived class.
  `classes_constant_through_self` keeps `self.K` refused.
  `modules_generics` takes the bare name in a generic class of a library.
- Each commit changes `docs/anti-syntax-overview.md`, whose three "Not
  built yet" items of Q18 to Q20 are gone, and the guide. Chapter 10 gains
  the simd row, the inference through bases and a pitfall. Chapter 9
  gains the bare name and a pitfall. `docs/anti-object-model.md` held Q19
  and Q20 as built rules already, and holds no section on simd structs.
  The notes `simd.md`, `generics.md` and `nested-types.md` follow.
- `f52d87f0`. `CLAUDE.md` counts the suite.

## Findings

None closed. One recorded:

- T202, `cebf0ada`. A constant of a generic class of a library cannot be
  named outside the class through a copy, `lib.Gen<u8>.M`. The refusal
  `` `Gen<byte>` has no function `M` `` carries no position.
  `carried_value` of `antl_write.c` writes no constant of a generic into
  the library file, by its DESIGN comment. `modules_generic_constant_dev`
  and `_release` show it. It belongs to the library file and is left for
  the step of that area.

`tests/known-failures.txt` held 5 entries before and holds 7 after, the
two new ones of T202.

## `[provisional]` entries

None added. The step builds the three entries of the record on Q18, Q19
and Q20 as they stand.

## Gates

- Zero warnings in the `host`, `asan` and `ubsan` builds,
  `language-build3.log`, `language-asan-build.log` and
  `language-ubsan-build.log`.
- Host: 6732 tests, 6725 passed, 2 skipped, the five of T201 and T202
  failed as listed. `language-host3-1.log` to `-3.log`.
- ASan: 6730 tests, all passed apart from the same 2 skips, the five
  listed ones failing as expected, `language-asan-*.log` in six parts.
- UBSan: 6730 tests, the same, `language-ubsan-*.log` in five parts.
- `emit_identity` and `link_identity_<target>` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## Not run on the target

Each change is in the checker alone. `matrix_types_simd_struct_meets_the_arithmetic_constraints`
on Linux and Windows would show the lanes of a copy at the native width
of those targets.

## State before the commit of the report

```text
$ git log --oneline -3
f52d87f0 Count the suite of the step language of the answers of 2026-10-24
d487b324 Reach a constant of a class body by its bare name inside the class
cebf0ada Record T202, a constant of a library generic named outside
$ git status --short
$ git rev-parse HEAD origin/main
f52d87f0ea594d78a7387483f628051c7d343d5d
f52d87f0ea594d78a7387483f628051c7d343d5d
```

The commit of this report follows, then its push.
