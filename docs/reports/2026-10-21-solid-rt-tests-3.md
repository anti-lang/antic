# Step rt-tests, third session

The session went on from the list of `2026-10-21-solid-rt-tests.md`,
which stays, as does the report of the first session. It wrote tests
for regions of `src/antic/`, `src/anti/` and `src/rt/` that a program,
an option or an input reaches. Logs are under `build/drive/logs/`,
named `rt3-*`. The list of the regions not reached is
`rt2-final-unreached.txt` at the start and `rt3-final-unreached.txt` at
the end. The probes are in `build/rt3/`: `ac.sh` compiles one source
with the coverage antic, `ap.sh` runs the coverage anti in a copy of a
project, `rtp.sh` measures one program against the instrumented runtime
and `mchk.sh` checks a matrix file against its expected output.

## Coverage per directory

Regions reached. The start is `571dfe72`, `rt2-final-summary.txt`, the
tree the last report measured, and `6a32bb34` above it changed a report
alone. The end is `591b629b`, `rt3-final-summary.txt`. Every part of the
run passed, `rt3-fin-1.log` to `-5.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 93.29% of 9252 | 93.77% of 9252 | 621, 576 | 93.46% | 93.79% |
| `src/antic/` | 97.81% of 55587 | 97.93% of 55587 | 1217, 1152 | 97.89% | 97.97% |
| `src/anti/` | 97.79% of 11663 | 97.82% of 11663 | 258, 254 | 97.93% | 97.94% |

## The tests

- antic, matrix: the arms of a `switch` on `size_of(bool)`, `char` and
  `f16` beside their numbers and a size with an operator, a `concrete
  fn` without `self` against its base twice, three open functions, five
  runtime parts in the value of an enum, two function types under `==`,
  generic lengths through a `c_long` and `mul_high` of one, a `dispatch`
  past the defaults, two names of the C headers, conversions the
  optimizer folds, constant float arrays and a bitfield of a whole width.
- antic, elsewhere: the traps `overflow_borrow` and
  `bitfield_write_one_bit`, the modules test `symbolic_lengths`, a
  synchronized class with `construct` and `destruct` in `views.h`, and
  a false constant and a variant of two fields in the `forms` doc page.
- anti: `anti check` of the project `one-warning`, `anti symbols
  resolve` of a frame of capital digits, `anti bind` of a union that a
  typedef names alone and of an enum of a signed base of 64 bits.
- runtime: `std/regex_patch_edges`, `std/regex_empty_steps` and
  `std/deserialize_float_range`, a union and a pattern field in
  `collection_text`, a class value written as `null` in
  `deserialize_parts`, the trap `pattern_patch_into_two_groups`, two
  used slots of an interface under `--anti.inspect` and
  `conf_outside_key`.

## Findings

The step closed none. Two are new, each with its tests listed.

- T163, minor: `operator_spelling` of `src/antic/types.c` writes `<<%`,
  `+|` and `mul_high` of a size as `>=`. `error_layout_text_wrapping`,
  `_saturating` and `_mul_high` hold it.
- T164, minor: `serialize` writes an infinite float as `inf`, which is
  no JSON, and `Object.deserialize` refuses the text.
  `std_serialize_infinite_float` holds it.

`tests/known-failures.txt` held 97 lines and 78 entries, 76 expected to
fail on the Mac, and holds 101 lines and 82 entries, 80 on the Mac. No
`[provisional]` entry, and no feature, option or command.

## Regions nothing reaches

- `src/antic/header.c` 192 to 195: a slice crosses no C signature, so
  an exported parameter, result or tuple that holds one is refused
  first. 922: a class body declares no union.
- `src/antic/lexer.c` 1570 to 1585: no call of the parser expects a
  literal or a comment by its kind, and the token dump stops before the
  end of file.
- `src/antic/docpage.c` 271: the parser takes no `...` without a
  parameter before it.
- `src/anti/check.c` 379: the front end class refuses a file that does
  not lex, and the first failing class ends the run.
- `src/anti/bindwrite.c` 350: clang refuses a variadic function without
  a parameter before `...` in the C that `anti bind --clang` reads, as
  `int f(...);` showed.

## Questions for Eddie

- A length of a generic copy `[N <<% (N - 4)]int` with `N = 3` leaves
  `3 <<% -1`, and the back end refuses it as the length 0. The same
  length with `<<` is refused as a shift out of range. Should a copy
  refuse `<<%` by a count below zero as a constant `1 <<% -1` is?

## Left for the next session

- `src/antic/`, 1046 places. `llvm_emit_function.c` 55,
  `sema_generic.c` 47, `driver_link.c` 43, `sema_const.c` 39,
  `sema_expr.c` 38, `antl_tree.c` 36 and `sema_copies.c` 35 have the
  most. Most of `llvm_emit_function.c` is the failure of a translation
  that `llvm_verify` never meets, and most of `sema_copies.c` guards a
  copy whose checks failed already.
- `src/anti/`, 230 places: `bindclang.c` 40 and `bindwrite.c` 33 need
  headers of rare forms, `deps.c` 31 a graph of three packages.
- `src/rt/`, 464 places: `trace.c` 85 and `symbols.c` 55 are mostly ELF
  and PE, `plugin.c` 70 needs damaged libraries, and `registry.c` 42 and
  `object.c` 40 are mostly guards of a descriptor.
- `src/antic/driver_link.c` 36 to 54 and 171 to 176 run on a Linux host
  alone, as the second report says.

## Not run on the target

- No change of this session runs on a Linux or a Windows machine alone.

## Gates

- No warning of our code: `rt3-host-b1.log` to `-b4.log`,
  `rt3-asan-build.log`, `rt3-ubsan-build.log` and `rt3-fin-1.log`.
- Host, 5563 tests, `-I 1,2000` and `-I 2001,5563`: 2000 and 3589
  passed, the second with the fixtures ctest adds. 80 expected failures
  fail as listed, and the two `mimalloc_environment` tests skip:
  `rt3-host-1.log` and `rt3-host-2.log`.
- ASan, 5561 tests, `-I 1,1150`, `1151,2250`, `2251,3800` and
  `3801,5561`: 1150, 1114, 1576 and 1766 passed in 236 s, 173 s, 183 s
  and 528 s, `rt3-asan-1.log` to `-4.log`. The last part stands near
  the limit of a call, so the next run splits it.
- UBSan, the same parts: all passed in 145 s, 117 s, 132 s and 338 s,
  `rt3-ubsan-1.log` to `-4.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no file of
  `tests/emit-identity/` or `tests/link-identity/` changed.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
591b629b Test two slots of an injected interface and a key outside the runtime
32b2846e Test a number past the range of the float field it fills
96c7fc5e Record T164: serialize writes an infinite float that deserialize refuses
$ git status --short
$ git rev-parse HEAD origin/main
591b629b9c1da2a5d3741d7966be9d24b17c6d14
591b629b9c1da2a5d3741d7966be9d24b17c6d14
```

This report is written after that push, and its commit follows it.
