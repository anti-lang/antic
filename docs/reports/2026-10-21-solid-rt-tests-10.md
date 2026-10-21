# Step rt-tests, tenth session

The session went on from the list of `2026-10-21-solid-rt-tests-9.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt10-*`. The list of the regions not reached is
`rt9-end-unreached.txt` at the start and `rt10-end-unreached.txt` at the
end. The probes are in `build/rt10/`, the scripts of `build/rt9/` aimed
at the list of the start. `explained.py` there lists the places of a list
that no report of the step names under its regions nothing reaches.

## Coverage per directory

Regions reached. The start is `rt9-end-summary.txt`, measured at
`cd260820`, whose sources equal those of `cc9ae264`. The end is
`8dffab78`, `rt10-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, every part of which passed, `rt10-end-1.log` to `-5.log`. 9
places of the list of the start are reached, and the end lists none that
the start did not.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.47% of 9252 | 95.48% of 9252 | 419, 418 | 95.85% | 95.87% |
| `src/antic/` | 98.04% of 55587 | 98.06% of 55587 | 1089, 1081 | 98.09% | 98.10% |
| `src/anti/` | 98.28% of 11663 | 98.28% of 11663 | 201, 201 | 98.31% | 98.31% |

## The tests

- antic, matrix refusals: `try` before a local that `&` takes, which
  the checker reads as a place, `09ad9758`. `count` of a `List` written
  through a `use` field of a class, `6b844854`.
- antic, matrix program: a `construct` whose `if` sets the field and
  ends each branch with `return;`, `db4543b7`.
- antic, `tests/errors/`: a case and a literal of a case of the generic
  variant `Maybe` of another module with two type arguments, and its
  generic class `Stack` built by a call that gives none, `ae85b7c4`.
- The runtime, `tests/inject/`: the line of `[injections]` that follows
  an array value of `[runtime]`, which the reader of the array leaves as
  a key of its own, `8dffab78`.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
111 lines and 92 entries, 90 expected to fail on the Mac, and holds the
same. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/rt/toml.c` 332 to 336: a table name is shorter than 232 bytes,
  so its number of at most 20 digits always fits the 256 of the buffer.
- `src/rt/regex.c` 594, the `-1`: a name that PCRE2 calls not unique
  stands in the table of names, so the scan always finds it.
- `src/antic/sema_value.c` 216 to 219: the first field that owns
  something is a class, a struct, a tuple or a variant. A field of a
  type parameter or of `own fn` owns nothing in the checked tree.

## Gates

- No warning of our code: `rt10-hb1.log`, `rt10-hb2.log`,
  `rt10-asan-build.log` and `rt10-ubsan-build.log`.
- Host, 5667 tests, `-I 1,2000` and `-I 2001,5667`: 2000 and 3693
  passed, in 359 s and 322 s. Part 2 runs 26 fixtures of part 1 again.
  90 expected failures fail as listed, and the two
  `mimalloc_environment` tests skip: `rt10-host-1.log` and
  `rt10-host-2.log`. Homebrew replaced cmake with 4.4.4 during the first
  run of part 2, which left 1440 tests without their command. The tree
  was configured again and both parts ran again.
- ASan, 5665 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5665`: 1151, 1105, 1576, 905 and 967 passed in 244 s, 187 s,
  187 s, 450 s and 363 s, `rt10-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1105, 1576, 905 and 967 passed in 246 s,
  178 s, 151 s, 181 s and 309 s, `rt10-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
8dffab78 Test an injection that follows an array of the runtime table
ae85b7c4 Test generic items of another module with the wrong type arguments
db4543b7 Test a construct whose if returns on every branch
$ git status --short
$ git rev-parse HEAD origin/main
8dffab786295cebc6ea9b1db0c9a5230512abb64
8dffab786295cebc6ea9b1db0c9a5230512abb64
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1081 places: `llvm_emit_function.c`, `sema_generic.c`,
  `driver_link.c`, `sema_const.c`, `antl_tree.c` and `sema_copies.c` have
  the most. Most of those read so far are guards of a checked tree.
- `src/anti/`, 201 places: `bindclang.c`, `bindwrite.c`, `repo.c`,
  `build.c` and `deps.c`.
- `src/rt/`, 418 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `registry.c`.
