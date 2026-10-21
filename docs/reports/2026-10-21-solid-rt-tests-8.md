# Step rt-tests, eighth session

The session went on from the list of `2026-10-21-solid-rt-tests-7.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt8-*`. The list of the regions not reached is
`rt7-end-unreached.txt` at the start and `rt8-end-unreached.txt` at the
end. The probes are in `build/rt8/`: `cr.sh` measures one run of antic
or anti against the list of the start, `rtprobe.sh` one program against
the instrumented runtime, and `fm.sh` with `sweep.sh` one fault count
after another.

## Coverage per directory

Regions reached. The start is `61f04907`, `rt7-end-summary.txt`, whose
tree is that of `791fcf4b`. The end is `b840d02e`, `rt8-end-summary.txt`.
Every part of the run passed but one test, `rt8-end-1.log` to `-5.log`:
`fmt_canonical` refused three of the new files, and `03c2eca4` writes
them in the form of `anti fmt`. That commit changes no region.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.16% of 9252 | 95.21% of 9252 | 448, 443 | 95.52% | 95.59% |
| `src/antic/` | 97.99% of 55587 | 98.03% of 55587 | 1119, 1095 | 98.03% | 98.06% |
| `src/anti/` | 98.22% of 11663 | 98.26% of 11663 | 208, 203 | 98.24% | 98.30% |

## The tests

- antic, matrix refusals: an operator whose operand is a tuple of one
  function type twice, `3d79760f`. An array literal that gives out a
  pointer into the fields, `3eb5240d`. The `ptr` of a slice of a guarded
  array used after its `sync`, `ed9b9708`. An unchecked optional value on
  the right of an operator, `872c7268`. A class field followed by no
  comma, `6e720423`. A type alias without a name, `de7a5e5c`, and without
  its semicolon, `b5c776d5`. A `defer` of a statement that does not
  parse, `55404ea4`. A switch arm whose handler does not parse,
  `ee263b94`.
- antic, matrix program: a thread-safe class points a parameter and a
  field at its fields, which stays inside the object, `05c97748`.
- antic and the runtime, `tests/std/`: `destroy(p, from)` of a struct
  that holds a class value with an `own` field, `509d88d7`. A replace with
  a template of a pattern that reaches its match limit, from the start and
  with a limit from the end, `e76128b2`. `toml_subset` takes an array with
  an empty element, `d57ddb1f`, and `regex_classes` four classes of byte
  patterns, `b840d02e`.
- anti: a licence file that the manifest names and that is missing, and a
  fault of its second read for the notice, `73a1666d`.

The case of the template replace first stood in
`tests/programs/pattern_replace.anti`, whose hash `emit_identity` pins.
It moved to `tests/std/`, so no file of `tests/emit-identity/` or
`tests/link-identity/` changed, and both tests pass.

## Fault sweeps

`rt8-sweep-build.log`, `rt8-sweep-cmds.log`, `rt8-sweep-cmds2.log` and
`rt8-sweep-antic.log` hold every count of the kinds `open`, `read`,
`write`, `close`, `remove`, `rename`, `mkdir`, `stat` and `seek` over
`anti build`, `build --release`, `check`, `doc`, `doc --markdown`, `fmt`,
`test` and `license --project` of a project with a licence file, and over
antic for a program of macOS, Linux and Windows, a release program and
the three kinds of library. Only the licence file gave a region nothing
reached, the case of `73a1666d`.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
111 lines and 92 entries, 90 expected to fail on the Mac, and holds the
same. No feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/anti/repo.c` 412 to 434, `repo_module`: `lock_package_valid` and
  `lock_module_valid` of `deps.c` refuse a lock file, and the reader of an
  index refuses an index, whose name, version, module or digest is not of
  its grammar before any fetch.
- `src/anti/build.c` 1296 to 1300 and 1387 to 1390: the host is a target
  of Anti on the Mac.
- `src/anti/fmt.c` 127: the lexer refuses a block comment that does not
  end, so a gap holds none. 1369 to 1371: each doc line follows a flush
  that cleared the blank mark.
- `src/rt/toml.c` 333 to 336: a table name is 24 bytes shorter than its
  room, which holds the dot and an index of 19 digits.
- `src/antic/parser_decl.c` 507 to 511: `tests/diagnostics-open.txt`
  gives the reason.

## Not run on the target

- `anti_build_message_fault_license_text_unread_for_the_notice` on Linux
  and Windows: the count of opens may differ there. The case would show
  the message of the second read.

## Gates

- No warning of our code: `rt8-hb1.log` to `-hb8.log`,
  `rt8-asan-build.log` and `rt8-ubsan-build.log`.
- Host, 5644 tests, `-I 1,2000` and `-I 2001,5644`: 2000 and 3670
  passed, in 285 s and 308 s. Part 2 runs 26 fixtures of part 1 again.
  90 expected failures fail as listed, and the two
  `mimalloc_environment` tests skip: `rt8-host-1.log` and
  `rt8-host-2.log`.
- ASan, 5642 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5642`: 1151, 1114, 1576, 905 and 944 passed in 240 s, 182 s,
  184 s, 438 s and 341 s, `rt8-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1114, 1576, 905 and 944 passed in 241 s,
  160 s, 141 s, 172 s and 294 s, `rt8-ubsan-1.log` to `-5.log`.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
03c2eca4 Write three new matrix refusals in the form of anti fmt
b840d02e Test four more classes of byte patterns
e76128b2 Test an expensive pattern under a template replace
$ git status --short
$ git rev-parse HEAD origin/main
03c2eca4760461a26b4e2ca71fdd3fffa3dbf187
03c2eca4760461a26b4e2ca71fdd3fffa3dbf187
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1095 places: `llvm_emit_function.c`, `sema_generic.c`,
  `driver_link.c`, `sema_const.c`, `antl_tree.c` and `sema_copies.c` have
  the most. Most of those read so far are guards of a checked tree.
- `src/anti/`, 203 places: `bindclang.c`, `bindwrite.c`, `build.c` and
  `deps.c`.
- `src/rt/`, 443 places: `trace.c`, mostly ELF and PE, `plugin.c` and
  `object.c`.
