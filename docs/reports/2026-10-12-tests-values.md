# The values area of the test matrix

The step `values` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area values of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 170 sources of the area: 27 programs, each in dev,
  release and `--memory-checks`, and 143 refusals of one message each.
- Six programs hold the forty constructs in the 362 open contexts a
  program reaches. `anti_test_values` holds the `tests` and the `fixtures`
  block, and `values_modules` the library module, plain and generic.
  `values_at_module_level` and five refusals hold the module level.
- Three programs hold every operator and every compound assignment on the
  eleven integer types at their limits, and two the floats and `as`
  between the numbers. A line holds the value the checker folds in a
  constant and the value the program computes from operands it cannot
  know before it runs.
- Two programs cross 37 kinds of value with nine constructs. They are an
  array literal, an array repeat, a slice literal, an element of a tuple,
  `let`, `const`, `=`, a field and a call.
- Five programs hold `f16`, the conversions of `bool`, `char` and enums,
  text and bytes, `f"..."`, tuples, folding, inference, `size_of`, the
  comparisons of the other kinds, simd, and the pairs with a lent pointer,
  a type parameter, an optional and a failing call.
- `tests/traps/` holds 15 new dev-mode checks, `check_<name>`.
- The note marks 456 cells and 615 kinds. One kind cannot exist: no
  constant is lent. Four rows of decisions 24 and 27 name their tests.
- Seven messages left `tests/diagnostics-open.txt`.
- `CLAUDE.md` holds the counts, the times and one rule under "Tests".

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 60 lines, 30 of them new.

- T008, severe. A constant `int` as `f32` is rounded twice.
  `1152921573326323713 as f32` folds to 2^60 and computes to 2^60 + 2^37.
- T009, major. `-9223372036854775808.0 as int` is refused in a constant.
- T010, minor. `2 as Kind` in a constant is refused as a pointer
  conversion.
- T011, severe. A negative value of an enum converts as an unsigned
  number: `Sign.Minus as int` is 4294967295 at run time.
- T012, major. antic crashes on a constant of an optional type.
- T013, major. `[5; 3]` is refused where the element is a `?int`.
- T014, minor. A function in a constant is refused as a variable.
- T015, minor. `is` on a value with an error adds a message about a
  module.
- T016, major. A constant index past the end of a fixed array compiles,
  against decision 24.
- T017, major. A dev build does not check `xs[lo..hi]`, against decision
  24.
- T018, major. `Limits.MAX` of a class is refused in a constant and as the
  length of an array.
- T019, major. antic crashes on `Limits.MAX` as the default of a field.

## Where the task and the code differ

- The task asks for one program per cell that compiles. The area held 91
  programs at first, 273 executables. macOS checks each new executable in
  turn, and the ASan suite took 597 s, in `values-asan-suite.log`.
  "Repository layout" in `docs/decisions.md` keeps a suite under 9 minutes
  by shared builds, so 27 programs hold the same lines, 8 of them the
  programs of the findings. A line names its construct and its place or
  kind.
- One program for all forty constructs was no option. antic 0.2.0 writes
  the LLVM text of 150 of the context functions in 21 s, and of 72 in
  2.4 s. It is no defect by the list of the task. Eddie may want to know.
- The dev-mode checks stop a dev build, so their programs stand in
  `tests/traps/` and not in the matrix. That took 15 entries in
  `tests/CMakeLists.txt`.
- The developer's guide says that `7 / 0` as constants compiles. antic
  refuses it, as "Numeric types" in `docs/decisions.md` asks.
- The row of the character literal named the kind `byte` twice.

## Rules the documents do not state

- A narrowing `as` of a constant keeps the low bits without a message:
  `300 as u8` is 44. The same conversion at run time stops a dev build.
- `0xD800 as u32 as char` in a constant gives a surrogate without a check.
- A float divided by zero in a constant gives an infinity or a NaN.
- A constant index below zero on a fixed array, `xs[-1]`, compiles.
  Decision 24 names an index past the end.
- Decision 24 gives no text. The refusal expects `the index 5 is outside
  the array of length 3`, and the check a line that holds `out of bounds`
  and `length 3`.
- The least value divided by -1 and unary `-` of the least value give the
  least value in a dev build too, with no check.
- `<<%` by a negative count gives 0.
- `.len` of a constant `str` is no constant expression.
- `==` on two slices and on two unions is refused.
- A message names `u64` where the source says `uint`, and `int`, `byte`
  and `float` where it says `i64`, `u8` and `f64`.
- `chan int(1)` in a constant is refused as a call.
- An array literal, an array repeat and a local take a lent pointer. A
  field, a tuple and a slice literal refuse one.
- A compound assignment on a simd struct and on a value of a type
  parameter is refused, as decision 27 refuses it for `operator fn add`.
- `>`, `<=` and `>=` on a struct come from its `operator fn lt`.
- A concurrent class with atomic fields alone is copied by `=`.
- T010 and T014 expect `a conversion to an enum` and `a function` in the
  text of the two messages.

## Counts and checks

- Tests: 1992 before and 2234 after, by `ctest --preset host -N`.
- Host suite: 2234 of 2234 passed in 409 s, 60 of them expected failures.
  Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/values-host-suite4.log` holds the run.
- ASan: 2233 of 2233 passed in 535 s, in `values-asan-suite2.log`. UBSan:
  2233 of 2233 in 479 s, in `values-ubsan-suite.log`.
- The three trees built with zero warnings, in `values-host-build7.log`,
  `values-asan-build2.log` and `values-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/values-known-v.log` holds the output of 14 listed
  tests. Each fails for the reason its finding names.
- Red before green: `anti_test_values` and `values_modules` each failed on
  a changed expected file, in `values-modules-red.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- An integer literal above the greatest `u64` has no test here. It does
  not lex, and `fmt_canonical` keeps such a source out of `tests/matrix/`.
- The ASan suite stands 5 s below its 9 minutes. The next matrix step has
  no room for new executables there without sharing more.
- The generators are scratch scripts under `build/scratch/values/` and are
  not committed. Each holds a model of the rule, and no line of a
  generated expected file comes from a run.
- `values_modules` runs in release and dev, not under `--memory-checks`.
- The atomic calls, `own` values and the flags form belong to other areas
  and have one refusal each here at most.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
72c9baf3 State the values tests and the new counts in CLAUDE.md
a976af14 Mark the values cells of the test matrix
ec3142c7 Share the builds of the values programs
```

`git rev-parse HEAD origin/main` after that push:

```text
72c9baf37fa2aa083fe7447cf5773a0f86ec5fa6
72c9baf37fa2aa083fe7447cf5773a0f86ec5fa6
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
