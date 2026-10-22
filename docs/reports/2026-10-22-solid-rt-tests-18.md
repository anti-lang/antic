# Step rt-tests, eighteenth session

The session went on from the list of `2026-10-22-solid-rt-tests-17.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/anti/` and `src/rt/` that a program, a command or an
injected fault reaches, and gave the reason for regions nothing reaches.
Logs are under `build/drive/logs/`, named `rt18-*`. The list of the
regions not reached is `rt17-end-unreached.txt` at the start and
`rt18-end-unreached.txt` at the end. The probes and scripts are in
`build/rt18/`.

## Coverage per directory

Regions reached. The start is `rt17-end-summary.txt`, measured at
`2deaeca7`. The end is `rt18-end-summary.txt`, measured at `7bcd2caa`
with `./c coverage part 1/5` to `5/5`, `rt18-end-1.log` to `-5.log`.
`2ae2819b` came after it and reaches `project.c` 95 as well, which the
probe `build/rt18/sc/new7` shows.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.94% of 9252 | 96.00% of 9252 | 376, 370 | 96.18% | 96.18% |
| `src/antic/` | 98.20% of 55587 | 98.20% of 55587 | 1003, 1003 | 98.26% | 98.26% |
| `src/anti/` | 98.92% of 11663 | 99.07% of 11663 | 126, 109 | 98.93% | 99.06% |

## The tests

- anti, `fault_sdk_export_listing_unread` of `tests/anti_messages.cmake`:
  the listing of the SDKs of the Command Line Tools fails, `sdk_export`
  of `sdk.c`, `8fb05193`. Its message left `tests/diagnostics-open.txt`
  in `7bcd2caa`.
- anti, `fault_new_test_directory_not_made`: anti new cannot make the
  directory `test`, `project_new` of `project.c`, `8fb05193` and
  `2ae2819b`. It is a test of T153 and is listed under it.
- anti, `fault_shim_not_written` of `tests/bind/messages.cmake`: the shim
  of a static inline function cannot be written, `bind_run` of `bind.c`,
  `40d91e6a`.
- anti, `fault_import_not_compiled` of `tests/anti-test/messages.cmake`:
  the library file of `anti.text` that the runner imports does not
  compile, `compile_imports` of `test.c`, `b259e950`.
- anti, the fourth part of `anti_build_resolve`: a pick taken back whose
  two dependencies need one another, `graph_remove` of `deps.c`,
  `e1719819`.
- anti, `unit_syms`: a map whose last line ends without a newline,
  `syms_map_lookup` of `syms.c`, `369cc3a3`.
- anti, `anti_bind_forms`: a record that holds a record of zeros
  twice, `held_none` and `on_path` of `bindwrite.c`, `2e7a08f2`.
- runtime, `std_regex_test_other_limits`: the limits of depth and of the
  heap of PCRE2, `failure` of `patterns.c`, `78697137`.
- runtime, `std_regex_classes`: a byte pattern whose `\Q...\E` holds a
  wide character and `~`, `is_alnum` of `regex.c`, `aef0a992`.
- runtime, `matrix_closures_snapshot_on_the_heap_keeps_the_nul_of_an_empty_text`:
  `anti_rt_snapshot_text` of `snapshot.c` for a text of no bytes,
  `44d9c2e2`.

## Findings

