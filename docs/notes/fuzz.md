# The fuzzer

`tools/fuzz/` holds a generator of random valid Anti programs after Csmith.
Beside it stand a runner that checks them in the three modes and a reducer
that shrinks a find. All three are CMake scripts, as the other tools of the repository
are. This note gives the commands, the rules that keep a program defined,
the kinds of find and the tests of the suite.

## The commands

Each script runs from the root and takes the Debug build of `build/host`
unless `-DANTIC` and `-DRUNTIME` name others.

```bash
cmake -DSEED=7 -DOUT=build/fuzz/seven.anti -P tools/fuzz/generate.cmake
cmake -DFIRST=1 -DCOUNT=200 -P tools/fuzz/run.cmake
cmake -DFIND=build/fuzz/finds/1046 -P tools/fuzz/reduce.cmake
```

- `generate.cmake` writes the program of one seed. `-DSIZE` sets the count
  of functions, 6 by default, and 16 at most. A size above 16 names a
  function `f16`, a keyword, and antic refuses every such program. A seed has at most nine digits, and one
  seed gives the same program on every run and every host.
- `run.cmake` checks the seeds `FIRST` to `FIRST + COUNT - 1`, `JOBS` of
  them at once, 8 by default. A seed that shows nothing leaves an empty
  directory under `build/fuzz/work/`, and `-DKEEP=ON` keeps its program and
  outputs. The script fails when a seed shows a find and names each.
- `reduce.cmake` shrinks the program of a find and writes
  `reduced.anti` beside it after every step that shrank it, so a run that
  stops keeps what it reached.
- `common.cmake` holds the names of every path, the time limits and the
  check of one program, which the runner and the reducer share.

A seed names a program of the generator of one commit. A change of the
generator gives a seed another program, so a find saves its program beside its
seed.

## What a program holds

A program imports `anti.lang` alone and declares the same frame each time:
a class `Sink` with a static atomic total, one to three structs of scalar
fields and an array, the generic `Cell<T>`, the variant `Token`, an
abstract class `Shape` with a chain of classes below it, the interface
`Scaled`, resources whose `destruct` adds to the sink, a synchronized class
`Tally` and the workers of `parallel` and `dispatch`. Then come the
functions `f0` to `fN`, and `main` calls each with literal arguments and
prints a checksum after each call and the total of the sink last.

A function body draws its statements from the constructs of
`docs/notes/test-matrix.md`:

- locals of `int`, `i32`, `u32`, `u8` and `u64`, and every compound
  assignment of the wrapping, the saturating and the bitwise operators
- `if`, `else if`, `for` over a range with and without `by`, `for` over an
  array with and without an index, `while`, `do`, `switch` with
  `fallthrough`, `break`, `continue` and `return`
- calls of the functions written before, struct literals, copies and
  struct values passed and returned
- a pointer to a struct and a pointer to a local, arrays and module
  constants
- tuples taken apart with `let (a, b)` and the flags form with a carry in
- dispatch through `*Shape`, `self.super`, a bound function and an
  interface pointer
- the generic `pick<T>`, `gsum<T: Number>` and `Cell<T>`
- `?int` with `if let`, `??`, `let ... else` and a local that starts as
  `none`
- `may fail` with `catch e`, `catch`, `catch none`, `try`, a `try` block,
  `fail` and `undo`
- ownership: a move into an `own` parameter, an owning field, `alloc` and
  `delete`, `dup` and a resource returned by value, each torn down on every
  exit of its block
- closures that read and change the locals they capture, and `snapshot fn`
- `defer` at the end of a block
- `parallel` over an array, `dispatch` with `join`, a synchronized class
  that the chunks of a `parallel` add to, and a channel with `select`

## The rules that keep a program defined

The generator keeps each rule by the way it writes the code, so no program
needs a filter after it is written:

- Integer arithmetic uses the wrapping, the saturating and the bitwise
  operators, whose results every mode defines. A divisor is `(e & 15) +% 1`
  and a shift count is masked below the width of its type.
