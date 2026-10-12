# The types area of the test matrix

The step `types` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area types of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 112 sources of the area: 9 programs, each in dev,
  release and `--memory-checks`, and 103 refusals of one message each.
- `types_forms` holds every form of type in the places no test held: a
  `type` alias, a case of a variant, `size_of`, a slice literal, a type
  argument of a call, `alloc`, a `const` and a result.
- `types_packed_and_aligned` holds `packed` and `align(N)` on a struct, a
  union, a variant, a class and a generic struct. A packed struct puts a
  field of every kind at an odd offset. Each is read, written, passed and
  addressed there.
- `types_bitfields_and_unions`, `types_enums_and_variants`, `types_arrays`
  and `types_nested_and_simd` hold the other types. A fixed array of each
  kind of element is read and written with a constant index and with one
  the program computes.
- Three programs show a defect each: `types_atomic_in_packed_class`,
  `types_mutex_in_packed_class` and
  `types_variant_without_fields_as_a_field`.
- `types_modules` reads the types of a library module from another module,
  in release and dev mode.
- `abi_probe_types` compares the layouts with C. `tests/abi/probe_types.c`
  and `probe_types.anti` print the size, the alignment and the bytes of a
  field for the same types, and the two outputs are equal on the Mac.
  `run_probe.cmake` takes the name of the pair as `NAME`.
- `fmt_alias_of_function` runs `anti fmt --check` on a source that the
  formatter changes, and `run_fmt_canonical.cmake` leaves that source out.
- The note marks 63 of the 83 open cells of the area. The other 20 cannot
  exist, and each says why.
- Ten messages left `tests/diagnostics-open.txt`.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 82 lines, 22 of them new.

- T020, major. A call compiles without a message where a declaration takes
  a constant. The places are the value of an enum, the width of a
  bitfield, the N of `align(N)` and the length of the array of a field.
  The enum value is 0, and a literal of the struct with the array ends in
  an internal error.
- T021, severe. `Word = size_of(int)` in an enum compiles, and the value
  is 0 at run time.
- T022, severe. An `atomic` field or a `Mutex` whose word crosses a
  multiple of 16 in a packed class stops the program with a bus error.
- T023, major. antic crashes on a field whose type is a variant without
  fields, in dev mode for a struct and in every mode for a class.
- T024, minor. `switch` and `if let` on a pointer to a variant give three
  and four messages for one mistake.
- T025, major. `anti fmt` moves every function that follows a `type` alias
  of a function type.
- T026, minor. `n is Shape.Circle` on an `int` says `` cannot find module
  `Shape` ``.
- T027, minor. `Kind.C` for an unknown value of an enum says `` `Kind` has
  no function `C` ``.
- Three more refusals show T016: a constant index past the end of an array
  in a field, in an inner array and in a write.

## Where the task and the code differ

- The task counts an error of the back end without a position as a
  defect. An `align(N)` below the alignment of the fields is such an
  error, and "Expressions and types" in `docs/decisions.md` states it. So
  is a bitfield whose unit is larger than its struct. `errors/layout` and
  `errors/layout_unit` hold both, and the documented behaviour stands.
- The task asks for one program per cell that compiles. Six programs hold
  the cells, as the step `values` shared its builds, for the 9 minutes of
  a suite.
- The task asks for every pattern on a variant. An arm of two cases and
  an arm with a qualified case are refused by the parser with two
  messages each. A refusal holds one message, so neither has a test.
- `tests/abi/probe.anti` stands unchanged. `dump_ir_probe` pins its IR,
  and every type added there moved 401 lines of the pin.
- The ASan suite took 569 s, above the 9 minutes of "Repository layout" in
  `docs/decisions.md`. See "Counts and checks".

## Rules the documents do not state

- A value past the width of a bitfield keeps its low bits. A literal, a
  write and a compound assignment give no message and no check in dev
  mode.
- `+%=` and `+|=` on a bitfield compute in the type of the field and then
  keep the low bits, so a saturated value is cut again.
- `alloc(T, n)` gives the memory of `malloc` for a type with an `align(N)`
  above 16. A dev build got an address 16 past a multiple of 64 for
  `align(64)`. No test holds it, since the address depends on the host.
- An enum without values compiles, with the size of `c_int`.
- Two values of an enum may share a number, and a `switch` takes one arm
  for both.
