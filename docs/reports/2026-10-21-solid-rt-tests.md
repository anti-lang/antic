# Step rt-tests, second session

The session went on from the list of `2026-10-20-solid-rt-tests.md`,
which stays. It sweeps every kind of fault of `src/rt/fault.h` over
each command of anti, with the instrumented `anti_faults`, and keeps
the counts that reach a region nothing reached. It also writes programs
and inputs for the regions of `src/anti/` and `src/antic/` that a
program or an option reaches. Logs are under `build/drive/logs/`,
named `rt2-*`. The lists of the regions not reached are
`rtt-final-unreached.txt` at the start and `rt2-final-unreached.txt`
at the end. The scripts are in `build/rtt/`: `allkinds.sh`,
`sweepcov.sh` and `greedy.py` sweep the faults, `covrun.sh` and
`rtprobe.sh` measure one run, `fmtcov.py` one source of `anti fmt`.

## Coverage per directory

Regions reached. The start is `0da17f65`, `rtt-final-summary.txt`, and
the end is `571dfe72`, `rt2-final-summary.txt`. Every part of both
runs passed, `rt2-fin-1.log` to `-4.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 93.26% of 9252 | 93.29% of 9252 | 624, 621 | 93.44% | 93.46% |
| `src/antic/` | 97.68% of 55587 | 97.81% of 55587 | 1288, 1217 | 97.79% | 97.89% |
| `src/anti/` | 97.00% of 11663 | 97.79% of 11663 | 350, 258 | 97.00% | 97.93% |

## The tests

- Faults of anti. `anti test`: the library file read for its imports
  and its frameworks, the source of the runner, llc of a dev run and
  three directories. `anti check`: the source read again for the doc
  blocks and the formatting. `anti doc` and `anti fmt`: the listing of
  the source directory and a source. `anti build`: eight calls of a path
  dependency and the closure of the link. antic: the runtime library
  read for the build id.
- `main.c` of anti: the runtime archive of the data directory for
  `check`, `doc`, `test` and `sdk import`, a manifest that is no TOML
  for four commands, and two usages.
- `anti bind`: four macros of `values.h`, the description
  `api_aliases.json` and `miniaudio.json`.
- `unit_fmt`: five rare forms. `anti symbols`: ten cases of crafted
  archives, `check` and configurations. `conf_threads_no_value`.
- antic: `types_generic_length_folds`, four refusals
  `tests/errors/generic_length_*`, six refusals of the matrix and
  `types_generic_operator_named_through_a_function_type`.

## Findings

The step closed none. Four are new, each with its test listed.

- T159, minor: a chain of more than 32 aliases of a description is
  refused as an alias that names itself.
- T160, minor: a comparison cast to `int` in the length of a generic
  copy stays unfolded, so `Pick<3>` takes no `[1]int`.
- T161, minor: a loop over an `iter` of an unknown type gives a second
  message, that the class has no `iter`.
- T162, minor: no call infers a type argument through `chan T`.

Five tests of a directory anti cannot make show T153 again and are
listed under it. `tests/known-failures.txt` held 84 lines and 65
entries, 63 expected to fail on the Mac, and holds 97 lines and 78
entries, 76 on the Mac. No `[provisional]` entry, and no feature,
option or command.

## Regions nothing reaches

- `src/anti/bindexpr.c` 564: the levels of `binary` pass no other
  operator. `bindtype.c` 552: a declarator meets the bound of 256 levels
  before the parse of a typedef does, at the nestings 254 to 257.
- `src/anti/fmt.c` 127: the lexer refuses a block comment without its
  end before the formatter scans the gap.
- `src/antic/sema_const.c` 227: the shortest digits never end in 0. 236
  to 242: a float below 1 truncates to 0, which every integer type
  holds. 362: the least value prints with its `-`. 576 and 656: the
  checker refuses the cast and the negation before the evaluator. 1225:
  the checker refuses the index, as its comment says. 1379: every kind
  of expression has its case.
- `src/antic/llvm_emit_function.c` 895 to 898 and 947: lowering writes
  `>>` of a simd struct lane by lane, so `IR_VBINARY` carries no right
  shift. 962: `type_suffix` takes the integer folds alone.
- `src/rt/registry.c` 696 to 711: a number the JSON scanner validated
  always reads as a float.

## Left for the next session

- `src/antic/`, 1107 places. `sema_const.c` 60, `sema_generic.c` 47,
  `antl_tree.c` 36, `sema_copies.c` 35, `whole.c` 32, `sema_safety.c`
  31, `driver.c` 28 and `sema_stmt.c` 28 have the most. `sema_const.c`
  keeps the guards of `class_value`, `const_field` and
  `sema_refuse_runtime_parts`.
- `src/anti/`, 234 places: `bindclang.c` and `bindwrite.c` need headers
  of rare forms, `deps.c` a graph of three packages and a repository,
  `repo.c` a fetch over https.
- `src/rt/`, 493 places: the list of the first session stands, apart
  from `conf.c` 346. `conf.c` 365 needs `ANTI_CONF` without a directory
  in the directory of the run, and 667 a failed `anti_rt_getenv`, which
  the kind `env` does not reach.
- `src/antic/driver_link.c` 36 to 54 and 171 to 176 run on a Linux host
  alone, and 133 to 145 on a host without the SDK of Apple's Command
  Line Tools.

## Not run on the target

- The cases `runtime_of_the_data_directory` and
  `sdk_import_sysroot_of_the_data_directory` set `LOCALAPPDATA` for
  Windows. `anti_test_message_fault_object_not_compiled` names
  `llc.exe` there. The suite of anti-windows would show both.

## Gates

- No warning of our code: `rt2-host-b8.log`, `rt2-asan-build.log`,
  `rt2-ubsan-build.log` and `rt2-fin-1.log`.
- Host, 5518 tests, `-I 1,2000` and `-I 2001,5518`: 2000 and 3544
  passed, the second with the fixtures ctest adds. 76 expected failures
  fail as listed, and the two `mimalloc_environment` tests skip:
  `rt2-host-1.log` and `rt2-host-2.log`.
- ASan, 5516 tests, `-I 1,1150`, `1151,2250`, `2251,3800` and
  `3801,5516`: 1150, 1114, 1576 and 1721 passed in 241 s, 174 s, 187 s
  and 524 s, `rt2-asan-1.log` to `-4.log`.
- UBSan, the same parts: all passed in 238 s, 150 s, 141 s and 341 s,
  `rt2-ubsan-1.log` to `-4.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no file of
  `tests/emit-identity/` or `tests/link-identity/` changed.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
571dfe72 Test a generic operator named through a function type, and T162
88468029 Test an option of the runtime without its value and two refusals
8fe763de Keep the new programs of the generic lengths in canonical form
$ git status --short
$ git rev-parse HEAD origin/main
571dfe72b9edf4e65c87f9f1d473d5b8d9b257e7
571dfe72b9edf4e65c87f9f1d473d5b8d9b257e7
```

This report is written after that push, and its commit follows it.
