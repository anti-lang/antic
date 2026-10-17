# Chapters 13 to 15 of the developer's guide

The step `guide-parallel` of the documentation driver, on 2026-10-11, on
the branch `docs`. It writes chapters 13, 14 and 15 of
`docs/site/developers-guide/` in full. It changes no code under `src/`.
Under `tests/` it changes `tests/runners/run_document_examples.cmake` twice.

## What the step wrote

- `13-threads.md` has five sections: dispatch and join, `parallel`,
  channels and `select`, atomics, and locks.
- `14-concurrent-classes.md` has five: synchronized classes, concurrent
  classes, `sync` blocks, the lock order checks and the thread-safe
  collections.
- `15-hooks.md` has five: hooks, tracing, injection, the runtime
  configuration and plugins.

Every section has the five parts of the task. Chapter 14 states what the
checker proves of a synchronized and of a concurrent class and what it
leaves to the programmer, from `sema_safety.c` and `sema_thread.c`. The
hidden lock stands in the module and in the machine code of a function.
The size of a `Mutex` stands as the constant of three targets. Chapter 15
gives what `anti_rt_hook` does with nothing attached, with a handler
installed and with a hook of the class.

## The examples it added

The three pages hold 17 `anti` blocks that compile, 14 of them programs
that run against their output. Every program that starts a job joins it.
The compiler refuses 101 more blocks with the error each names, and the
pages hold 36 excerpts.

- Chapter 13: `workers`, `chunks`, `pipes`, `counters`, `locks`.
- Chapter 14: `tally`, `stock`, `pairs`, `orders`, `shared`.
- Chapter 15: `hooks`, `traced`, `wiring`, `settings`.

The pages hold no time. `tests/bench` has no program that makes objects,
takes a lock or starts a job in its loop, so the costs are counts.

The plugin of chapter 15 and its host were built and run by hand on
macos-arm64, since the runner builds one module per block. The page holds
their output and their sizes. The dev report of `orders` and the output
of `settings` with options were taken the same way.

## The runner

- llvm-mc gets `-mattr=+all` for an arm64 triple. llc writes the
  instructions of the level of the target, and llvm-mc refused `ldaddal`
  of macos-arm64. A page with a wrong line and one with the right line in
  `build/scratch-guide/proof-mattr/` fail and pass.
- An excerpt header takes `option=no-hooks`, `option=trace` and
  `option=no-trace`. The two excerpts of chapter 15 that use them hold
  lines that stand next to each other only under the option.

## What was wrong in antic

None of these is changed. The guide states each as it is.

- A `dispatch` with arguments keeps them in one slot of the frame per
  `dispatch` in the source. In a loop, a later turn overwrites the
  arguments of a job that has not started. A program of 38 lines gave
  5,106 of 8,000 jobs the value of a later turn, in release and in dev.
- The same slot is dead when the function returns before it joins. A
  second `join` of one job reads freed memory. Neither is refused.
- An `atomic` field of `f64` passes the checker and fails in opt with
  "invalid cast opcode". An atomic local of `f64` is refused.
- A class value in a variable runs `created`. It runs `destroyed` only
  when its class has a teardown. `dup` runs `copied` and no `created`.
- `sync a, b` inside a public function of the class of `a` takes `self`
  first, since the function holds it. Two such calls lock in opposite
  orders, and the dev build reports them.
- In a synchronized class, a function reads `other.count` under the lock
  of `self` alone, and a function without `self` calls a private function
  with no lock. Both compile.
- A `sync` through a copied pointer inside a `sync` on the same `Mutex`
  compiles and waits for itself.
- The lock order records of a hidden lock are never cleared. A new object
  at the address of a freed one can meet an order it never took.
- A job has no type a program can write, so it stands in no field.

## What was wrong in older documents

- The skeleton of chapter 13 said `parallel` runs over a range and over a
  collection. It splits a slice.
- `docs/notes/threads.md` says nothing frees the results of `parallel`.
  The program frees them with `free(r.ptr)`, as `docs/decisions.md` says.
- `docs/notes/runtime-conf.md` lists five keys and says nothing reads
  `plugins`. `src/rt/conf.c` has six, with `hash_seed`, and discovery
  reads `plugins`.
- `docs/notes/injection.md` says `--anti.inject` ends the program while
  plugins are not built. They are built.
- The overview calls the cost of a hook site without a handler not built.
  The site is one call, with one atomic load, two loads and one compare.
- The header of the runner says numbered names such as `%12` are made
  equal. A name such as `%v232` is compared as written.

## Answers from the code

- The pool has the processors of the machine less one. On one processor
  it has no thread, and `join` runs the job.
- `add`, `sub`, `and`, `or` and `swap` of an atomic give the value before.
  `add` wraps in a dev build and has no overflow check.
- A worker may write the elements of its chunk.
- A channel is a header of 56 bytes and its ring in one block, and a
  monitor in another. Every `send` wakes every waiting `select`.
- The hidden lock is 24 bytes on all six targets, the last field of the
  first synchronized class of a chain.
- The checker reads a guard in one function at a time. A private helper
  that reads a guarded field is refused although its callers hold the lock.
- The hooks are entries 8 to 16 of a table, and the functions of a handler
  17 to 25.
- The build of the module that holds a function decides its tracing.
- A build finds a provider under `--inject` first and in `default` of the
  interface after it.
- A host of the example is 255,296 bytes and 134,896 with `--closed`.

## Open points for Eddie

- `check-web-content` reports the part headings and "Where it lives:", as
  in every chapter. It also counts a sentence that starts with a code span
  as longer than the docs-style checker does.
- The three chapters hold no time table. A program for `tests/bench` that
  allocates in its loop would give the hook site a time.
- `guide_examples` took 182 s in the suite.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 352 s with `ctest --preset host -j14`. The sanitizer suites did not
run, since the step changed no C file under `tests/` and not
`tests/CMakeLists.txt`. The docs-style checker reports nothing on the three
pages and on this report. `git grep` finds no marker of unfinished text in
the three pages. This is the state before the commit of this report:

```text
$ git log --oneline -3
54342ebf Name the pool of a machine with one processor in chapter 13
899b2855 Write chapter 15 of the developer's guide, hooks
83d6c302 Take three more options of antic in an excerpt of the document runner
$ git status --short
?? docs/reports/2026-10-11-docs-guide-parallel.md
$ git rev-parse HEAD origin/docs
54342ebfa3edcacf5808544b580a5089e338aaff
54342ebfa3edcacf5808544b580a5089e338aaff
```