- `as` converts an enum to another enum, and a float to an enum.
- A value of an enum cannot name an earlier value of its own enum.
- `Shape.Empty { }` and a case declared as `A { }` compile.
- No arm names a value of the `tag` of a variant, so a `switch` on it
  takes `else` alone. `v.tag as int` gives the number.
- `as ?T` reads as the nullable downcast `as? T`, and `anti fmt` writes it
  so. No `as` gives an optional value.
- `as` to the type a value has already is refused for a tuple, a slice
  and a channel.
- `packed` before `enum`, `simd struct` and `synchronized class` is
  refused with `expected an item`. A union and a `type` alias take no type
  parameters.
- A field default in a union is refused with the message of a struct.
- `align(4294967296)` compiles, and the type has that size.
- `==` on a struct that holds a simd struct is refused.
- A `simd struct` of one `f64` lane compiles.
- `anti fmt` writes a space after `]` before `fn`, `chan` and `?`, as
  `[2] fn(int) -> int`.
- T021 expects `a value of an enum is a constant, not a value computed
  from size_of`, T026 the message of the qualified form and T027 `` `Kind`
  has no value `C` ``.

## Counts and checks

- Tests: 2234 before and 2369 after, by `ctest --preset host -N`.
- Host suite: 2369 of 2369 passed in 414 s, 82 of them expected failures.
  Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/types-host-suite3.log` holds the run.
- ASan: 2368 of 2368 passed in 569 s, in `types-asan-suite2.log`. UBSan:
  2368 of 2368 in 472 s, in `types-ubsan-suite4.log`.
- The three trees built with zero warnings, in `types-host-build5.log`,
  `types-asan-build2.log` and `types-ubsan-build2.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/types-known-v.log` holds the output of the 22 listed
  tests. Each fails for the reason its finding names.
- Red before green: `diagnostics_covered` failed until the ten lines left
  its list, in `types-diag1.log`, and `dump_ir_probe` failed on the first
  form of the probe, in `types-host-suite1.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md`, `docs/notes/hosts-and-harness.md` and this
  report.

The ASan suite is above its 9 minutes:

- The Mac ran its file indexer, a mail agent and a game during the runs,
  at a load of 8 to 24 before a suite started.
- Under ASan the times of all tests add up to 7953 s, 14 times the 569 s
  of the suite. The 135 tests of this step take 100 to 144 s of them, 7
  to 10 s of the suite. The step `values` measured 535 s.
- `emit_identity` and the six `llvm_verify` tests each write the LLVM
  text of every program of `tests/programs/` in release mode for the six
  targets. `emit_identity` takes 465 s of the 7953. One text for both
  would save about 33 s of the suite. That is a change to tests of other
  steps, and it is Eddie's decision.

UBSan failed three times before it passed:

- `matrix_values_aggregates_dev` and `matrix_values_bindings_and_calls_dev`
  passed the 60 s of the matrix runner, in `types-ubsan-suite1.log` to
  `types-ubsan-suite3.log`. Each takes antic 20 s alone.
- ctest starts a failed test first, beside the heaviest tests. Run alone
  once, each passed, and the next suite passed.
  `docs/notes/hosts-and-harness.md` holds the trap.

## Not done

- Neither VM ran the new tests. The programs print no size of a struct of
  bitfields, since Windows lays those out by another rule.
- No dev-mode check joined `tests/traps/`. An index past the inner array
  of an array of arrays stops a dev build, and no test holds it.
- `` `%s` is not defined on the lanes of `%s` `` stays in
  `tests/diagnostics-open.txt`. No program reached it.
- A simd struct at an odd offset of a packed struct ran on ARM64 alone.
  An x86_64 target loads it with other instructions.
- The scripts that wrote the sources are scratch files under
  `build/scratch/types/` and are not committed. The expected lines of the
  programs were written from the rules. Four of them were wrong on the
  first run, by the sums of the author, and antic was right.
- A worker and a `dispatch` take no type of the area here. The area
  threads holds them.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
4f63e96d State the types tests, the counts and the suite times in CLAUDE.md
a5dfcf65 Mark the types cells of the test matrix
ef5e8593 Drop four refusals that the values area holds already
```

`git rev-parse HEAD origin/main` after that push:

```text
4f63e96d54cb74d5d7cabdf8452df42d1671649b
4f63e96d54cb74d5d7cabdf8452df42d1671649b
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
