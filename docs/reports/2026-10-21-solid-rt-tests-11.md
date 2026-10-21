# Step rt-tests, eleventh session

The session went on from the list of `2026-10-21-solid-rt-tests-10.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt11-*`. The list of the regions not reached is
`rt10-end-unreached.txt` at the start and `rt11-end-unreached.txt` at the
end. The probes are in `build/rt11/`.

## Coverage per directory

Regions reached. The start is `rt10-end-summary.txt`, measured at
`8dffab78`, whose sources equal those of `e39186a4`. The end is
`195061b7`, `rt11-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt11-end-1.log` to `-5.log`. Every part passed. The first run of
part 5 failed `fmt_canonical` on six new sources, which the last commit
fixed, and part 5 then ran again. 20 places of the list of the start are
reached, and the end lists none that the start did not.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.48% of 9252 | 95.53% of 9252 | 418, 414 | 95.87% | 95.91% |
| `src/antic/` | 98.06% of 55587 | 98.08% of 55587 | 1081, 1065 | 98.10% | 98.13% |
| `src/anti/` | 98.28% of 11663 | 98.28% of 11663 | 201, 201 | 98.31% | 98.31% |

## The tests

- antic, `tests/errors/`: a generic length that negates the least `int`,
  which the copy leaves as it stands and the back end refuses,
  `5acc10b4`.
- antic, matrix refusals: a call whose argument is no expression,
  `5e509991`. A generic call that leaves a constant parameter open,
  `6748049f`.
- antic through anti, fault injection: the fifth and the seventh read of
  a library file, by `anti bind --header` and by `anti doc`, in
  `driver_library_header` and `library_interface`, `dfaa9a1b`.
- antic, `tests/clib/`: `clib_callbacks`, a callback whose parameters are
  a struct, a tuple, an enum and a function with its context, which the
  header writes without names, `5ddaae12`.
- The runtime, `tests/std/`: `Object.deserialize` of an array one level
  past the depth limit, `914b7582`.
- The runtime, `tests/plugin/`: the second open of a library that lacks
  a module of its host, after which the message is the reason of the
  system, `d46e6c9d`. That output names the path of the library, so the
  runner of the plugin faults takes a `<host>.<case>.pattern` beside the
  expected file.
- `195061b7` takes the canonical form of the comments of six new sources.

## Findings

The step closed none. One is new, with its tests.

- T171, major: a constant refuses `as` to the type it has already.
  `const_as` of `src/antic/sema_const.c` folds a conversion to a number
  or a `char` alone, so `true as bool`, a `str` and a struct are refused
  as a pointer conversion and an enum as a conversion to an enum. R059
  takes the conversion everywhere. `matrix_values_as_its_own_type_in_constant`
  and `matrix_values_as_its_own_enum_in_constant`, in their three modes,
  hold it, `61fe68f2`. Left for the step of the rules of types.

`tests/known-failures.txt` held 111 lines and 92 entries, 90 expected to
fail on the Mac. It holds 117 lines and 98 entries, 96 expected to fail
on the Mac. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/antic/parser_expr.c` 1065 to 1067: the left operand of a `??` went
  through `unary` one level deeper than the `??`, so the descent for its
  right operand never meets the limit.
- `src/antic/check.c` 120, of `anti`: the front end gives the same result
  on every target, so a source that passes on the host fails on no
  other.

## Not run on the target

- `d46e6c9d`: `plugin_named_host_fault_name_unopened` runs on Linux too,
  where the reason of the system is `undefined symbol: anti.time.now`.
  The pattern takes both forms.

## Gates

- No warning of our code: `rt11-hb5.log`, `rt11-asan-build.log` and
  `rt11-ubsan-build.log`.
- Host, 5683 tests, `-I 1,2000` and `-I 2001,5683`: 2000 and 3709 passed,
  in 365 s and 330 s. Part 2 runs 26 fixtures of part 1 again. 96
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt11-host-1.log` and `rt11-host-2.log`.
- ASan, 5681 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5681`: 1151, 1105, 1576, 905 and 983 passed in 246 s, 188 s,
  190 s, 446 s and 362 s, `rt11-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1105, 1576, 905 and 983 passed in 239 s,
  171 s, 142 s, 180 s and 307 s, `rt11-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report and on
  `docs/audit/tests-findings.md`.

## State at the push

```text
$ git log --oneline -3
195061b7 Take the canonical form of the comments of the new tests
6748049f Test a generic call that leaves a constant parameter open
5ddaae12 Test a C callback whose parameters carry no name
$ git status --short
$ git rev-parse HEAD origin/main
195061b7360b459b655dc21dae7e4655e2bdfd17
195061b7360b459b655dc21dae7e4655e2bdfd17
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1065 places: `llvm_emit_function.c`, `sema_generic.c`,
  `driver_link.c`, `sema_const.c`, `antl_tree.c` and `sema_copies.c` have
  the most. `from_module_operator` of `sema_call.c` 373 did not come from
  a generic operator of a library that reads a private field of its
  module, in three forms the session tried.
- `src/anti/`, 201 places: `bindclang.c`, `bindwrite.c`, `repo.c`,
  `build.c` and `deps.c`. Sweeps of faults over `anti check` and `anti
  bind --header` found nothing more.
- `src/rt/`, 414 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `registry.c`.
