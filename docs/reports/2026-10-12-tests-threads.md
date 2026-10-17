# The threads area of the test matrix

The step `threads` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area threads of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 116 sources of the area: 18 programs, each in dev,
  release and `--memory-checks`, and 98 refusals of one message each.
- Seven programs hold `dispatch`, `join` and `join_all`, `parallel` with
  and without `by`, `Mutex.new()`, `sync` and `destroy`, the channel
  operations, `select` and the eight atomic operations in fifteen places
  each. `anti_test_threads` holds the `tests` block and the `fixtures`
  block, and `threads_modules` a library module, plain and generic.
- Three programs take every kind of value through a channel and through
  the atomic operations on a field and on a local. The third takes each
  through a `dispatch` and a `parallel`.
- Eight programs hold the other rules. Two hold decision 23, and two the
  split of `parallel` and the order of `select`. The others hold guarded
  fields, what a job stands in, atomic locals that closures capture and a
  constant with a Mutex.
- `tests/traps/` holds six programs. Three stop at a channel made for no
  value and at a `send` on a closed channel. `lock_orders` and
  `lock_order_once` hold the lock orders of one thread, and
  `check_join_twice` the trap of decision 23.
  `tests/runners/run_dev_report.cmake` runs the last three and stops a program
  that hangs.
- The note marks 331 open cells and 137 open kinds. Four cells and seven
  kinds stay open, each with the reason it cannot exist.
- Fourteen messages left `tests/diagnostics-open.txt`.

Every program ran ten times in each of the three modes before its commit,
through `build/scratch/threads/run3.py`. No output varied.

The triples: a `dispatch`, a loop and the arguments of a job. A generic, a
channel and a type argument that is a pointer. A generic class, an atomic
field and its type argument. A generic, a library file and a `dispatch`. A
closure, a `concurrent` parameter and each way to change a captured
variable. A closure or a `defer`, a guarded field and a `sync`. A
thread-safe class, a public function and a pointer inside a tuple. An
atomic local, a `snapshot fn` and a `parallel`. "Threads" in the note names
the test of each.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 223 lines, 40 of them new.

- T053, major. A second `join` of one job compiles, and the program waits
  for ever. Decision 23.
- T054, severe. A second `join` that the checker cannot see hangs in a
  dev build, where decision 23 asks for a trap.
- T055, severe. One slot of the frame holds the arguments of a
  `dispatch`. Every job of a loop reads the last turn, and a job whose
  function returned reads a dead frame. Decision 23.
- T056, major. antic crashes on a read of a constant of a struct that
  holds a Mutex.
- T057, major. The default of a static field that no code reads is not
  checked, so a call compiles there.
- T058, major. A `chan T` whose type argument is a pointer compiles.
- T059, major. No atomic field is held to the types of an atomic. An
  `f64` ends in an error of opt without a position, a struct, a `str` and
  a `?int` crash antic, and an enum compiles. Decision 27 for the static
  field.
- T060, severe. `add` and `sub` on an atomic `bool` leave 2 in it, and on
  an atomic pointer they leave `none` in a `*T`.
- T061, major. The library file of a generic function that holds a
  `dispatch` cannot be read: `needs struct anti.lang.Job`.
- T062, major. A public function of a thread-safe class returns a pointer
  into its fields inside a tuple, inside a struct and from a private
  function.
- T063, minor. Two locks in opposite orders are reported once per order,
  not once per pair.

## Where the task and the code differ

- The task asks for one program per cell. The cells share programs, as in
  the steps before, for the 9 minutes of a suite.
- `join_all`, `m.destroy()`, `close` and `store` give no value, so none
  has a cell at module level. No channel carries a pointer, so six kinds
  of `send` and the lent kind have no cell.
- The lock order checks print file and line and differ between dev and
  release. They are tests of `tests/traps/`, not matrix programs.
- An atomic local of `f64` is refused already. The field is T059.
- The row `static atomic` on an `f64` of decision 27 belongs to the area
  classes. This step marked it with the atomic fields.
- Each sanitizer suite ran in two parts, as the task allows.

## Rules the documents do not state

- A worker of another module is named through a direct import. `m.f`
  after the arrow is refused.
- `by 0` and a count below zero give the chunk count of the machine.
- A `dispatch` writes every argument out. The default value of a
  parameter of the worker is not filled in.
- An atomic `add` wraps in a dev build as in release.
- A `snapshot fn` copies an atomic local, and its operations change the
  copy.
- An atomic field takes a `char`, and an atomic `bool` takes `and` and
  `or`.
- A pointer to a guarded field that was taken inside its `sync` is used
  after the block.
- `sync m` in an anonymous function inside `sync m` compiles, and so does
  `sync` on the same Mutex through a pointer. Both wait for themselves.
- A public function of a thread-safe class stores a pointer into its
  fields through an out parameter and into another object.
- A fresh value of a synchronized class, of a concurrent class and of a
  Mutex moves into a channel.
- A job stands in a tuple and in an array, and a closure joins one it
  captured.
- An `own fn` field passes to a `concurrent` parameter.
- A `chan f16` carries an `f16`, although no parameter takes one.
- No rule says whether a constant may hold a Mutex. T056 expects the
  value of the literal.
- T053, T059 and T060 expect messages of this step's wording. T058 and
  T062 expect a message of antic at a position this step chose.

## Counts and checks

- Tests: 2866 before and 3029 after, by `ctest --preset host -N`.
- Host suite: 3029 of 3029 passed in 413 s, 223 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/threads-host-suite1.log` holds the run.
- ASan: 3028 tests in two parts, 1300 in 523 s and 1736 in 201 s, all
  passed. Eight fixture tests ran in both parts.
  `threads-asan-suite-a.log` and `threads-asan-suite-b.log` hold them.
- UBSan: the same two parts in 463 s and 125 s, all passed, in
  `threads-ubsan-suite-a.log` and `threads-ubsan-suite-b.log`.
- The three trees built with zero warnings, in `threads-host-build3.log`,
  `threads-asan-build.log` and `threads-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/threads-known-v.log` holds the output of the 40 new
  listed tests. Each fails for the reason its finding names.
- The output of the context programs and of the kinds programs was
  compared with a model written before the run. The output of every other
  program was checked line by line against the rules after its first run.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- `the function is not marked concurrent` stays in
  `tests/diagnostics-open.txt`. No program of this step reached it.
- The hooks `dispatched` and `joined` have no test of this step.
- The scripts that wrote the sources are scratch files under
  `build/scratch/threads/` and are not committed.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
f19c8746 State the threads tests, the counts and the suite times in CLAUDE.md
a24a5fa5 Mark the threads cells of the test matrix
8aea48f8 Hold the rules of the threads area where they meet, and what they show
```

`git rev-parse HEAD origin/main` after that push:

```text
f19c87467b5882ce7515ed40e43cd92d84fa89f8
f19c87467b5882ce7515ed40e43cd92d84fa89f8
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
