# Step fuzz: a generator of random valid programs

The step built a fuzzer after Csmith under `tools/fuzz/`, in CMake scripts
as the other tools are. `docs/notes/fuzz.md` describes it, and
`CLAUDE.md` points to it under "Building". Logs are under
`build/drive/logs/`, named `fuzz-*`.

## What was built

- `tools/fuzz/generate.cmake` writes one program from a seed. The checker
  accepts it, it ends, and its behaviour is defined and the same in every
  mode. `main` prints a checksum after each call. The program draws
  functions and calls, structs, pointers, classes with dispatch and an
  interface, generics, optionals and `may fail` with every handler. It
  also draws ownership with moves and teardown, closures, snapshots,
  defers, variants, tuples, the flags form, channels and threads whose
  results meet in a sum.
- `tools/fuzz/run.cmake` checks a range of seeds, ten at once by default,
  in dev mode, in release mode and with `--memory-checks`. It saves each
  find under `build/fuzz/finds/<seed>/` with the program and every output.
- `tools/fuzz/reduce.cmake` removes units of lines while the find keeps
  its kind and its message. A refused find of 562 lines went to 6 in 98
  checks, and a difference of 851 lines to 77 in 492 checks and 130 s.
- `tools/fuzz/common.cmake` holds the names of the paths, the limits and
  the check of one program.
- `fuzz_smoke` runs the seeds 1 to 20 in 18 s of the host suite and 20 s
  under ASan. `fuzz_reduce` shrinks a refused find, through
  `tests/runners/run_fuzz_reduce.cmake`.

## Campaigns

Seeds 1 to 200 in `fuzz-run3.log`, 1000 to 1299 in `fuzz-run5.log`, 2000
to 2499 in `fuzz-run6.log` and 3000 to 3299 in `fuzz-run7.log` and
`fuzz-run8.log`. Each ran the generator of its moment. The finds of
antic are the three findings below. The other finds were faults of the
generator, each fixed in the generator before the commit.

## Findings

The step closed none. It recorded three, each with a matrix program in
the three modes, and left each for the step of its area:

- T180, `matrix_values_flags_form_of_constants_*`, commit `ccd76610`. The
  flags form of two signed constants out of range is refused as a
  constant by R029, where the flags form traps in no build.
- T181, `matrix_closures_snapshot_of_an_if_let_binding_*`, commit
  `635ee979`. A snapshot refuses the `int` that `if let` binds from a
  `?int`, reading the declared type of the symbol.
- T182, `matrix_flow_do_while_before_a_parenthesized_statement_*`, commit
  `03f48ebb`. A statement that starts with `(` after `do { } while cond`
  is read as a call of the condition. Decision 49 gives the loop no
  trailing semicolon and says nothing of the next statement.
  Whether the condition ends at the line is a question for Eddie, since
  the fix changes the grammar.

The generator steers around the three, each at the line that does so.
`tests/known-failures.txt` held 133 lines before and 142 after, and the
Mac expects 121 tests to fail, 112 before.

## `[provisional]` entries

- The fuzzer counts `refused` and `status` as finds beside the five its
  task names, and steers around each open finding it met. Under "What
  the solid run decided" in `docs/decisions.md`.

## Gates

- No warning of our code: `fuzz-final-host-build.log`,
  `fuzz-gate-asan-build.log` and `fuzz-gate-ubsan-build.log`.
- Host, 5800 tests, `-I 1,2000`, `2001,3900` and `3901,5800`: 2000, 1927
  and 1906 passed in 377 s, 176 s and 287 s. The later parts run the
  fixtures of the first again. 121 expected failures fail as listed, and
  the two `mimalloc_environment` tests skip: `fuzz-final-host-1.log` to
  `-3.log`.
- ASan, 5798 tests, `-I 1,1150`, `1151,2250`, `2251,4000` and
  `4001,5798`: 1151, 1106, 1777 and 1804 passed in 243 s, 184 s, 211 s
  and 537 s, `fuzz-gate-asan-1.log` to `-4.log`.
- UBSan, 5798 tests, the same parts with the last split at 4900: 1151,
  1106, 1777, 906 and 898 passed in 239 s, 168 s, 166 s, 185 s and
  310 s, `fuzz-gate-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on the `.md` files of the step.

## Not run on the target

- `tools/fuzz/common.cmake` runs the executable that antic writes by the
  name `-o` gives, as `run_program.cmake` does, and removes it by that
  name before a build. On Windows the file carries `.exe`. `fuzz_smoke` on
  anti-windows would show it.
- `tools/fuzz/run.cmake` runs a batch of children as the commands of one
  `execute_process`, joined by pipes. `fuzz_smoke` on anti-linux and
  anti-windows would show that the batch runs there.

## State at the push

```text
$ git log --oneline -3
8a5a2595 Hold the fuzzer to the rules of runners and scripts
68494395 Format the programs of T181 and T182
0fd2d702 Add a fuzzer of random valid programs after Csmith
$ git status --short
?? docs/reports/2026-10-22-solid-fuzz.md
$ git rev-parse HEAD origin/main
8a5a25952aa9551967df685137fc883feac073a0
8a5a25952aa9551967df685137fc883feac073a0
```

The one untracked file is this report, whose commit follows the push.
