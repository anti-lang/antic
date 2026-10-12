# The classes area of the test matrix

The step `classes` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area classes of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 137 sources of the area: 28 programs, each in dev,
  release and `--memory-checks`, and 109 refusals of one message each.
- Seven programs hold their constructs in fifteen places each. They hold
  the calls through the tables, the ways an object is made and torn down,
  and `is`, `as` and `==`. They hold the static members, the operators,
  the generics and the root as well.
- `anti_test_classes` holds the same in a `tests` block and a `fixtures`
  block. `classes_modules` holds them in a library module, plain and
  generic, with a client that inherits and implements across the boundary.
- Ten programs hold the rules: the tables, the order of `construct` and
  `destruct`, the default `equals` and hash, the levels, a singleton, `dup`,
  every operator, a nested type, injection and a generic class.
- Two programs take the kinds of value: 29 kinds as a field through
  `equals`, the hash and `dup`, and 31 kinds through a generic class,
  function, variant and interface.
- Six programs run once more under `--no-reflect` or `--no-hooks`.
- The note marks the nine open cells of the area and twelve cells in all,
  and the five rows of decision 27.
- 28 messages left `tests/diagnostics-open.txt`.

The triples: a generic class, a constant or a static field and the type
argument. A nested class, a qualified `concrete fn` and a base of the same
body. A pointer to an interface, a pointer to the object and `==`. An
interface, an abstract base and `as`. An `operator fn`, a parameter of
another type and a literal. A generic function, a library file and a
constant of a class. A class, `dup` and a replaced `copy`. "Classes" in
the note names the test of each.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 286 lines, 63 of them new.

- T064 to T068, major. Five forms of decision 27 compile: an open function
  without `abstract`, `mutable` in a plain class, `==` on unrelated
  pointers, `equals` without `hash`, and `self.super.destruct()`.
- T069, major. A field after a constant or a function is refused.
- T070 and T071, minor. Four messages differ from "Messages" of the object
  model, and six mistakes get two messages.
- T072, severe. A constant and a static field of a generic class are one
  for every copy, so `size_of(T)` is 8 in `Box<u8>`.
- T073, major. `as` from an interface to the abstract base is refused.
- T074, major. A literal as the second operand of an `operator fn` of
  another type is refused.
- T075 and T076, major. Static members without a marker are reached from
  outside, and public ones are refused from another module.
- T077, minor. Three messages name another kind or deny a member.
- T078, major. An abstract class compiles as a parameter, a result, a
  slice, an array, an optional, a tuple and a type argument.
- T079, major. A fixed field of a singleton is written through its address.
- T080, major. `T { }` in a generic ends in an error of the back end.
- T081, severe. The default `equals`, hash and `==` compare an `own`
  pointer by its address.
- T082, severe. `dup` hands a replaced `copy` an object without its table.
- T083, severe. `==` on a pointer to an interface and one to the object
  compares the addresses.
- T084, major. A `concrete fn` in a nested class takes no qualifier.
- T019 and T029 got one more test each.

## Where the task and the code differ

- The task asks for one program per cell. The cells share programs, as in
  the steps before, for the 9 minutes of a suite.
- A matrix program takes no option of antic. The trace hooks, `--inject`
  and the files of the runtime configuration stay with their own tests.
- antic adds what needs the value to `` `Shape` is abstract and has no
  complete value ``. The tests hold the longer text.
- `` `compatible` names a version `` cannot be reached: the parser reads
  the line only when a number follows.

## Rules the documents do not state

- A nested class and the class around it reach no private member of each
  other.
- `==` on values of a class that replaces `equals` calls its `equals`.
- An object equals an object of its own class alone, never one of a class
  below with the same fields.
- A singleton whose `construct` takes arguments is built from its defaults.
- `dup(Config.get())` and `*Config.get()` copy a singleton.
- A singleton inherits a singleton, and a plain class inherits one.
- A class with an `inject` field has no `T { }` default as an inline field.
- `delete` tears down what an object owns from its last field to its first.
- The `construct` of a base without arguments runs before a `construct`
  with arguments. When that one fails, no `destruct` of the base runs.
- A local of a class that replaces `destroyed`, has no `destruct` and owns
  nothing is not torn down, so the hook does not run.
- A class below reaches a protected member on another object of its base.
- `T(args)` runs a `construct` without `pub` anywhere in its module.
- T065, T066, T080, the reverse of T067 and two tests of T077 expect
  messages of this step's wording. T078 expects the short message of the
  object model.

## Counts and checks

- Tests: 3029 before and 3235 after, by `ctest --preset host -N`.
- Host suite: 3235 of 3235 passed in 425 s, 286 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/classes-host-suite1.log` holds the run.
- ASan: 3234 tests in two parts, 1000 in 474 s and 2246 in 376 s, all
  passed. Twelve fixture tests ran in both parts.
  `classes-asan-suite-a.log` and `classes-asan-suite-b.log` hold them.
- UBSan: the same two parts in 437 s and 237 s, all passed, in
  `classes-ubsan-suite-a.log` and `classes-ubsan-suite-b.log`.
- The three trees built with zero warnings, in `classes-host-build5.log`,
  `classes-asan-build.log` and `classes-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- The output of each context program and of both kinds programs was
  compared with a model written before the run. The 63 listed tests were
  compared with what their findings name, test by test.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- Nine messages of the area stay in `tests/diagnostics-open.txt`, among
  them the two of a copy that antic does not compile yet.
- The scripts that wrote the sources are scratch files under
  `build/scratch/classes/` and are not committed.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
b48c6945 State the classes tests, the counts and the suite times in CLAUDE.md
373550c8 Mark the classes cells of the test matrix
512ed9d2 Record the findings T064 to T084 of the classes area with their tests
```

`git rev-parse HEAD origin/main` after that push:

```text
b48c6945784a8a5b8422ce744e7b0aa06b48b965
b48c6945784a8a5b8422ce744e7b0aa06b48b965
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
