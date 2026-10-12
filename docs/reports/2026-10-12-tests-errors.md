# The errors area of the test matrix

The step `errors` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area errors of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 88 sources of the area: 37 programs, each in dev,
  release and `--memory-checks`, and 51 refusals of one message each.
- Nine programs hold `fail`, `try` on a call, a `try` block, `catch e { }`,
  `catch { }`, `catch fatal`, `yield`, `undo` and `here` in up to 15 places
  each. `anti_test_errors` holds the `tests` block and the `fixtures`
  block, and `errors_modules` a library module, plain and generic.
- Four programs take every kind of value through a `yield`, through a
  `?T` that a failing function gives and through a generic that may fail.
- Nine refusals hold decision 20 and five hold decision 21.
  `errors_handler_exits` holds the ways a handler of a value may end.
- Programs of their own hold the origin of an error, `catch fatal` that
  ends the program, `-> never`, a result that may fail and may be `none`,
  function types that may fail, handlers inside handlers, a `construct`
  that fails, a synchronized function that fails and atomics.
- The note marks all 124 open cells and all 30 open kinds. Four of them
  hold a refusal, since the rules give the construct no place there.
- Six messages left `tests/diagnostics-open.txt`.

The triples: a generic, `may fail` and a `?T` whose argument may be `none`.
A generic, an `own` parameter and a `yield`. A generic, a library file and
an origin. A failing function, a `?T` and each kind. `p?.f()`, a failing
function and a handler. A `try` block, a handler in it and a failing call
in that handler. A `construct`, a base and an `own` field. A synchronized
class, a failing function and a worker. "Errors" in the note names the
test of each.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 183 lines, 41 of them new.

- T043, minor. A `fail` and a `here` in a generic of a library name the
  file and the module of the client, with the line of the library.
- T044, major. A handler of a value that can reach its end compiles, and
  the binding holds what stood in its memory. Decision 20.
- T045, severe. `let v = try maybe(n);` on a call that cannot fail
  compiles, and `v` holds a value where the call gave `none`. Decision 21.
- T046, minor. `try` on an optional name, field or operand is refused with
  a message that names neither `let ... else` nor `??`.
- T047, severe. The error a handler binds is stored by `=`, by `yield` and
  in a literal, and the program reads it after the handler deleted it.
- T048, major. A `?int` is refused where a `??int` is expected.
- T049, severe. `??` in a generic over a `?*int` gives values that differ
  between the modes, and over a `?int` the copy is refused.
- T050, major. `try alloc Machine(n)` is refused as no call.
- T051, minor. An unhandled `alloc T(args)` gets a second message.
- T052, severe. A derived `construct` that fails after its base succeeded
  leaves what the base took, and `--memory-checks` reports the leak.
- One more program shows T038, a `yield` of an `own` parameter of a type
  parameter.

## Where the task and the code differ

- The task asks for one program per cell. The cells share programs, as in
  the steps before, for the 9 minutes of a suite.
- `fail` and `try` have no cell in a `defer` or an `undo`. Decision 19
  refuses both, and the refusals of the step `flow` hold them, T001.
- A worker that may fail is refused at `dispatch` and `parallel`.
  `errors_fail` and `errors_try` call such a worker directly.
- A function of a `tests` block takes no `may fail`, a constant holds no
  `here`, and no function gives an `f16`. Each cell holds a refusal.
- `lent` marks the result of `operator fn value` alone, so the lent kind
  of `yield` is a lent parameter, which is refused.
- A trace differs between dev and release, so no matrix program prints
  one. `tests/trace/` holds the traces.
- Each sanitizer suite ran in two parts, as the task allows.

## Rules the documents do not state

- A `worker fn` that may fail is called directly with a handler.
- `yield;` in the handler of a `try` block is refused with `` `yield`
  stands in a `catch` handler ``, although it stands in one.
- A `yield` in the handler of a `try` block inside the handler of a `let`
  gives the value of the `let`.
- `try f()` inside a `try` block forwards to the caller, not to the
  handler of the block.
- `fail` of a `str` in a variable and of an `f"..."` is refused. `fail
  "text"` takes a literal alone.
- An `undo` in a function that cannot fail, in the block of a `defer` and
  in the block of another `undo` compiles and never runs.
- A `fail` in an anonymous function does not answer the warning
  `never-fails` of the function around it.
- The function of an origin is `m.f.0` for an anonymous function,
  `m.f<int>` for a copy and `m.Outer.Inner.f` for a nested type.
- The `NoneDereference` of a guard has code 1, the message `the pointer is
  none` for a `?T` of a value as well, and no origin.
- `catch fatal` in a worker ends the whole program, and no `defer` runs.
- A `let` takes a lent pointer, and a `yield` into that `let` refuses it.
- A function that may fail and gives a function that cannot fail needs a
  `type` alias, since `may fail` after a function type belongs to it.
- An `own` parameter of a pointer type that a generic does not hand on is
  not deleted.
- `dup` of a class value runs no `construct`.
- The handler of a call statement whose result is dropped may reach its
  end.
- T044 to T046 expect messages of this step's wording. No document gives
  the text of either decision.
- T043 is graded minor as the position of a message, although the origin
  is a value of the program. T052 has no rule that says what frees the
  part of the base.

## Counts and checks

- Tests: 2698 before and 2866 after, by `ctest --preset host -N`.
- Host suite: 2866 of 2866 passed in 411 s, 183 of them expected failures.
  Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/errors-host-suite2.log` holds the run.
- ASan: 2865 tests in two parts, 1300 in 516 s and 1573 in 187 s, all
  passed. Eight fixture tests ran in both parts.
  `errors-asan-suite2a.log` and `errors-asan-suite2b.log` hold them.
- UBSan: the same two parts in 460 s and 146 s, all passed, in
  `errors-ubsan-suite2a.log` and `errors-ubsan-suite2b.log`.
- The three trees built with zero warnings, in `errors-host-build4.log`,
  `errors-asan-build2.log` and `errors-ubsan-build2.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/errors-known-v.log` holds the output of the 41 new
  listed tests. Each fails for the reason its finding names.
- The output of the context programs and of the kinds programs was
  compared with a model written before the run. The output of every other
  program was checked line by line against the rules after its first run.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- Three messages of `fail` stay in `tests/diagnostics-open.txt`. Each
  needs an `anti.lang` that lacks a part of `Error`.
- `?.` on a local that `as?` gave is refused, which looks like T036. No
  test of this step holds it.
- The scripts that wrote the sources are scratch files under
  `build/scratch/errors/` and are not committed.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
0532fa7a Hold a try inside a try block and an undo that cannot run
9d78eb07 State the errors tests, the counts and the suite times in CLAUDE.md
3e4169cb Mark the errors cells of the test matrix
```

`git rev-parse HEAD origin/main` after that push:

```text
0532fa7a354c8986b3ca227522a659c101956c4d
0532fa7a354c8986b3ca227522a659c101956c4d
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
