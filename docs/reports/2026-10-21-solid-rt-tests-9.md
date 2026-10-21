# Step rt-tests, ninth session

The session went on from the list of `2026-10-21-solid-rt-tests-8.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt9-*`. The list of the regions not reached is
`rt8-end-unreached.txt` at the start and `rt9-end-unreached.txt` at the
end. The probes are in `build/rt9/`: `ac.sh` measures antic on one
source, `rtprobe.sh` and `fprobe.sh` one program against the
instrumented runtime, `pl/run.sh` a plugin host and its library, and
`fm.sh` one fault count after another.

## Coverage per directory

Regions reached. The start is `rt8-end-summary.txt`, whose tree differs
from `9d242eca` in no region. The end is `cd260820`,
`rt9-end-summary.txt`, from `./c coverage part 1/5` to `5/5`, every part
of which passed, `rt9-end-1.log` to `-5.log`. 29 places of the list of
the start are reached, and the end lists none that the start did not.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.21% of 9252 | 95.47% of 9252 | 443, 419 | 95.59% | 95.85% |
| `src/antic/` | 98.03% of 55587 | 98.04% of 55587 | 1095, 1089 | 98.06% | 98.09% |
| `src/anti/` | 98.26% of 11663 | 98.28% of 11663 | 203, 201 | 98.30% | 98.31% |

## The tests

- The runtime and plugins: the class of the busy library holds arrays of
  structs, variants, optional values and tuples, and an array of arrays
  of structs, whose widths the loader checks, `d68d0c0e`. A library named
  at start for the logger, which provides the greeting alone,
  `299e4304`. Seventeen interfaces named for libraries at start, past
  the room for sixteen, `b39ebf17`, and a library named for a closed
  program, `efa43f54`.
- The runtime and `tests/std/`: a byte patch with a limit from the start,
  `a717f354`. `if try s.matches(r)` that reaches the match limit, for a
  text and for bytes, `3e28c006`.
- The runtime, `tests/traps/`: a walk stopped by a change whose place
  was never written, `cd860968`.
- antic, matrix refusals: `delete` of a channel parameter without `own`,
  `1b8f01cd`. `is` with two type arguments for one, `e2e3a9a0`. Type
  arguments after a local, `cb249d50`. `+=` on a simd struct of `bool`
  lanes, `ff20e63e`. A generic struct literal whose field names nothing,
  `a4937e3b`.
- antic, matrix program: a constant parameter inferred from a length
  that `size_of` gives, `cd260820`.
- anti: `anti check` on a file without its last newline, `21f756c8`, and
  `--targets all` with a file that does not parse, `06c02d7e`.

`b246d85a` corrects a test. `conf_path_memory` of
`tests/std/faults_start.faults` failed the fifth allocation, the read of
the configuration file, which gives the same message as the copy of the
path of `ANTI_CONF`. The copy is the fourth, and the case now fails it.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
111 lines and 92 entries, 90 expected to fail on the Mac, and holds the
same. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/rt/plugin.c` 580 to 626 apart from the new cases: a record of a
  level or an element outside its image is a damaged library.
- `src/rt/patterns.c` 844 to 849: a pattern literal that reaches its
  match limit has a text, so its length is never 0.
- `src/rt/fault.c` 293 to 301: no file of the runtime, anti or antic
  calls `ANTI_FFLUSH`.
- `src/rt/conf.c` 704: every option and its value are shorter than the
  column of the help.
- `src/antic/sema_expr.c` 1180 to 1186: `tests/diagnostics-open.txt`
  gives the reason. 1281 and 2524: a loop or `to_slice` over a hook whose
  signature failed counts the hook as missing, T161, so the element
  never comes back as an error.
- `src/antic/sema_thread.c` 48 to 50: a static atomic field of another
  module stays a field after the checker resolves its path.
- `src/antic/sema_class.c` 333: a qualifier that names no base and no
  interface is refused before the replacement is checked.
- `src/antic/whole.c` 908 to 910: the first reach of the pass covers the
  table of the injections already, so the second finds nothing new.
- `src/antic/lexer.c` 1570 to 1585: no `parser_expect` names those kinds,
  and the dump of the tokens stops before the end.
- `src/anti/doc.c` 400 and 446: a list holds an item from its first line.
- `src/anti/test.c` 178 to 188: no fault of the kinds `open`, `read`,
  `write`, `close`, `rename`, `stat`, `mkdir`, `remove` and `seek` makes
  the compile of an import fail, `rt9-sweep-test1.log` and
  `rt9-sweep-test2.log`.
- `src/anti/bindclang.c` 1595: a header that ends with an anonymous
  record reaches it. clang then writes a warning of its own into the
  standard error, which the case compares.

## Not run on the target

- The step `two` of `plugin_host` on Windows: the library of the host
  `two` links against its import library, in a directory of its own.
  The suite of anti-windows would show the message.

## Gates

- No warning of our code: `rt9-hb1.log`, `rt9-hb2.log`,
  `rt9-asan-build.log` and `rt9-ubsan-build.log`.
- Host, 5658 tests, `-I 1,2000` and `-I 2001,5658`: 2000 and 3684
  passed, in 354 s and 314 s. Part 2 runs 26 fixtures of part 1 again.
  90 expected failures fail as listed, and the two
  `mimalloc_environment` tests skip: `rt9-host-1.log` and
  `rt9-host-2.log`.
- ASan, 5656 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5656`: 1151, 1114, 1576, 905 and 958 passed in 238 s, 180 s,
  188 s, 445 s and 341 s, `rt9-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1114, 1576, 905 and 958 passed in 149 s,
  145 s, 139 s, 170 s and 299 s, `rt9-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_<target>` pass unchanged.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
cd260820 Test a constant parameter inferred from a length of size_of
a4937e3b Test a generic struct literal that infers from an unknown name
06c02d7e Test anti check --targets all with a file that does not parse
$ git status --short
$ git rev-parse HEAD origin/main
cd260820dc864ceda183205015bb4025b556fe9b
cd260820dc864ceda183205015bb4025b556fe9b
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1089 places: `llvm_emit_function.c`, `sema_generic.c`,
  `driver_link.c`, `sema_const.c`, `antl_tree.c` and `sema_copies.c` have
  the most. Most of those read so far are guards of a checked tree.
- `src/anti/`, 201 places: `bindclang.c`, `bindwrite.c`, `repo.c`,
  `build.c` and `deps.c`.
- `src/rt/`, 419 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `registry.c`.
