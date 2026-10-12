# The ownership area of the test matrix

The step `ownership` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area ownership of `docs/notes/test-matrix.md`. No file under
`src/` changed.

## What the step added

- `tests/matrix/` holds 241 sources of the area: 39 programs, each in dev,
  release and `--memory-checks`, and 202 refusals of one message each.
- Two programs hold the 13 constructs of the area in 14 places each.
  `anti_test_ownership` holds the `tests` block and the `fixtures` block,
  and `ownership_modules` a library module, plain and generic.
- Four programs and 50 refusals hold the kinds of `dup`, of a read through a
  pointer, of `??`, of `alloc(T, n)` and of `free`.
- Eleven programs take seven kinds of owning value through every place a
  value goes. The kinds are a class with a `destruct`, a class with an `own`
  field, an owning struct, tuple, variant, `?T` and array. The places are a
  `let`, `=`, `return`, an argument, a field, an element, a capture, a
  dispatch, a channel, a `yield` and a temporary. 98 refusals hold what the
  rules refuse there, 14 forms per kind.
- Programs of their own hold lent values, `own` fields and parameters, the
  teardown at every exit of a block, slices and views, `anti.mem`, a handle
  of `Shared<T>`, a value of a type parameter and a `List` of owning values.
- Three trap tests in `tests/traps/` hold the slice bounds of decision 24.
- The note marks 169 of the 170 open cells and all 80 open kinds. One cell
  cannot exist and says why.
- Nine messages left `tests/diagnostics-open.txt`.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 142 lines, 60 of them new.

- T028, major. `??` compiles in a constant when its left side is `none`,
  and antic crashes on `??` with a call in the default of a field.
- T029, major. antic cannot read back a library file whose generic changes
  a `static atomic` field.
- T030, major. `built.Built(4)` and `alloc built.Built(5)` are refused as
  `a type, not a value`.
- T031, major. `dup` of an array whose elements own something is refused,
  and `=` on the same array names `dup`.
- T032, severe. `alloc(T, n)` gives an empty block for a count whose size
  passes 2^64, and the program writes outside it.
- T033, severe. A release build takes an `alloc(T, n)` that fails as memory.
- T034, severe. `--memory-checks` stops the program at an `alloc(T, n)`
  that fails.
- T035, minor. `dup` of a `?T` before its test gives two messages.
- T036, major. `if let` and `let ... else` refuse the result of `as?`.
- T037, severe. `return` of a field, of `*p` and of an element that owns
  something compiles, against decision 22, and the program crashes.
- T038, severe. `yield` of a local that owns something compiles, and the
  program crashes.
- T039, severe. A fresh owning argument of a `dispatch` is never torn down.
- T040, severe. A local moved into an `own` parameter of a `dispatch`
  crashes the program.
- T041, severe. A call result read by `is`, by a test for `none` or by an
  index alone is never torn down.
- T042, severe. A `may fail` function that gives an owning variant traps in
  a release build.
- One more program shows T012, `none` as the default of a `?int` parameter.
  Three more trap tests show T017.

## Where the task and the code differ

- The task asks for one program per cell that compiles. The cells share
  programs, as in the steps before, for the 9 minutes of a suite.
- The task names a channel send and a dispatch argument for every value. A
  channel and a worker take no pointer, by `a channel carries values alone`
  and `A worker takes and returns values alone`. A lent pointer therefore
  has no cell there.
- The task has `*p` in every context. At module level it cannot stand,
  since a constant holds no pointer but `none`.
- Each sanitizer suite stands above 9 minutes as one run. The step ran each
  in two parts, as the task allows.

## Rules the documents do not state

- `dup` of a number, a `str`, a slice, a plain array, a union, an enum and
  a channel is refused outside a generic. `dup(*p)` in a generic copies
  each.
- `dup` of a `?T` before its test is refused, and after it gives a `T`.
- The fields of an object are torn down last to first.
- `=` to an `own` pointer or an `own` slice field leaves the old memory
  alone. The program deletes it first.
- `delete(c)` tears down no value the channel still holds, and nothing
  tears down the slot that `recv` fills. The program calls `destroy` on
  what it received. `--memory-checks` reports the rest as a leak.
- A class value with an `own` pointer is an argument of a worker and an
  element of a channel, where a plain pointer is refused.
- A closure that a `dispatch` passes to its worker may `delete`. The walk
  of a worker follows the calls by name.
- `let xs = [p, p];` of a lent pointer makes an array of lent pointers.
- An array literal takes no owning local, where a tuple, a struct and a
  class literal move one.
- A local passed to a value parameter that is not `own` is a copy of its
  bytes, which the function does not tear down.
- T038 expects the refusal of `=` for `yield` of a local. A move would be
  as safe.
- T028 expects `` `??` is not a constant expression ``, since the entry on
  `const` does not list `??`.
- `destroy(node)` on a value says `` `destroy` needs a class pointer ``,
  though it takes a pointer to a value of any type.

## Counts and checks

- Tests: 2369 before and 2698 after, by `ctest --preset host -N`.
- Host suite: 2698 of 2698 passed in 417 s, 142 of them expected failures.
  Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/ownership-host-suite1.log` holds the run.
- ASan: 2697 tests in two parts, 1300 in 501 s and 1405 in 152 s, all
  passed. Eight fixture tests ran in both parts.
  `ownership-asan-suite1a.log` and `ownership-asan-suite1b.log` hold them.
- UBSan: the same two parts in 448 s and 121 s, all passed, in
  `ownership-ubsan-suite1a.log` and `ownership-ubsan-suite1b.log`.
- The three trees built with zero warnings, in `ownership-host-build9.log`,
  `ownership-asan-build1.log` and `ownership-ubsan-build1.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/ownership-known-v.log` holds the output of the 60 new
  listed tests. Each fails for the reason its finding names.
- Red before green: `diagnostics_covered` failed until nine lines left its
  list, in `ownership-ctest3.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- Three messages of the area stay in `tests/diagnostics-open.txt`: the bare
  `` `%.*s` was moved ``, `` `lent` marks a pointer result `` and the one of
  `keep own` in an `extern fn`. No program reached them.
- `own fn` values have no program of the places. `programs/snapshots`
  holds them.
- A collection other than `List` and a handle of `Shared<T>` on more than
  one thread have no matrix program. `tests/std/` holds both.
- The expected lines were written from the rules before each first run.
  Two were wrong by the sums of the author. One missed the rule of
  `ArenaAllocator` for an `align` above 16, which `src/std/anti/mem.anti`
  states. One missed the order of the fields at a teardown, which no
  document states.
- The scripts that wrote the sources are scratch files under
  `build/scratch/ownership/` and are not committed.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
22528b8b State the ownership tests, the counts and the suite times in CLAUDE.md
915878d1 Mark the ownership cells of the test matrix
fdeea33c Hold the teardown at every exit and the refused moves
```

`git rev-parse HEAD origin/main` after that push:

```text
22528b8be3454a3fb45bbb9479eb7117b4fa6fcf
22528b8be3454a3fb45bbb9479eb7117b4fa6fcf
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