- A narrowing `as` takes a value masked into the range of its type first. A
  literal of a type other than `int` names its type, `(7 as u8)`, since an
  expression of literals alone is an `int`.
- An index is masked below the length of its array, a power of two.
- Every loop runs at most eight turns. A `while` and a `do` loop count
  their turn as the first statement of the body, so `continue` cannot skip
  it.
- A function calls only the functions written before it, so no call
  recurses. Each function carries an estimate of the statements it runs,
  and a call or a loop that would take it past 20000 is not written.
- Every local is written where it is declared.
- The threads join before their results are read. The results meet
  through `+%` and the atomic adds of `Sink` and `Tally`, which do not
  depend on the order.
- Nothing reads the time, the environment or a random source. The default
  hash is left out, since the runtime chooses its seed at start.

The generator also steers around two open findings of
`docs/audit/tests-findings.md`, each at the line that does so: a snapshot
of an `if let` binding, T181, and a statement that starts with `(` after a
`do` loop, T182. A fix of a finding removes its workaround, as the fix of
T180 removed the one of the flags form of two constants.

## The kinds of find

`fuzz_check` of `common.cmake` builds a program in dev mode, in release mode
and with `--memory-checks`, runs each and gives one kind:

| Kind | What happened |
|---|---|
| `refused` | the checker refused the program, a fault of the generator or of antic |
| `crash` | antic ended through a signal, an assertion or a report of a sanitizer |
| `backend` | antic failed after the checker, in LLVM or in the link |
| `timeout` | antic ran past 60 s or a program past 20 s |
| `memory` | the run with `--memory-checks` wrote a report to its standard error |
| `differs` | two modes printed other bytes or ended with other statuses |
| `status` | the three modes agree and end with a status other than 0 |

A find is saved under `build/fuzz/finds/<seed>/` with the program, the
files `seed`, `kind` and `detail.txt`, and what antic and each program
wrote. A find of a defect of antic gets a row of
`docs/audit/tests-findings.md` and a test of its own, written from the
reduced program.

## The reducer

The reducer removes whole units of lines. A unit is a statement, or a
block from the line that opens it to the line that closes it. The header
of an item joins the body on the line after it. The reducer removes the
halves of the units of one depth first, then the quarters, down to one
unit. It takes the depths in turn and starts again until a pass removes
nothing.

A candidate shows the find when its check gives the same kind. A refusal
must give the same messages without their positions. A crash or a failure
of the back end must give the same first line. The reduction then cannot
drift to another defect. A refusal, a crash and a failure of the back end
build in the one mode that showed them. A report of `--memory-checks`
builds in that mode, and a difference builds all three.

## The tests

- `fuzz_smoke` checks the seeds 1 to 20 at the default size, ten at once.
  It took 13 s on the development Mac on 2026-10-22. A find fails it, and
  the find stays under `build/host/tests/fuzz/finds/`, or the same
  directory of the tree that ran it.
- `fuzz_reduce` writes the program of seed 7 with two functions, adds a
  line the checker refuses and saves the two as a find. The reducer must
  shrink it to at most 10 lines that keep the line and give its one
  message. `tests/runners/run_fuzz_reduce.cmake` holds it.

## The campaigns of 2026-10-22

While it was written, the generator ran the seeds 1 to 200, 1000 to 1299,
2000 to 2499 and 3000 to 3299. Each range ran the generator of that
moment. The finds were the three defects T180, T181 and T182 and faults
of the generator itself. The generator fixed each of them: a variant
captured by a snapshot, a function that never failed and a name that was
a type. Literals were read as `int`.

## The campaign of the step fuzz-campaign

On 2026-10-22 the generator of commit `cf4fc31a` ran 5840 seeds at the
default size, 10001 to 15840, and 3200 at size 16, 20001 to 23200, in
116 minutes. None showed a find. 150 seeds at size 40 were all refused
at `fn f16`, which is the limit above.