The step closed none and recorded none. `tests/known-failures.txt` held
132 lines and 113 entries, 111 expected to fail on the Mac. It holds 133
lines and 114 entries, 112 expected to fail on the Mac. No feature,
option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/anti/doc.c` 400 and 446: `item` is true whenever `listed` is,
  since a bullet sets both and they are cleared together. 480:
  `write_doc` returns before for an empty text.
- `src/anti/licensing.c` 282: each path of the listing names the
  directory `licenses/`. 472 and 473: a field of an ar header holds 15
  digits at most.
- `src/anti/units.c` 233: every unit not placed imports another not
  placed. 276: a list that is not ordered holds a unit not placed, and
  the loop stops at it.
- `src/anti/bindexpr.c` 722 to 724: the switch has a case for each of
  the three kinds of value.
- `src/anti/bindwrite.c` 439 to 445: clang gives an array parameter its
  pointer type, so no C type of a parameter holds a bracket without `(*`.
- `src/anti/check.c` 323 and 375: the front-end class runs first, and a
  file that does not lex fails it and ends the run.
- `src/anti/` `target_host` and `unit_host`: `bind.c` 41, `check.c` 61,
  62 and 465, `doc.c` 851, `test.c` 58, 59 and 356, `units.c` 38 to 40,
  `main.c` 421, `sdk.c` 145, `build.c` 1296 to 1300 and 1387 to 1390.
  Each fails only on a host that is no target of Anti.
- `src/anti/files.c` 25, 42 and 59: no list reaches `SIZE_MAX / size`
  elements. `zip.c` 149 to 152: an archive of more than 4 GiB.
- `src/antic/driver.c` 443: `anti check` promotes every warning but the
  doc warnings, and the lexer and the parser give no warning.
- `src/antic/platform.c` 50, 1026 and 1027: `antic_message` reaches
  them, and the `_Exit` of `work_not_waited` writes no profile.
- `src/antic/sema_copies.c` 1385 to 1387: no slice or array meets `iter`,
  `Iterable` or `Iterator`, and a named set names constraints alone.
- `src/antic/sema_const.c` 689, the `1`: a constant of a pointer type is
  `none`, since `.ptr` of a text and a function are no constants.
- `src/antic/llvm_emit_function.c` 147, 388, 393, 473 to 477, 555, 559,
  613, 774, 902, 1051, 1358, 1399, 1684 and 1740: each guard follows
  `unexpected`, which alone sets `failed`, and 48 to 60 are not reached.
- `src/rt/text.c` 424, the `-1`: `out` has a lower point than `high` only
  when `high` is a power of ten with an odd `m`. Below every power of ten,
  `1e23` alone has such a `high`, and its `m` is even.

## Not run on the target

- `fault_new_test_directory_not_made` counts the directories of the Mac.

## Gates

- No warning of our code: `rt18-hb0.log` to `-hb8.log`, `rt18-cb0.log`,
  `rt18-asan-build.log` and `rt18-ubsan-build.log`.
- Host, 5781 tests, `-I 1,2000` and `-I 2001,5781`: 2000 and 3808 passed,
  in 366 s and 322 s. Part 2 runs 27 fixtures of part 1 again. 112
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt18-host-1.log` and `rt18-host-2.log`.
- ASan, 5779 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5779`: 1151, 1106, 1577, 906 and 1081 passed in 238 s, 186 s,
  187 s, 447 s and 349 s, `rt18-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1081 passed in 237 s,
  163 s, 140 s, 173 s and 296 s, `rt18-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report.

## State at the push

```text
$ git log --oneline -3
2ae2819b Make the seventh mkdir of anti new fail for the directory test
7bcd2caa Take the message of an empty SDK listing off the open list
2e7a08f2 Test the probe of a record that holds a record of zeros twice
$ git status --short
$ git rev-parse HEAD origin/main
2ae2819b754e6d8126296313d126ec558039bd5a
2ae2819b754e6d8126296313d126ec558039bd5a
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 108 places. `bindwrite.c` 18, `fmt.c` 12, among them
  `emit_comment` 1470 and `flush_doc_line` 1369, which no input of the
  probes of `build/rt18/f` reached. `repo.c` 349 and 412 to 434 need a
  library file whose header `antic --dependency` writes.
- `src/antic/`, 1003 places. `build/rt18/untriaged.py` lists those no
  report has given a reason for. `sema_generic.c`, `sema_copies.c`,
  `whole_tables.c`, `coff.c`, `antl_tree.c` and `antl_read_ir.c` hold the
  most.
- `src/rt/`, 370 places: `trace.c` of ELF and PE, `object.c` 243 and
  341, `sync.c` 191, which needs a channel of owning values whose handle
  is null, and `plugin.c` 642, 719 and 864, which need a damaged library.

CONTINUE
